//----------------------------------------------------------------------------
//  EPI MD2 Model Loader – implementation
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
#include "model_md2.h"

#include <cmath>
#include <cstring>

namespace epi
{

//------------------------------------------------------------------------
// Binary layout (all values little-endian)
//------------------------------------------------------------------------

#define MD2_MAGIC    0x32504449  // "IDP2"
#define MD2_VERSION  8

#pragma pack(push, 1)

struct md2_header_t
{
	s32_t magic;
	s32_t version;

	s32_t skin_w;        // skin width  (texels)
	s32_t skin_h;        // skin height (texels)
	s32_t frame_size;    // bytes per frame
	s32_t num_skins;
	s32_t num_verts;     // vertices per frame
	s32_t num_st;        // texture coordinate pairs
	s32_t num_tris;
	s32_t num_glcmds;    // not used by this loader
	s32_t num_frames;
	s32_t ofs_skins;
	s32_t ofs_st;
	s32_t ofs_tris;
	s32_t ofs_frames;
	s32_t ofs_glcmds;
	s32_t ofs_end;
};

struct md2_skin_t
{
	char name[64];
};

struct md2_texcoord_t
{
	s16_t s;
	s16_t t;
};

struct md2_triangle_t
{
	u16_t vert_idx[3];
	u16_t tc_idx[3];
};

struct md2_vertex_t
{
	u8_t v[3];         // compressed position (scale + translate applied per frame)
	u8_t normal_idx;   // index into MD2 anorm table
};

struct md2_frame_t
{
	float scale[3];
	float translate[3];
	char  name[16];
	// followed immediately by num_verts md2_vertex_t entries
};

#pragma pack(pop)

//------------------------------------------------------------------------
// Quake 2 pre-calculated vertex normal table (162 entries)
// Source: Quake2/ref_gl/gl_model.c  –  r_avertexnormals[]
//------------------------------------------------------------------------
static const float s_md2_normals[162][3] =
{
	{-0.525731f,  0.000000f,  0.850651f},
	{-0.442863f,  0.238856f,  0.864188f},
	{-0.295242f,  0.000000f,  0.955423f},
	{-0.309017f,  0.500000f,  0.809017f},
	{-0.162460f,  0.262866f,  0.951056f},
	{ 0.000000f,  0.000000f,  1.000000f},
	{ 0.000000f,  0.850651f,  0.525731f},
	{-0.147621f,  0.716567f,  0.681718f},
	{ 0.147621f,  0.716567f,  0.681718f},
	{ 0.000000f,  0.525731f,  0.850651f},
	{ 0.309017f,  0.500000f,  0.809017f},
	{ 0.525731f,  0.000000f,  0.850651f},
	{ 0.295242f,  0.000000f,  0.955423f},
	{ 0.442863f,  0.238856f,  0.864188f},
	{ 0.162460f,  0.262866f,  0.951056f},
	{-0.681718f,  0.147621f,  0.716567f},
	{-0.809017f,  0.309017f,  0.500000f},
	{-0.587785f,  0.425325f,  0.688191f},
	{-0.850651f,  0.525731f,  0.000000f},
	{-0.864188f,  0.442863f,  0.238856f},
	{-0.716567f,  0.681718f,  0.147621f},
	{-0.688191f,  0.587785f,  0.425325f},
	{-0.500000f,  0.809017f,  0.309017f},
	{-0.238856f,  0.864188f,  0.442863f},
	{-0.425325f,  0.688191f,  0.587785f},
	{-0.716567f,  0.681718f, -0.147621f},
	{-0.500000f,  0.809017f, -0.309017f},
	{-0.525731f,  0.850651f,  0.000000f},
	{ 0.000000f,  0.850651f, -0.525731f},
	{-0.238856f,  0.864188f, -0.442863f},
	{ 0.000000f,  0.955423f, -0.295242f},
	{-0.262866f,  0.951056f, -0.162460f},
	{ 0.000000f,  1.000000f,  0.000000f},
	{ 0.000000f,  0.955423f,  0.295242f},
	{-0.262866f,  0.951056f,  0.162460f},
	{ 0.238856f,  0.864188f,  0.442863f},
	{ 0.262866f,  0.951056f,  0.162460f},
	{ 0.500000f,  0.809017f,  0.309017f},
	{ 0.238856f,  0.864188f, -0.442863f},
	{ 0.262866f,  0.951056f, -0.162460f},
	{ 0.500000f,  0.809017f, -0.309017f},
	{ 0.850651f,  0.525731f,  0.000000f},
	{ 0.716567f,  0.681718f,  0.147621f},
	{ 0.716567f,  0.681718f, -0.147621f},
	{ 0.525731f,  0.850651f,  0.000000f},
	{ 0.425325f,  0.688191f,  0.587785f},
	{ 0.864188f,  0.442863f,  0.238856f},
	{ 0.688191f,  0.587785f,  0.425325f},
	{ 0.809017f,  0.309017f,  0.500000f},
	{ 0.681718f,  0.147621f,  0.716567f},
	{ 0.587785f,  0.425325f,  0.688191f},
	{ 0.955423f,  0.295242f,  0.000000f},
	{ 1.000000f,  0.000000f,  0.000000f},
	{ 0.951056f,  0.162460f,  0.262866f},
	{ 0.850651f, -0.525731f,  0.000000f},
	{ 0.955423f, -0.295242f,  0.000000f},
	{ 0.864188f, -0.442863f,  0.238856f},
	{ 0.951056f, -0.162460f,  0.262866f},
	{ 0.809017f, -0.309017f,  0.500000f},
	{ 0.681718f, -0.147621f,  0.716567f},
	{ 0.850651f,  0.000000f,  0.525731f},
	{ 0.864188f,  0.442863f, -0.238856f},
	{ 0.809017f,  0.309017f, -0.500000f},
	{ 0.951056f,  0.162460f, -0.262866f},
	{ 0.525731f,  0.000000f, -0.850651f},
	{ 0.681718f,  0.147621f, -0.716567f},
	{ 0.681718f, -0.147621f, -0.716567f},
	{ 0.850651f,  0.000000f, -0.525731f},
	{ 0.809017f, -0.309017f, -0.500000f},
	{ 0.864188f, -0.442863f, -0.238856f},
	{ 0.951056f, -0.162460f, -0.262866f},
	{ 0.147621f,  0.716567f, -0.681718f},
	{ 0.309017f,  0.500000f, -0.809017f},
	{ 0.425325f,  0.688191f, -0.587785f},
	{ 0.442863f,  0.238856f, -0.864188f},
	{ 0.162460f,  0.262866f, -0.951056f},
	{ 0.238856f, -0.864188f, -0.442863f},
	{ 0.500000f, -0.809017f, -0.309017f},
	{ 0.425325f, -0.688191f, -0.587785f},
	{ 0.716567f, -0.681718f, -0.147621f},
	{ 0.688191f, -0.587785f, -0.425325f},
	{ 0.587785f, -0.425325f, -0.688191f},
	{ 0.000000f, -0.955423f, -0.295242f},
	{ 0.000000f, -1.000000f,  0.000000f},
	{ 0.262866f, -0.951056f, -0.162460f},
	{ 0.000000f, -0.850651f,  0.525731f},
	{ 0.000000f, -0.955423f,  0.295242f},
	{ 0.238856f, -0.864188f,  0.442863f},
	{ 0.262866f, -0.951056f,  0.162460f},
	{ 0.500000f, -0.809017f,  0.309017f},
	{ 0.716567f, -0.681718f,  0.147621f},
	{ 0.525731f, -0.850651f,  0.000000f},
	{-0.238856f, -0.864188f, -0.442863f},
	{-0.500000f, -0.809017f, -0.309017f},
	{-0.262866f, -0.951056f, -0.162460f},
	{-0.850651f, -0.525731f,  0.000000f},
	{-0.716567f, -0.681718f, -0.147621f},
	{-0.716567f, -0.681718f,  0.147621f},
	{-0.525731f, -0.850651f,  0.000000f},
	{-0.500000f, -0.809017f,  0.309017f},
	{-0.238856f, -0.864188f,  0.442863f},
	{-0.262866f, -0.951056f,  0.162460f},
	{-0.864188f, -0.442863f,  0.238856f},
	{-0.809017f, -0.309017f,  0.500000f},
	{-0.688191f, -0.587785f,  0.425325f},
	{-0.681718f, -0.147621f,  0.716567f},
	{-0.442863f, -0.238856f,  0.864188f},
	{-0.587785f, -0.425325f,  0.688191f},
	{-0.809017f, -0.309017f, -0.500000f},
	{-0.681718f, -0.147621f, -0.716567f},
	{-0.587785f, -0.425325f, -0.688191f},
	{-0.850651f,  0.000000f, -0.525731f},
	{-0.688191f, -0.587785f, -0.425325f},
	{-0.425325f, -0.688191f, -0.587785f},
	{-0.425325f, -0.688191f,  0.587785f},
	{-0.587785f, -0.425325f,  0.688191f}, // duplicate of entry 108 in original table
	{ 0.000000f, -0.525731f,  0.850651f},
	{-0.295242f,  0.000000f, -0.955423f},
	{-0.162460f,  0.262866f, -0.951056f},
	{-0.309017f,  0.500000f, -0.809017f},
	{-0.442863f,  0.238856f, -0.864188f},
	{-0.147621f,  0.716567f, -0.681718f},
	{-0.681718f,  0.147621f, -0.716567f},
	{-0.864188f,  0.442863f, -0.238856f},
	{-0.688191f,  0.587785f, -0.425325f},
	{-0.309017f, -0.500000f,  0.809017f},
	{-0.147621f, -0.716567f,  0.681718f},
	{-0.000000f, -0.525731f, -0.850651f},
	{-0.295242f,  0.000000f,  0.955423f},
	{-0.000000f,  0.000000f, -1.000000f},
	{-0.162460f, -0.262866f,  0.951056f},
	{-0.309017f, -0.500000f, -0.809017f},
	{-0.147621f, -0.716567f, -0.681718f},
	{-0.000000f, -0.850651f, -0.525731f},
	{-0.525731f,  0.000000f, -0.850651f},
	{-0.442863f, -0.238856f, -0.864188f},
	{-0.295242f,  0.000000f, -0.955423f}, // duplicate of entry 121; kept for index alignment
	{ 0.000000f,  0.525731f, -0.850651f},
	{-0.162460f, -0.262866f, -0.951056f},
	{ 0.000000f,  0.000000f, -1.000000f},
	{-0.850651f,  0.000000f,  0.525731f},
	{-0.955423f,  0.295242f,  0.000000f},
	{-1.000000f,  0.000000f,  0.000000f},
	{-0.951056f,  0.162460f,  0.262866f},
	{-0.951056f,  0.162460f, -0.262866f},
	{-0.955423f, -0.295242f,  0.000000f},
	{-0.864188f, -0.442863f, -0.238856f},
	{-0.951056f, -0.162460f, -0.262866f},
	{-0.809017f,  0.309017f, -0.500000f},
	{-0.864188f,  0.442863f,  0.238856f}, // corrected sign
	{-0.951056f, -0.162460f,  0.262866f},
	{-0.809017f,  0.309017f,  0.500000f},
	{ 0.162460f, -0.262866f,  0.951056f},
	{ 0.309017f, -0.500000f,  0.809017f},
	{ 0.162460f, -0.262866f, -0.951056f},
	{ 0.309017f, -0.500000f, -0.809017f},
	{ 0.147621f, -0.716567f,  0.681718f},
	{ 0.000000f, -0.525731f, -0.850651f},
	{ 0.147621f, -0.716567f, -0.681718f},
	{ 0.000000f, -0.850651f,  0.525731f},
};


//------------------------------------------------------------------------
// MD2Loader::Probe
//------------------------------------------------------------------------

bool MD2Loader::Probe(file_c *f)
{
	s32_t magic = 0;
	if (f->Read(&magic, 4) != 4)
		return false;

	magic = EPI_LE_S32(magic);
	return (magic == MD2_MAGIC);
}

//------------------------------------------------------------------------
// MD2Loader::Load
//------------------------------------------------------------------------

model_data_c *MD2Loader::Load(file_c *f)
{
	// ------ read & validate header ------
	md2_header_t hdr;
	if (f->Read(&hdr, sizeof(hdr)) != sizeof(hdr))
	{
		I_Warning("MD2: failed to read header\n");
		return NULL;
	}

	hdr.magic       = EPI_LE_S32(hdr.magic);
	hdr.version     = EPI_LE_S32(hdr.version);
	hdr.skin_w      = EPI_LE_S32(hdr.skin_w);
	hdr.skin_h      = EPI_LE_S32(hdr.skin_h);
	hdr.frame_size  = EPI_LE_S32(hdr.frame_size);
	hdr.num_skins   = EPI_LE_S32(hdr.num_skins);
	hdr.num_verts   = EPI_LE_S32(hdr.num_verts);
	hdr.num_st      = EPI_LE_S32(hdr.num_st);
	hdr.num_tris    = EPI_LE_S32(hdr.num_tris);
	hdr.num_frames  = EPI_LE_S32(hdr.num_frames);
	hdr.ofs_skins   = EPI_LE_S32(hdr.ofs_skins);
	hdr.ofs_st      = EPI_LE_S32(hdr.ofs_st);
	hdr.ofs_tris    = EPI_LE_S32(hdr.ofs_tris);
	hdr.ofs_frames  = EPI_LE_S32(hdr.ofs_frames);

	if (hdr.magic != MD2_MAGIC)
	{
		I_Warning("MD2: bad magic 0x%08x\n", hdr.magic);
		return NULL;
	}
	if (hdr.version != MD2_VERSION)
	{
		I_Warning("MD2: unsupported version %d (expected %d)\n", hdr.version, MD2_VERSION);
		return NULL;
	}
	if (hdr.num_verts <= 0 || hdr.num_tris <= 0 || hdr.num_frames <= 0)
	{
		I_Warning("MD2: empty model\n");
		return NULL;
	}

	// ------ read skins ------
	std::vector<md2_skin_t> raw_skins((size_t)hdr.num_skins);
	if (hdr.num_skins > 0)
	{
		f->Seek(hdr.ofs_skins, file_c::SEEKPOINT_START);
		f->Read(raw_skins.data(), sizeof(md2_skin_t) * (unsigned)hdr.num_skins);
	}

	// ------ read texture coordinates ------
	std::vector<md2_texcoord_t> raw_st((size_t)hdr.num_st);
	f->Seek(hdr.ofs_st, file_c::SEEKPOINT_START);
	f->Read(raw_st.data(), sizeof(md2_texcoord_t) * (unsigned)hdr.num_st);
	for (int i = 0; i < hdr.num_st; i++)
	{
		raw_st[i].s = EPI_LE_S16(raw_st[i].s);
		raw_st[i].t = EPI_LE_S16(raw_st[i].t);
	}

	// ------ read triangles ------
	std::vector<md2_triangle_t> raw_tris((size_t)hdr.num_tris);
	f->Seek(hdr.ofs_tris, file_c::SEEKPOINT_START);
	f->Read(raw_tris.data(), sizeof(md2_triangle_t) * (unsigned)hdr.num_tris);
	for (int i = 0; i < hdr.num_tris; i++)
	{
		for (int k = 0; k < 3; k++)
		{
			raw_tris[i].vert_idx[k] = EPI_LE_U16(raw_tris[i].vert_idx[k]);
			raw_tris[i].tc_idx[k]   = EPI_LE_U16(raw_tris[i].tc_idx[k]);
		}
	}

	// ------ read frame data ------
	// Each frame is: md2_frame_t header + num_verts * md2_vertex_t
	std::vector<u8_t> frame_buf((size_t)hdr.frame_size);
	f->Seek(hdr.ofs_frames, file_c::SEEKPOINT_START);

	// ------ build model_data_c ------
	model_data_c *mdl = new model_data_c();
	mdl->format_name = "MD2";
	mdl->fps         = 10;  // Quake 2 standard rate

	// Skins
	for (int i = 0; i < hdr.num_skins; i++)
	{
		model_tex_c *tex = new model_tex_c();
		tex->name   = raw_skins[i].name;
		tex->width  = hdr.skin_w;
		tex->height = hdr.skin_h;
		mdl->skins.push_back(tex);
	}

	// Single body part (MD2 has no body hierarchy)
	model_body_c *body = new model_body_c();
	body->name                = "mesh";
	body->skin_index          = 0;
	body->num_verts_per_frame = hdr.num_verts;

	// Triangles – MD2 uses per-triangle texture coords, so we must
	// build a canonical expanded vertex list where each unique
	// (vert_index, tc_index) pair becomes its own model_vert_c.
	// We do this once here and store the triangle indices.

	// Map (vert_idx, tc_idx) -> canonical index
	struct VTKey { u16_t v, t; };
	std::vector<VTKey>     key_map;
	std::vector<model_tri_c> tris;
	tris.reserve((size_t)hdr.num_tris);

	auto find_or_add = [&](u16_t vi, u16_t ti) -> u16_t
	{
		for (size_t i = 0; i < key_map.size(); i++)
		{
			if (key_map[i].v == vi && key_map[i].t == ti)
				return (u16_t)i;
		}
		VTKey k = { vi, ti };
		key_map.push_back(k);
		return (u16_t)(key_map.size() - 1);
	};

	for (int i = 0; i < hdr.num_tris; i++)
	{
		model_tri_c tri;
		for (int k = 0; k < 3; k++)
		{
			tri.index[k] = find_or_add(raw_tris[i].vert_idx[k],
			                           raw_tris[i].tc_idx[k]);
		}
		tris.push_back(tri);
	}

	body->tris               = std::move(tris);
	body->num_verts_per_frame = (int)key_map.size();
	mdl->bodies.push_back(body);

	// Frames
	float skin_s = (hdr.skin_w > 0) ? (1.0f / (float)hdr.skin_w) : 1.0f;
	float skin_t = (hdr.skin_h > 0) ? (1.0f / (float)hdr.skin_h) : 1.0f;

	mdl->frames.resize((size_t)hdr.num_frames);

	for (int fi = 0; fi < hdr.num_frames; fi++)
	{
		if (f->Read(frame_buf.data(), (unsigned)hdr.frame_size) != (unsigned)hdr.frame_size)
		{
			I_Warning("MD2: short read on frame %d\n", fi);
			break;
		}

		const md2_frame_t *fhdr = (const md2_frame_t *)frame_buf.data();
		float sx = fhdr->scale[0], sy = fhdr->scale[1], sz = fhdr->scale[2];
		float tx = fhdr->translate[0], ty = fhdr->translate[1], tz = fhdr->translate[2];

		model_frame_c &frame = mdl->frames[fi];
		char fname[17]; memcpy(fname, fhdr->name, 16); fname[16] = 0;
		frame.name = fname;

		const md2_vertex_t *raw_verts =
			(const md2_vertex_t *)(frame_buf.data() + sizeof(md2_frame_t));

		// Build per-body vertex list in canonical (vert,tc) order
		frame.verts.resize(1);
		std::vector<model_vert_c> &fv = frame.verts[0];
		fv.resize(key_map.size());

		bool first_v = true;
		for (size_t ci = 0; ci < key_map.size(); ci++)
		{
			u16_t vi = key_map[ci].v;
			u16_t ti = key_map[ci].t;

			const md2_vertex_t &rv = raw_verts[vi];

			// Decompress position
			float px = rv.v[0] * sx + tx;
			float py = rv.v[1] * sy + ty;
			float pz = rv.v[2] * sz + tz;

			// Lookup precomputed normal
			int   ni  = rv.normal_idx < 162 ? rv.normal_idx : 0;
			float nx  = s_md2_normals[ni][0];
			float ny  = s_md2_normals[ni][1];
			float nz  = s_md2_normals[ni][2];

			// Texture coordinate (normalised)
			float u = raw_st[ti].s * skin_s;
			float v = raw_st[ti].t * skin_t;

			fv[ci] = model_vert_c(
				vec3_c(px, py, pz),
				vec3_c(nx, ny, nz),
				vec2_c(u, v)
			);

			if (first_v) { frame.bbox = bbox3_c(vec3_c(px,py,pz)); first_v = false; }
			else          { frame.bbox.Insert(vec3_c(px,py,pz)); }
		}
	}

	return mdl;
}

// ---------------------------------------------------------------------------
// MD2 interpolation helpers
// ---------------------------------------------------------------------------

bool MD2AnimState::Advance(float dt)
{
	if (ClipLength() <= 0)
		return false;

	cursor += fps * dt;

	float len = (float)ClipLength();

	if (loop)
	{
		// Wrap around within the clip.
		while (cursor >= len) cursor -= len;
		while (cursor <  0.0f) cursor += len;
		return true;
	}
	else
	{
		// Clamp at end.
		if (cursor >= len)
		{
			cursor = len - 1e-6f;
			return false; // clip finished
		}
		return true;
	}
}

int MD2AnimState::CurrentFrame() const
{
	int rel = (int)cursor;
	if (rel < 0) rel = 0;
	if (rel >= ClipLength()) rel = ClipLength() - 1;
	return first_frame + rel;
}

int MD2AnimState::NextFrame() const
{
	int rel  = (int)cursor + 1;
	if (loop)
		rel = rel % ClipLength();
	else if (rel >= ClipLength())
		rel  = ClipLength() - 1;
	return first_frame + rel;
}

float MD2AnimState::LerpParam() const
{
	float frac = cursor - (float)(int)cursor;
	if (frac < 0.0f) frac = 0.0f;
	if (frac > 1.0f) frac = 1.0f;
	return frac;
}

// ---------------------------------------------------------------------------

bool MD2_FindFrameRange(const model_data_c *mdl, const char *clip_name,
                        int &first_out, int &last_out)
{
	if (!mdl || !clip_name || clip_name[0] == '\0')
		return false;

	int n = mdl->NumFrames();
	if (n == 0) return false;

	// Determine prefix length of clip_name (strip any trailing digits from
	// the clip_name parameter itself, if the caller passes "stand01").
	int clip_prefix_len = (int)strlen(clip_name);
	while (clip_prefix_len > 0 &&
	       clip_name[clip_prefix_len - 1] >= '0' &&
	       clip_name[clip_prefix_len - 1] <= '9')
	{
		clip_prefix_len--;
	}

	int first = -1, last = -1;

	for (int i = 0; i < n; i++)
	{
		const std::string &fname = mdl->frames[i].name;

		// Strip trailing digits from the frame name to get its prefix.
		int flen = (int)fname.size();
		while (flen > 0 &&
		       fname[flen - 1] >= '0' &&
		       fname[flen - 1] <= '9')
		{
			flen--;
		}

		if (flen != clip_prefix_len) continue;
		if (strncmp(fname.c_str(), clip_name, (size_t)clip_prefix_len) != 0)
			continue;

		if (first == -1) first = i;
		last = i;
	}

	if (first == -1) return false;

	first_out = first;
	last_out  = last;
	return true;
}

// ---------------------------------------------------------------------------

bool MD2_LerpFrame(const model_data_c *mdl, int body_idx,
                   MD2AnimState &state, float dt,
                   std::vector<model_vert_c> &out)
{
	SYS_ASSERT(mdl);

	bool playing = state.Advance(dt);

	int   fa = state.CurrentFrame();
	int   fb = state.NextFrame();
	float t  = state.LerpParam();

	mdl->LerpVertices(body_idx, fa, fb, t, out);

	return playing;
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
