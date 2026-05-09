//----------------------------------------------------------------------------
//  EPI MD3 Model Loader – implementation
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
#include "model_md3.h"

#include <cmath>
#include <cstring>

namespace epi
{

//------------------------------------------------------------------------
// Binary layout (all values little-endian)
//------------------------------------------------------------------------

#define MD3_MAGIC    0x33504449  // "IDP3"
#define MD3_VERSION  15

// MD3 vertex position scale factor: 1/64 (coordinates stored as 1/64 units)
#define MD3_XYZ_SCALE  (1.0f / 64.0f)

#pragma pack(push, 1)

struct md3_header_t
{
	s32_t magic;
	s32_t version;
	char  name[64];
	s32_t flags;
	s32_t num_frames;
	s32_t num_tags;
	s32_t num_surfaces;
	s32_t num_skins;   // unused in Q3; surfaces carry their own shader list
	s32_t ofs_frames;
	s32_t ofs_tags;
	s32_t ofs_surfaces;
	s32_t ofs_eof;
};

struct md3_frame_t
{
	float min_bounds[3];
	float max_bounds[3];
	float local_origin[3];
	float radius;
	char  name[16];
};

// A "tag" is a named attachment point (bone) with position + orientation
struct md3_tag_t
{
	char  name[64];
	float origin[3];
	float axis[3][3];  // column-major rotation matrix
};

struct md3_surface_header_t
{
	s32_t magic;          // also IDP3
	char  name[64];
	s32_t flags;
	s32_t num_frames;
	s32_t num_shaders;
	s32_t num_verts;
	s32_t num_tris;
	s32_t ofs_tris;
	s32_t ofs_shaders;
	s32_t ofs_st;         // texture coordinates (one per vertex, frame-independent)
	s32_t ofs_xyznormal;  // per-frame vertex data
	s32_t ofs_end;
};

struct md3_shader_t
{
	char  name[64];
	s32_t shader_index;  // set at runtime by Q3; ignored here
};

struct md3_triangle_t
{
	s32_t index[3];  // vertex indices within this surface
};

struct md3_texcoord_t
{
	float st[2];
};

// Per-vertex, per-frame entry
struct md3_vertex_t
{
	s16_t xyz[3];     // position * 64
	u8_t  normal[2];  // spherical coordinates (lat, lng) of the normal
};

#pragma pack(pop)

// Decode the MD3 packed spherical normal encoding
static inline vec3_c md3_decode_normal(u8_t lat, u8_t lng)
{
	float la = (float)lat * (2.0f * 3.14159265358979f / 255.0f);
	float lo = (float)lng * (2.0f * 3.14159265358979f / 255.0f);
	return vec3_c(
		cosf(la) * sinf(lo),
		sinf(la) * sinf(lo),
		cosf(lo)
	);
}


//------------------------------------------------------------------------
// MD3Loader::Probe
//------------------------------------------------------------------------

bool MD3Loader::Probe(file_c *f)
{
	s32_t magic = 0;
	if (f->Read(&magic, 4) != 4)
		return false;

	magic = EPI_LE_S32(magic);
	return (magic == MD3_MAGIC);
}

//------------------------------------------------------------------------
// MD3Loader::Load
//------------------------------------------------------------------------

model_data_c *MD3Loader::Load(file_c *f)
{
	// ------ read & validate header ------
	md3_header_t hdr;
	if (f->Read(&hdr, sizeof(hdr)) != sizeof(hdr))
	{
		I_Warning("MD3: failed to read header\n");
		return NULL;
	}

	hdr.magic        = EPI_LE_S32(hdr.magic);
	hdr.version      = EPI_LE_S32(hdr.version);
	hdr.num_frames   = EPI_LE_S32(hdr.num_frames);
	hdr.num_tags     = EPI_LE_S32(hdr.num_tags);
	hdr.num_surfaces = EPI_LE_S32(hdr.num_surfaces);
	hdr.ofs_frames   = EPI_LE_S32(hdr.ofs_frames);
	hdr.ofs_tags     = EPI_LE_S32(hdr.ofs_tags);
	hdr.ofs_surfaces = EPI_LE_S32(hdr.ofs_surfaces);

	if (hdr.magic != MD3_MAGIC)
	{
		I_Warning("MD3: bad magic 0x%08x\n", hdr.magic);
		return NULL;
	}
	if (hdr.version != MD3_VERSION)
	{
		I_Warning("MD3: unsupported version %d\n", hdr.version);
		return NULL;
	}
	if (hdr.num_frames <= 0 || hdr.num_surfaces <= 0)
	{
		I_Warning("MD3: empty model\n");
		return NULL;
	}

	// ------ read frames (for bounding boxes) ------
	std::vector<md3_frame_t> raw_frames((size_t)hdr.num_frames);
	f->Seek(hdr.ofs_frames, file_c::SEEKPOINT_START);
	f->Read(raw_frames.data(), sizeof(md3_frame_t) * (unsigned)hdr.num_frames);
	// Note: md3_frame_t contains floats – on big-endian platforms these
	// would need byte-swapping; we omit that for brevity (all target
	// platforms are little-endian).

	// ------ build model ------
	model_data_c *mdl = new model_data_c();
	mdl->format_name = "MD3";
	mdl->fps         = 20;

	// Prime the frame list
	mdl->frames.resize((size_t)hdr.num_frames);
	for (int fi = 0; fi < hdr.num_frames; fi++)
	{
		char name[17]; memcpy(name, raw_frames[fi].name, 16); name[16] = 0;
		mdl->frames[fi].name = name;
	}

	// ------ iterate surfaces ------
	int surf_offset = hdr.ofs_surfaces;

	for (int si = 0; si < hdr.num_surfaces; si++)
	{
		f->Seek(surf_offset, file_c::SEEKPOINT_START);

		md3_surface_header_t shdr;
		if (f->Read(&shdr, sizeof(shdr)) != sizeof(shdr))
			break;

		int surf_base = surf_offset; // absolute offset of this surface

		shdr.magic       = EPI_LE_S32(shdr.magic);
		shdr.num_frames  = EPI_LE_S32(shdr.num_frames);
		shdr.num_shaders = EPI_LE_S32(shdr.num_shaders);
		shdr.num_verts   = EPI_LE_S32(shdr.num_verts);
		shdr.num_tris    = EPI_LE_S32(shdr.num_tris);
		shdr.ofs_tris    = EPI_LE_S32(shdr.ofs_tris);
		shdr.ofs_shaders = EPI_LE_S32(shdr.ofs_shaders);
		shdr.ofs_st      = EPI_LE_S32(shdr.ofs_st);
		shdr.ofs_xyznormal = EPI_LE_S32(shdr.ofs_xyznormal);
		shdr.ofs_end     = EPI_LE_S32(shdr.ofs_end);

		// ----- read first shader (skin) -----
		md3_shader_t shader;
		f->Seek(surf_base + shdr.ofs_shaders, file_c::SEEKPOINT_START);
		f->Read(&shader, sizeof(shader));

		model_tex_c *tex = new model_tex_c();
		tex->name = shader.name;
		int skin_idx = (int)mdl->skins.size();
		mdl->skins.push_back(tex);

		// ----- read triangles -----
		std::vector<md3_triangle_t> raw_tris((size_t)shdr.num_tris);
		f->Seek(surf_base + shdr.ofs_tris, file_c::SEEKPOINT_START);
		f->Read(raw_tris.data(), sizeof(md3_triangle_t) * (unsigned)shdr.num_tris);

		// ----- read texture coordinates -----
		std::vector<md3_texcoord_t> raw_st((size_t)shdr.num_verts);
		f->Seek(surf_base + shdr.ofs_st, file_c::SEEKPOINT_START);
		f->Read(raw_st.data(), sizeof(md3_texcoord_t) * (unsigned)shdr.num_verts);

		// ----- read per-frame vertex data -----
		int verts_per_frame = shdr.num_verts;
		std::vector<md3_vertex_t> raw_verts((size_t)(shdr.num_frames * verts_per_frame));
		f->Seek(surf_base + shdr.ofs_xyznormal, file_c::SEEKPOINT_START);
		f->Read(raw_verts.data(), sizeof(md3_vertex_t) * (unsigned)(shdr.num_frames * verts_per_frame));

		// ----- build body -----
		model_body_c *body = new model_body_c();
		body->name                = shdr.name;
		body->skin_index          = skin_idx;
		body->num_verts_per_frame = verts_per_frame;

		for (int ti = 0; ti < shdr.num_tris; ti++)
		{
			model_tri_c tri;
			for (int k = 0; k < 3; k++)
			{
				s32_t idx = EPI_LE_S32(raw_tris[ti].index[k]);
				tri.index[k] = (u16_t)idx;
			}
			body->tris.push_back(tri);
		}

		int body_idx = (int)mdl->bodies.size();
		mdl->bodies.push_back(body);

		// ----- expand per-frame vertices into model frames -----
		int model_frames = (int)mdl->frames.size();
		for (int fi = 0; fi < shdr.num_frames && fi < model_frames; fi++)
		{
			model_frame_c &frame = mdl->frames[fi];

			// Ensure frame.verts has enough slots for this body index
			while ((int)frame.verts.size() <= body_idx)
				frame.verts.push_back(std::vector<model_vert_c>());

			std::vector<model_vert_c> &fv = frame.verts[body_idx];
			fv.resize((size_t)verts_per_frame);

			const md3_vertex_t *base = raw_verts.data() + fi * verts_per_frame;
			bool first = true;

			for (int vi = 0; vi < verts_per_frame; vi++)
			{
				s16_t rx = EPI_LE_S16(base[vi].xyz[0]);
				s16_t ry = EPI_LE_S16(base[vi].xyz[1]);
				s16_t rz = EPI_LE_S16(base[vi].xyz[2]);

				float px = (float)rx * MD3_XYZ_SCALE;
				float py = (float)ry * MD3_XYZ_SCALE;
				float pz = (float)rz * MD3_XYZ_SCALE;

				vec3_c norm = md3_decode_normal(base[vi].normal[0], base[vi].normal[1]);

				fv[vi] = model_vert_c(
					vec3_c(px, py, pz),
					norm,
					vec2_c(raw_st[vi].st[0], raw_st[vi].st[1])
				);

				if (first) { frame.bbox = bbox3_c(vec3_c(px,py,pz)); first = false; }
				else        { frame.bbox.Insert(vec3_c(px,py,pz)); }
			}
		}

		// Advance to the next surface
		surf_offset += shdr.ofs_end;
	}

	return mdl;
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
