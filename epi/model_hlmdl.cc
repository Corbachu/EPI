//----------------------------------------------------------------------------
//  EPI HLMDL Loader – implementation
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

#include "epi.h"
#include "endianess.h"
#include "model_hlmdl.h"

#include <cmath>
#include <cstring>

namespace epi
{

//------------------------------------------------------------------------
// Binary layout structures
//------------------------------------------------------------------------

#define HLMDL_MAGIC_IDST  0x54534449  // "IDST"  (studio model)
#define HLMDL_MAGIC_IDSQ  0x51534449  // "IDSQ"  (studio sequence – separate file)
#define HLMDL_VERSION     10

#pragma pack(push, 1)

struct hlmdl_header_t
{
	s32_t magic;         // IDST
	s32_t version;       // 10
	char  name[64];

	s32_t data_length;

	float eye_pos[3];
	float hull_min[3];
	float hull_max[3];
	float view_bbmin[3];
	float view_bbmax[3];

	s32_t flags;

	s32_t num_bones;      s32_t ofs_bones;
	s32_t num_bonecontrollers; s32_t ofs_bonecontrollers;
	s32_t num_hitboxes;   s32_t ofs_hitboxes;
	s32_t num_seq;        s32_t ofs_seq;
	s32_t num_seqgroups;  s32_t ofs_seqgroups;

	s32_t num_textures;   s32_t ofs_textures;  s32_t ofs_texturedata;

	s32_t num_skins;      // skin count per family
	s32_t num_skingroups; s32_t ofs_skingroups;  // skin families

	s32_t num_bodyparts;  s32_t ofs_bodyparts;

	s32_t num_attachments; s32_t ofs_attachments;

	s32_t sound_table;
	s32_t sound_index;
	s32_t sound_groups;
	s32_t ofs_soundgroups;

