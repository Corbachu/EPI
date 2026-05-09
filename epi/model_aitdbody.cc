//----------------------------------------------------------------------------
//  EPI AITD Body Loader – implementation
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
#include "model_aitdbody.h"

#include <cassert>
#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace epi
{

//------------------------------------------------------------------------
// AITD body flags (from fitd/anim.h in jmimu/AITD_PakEdit)
//------------------------------------------------------------------------

#define AITD_INFO_TRI      0x01  // model has only triangles (no quads)
#define AITD_INFO_ANIM     0x02  // model has skeleton groups
#define AITD_INFO_TORTUE   0x04  // "turtle" mode – unsupported render path
#define AITD_INFO_OPTIMISE 0x08  // AITD2+ optimised group layout

//------------------------------------------------------------------------
// Primitive type identifiers
//------------------------------------------------------------------------

enum aitd_prim_e
{
	AITD_PRIM_LINE     = 0,
	AITD_PRIM_POLY     = 1,
	AITD_PRIM_POINT    = 2,
	AITD_PRIM_SPHERE   = 3,
	AITD_PRIM_DISK     = 4,
	AITD_PRIM_CYLINDER = 5,
	AITD_PRIM_BIGPOINT = 6,
	AITD_PRIM_ZIXEL    = 7,
	AITD_PRIM_POLYTEX8 = 8,   // textured poly, no UV
	AITD_PRIM_POLYTEX9 = 9,   // textured poly + per-vertex u,v bytes
	AITD_PRIM_POLYTEX10= 10,  // textured poly + per-vertex u,v bytes
};

//------------------------------------------------------------------------
// Internal parsed structures
//------------------------------------------------------------------------

struct aitd_vert_t
{
	s16_t x, y, z;
};

struct aitd_group_t
{
	s16_t vert_start;     // first vertex index in this bone's range
	s16_t vert_count;     // number of vertices owned by this bone
	s16_t base_vert;      // parent-bone base vertex (for relative anim)
	s8_t  org_group;      // parent group index (-1 = root)
	s8_t  group_id;       // this group's own index
	s16_t state_type;
	s16_t delta[3];       // position delta (bind-pose rotation)
	s16_t rotate_delta[3]; // rotation delta (AITD2+ optimised only)
};

struct aitd_prim_t
{
	aitd_prim_e   type;
	u8_t          sub_type;
	u8_t          color;    // palette index
	u8_t          padding;
	u16_t         size;     // used only by Sphere/Disk/Cylinder
	std::vector<u16_t> vert_refs; // vertex indices (already divided by 6)
	std::vector<u8_t>  u_vals;   // per-vertex U coord (range 0-255), type 9/10
	std::vector<u8_t>  v_vals;   // per-vertex V coord (range 0-255), type 9/10
};

//------------------------------------------------------------------------
// Safe byte-stream reader
//------------------------------------------------------------------------

class ByteReader
{
public:
	ByteReader(const u8_t *data, int size)
		: ptr_(data), end_(data + size), ok_(true)
	{ }

	bool Ok() const { return ok_; }

	u8_t  ReadU8()
	{
		if (ptr_ + 1 > end_) { ok_ = false; return 0; }
		return *ptr_++;
	}

	s8_t  ReadS8()  { return (s8_t)ReadU8(); }

	u16_t ReadU16LE()
	{
		if (ptr_ + 2 > end_) { ok_ = false; return 0; }
		u16_t v = (u16_t)(ptr_[0]) | ((u16_t)(ptr_[1]) << 8);
		ptr_ += 2;
		return v;
	}

	s16_t ReadS16LE() { return (s16_t)ReadU16LE(); }

	void Skip(int n)
	{
		if (ptr_ + n > end_) { ok_ = false; ptr_ = end_; return; }
		ptr_ += n;
	}

	int Remaining() const { return (int)(end_ - ptr_); }

private:
	const u8_t *ptr_;
	const u8_t *end_;
	bool ok_;
};

//------------------------------------------------------------------------
// Skin helpers
//------------------------------------------------------------------------

// Compute a face normal from three world-space vertices.
static vec3_c FaceNormal(const vec3_c &a, const vec3_c &b, const vec3_c &c)
{
	vec3_c ab = b - a;
	vec3_c ac = c - a;

	vec3_c n;
	n.x = ab.y * ac.z - ab.z * ac.y;
	n.y = ab.z * ac.x - ab.x * ac.z;
	n.z = ab.x * ac.y - ab.y * ac.x;

	float len = std::sqrt(n.x*n.x + n.y*n.y + n.z*n.z);
	if (len > 1e-6f)
	{
		n.x /= len;
		n.y /= len;
		n.z /= len;
	}
	else
	{
		n = vec3_c(0, 0, 1);
	}
	return n;
}

//------------------------------------------------------------------------
// AITDBodyLoader
//------------------------------------------------------------------------

AITDBodyLoader::AITDBodyLoader()
{
	// Initialise palette to a linear greyscale ramp so that bodies loaded
	// without a palette still produce visually distinct per-colour skins.
	for (int i = 0; i < 256; i++)
	{
		palette_[i * 3 + 0] = (u8_t)i;
		palette_[i * 3 + 1] = (u8_t)i;
		palette_[i * 3 + 2] = (u8_t)i;
	}
}

void AITDBodyLoader::SetPalette(const u8_t *palette_rgb768)
{
	SYS_ASSERT(palette_rgb768);
	memcpy(palette_, palette_rgb768, 768);
}

bool AITDBodyLoader::Probe(file_c * /*f*/)
{
	// The AITD body format has no magic number.  Never claim to recognise
	// an arbitrary file during AUTO detection.
	return false;
}

model_data_c *AITDBodyLoader::Load(file_c *f)
{
	SYS_ASSERT(f);

	// Read entire file into memory.
	int file_size = f->GetLength();
	if (file_size < 16)
	{
		I_Warning("AITDBodyLoader::Load: file too short (%d bytes)\n", file_size);
		return NULL;
	}

	std::vector<u8_t> raw((size_t)file_size);
	f->Seek(0, file_c::SEEKPOINT_START);
	if (f->Read(raw.data(), (unsigned)file_size) != (unsigned)file_size)
	{
		I_Warning("AITDBodyLoader::Load: read error\n");
		return NULL;
	}

	ByteReader r(raw.data(), file_size);

	//--------------------------------------------------------------------
	// Header: flags + bounding box
	//--------------------------------------------------------------------

	u16_t flags = r.ReadU16LE();

	// Bounding box (ZV: X1,X2, Y1,Y2, Z1,Z2) – stored but not critical for
	// the basic bind-pose load.
	s16_t zvx1 = r.ReadS16LE(), zvx2 = r.ReadS16LE();
	s16_t zvy1 = r.ReadS16LE(), zvy2 = r.ReadS16LE();
	s16_t zvz1 = r.ReadS16LE(), zvz2 = r.ReadS16LE();
	(void)zvx1; (void)zvx2;
	(void)zvy1; (void)zvy2;
	(void)zvz1; (void)zvz2;

	//--------------------------------------------------------------------
	// Scratch buffer (runtime-only; skip)
	//--------------------------------------------------------------------

	u16_t scratch_size = r.ReadU16LE();
	r.Skip((int)scratch_size);

	if (!r.Ok())
	{
		I_Warning("AITDBodyLoader::Load: truncated header\n");
		return NULL;
	}

	//--------------------------------------------------------------------
	// Vertices
	//--------------------------------------------------------------------

	u16_t num_verts = r.ReadU16LE();
	std::vector<aitd_vert_t> verts(num_verts);
	for (int i = 0; i < (int)num_verts; i++)
	{
		verts[i].x = r.ReadS16LE();
		verts[i].y = r.ReadS16LE();
		verts[i].z = r.ReadS16LE();
	}

	if (!r.Ok())
	{
		I_Warning("AITDBodyLoader::Load: truncated vertex list\n");
		return NULL;
	}

	//--------------------------------------------------------------------
	// Groups (skeleton, if INFO_ANIM)
	//--------------------------------------------------------------------

	std::vector<aitd_group_t> groups;
	std::vector<u16_t> group_order;

	if (flags & AITD_INFO_ANIM)
	{
		bool optimised = (flags & AITD_INFO_OPTIMISE) != 0;

		u16_t num_groups = r.ReadU16LE();
		groups.resize(num_groups);
		group_order.resize(num_groups);

		// Group-order table: byte offsets into the group array.
		// Divide by entry size to get indices.
		int entry_size = optimised ? 0x18 : 0x10;
		for (int i = 0; i < (int)num_groups; i++)
		{
			u16_t ofs = r.ReadU16LE();
			group_order[i] = (u16_t)(ofs / (u16_t)entry_size);
		}

		// Group entries
		for (int i = 0; i < (int)num_groups; i++)
		{
			// m_start is stored as a byte offset into the s16-triplet point
			// buffer (each vertex = 6 bytes), so divide by 6.
			groups[i].vert_start  = r.ReadS16LE() / 6;
			groups[i].vert_count  = r.ReadS16LE();
			groups[i].base_vert   = r.ReadS16LE() / 6;
			groups[i].org_group   = r.ReadS8();
			groups[i].group_id    = r.ReadS8();
			groups[i].state_type  = r.ReadS16LE();
			groups[i].delta[0]    = r.ReadS16LE();
			groups[i].delta[1]    = r.ReadS16LE();
			groups[i].delta[2]    = r.ReadS16LE();
			if (optimised)
			{
				groups[i].rotate_delta[0] = r.ReadS16LE();
				groups[i].rotate_delta[1] = r.ReadS16LE();
				groups[i].rotate_delta[2] = r.ReadS16LE();
				r.Skip(2); // padding
			}
			else
			{
				groups[i].rotate_delta[0] = 0;
				groups[i].rotate_delta[1] = 0;
				groups[i].rotate_delta[2] = 0;
			}
		}
	}

	if (!r.Ok())
	{
		I_Warning("AITDBodyLoader::Load: truncated group list\n");
		return NULL;
	}

	//--------------------------------------------------------------------
	// Primitives
	//--------------------------------------------------------------------

	u16_t num_prims = r.ReadU16LE();
	std::vector<aitd_prim_t> prims(num_prims);

	for (int i = 0; i < (int)num_prims; i++)
	{
		aitd_prim_t &p = prims[i];
		p.type     = (aitd_prim_e)r.ReadU8();
		p.sub_type = 0;
		p.color    = 0;
		p.padding  = 0;
		p.size     = 0;

		switch (p.type)
		{
		case AITD_PRIM_LINE:
			p.sub_type = r.ReadU8();
			p.color    = r.ReadU8();
			p.padding  = r.ReadU8();
			p.vert_refs.resize(2);
			for (int j = 0; j < 2; j++)
				p.vert_refs[j] = r.ReadU16LE() / 6;
			break;

		case AITD_PRIM_POLY:
		case AITD_PRIM_POLYTEX8:
		{
			u8_t cnt   = r.ReadU8();
			p.sub_type = r.ReadU8();
			p.color    = r.ReadU8();
			p.vert_refs.resize(cnt);
			for (int j = 0; j < (int)cnt; j++)
				p.vert_refs[j] = r.ReadU16LE() / 6;
			break;
		}

		case AITD_PRIM_POLYTEX9:
		case AITD_PRIM_POLYTEX10:
		{
			u8_t cnt   = r.ReadU8();
			p.sub_type = r.ReadU8();
			p.color    = r.ReadU8();
			p.vert_refs.resize(cnt);
			for (int j = 0; j < (int)cnt; j++)
				p.vert_refs[j] = r.ReadU16LE() / 6;
			// Per-vertex UV bytes (u then v, each 0–255)
			p.u_vals.resize(cnt);
			p.v_vals.resize(cnt);
			for (int j = 0; j < (int)cnt; j++)
			{
				p.u_vals[j] = r.ReadU8();
				p.v_vals[j] = r.ReadU8();
			}
			break;
		}

		case AITD_PRIM_POINT:
		case AITD_PRIM_BIGPOINT:
		case AITD_PRIM_ZIXEL:
			p.sub_type = r.ReadU8();
			p.color    = r.ReadU8();
			p.padding  = r.ReadU8();
			p.vert_refs.resize(1);
			p.vert_refs[0] = r.ReadU16LE() / 6;
			break;

		case AITD_PRIM_SPHERE:
		case AITD_PRIM_DISK:
		case AITD_PRIM_CYLINDER:
			p.sub_type = r.ReadU8();
			p.color    = r.ReadU8();
			p.padding  = r.ReadU8();
			p.size     = r.ReadU16LE();
			p.vert_refs.resize(1);
			p.vert_refs[0] = r.ReadU16LE() / 6;
			break;

		default:
			I_Warning("AITDBodyLoader::Load: unknown primitive type %d at "
			          "primitive %d – stopping parse\n",
			          (int)p.type, i);
			num_prims = (u16_t)i;
			prims.resize(i);
			// Force the reader into an error state so we exit cleanly.
			r.Skip(r.Remaining() + 1);
			break;
		}

		if (!r.Ok())
		{
			I_Warning("AITDBodyLoader::Load: truncated primitive %d\n", i);
			num_prims = (u16_t)i;
			prims.resize(i);
			break;
		}
	}

	//--------------------------------------------------------------------
	// Build model_data_c
	//--------------------------------------------------------------------
	//
	// Strategy
	// --------
	//  • One model_body_c per unique (skin_index) used by polygon primitives.
	//  • Each unique colour creates a 1×1 RGBA skin (flat colour tile).
	//  • Textured polys (types 9/10) create a name-based skin so the engine
	//    can substitute the real texture later.
	//  • Poly primitives are triangulated (fan from vertex 0).
	//  • Line / point / sphere / disk / cylinder primitives are skipped
	//    (they carry no surface geometry).
	//  • One frame (bind pose) is created; normals are computed per face
	//    and then averaged onto each shared vertex.
	//--------------------------------------------------------------------

	model_data_c *mdl = new model_data_c();
	mdl->format_name = "AITDBody";
	mdl->fps         = 0; // no animation in the bind-pose load

	// Scale factor: AITD integers are in units of ~1/1000 of a metre.
	const float kScale = 1.0f / 1000.0f;

	// Build a lookup from AITD palette index → EPI skin index.
	// key: packed u16 where high byte = 0xFF for flat-colour or the
	//       lower bits for the sub_type of a textured poly.
	std::map<u32_t, int> skin_map; // AITD_color_key → skins[] index

	auto get_flat_skin = [&](u8_t color_idx) -> int
	{
		u32_t key = 0xFF000000u | (u32_t)color_idx;
		auto it = skin_map.find(key);
		if (it != skin_map.end())
			return it->second;

		model_tex_c *tex = new model_tex_c();
		tex->name   = std::string("aitd:color:") + std::to_string(color_idx);
		tex->width  = 1;
		tex->height = 1;
		tex->pixels = new u8_t[4];
		tex->pixels[0] = palette_[color_idx * 3 + 0];
		tex->pixels[1] = palette_[color_idx * 3 + 1];
		tex->pixels[2] = palette_[color_idx * 3 + 2];
		tex->pixels[3] = 255;

		int idx = (int)mdl->skins.size();
		mdl->skins.push_back(tex);
		skin_map[key] = idx;
		return idx;
	};

	auto get_tex_skin = [&](u8_t color_idx, u8_t tex_type) -> int
	{
		u32_t key = ((u32_t)tex_type << 8) | (u32_t)color_idx;
		auto it = skin_map.find(key);
		if (it != skin_map.end())
			return it->second;

		model_tex_c *tex = new model_tex_c();
		tex->name   = std::string("aitd:tex:") + std::to_string(color_idx)
		              + ":" + std::to_string(tex_type);
		tex->width  = 0;  // unknown until engine loads the real texture
		tex->height = 0;
		tex->pixels = NULL;

		int idx = (int)mdl->skins.size();
		mdl->skins.push_back(tex);
		skin_map[key] = idx;
		return idx;
	};

	// Group polygon primitives by skin, building per-skin bodies.
	// We collect the expanded vertices and triangles for each body in these
	// parallel vectors, then materialise them into model_body_c objects.

	struct BodyBuild
	{
		int                      skin_idx;
		std::vector<model_vert_c> verts;  // expanded (pos, normal, uv)
		std::vector<model_tri_c>  tris;

		// Map from (aitd_vert_idx, u_byte, v_byte) → local vert index.
		// Packed as u64: (vert_idx<<16)|(u<<8)|v
		std::map<u64_t, u16_t> vert_cache;
	};

	std::map<int, BodyBuild> body_map; // skin_idx → BodyBuild

	auto get_or_add_vert = [&](BodyBuild &bb,
	                            u16_t aitd_idx,
	                            float u, float v) -> u16_t
	{
		u8_t ub = (u8_t)(u * 255.f + 0.5f);
		u8_t vb = (u8_t)(v * 255.f + 0.5f);
		u64_t key = ((u64_t)aitd_idx << 16) | ((u64_t)ub << 8) | (u64_t)vb;

		auto it = bb.vert_cache.find(key);
		if (it != bb.vert_cache.end())
			return it->second;

		model_vert_c mv;
		if (aitd_idx < (u16_t)verts.size())
		{
			mv.pos.x =  (float)verts[aitd_idx].x * kScale;
			mv.pos.y =  (float)verts[aitd_idx].y * kScale;
			mv.pos.z =  (float)verts[aitd_idx].z * kScale;
		}
		mv.normal = vec3_c(0, 0, 1); // placeholder; averaged below
		mv.uv.x   = u;
		mv.uv.y   = v;

		u16_t idx = (u16_t)bb.verts.size();
		bb.verts.push_back(mv);
		bb.vert_cache[key] = idx;
		return idx;
	};

	// Process all polygon-type primitives.
	for (int pi = 0; pi < (int)prims.size(); pi++)
	{
		const aitd_prim_t &p = prims[pi];
		int n = (int)p.vert_refs.size();

		// Only polygon types produce surface geometry.
		bool is_poly = (p.type == AITD_PRIM_POLY     ||
		                p.type == AITD_PRIM_POLYTEX8  ||
		                p.type == AITD_PRIM_POLYTEX9  ||
		                p.type == AITD_PRIM_POLYTEX10);
		if (!is_poly || n < 3)
			continue;

		// Determine skin.
		int skin_idx;
		bool has_uv = (p.type == AITD_PRIM_POLYTEX9 ||
		               p.type == AITD_PRIM_POLYTEX10);

		if (has_uv)
			skin_idx = get_tex_skin(p.color, p.sub_type);
		else
			skin_idx = get_flat_skin(p.color);

		BodyBuild &bb = body_map[skin_idx];
		bb.skin_idx   = skin_idx;

		// Per-vertex UV (in [0,1]).
		auto get_uv = [&](int j) -> std::pair<float,float>
		{
			if (has_uv && j < (int)p.u_vals.size())
				return { p.u_vals[j] / 255.f, p.v_vals[j] / 255.f };
			return { 0.f, 0.f }; // flat-colour: UV into 1×1 texel
		};

		// Triangulate as a fan from vertex 0.
		for (int j = 1; j + 1 < n; j++)
		{
			auto [u0, v0] = get_uv(0);
			auto [u1, v1] = get_uv(j);
			auto [u2, v2] = get_uv(j + 1);

			u16_t i0 = get_or_add_vert(bb, p.vert_refs[0],   u0, v0);
			u16_t i1 = get_or_add_vert(bb, p.vert_refs[j],   u1, v1);
			u16_t i2 = get_or_add_vert(bb, p.vert_refs[j+1], u2, v2);

			bb.tris.push_back(model_tri_c(i0, i1, i2));
		}
	}

	//--------------------------------------------------------------------
	// Average normals across each body's vertex list (flat shading per
	// triangle, summed and re-normalised for Gouraud if desired).
	//--------------------------------------------------------------------

	for (auto &kv : body_map)
	{
		BodyBuild &bb = kv.second;

		// Accumulator for normals (un-normalised sum).
		std::vector<vec3_c> norm_acc(bb.verts.size(), vec3_c(0, 0, 0));

		for (const model_tri_c &tri : bb.tris)
		{
			const vec3_c &pa = bb.verts[tri.index[0]].pos;
			const vec3_c &pb = bb.verts[tri.index[1]].pos;
			const vec3_c &pc = bb.verts[tri.index[2]].pos;
			vec3_c fn = FaceNormal(pa, pb, pc);

			norm_acc[tri.index[0]] = norm_acc[tri.index[0]] + fn;
			norm_acc[tri.index[1]] = norm_acc[tri.index[1]] + fn;
			norm_acc[tri.index[2]] = norm_acc[tri.index[2]] + fn;
		}

		for (int vi = 0; vi < (int)bb.verts.size(); vi++)
		{
			vec3_c &n = norm_acc[vi];
			float len = std::sqrt(n.x*n.x + n.y*n.y + n.z*n.z);
			if (len > 1e-6f)
			{
				n.x /= len; n.y /= len; n.z /= len;
			}
			else
			{
				n = vec3_c(0, 0, 1);
			}
			bb.verts[vi].normal = n;
		}
	}

	//--------------------------------------------------------------------
	// Create model_body_c and model_frame_c entries
	//--------------------------------------------------------------------

	// If no polygon primitives were found, still produce an empty model
	// rather than returning NULL.
	if (body_map.empty())
	{
		I_Warning("AITDBodyLoader::Load: no polygon primitives found; "
		          "model is empty\n");
		return mdl;
	}

	// Assign body indices in skin_idx order for determinism.
	mdl->frames.resize(1);
	model_frame_c &frame = mdl->frames[0];
	frame.name = "bind";

	// Pre-size the per-body vertex vectors in the frame.
	std::vector<std::pair<int,BodyBuild*>> sorted_bodies;
	for (auto &kv : body_map)
		sorted_bodies.push_back({ kv.first, &kv.second });

	std::sort(sorted_bodies.begin(), sorted_bodies.end(),
	          [](const std::pair<int,BodyBuild*> &a,
	             const std::pair<int,BodyBuild*> &b)
	          { return a.first < b.first; });

	frame.verts.resize(sorted_bodies.size());

	for (int bi = 0; bi < (int)sorted_bodies.size(); bi++)
	{
		BodyBuild *bb = sorted_bodies[bi].second;

		model_body_c *body = new model_body_c();
		body->name               = std::string("part") + std::to_string(bi);
		body->skin_index         = bb->skin_idx;
		body->tris               = bb->tris;
		body->num_verts_per_frame = (int)bb->verts.size();
		mdl->bodies.push_back(body);

		frame.verts[bi] = bb->verts;

		// Compute AABB contribution for the frame.
		bool first_v = true;
		for (const model_vert_c &v : bb->verts)
		{
			if (first_v) { frame.bbox = bbox3_c(v.pos); first_v = false; }
			else          { frame.bbox.Insert(v.pos); }
		}
	}

	//--------------------------------------------------------------------
	// Optionally create one body per skeleton group (when INFO_ANIM).
	// These bodies contain only the vertices owned by each group and the
	// polygon primitives that reference those vertices, providing an
	// approximate segment mesh useful for debugging the skeleton.
	//
	// Groups are stored with names "group0", "group1", … and a skin_index
	// of -1 (no skin) so that rendering code can ignore them by default.
	//--------------------------------------------------------------------

	if (!groups.empty())
	{
		// Gather which global AITD vertex indices belong to each group.
		std::vector<std::vector<u16_t>> group_verts(groups.size());
		for (int gi = 0; gi < (int)groups.size(); gi++)
		{
			int start = groups[gi].vert_start;
			int cnt   = groups[gi].vert_count;
			for (int vi = 0; vi < cnt; vi++)
			{
				u16_t global_idx = (u16_t)(start + vi);
				if (global_idx < (u16_t)verts.size())
					group_verts[gi].push_back(global_idx);
			}
		}

		// Grow the frame's verts vector to accommodate group bodies.
		int group_body_base = (int)mdl->bodies.size();
		frame.verts.resize(group_body_base + groups.size());

		for (int gi = 0; gi < (int)groups.size(); gi++)
		{
			model_body_c *body = new model_body_c();
			body->name        = std::string("group") + std::to_string(gi);
			body->skin_index  = -1;

			// Build a local vertex array from the global indices.
			std::vector<model_vert_c> gverts;
			std::map<u16_t, u16_t> gidx_map;
			for (u16_t gv : group_verts[gi])
			{
				u16_t local = (u16_t)gverts.size();
				gidx_map[gv] = local;

				model_vert_c mv;
				mv.pos.x = (float)verts[gv].x * kScale;
				mv.pos.y = (float)verts[gv].y * kScale;
				mv.pos.z = (float)verts[gv].z * kScale;
				mv.normal = vec3_c(0, 0, 1);
				mv.uv     = vec2_c(0, 0);
				gverts.push_back(mv);
			}

			// Include polygon primitives whose first vertex belongs to
			// this group.
			for (const aitd_prim_t &p : prims)
			{
				bool is_poly = (p.type == AITD_PRIM_POLY     ||
				                p.type == AITD_PRIM_POLYTEX8  ||
				                p.type == AITD_PRIM_POLYTEX9  ||
				                p.type == AITD_PRIM_POLYTEX10);
				int n = (int)p.vert_refs.size();
				if (!is_poly || n < 3)
					continue;
				if (gidx_map.find(p.vert_refs[0]) == gidx_map.end())
					continue;

				// Fan triangulate; skip verts not in this group.
				for (int j = 1; j + 1 < n; j++)
				{
					u16_t vi0 = p.vert_refs[0];
					u16_t vi1 = p.vert_refs[j];
					u16_t vi2 = p.vert_refs[j+1];

					auto it0 = gidx_map.find(vi0);
					auto it1 = gidx_map.find(vi1);
					auto it2 = gidx_map.find(vi2);
					if (it0 == gidx_map.end() ||
					    it1 == gidx_map.end() ||
					    it2 == gidx_map.end())
						continue;

					body->tris.push_back(
					    model_tri_c(it0->second, it1->second, it2->second));
				}
			}

			body->num_verts_per_frame = (int)gverts.size();
			frame.verts[group_body_base + gi] = gverts;
			mdl->bodies.push_back(body);
		}
	}

	return mdl;
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
