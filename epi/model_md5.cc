//----------------------------------------------------------------------------
//  EPI MD5 Model Loader – implementation
//----------------------------------------------------------------------------
//
//  Copyright (c) 2026  The EDGE Team.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//----------------------------------------------------------------------------
//
//  The md5_conv logic from EDGE has been refactored here:
//
//    1.  Text parsing is done with a simple linear scan rather than
//        the original sscanf-based approach, which had undefined
//        behaviour on truncated inputs.
//    2.  The quaternion w-component reconstruction formula is identical
//        to the original md5_conv: w = -sqrt(max(0, 1 - x²-y²-z²)).
//    3.  Vertex skinning (bind-pose pose computation) is the same linear
//        blend skinning used by the original EDGE md5_conv tool.
//
//----------------------------------------------------------------------------

#include "epi.h"
#include "model_md5.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace epi
{

//------------------------------------------------------------------------
// Text parsing helpers
//------------------------------------------------------------------------

// Advance past whitespace (spaces, tabs, newlines)
static const char *SkipWS(const char *p)
{
	while (*p && isspace((unsigned char)*p)) p++;
	return p;
}

// Advance past rest of line (for comments)
static const char *SkipLine(const char *p)
{
	while (*p && *p != '\n') p++;
	if (*p == '\n') p++;
	return p;
}

// Read a quoted string into 'out'; returns pointer after closing '"'
static const char *ReadQuotedStr(const char *p, std::string &out)
{
	out.clear();
	p = SkipWS(p);
	if (*p != '"') return p;
	p++;
	while (*p && *p != '"') { out += *p++; }
	if (*p == '"') p++;
	return p;
}

// Read an integer; returns pointer after the value
static const char *ReadInt(const char *p, int &out)
{
	p = SkipWS(p);
	char *end;
	out = (int)strtol(p, &end, 10);
	return end;
}

// Read a float; returns pointer after the value
static const char *ReadFloat(const char *p, float &out)
{
	p = SkipWS(p);
	char *end;
	out = strtof(p, &end);
	return end;
}

// Skip to next '{' ; returns pointer past it
static const char *SkipToBlock(const char *p)
{
	while (*p && *p != '{') p++;
	if (*p == '{') p++;
	return p;
}

// Check if the next non-whitespace token matches 'keyword'
static bool MatchToken(const char *p, const char *keyword)
{
	p = SkipWS(p);
	size_t n = strlen(keyword);
	return strncmp(p, keyword, n) == 0 && !isalnum((unsigned char)p[n]) && p[n] != '_';
}


//------------------------------------------------------------------------
// quat_c helpers for MD5
//------------------------------------------------------------------------

// Reconstruct w from (x,y,z) as in the original md5_conv tool
static quat_c MD5Quat(float x, float y, float z)
{
	float t = 1.0f - x*x - y*y - z*z;
	float w = (t < 0.0f) ? 0.0f : -sqrtf(t);
	return quat_c(x, y, z, w);
}

// Rotate a point through a quaternion: q * v * q^-1
// Duplicates the EDGE md5_conv transform used in EDGE's w_model.cc
static vec3_c QuatRotate(const quat_c &q, const vec3_c &v)
{
	// Using the rodrigues formula: v' = 2(q.xyz × (q.xyz × v + q.w*v)) + v
	vec3_c qv(q.x, q.y, q.z);
	vec3_c t = qv.Cross(v) * 2.0f;
	return v + t * q.w + qv.Cross(t);
}


//------------------------------------------------------------------------
// MD5Loader::Probe
//------------------------------------------------------------------------

bool MD5Loader::Probe(file_c *f)
{
	char buf[32] = {};
	if (f->Read(buf, 31) < 3)
		return false;
	// MD5 mesh files start with "MD5Version "
	return strncmp(buf, "MD5Version", 10) == 0;
}


//------------------------------------------------------------------------
// MD5Loader::ParseMesh
//------------------------------------------------------------------------

bool MD5Loader::ParseMesh(const char *text, size_t /*len*/,
                           std::vector<md5_joint_t> &joints,
                           std::vector<md5_mesh_t>  &meshes)
{
	const char *p = text;

	// Find "numJoints"
	int num_joints = 0, num_meshes = 0;

	while (*p)
	{
		p = SkipWS(p);
		if (!*p) break;

		// Skip comments
		if (p[0] == '/' && p[1] == '/')
		{
			p = SkipLine(p);
			continue;
		}

		if (MatchToken(p, "MD5Version"))
		{
			p = SkipLine(p);  // version is validated by Probe
		}
		else if (MatchToken(p, "commandline"))
		{
			p = SkipLine(p);
		}
		else if (MatchToken(p, "numJoints"))
		{
			p += 9;
			p = ReadInt(p, num_joints);
			joints.reserve((size_t)num_joints);
		}
		else if (MatchToken(p, "numMeshes"))
		{
			p += 9;
			p = ReadInt(p, num_meshes);
			meshes.reserve((size_t)num_meshes);
		}
		else if (MatchToken(p, "joints"))
		{
			p += 6;
			p = SkipToBlock(p);

			// Each line: "name" parent ( px py pz ) ( qx qy qz )
			while (*p && *p != '}')
			{
				p = SkipWS(p);
				if (*p == '}') break;
				if (p[0] == '/' && p[1] == '/') { p = SkipLine(p); continue; }

				md5_joint_t j;
				p = ReadQuotedStr(p, j.name);
				p = ReadInt(p, j.parent);

				float px, py, pz, qx, qy, qz;
				p = SkipWS(p); if (*p=='(') p++;
				p = ReadFloat(p, px); p = ReadFloat(p, py); p = ReadFloat(p, pz);
				p = SkipWS(p); if (*p==')') p++;
				p = SkipWS(p); if (*p=='(') p++;
				p = ReadFloat(p, qx); p = ReadFloat(p, qy); p = ReadFloat(p, qz);
				p = SkipWS(p); if (*p==')') p++;

				j.pos    = vec3_c(px, py, pz);
				j.orient = MD5Quat(qx, qy, qz);

				joints.push_back(j);
				p = SkipLine(p);
			}
			if (*p == '}') p++;
		}
		else if (MatchToken(p, "mesh"))
		{
			p += 4;
			p = SkipToBlock(p);

			md5_mesh_t mesh;

			while (*p && *p != '}')
			{
				p = SkipWS(p);
				if (*p == '}') break;
				if (p[0] == '/' && p[1] == '/') { p = SkipLine(p); continue; }

				if (MatchToken(p, "shader"))
				{
					p += 6;
					p = ReadQuotedStr(p, mesh.shader);
				}
				else if (MatchToken(p, "numverts"))
				{
					p += 8;
					int n; p = ReadInt(p, n);
					mesh.verts.reserve((size_t)n);
				}
				else if (MatchToken(p, "vert"))
				{
					p += 4;
					int idx; p = ReadInt(p, idx);
					float u, v;
					p = SkipWS(p); if (*p=='(') p++;
					p = ReadFloat(p, u); p = ReadFloat(p, v);
					p = SkipWS(p); if (*p==')') p++;
					int ws, wc;
					p = ReadInt(p, ws); p = ReadInt(p, wc);

					md5_vertex_t mv;
					mv.uv           = vec2_c(u, v);
					mv.weight_start = ws;
					mv.weight_count = wc;
					mesh.verts.push_back(mv);
				}
				else if (MatchToken(p, "numtris"))
				{
					p += 7;
					int n; p = ReadInt(p, n);
					mesh.tris.reserve((size_t)n);
				}
				else if (MatchToken(p, "tri"))
				{
					p += 3;
					int idx, a, b, c;
					p = ReadInt(p, idx);
					p = ReadInt(p, a); p = ReadInt(p, b); p = ReadInt(p, c);
					mesh.tris.push_back(model_tri_c((u16_t)a, (u16_t)b, (u16_t)c));
				}
				else if (MatchToken(p, "numweights"))
				{
					p += 10;
					int n; p = ReadInt(p, n);
					mesh.weights.reserve((size_t)n);
				}
				else if (MatchToken(p, "weight"))
				{
					p += 6;
					int idx, ji; float bias, wx, wy, wz;
					p = ReadInt(p, idx);
					p = ReadInt(p, ji);
					p = ReadFloat(p, bias);
					p = SkipWS(p); if (*p=='(') p++;
					p = ReadFloat(p, wx); p = ReadFloat(p, wy); p = ReadFloat(p, wz);
					p = SkipWS(p); if (*p==')') p++;

					md5_weight_t w;
					w.joint_index = ji;
					w.bias        = bias;
					w.pos         = vec3_c(wx, wy, wz);
					mesh.weights.push_back(w);
				}

				p = SkipLine(p);
			}
			if (*p == '}') p++;

			meshes.push_back(std::move(mesh));
		}
		else
		{
			p = SkipLine(p);
		}
	}

	return (!joints.empty() && !meshes.empty());
}


//------------------------------------------------------------------------
// MD5Loader::SkinMesh  –  linear blend skinning (bind pose)
//------------------------------------------------------------------------
//
// Identical in result to EDGE's original md5_conv vertex transform:
//   pos = Σ_w  ( orient_w * weight_w.pos + pos_w )  *  weight_w.bias
//
void MD5Loader::SkinMesh(const md5_mesh_t             &mesh,
                          const std::vector<md5_joint_t> &joints,
                          std::vector<model_vert_c>      &out)
{
	out.resize(mesh.verts.size());

	for (size_t vi = 0; vi < mesh.verts.size(); vi++)
	{
		const md5_vertex_t &mv = mesh.verts[vi];
		vec3_c pos;

		for (int wi = 0; wi < mv.weight_count; wi++)
		{
			const md5_weight_t &w = mesh.weights[(size_t)(mv.weight_start + wi)];
			if (w.joint_index < 0 || w.joint_index >= (int)joints.size())
				continue;

			const md5_joint_t &j = joints[(size_t)w.joint_index];

			// Transform weight position into model space and accumulate
			vec3_c rotated = QuatRotate(j.orient, w.pos);
			pos += (rotated + j.pos) * w.bias;
		}

		out[vi].pos    = pos;
		out[vi].normal = vec3_c(0, 0, 1);  // computed below
		out[vi].uv     = mv.uv;
	}

	// Compute per-triangle normals and accumulate into vertices
	for (const model_tri_c &tri : mesh.tris)
	{
		const vec3_c &p0 = out[tri.index[0]].pos;
		const vec3_c &p1 = out[tri.index[1]].pos;
		const vec3_c &p2 = out[tri.index[2]].pos;
		vec3_c n = (p1 - p0).Cross(p2 - p0);
		out[tri.index[0]].normal += n;
		out[tri.index[1]].normal += n;
		out[tri.index[2]].normal += n;
	}
	for (model_vert_c &v : out)
	{
		float len = v.normal.Length();
		if (len > 1e-6f)
			v.normal *= (1.0f / len);
	}
}


//------------------------------------------------------------------------
// MD5Loader::Load
//------------------------------------------------------------------------

model_data_c *MD5Loader::Load(file_c *f)
{
	// Read entire file as text
	int flen = f->GetLength();
	if (flen <= 0)
	{
		I_Warning("MD5: empty file\n");
		return NULL;
	}

	std::vector<char> buf((size_t)flen + 1, '\0');
	if (f->Read(buf.data(), (unsigned)flen) < (unsigned)flen)
	{
		I_Warning("MD5: file read error\n");
		return NULL;
	}
	buf[(size_t)flen] = '\0';

	std::vector<md5_joint_t> joints;
	std::vector<md5_mesh_t>  meshes;

	if (!ParseMesh(buf.data(), (size_t)flen, joints, meshes))
	{
		I_Warning("MD5: failed to parse model\n");
		return NULL;
	}

	model_data_c *mdl = new model_data_c();
	mdl->format_name = "MD5";
	mdl->fps         = 24;

	// Create a single bind-pose frame
	mdl->frames.resize(1);
	mdl->frames[0].name = "bind";

	for (size_t mi = 0; mi < meshes.size(); mi++)
	{
		const md5_mesh_t &mesh = meshes[mi];

		// Skin texture
		model_tex_c *tex = new model_tex_c();
		tex->name = mesh.shader;
		int skin_idx = (int)mdl->skins.size();
		mdl->skins.push_back(tex);

		// Body
		model_body_c *body = new model_body_c();
		body->name        = mesh.shader;
		body->skin_index  = skin_idx;
		body->tris        = mesh.tris;

		int body_idx = (int)mdl->bodies.size();
		mdl->bodies.push_back(body);

		// Skinned vertices in bind pose
		std::vector<model_vert_c> skinned;
		SkinMesh(mesh, joints, skinned);

		body->num_verts_per_frame = (int)skinned.size();

		// Attach to frame
		model_frame_c &frame = mdl->frames[0];
		while ((int)frame.verts.size() <= body_idx)
			frame.verts.push_back(std::vector<model_vert_c>());
		frame.verts[body_idx] = skinned;

		// Bounding box
		for (size_t vi = 0; vi < skinned.size(); vi++)
		{
			if (vi == 0)
				frame.bbox = bbox3_c(skinned[vi].pos);
			else
				frame.bbox.Insert(skinned[vi].pos);
		}
	}

	return mdl;
}


//------------------------------------------------------------------------
// MD5Loader::LoadAnim
//------------------------------------------------------------------------
//
// Expands an md5_anim_t (parsed separately) into the model by appending
// one model_frame_c per animation frame, skinned with the animated
// skeleton.
//
bool MD5Loader::LoadAnim(file_c * /*f*/,
                          model_data_c *mdl,
                          const md5_anim_t &anim)
{
	if (!mdl || anim.frames.empty()) return false;

	mdl->fps = anim.frame_rate;

	// Rebuild meshes from the existing body geometry using the anim skeleton.
	// (We need the original md5_mesh data; in a full implementation this
	// would be cached in the loader.  For now this is a forward declaration
	// of the interface that a higher-level caller would implement.)

	for (const md5_anim_frame_t &af : anim.frames)
	{
		model_frame_c frame;
		frame.name = anim.name;
		frame.verts.resize(mdl->bodies.size());

		// Re-skin each body using the anim-frame skeleton.
		// (Requires the caller to supply the original md5_mesh_t list.)
		// Placeholder: just copy the bind pose verts for now.
		if (!mdl->frames.empty())
		{
			for (size_t bi = 0; bi < mdl->bodies.size(); bi++)
			{
				if (bi < mdl->frames[0].verts.size())
					frame.verts[bi] = mdl->frames[0].verts[bi];
			}
		}

		(void)af; // suppress unused-variable warning until skinning is wired up
		mdl->frames.push_back(std::move(frame));
	}

	return true;
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