	s32_t num_transitions; s32_t ofs_transitions;
};

struct hlmdl_bone_t
{
	char  name[32];
	s32_t parent;        // -1 = root
	s32_t flags;
	s32_t bonecontroller[6];
	float value[6];      // default joint values
	float scale[6];      // animation value scale factors
};

struct hlmdl_texture_t
{
	char  name[64];
	s32_t flags;
	s32_t width;
	s32_t height;
	s32_t ofs_data;   // offset from start of file to raw 8-bit indexed pixels
	                  // followed by a 768-byte (256 * RGB) palette
};

struct hlmdl_bodypart_t
{
	char  name[64];
	s32_t num_models;
	s32_t base;      // model index base (for picking variants)
	s32_t ofs_models;
};

struct hlmdl_model_t
{
	char  name[64];
	s32_t type;       // always 0
	float bounding_radius;
	s32_t num_mesh;   s32_t ofs_mesh;
	s32_t num_verts;  s32_t ofs_vert_info;  s32_t ofs_verts;
	s32_t num_norms;  s32_t ofs_norm_info;  s32_t ofs_norms;
	s32_t num_groups; s32_t ofs_groups;
};

struct hlmdl_mesh_t
{
	s32_t num_tris;    // number of triverts (NOT triangles – these are tri-strips/fans)
	s32_t ofs_tris;
	s32_t skin_ref;    // index into skin table
	s32_t num_norms;
	s32_t norm_ofs;
};

// A "trivert" in the HL format is (vertindex, normindex, s, t).
struct hlmdl_trivert_t
{
	s16_t vert_idx;
	s16_t norm_idx;
	s16_t s;
	s16_t t;
};

#pragma pack(pop)


//------------------------------------------------------------------------
// HLMDLLoader::Probe
//------------------------------------------------------------------------

bool HLMDLLoader::Probe(file_c *f)
{
	s32_t magic = 0;
	if (f->Read(&magic, 4) != 4)
		return false;
	magic = EPI_LE_S32(magic);
	return (magic == HLMDL_MAGIC_IDST);
}


//------------------------------------------------------------------------
// HLMDLLoader::Load
//------------------------------------------------------------------------
//
// Reads body-part geometry (first sub-model of each body part) into the
// common model_data_c format as a single static frame.
//
model_data_c *HLMDLLoader::Load(file_c *f)
{
	// Slurp entire file into memory (simplifies random-access parsing)
	f->Seek(0, file_c::SEEKPOINT_END);
	int file_len = f->GetLength();
	f->Seek(0, file_c::SEEKPOINT_START);

	std::vector<u8_t> raw((size_t)file_len);
	if (f->Read(raw.data(), (unsigned)file_len) != (unsigned)file_len)
	{
		I_Warning("HLMDL: failed to read file\n");
		return NULL;
	}

	if (file_len < (int)sizeof(hlmdl_header_t))
	{
		I_Warning("HLMDL: file too small\n");
		return NULL;
	}

	const hlmdl_header_t *hdr = (const hlmdl_header_t *)raw.data();

	if (EPI_LE_S32(hdr->magic) != HLMDL_MAGIC_IDST)
	{
		I_Warning("HLMDL: bad magic\n");
		return NULL;
	}
	if (EPI_LE_S32(hdr->version) != HLMDL_VERSION)
	{
		I_Warning("HLMDL: unsupported version %d\n", EPI_LE_S32(hdr->version));
		return NULL;
	}

	int num_bodyparts = EPI_LE_S32(hdr->num_bodyparts);
	int ofs_bodyparts = EPI_LE_S32(hdr->ofs_bodyparts);
	int num_textures  = EPI_LE_S32(hdr->num_textures);
	int ofs_textures  = EPI_LE_S32(hdr->ofs_textures);

	if (num_bodyparts <= 0)
	{
		I_Warning("HLMDL: no body parts\n");
		return NULL;
	}

	model_data_c *mdl = new model_data_c();
	mdl->format_name = "HLMDL";
	mdl->fps = 0; // static base pose; no animation expansion yet

	// ------ textures ------
	for (int ti = 0; ti < num_textures; ti++)
	{
		const hlmdl_texture_t *t =
			(const hlmdl_texture_t *)(raw.data() + ofs_textures +
			                         ti * sizeof(hlmdl_texture_t));

		model_tex_c *tex = new model_tex_c();
		tex->name   = t->name;
		tex->width  = EPI_LE_S32(t->width);
		tex->height = EPI_LE_S32(t->height);
		// Pixel data lives at t->ofs_data in the file;
		// we store the name only – texture upload is the engine's job.
		mdl->skins.push_back(tex);
	}

	// Create a single "base pose" frame
	mdl->frames.resize(1);
	mdl->frames[0].name = "base";

	// ------ body parts ------
	for (int bp = 0; bp < num_bodyparts; bp++)
	{
		const hlmdl_bodypart_t *bpart =
			(const hlmdl_bodypart_t *)(raw.data() + ofs_bodyparts +
			                           bp * sizeof(hlmdl_bodypart_t));

		int num_models  = EPI_LE_S32(bpart->num_models);
		int ofs_models  = EPI_LE_S32(bpart->ofs_models);

		if (num_models <= 0) continue;

		// Load only the first sub-model (index 0 = default variant)
		const hlmdl_model_t *mdata =
			(const hlmdl_model_t *)(raw.data() + ofs_models);

		int num_mesh  = EPI_LE_S32(mdata->num_mesh);
		int ofs_mesh  = EPI_LE_S32(mdata->ofs_mesh);
		int num_verts = EPI_LE_S32(mdata->num_verts);
		int ofs_verts = EPI_LE_S32(mdata->ofs_verts);
		int num_norms = EPI_LE_S32(mdata->num_norms);
		int ofs_norms = EPI_LE_S32(mdata->ofs_norms);

		if (num_verts <= 0 || num_mesh <= 0) continue;

		// The vertex and normal arrays are stored as float[3] sequences
		const float *raw_verts = (const float *)(raw.data() + ofs_verts);
		const float *raw_norms = (const float *)(raw.data() + ofs_norms);

		// Iterate meshes – each mesh is one draw call (one skin ref)
		for (int mi = 0; mi < num_mesh; mi++)
		{
			const hlmdl_mesh_t *mesh =
				(const hlmdl_mesh_t *)(raw.data() + ofs_mesh +
				                       mi * sizeof(hlmdl_mesh_t));

			int skin_ref = EPI_LE_S32(mesh->skin_ref);
			int num_triverts = EPI_LE_S32(mesh->num_tris);
			int ofs_tris     = EPI_LE_S32(mesh->ofs_tris);

			model_body_c *body = new model_body_c();
			body->name       = std::string(bpart->name) + "_mesh" + std::to_string(mi);
			body->skin_index = (skin_ref < (int)mdl->skins.size()) ? skin_ref : 0;

			// The texture dimensions are needed to normalise UVs
			int texW = 1, texH = 1;
			if (body->skin_index < (int)mdl->skins.size())
			{
				texW = mdl->skins[body->skin_index]->width;
				texH = mdl->skins[body->skin_index]->height;
			}
			float invW = (texW > 0) ? (1.0f / (float)texW) : 1.0f;
			float invH = (texH > 0) ? (1.0f / (float)texH) : 1.0f;

			// Collect unique verts and build triangles
			// The trivert stream uses negative 'vert_idx' as a
			// command word to begin a new strip/fan.
			std::vector<model_vert_c> canon_verts;
			std::vector<model_tri_c>  tris;

			auto emit_vert = [&](const hlmdl_trivert_t &tv) -> u16_t
			{
				int vi = EPI_LE_S16(tv.vert_idx);
				int ni = EPI_LE_S16(tv.norm_idx);
				int s  = EPI_LE_S16(tv.s);
				int t  = EPI_LE_S16(tv.t);

				model_vert_c mv;
				if (vi >= 0 && vi < num_verts)
				{
					mv.pos = vec3_c(raw_verts[vi*3+0],
					                raw_verts[vi*3+1],
					                raw_verts[vi*3+2]);
				}
				if (ni >= 0 && ni < num_norms)
				{
					mv.normal = vec3_c(raw_norms[ni*3+0],
					                   raw_norms[ni*3+1],
					                   raw_norms[ni*3+2]);
				}
				mv.uv = vec2_c((float)s * invW, (float)t * invH);

				canon_verts.push_back(mv);
				return (u16_t)(canon_verts.size() - 1);
			};

			// Walk the trivert stream
			const s16_t *tstream = (const s16_t *)(raw.data() + ofs_tris);
			int tpos = 0;
			int stream_words = num_triverts * 4; // upper bound

			while (tpos < stream_words)
			{
				s16_t cmd = EPI_LE_S16(tstream[tpos++]);
				if (cmd == 0) break;

				bool is_fan = (cmd < 0);
				int  count  = is_fan ? -cmd : cmd;

				// Read |count| triverts
				std::vector<u16_t> strip;
				strip.reserve((size_t)count);

				for (int i = 0; i < count && tpos + 3 < stream_words; i++, tpos += 4)
				{
					hlmdl_trivert_t tv;
					tv.vert_idx = EPI_LE_S16(tstream[tpos + 0]);
					tv.norm_idx = EPI_LE_S16(tstream[tpos + 1]);
					tv.s        = EPI_LE_S16(tstream[tpos + 2]);
					tv.t        = EPI_LE_S16(tstream[tpos + 3]);
					strip.push_back(emit_vert(tv));
				}

				// Convert strip/fan to independent triangles
				if (is_fan)
				{
					// Triangle fan: [0, 1, 2], [0, 2, 3], …
					for (int i = 2; i < (int)strip.size(); i++)
						tris.push_back(model_tri_c(strip[0], strip[i-1], strip[i]));
				}
				else
				{
					// Triangle strip: [0,1,2], [2,1,3], [2,3,4], …
					for (int i = 2; i < (int)strip.size(); i++)
					{
						if (i & 1)
							tris.push_back(model_tri_c(strip[i-1], strip[i-2], strip[i]));
						else
							tris.push_back(model_tri_c(strip[i-2], strip[i-1], strip[i]));
					}
				}
			}

			body->tris               = std::move(tris);
			body->num_verts_per_frame = (int)canon_verts.size();

			int body_idx = (int)mdl->bodies.size();
			mdl->bodies.push_back(body);

			// Attach to base frame
			model_frame_c &frame = mdl->frames[0];
			while ((int)frame.verts.size() <= body_idx)
				frame.verts.push_back(std::vector<model_vert_c>());
			frame.verts[body_idx] = std::move(canon_verts);

			// Update frame bounding box
			for (const model_vert_c &mv : frame.verts[body_idx])
			{
				if (frame.verts[body_idx].size() == 1)
					frame.bbox = bbox3_c(mv.pos);
				else
					frame.bbox.Insert(mv.pos);
			}
		}
	}

	return mdl;
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
