//----------------------------------------------------------------------------
//  EPI 3D Model Data
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
//  Common in-memory representation for low-poly 3D models produced by
//  the EPI model loaders (MD2, MD3, HLMDL, MD5).  The design is
//  deliberately format-agnostic so that rendering code can handle any
//  model type through a single interface.
//
//  Coordinate conventions (same as Quake/EDGE):
//    +X = right, +Y = into screen, +Z = up
//  Texture coordinates are (u, v) in [0,1]^2 with v increasing downward.
//
//----------------------------------------------------------------------------

#ifndef __EPI_MODEL_DATA_H__
#define __EPI_MODEL_DATA_H__

#include <cstring>
#include <string>
#include <vector>

#include "types.h"
#include "math_vector.h"
#include "math_bbox.h"
#include "rgl_vertex.h"

namespace epi
{

//------------------------------------------------------------------------
// model_vert_c – a single, fully-expanded vertex
//------------------------------------------------------------------------
//
// Each loaded vertex stores position, normal, and texture coordinate.
// When a model uses per-frame vertex positions (MD2/MD3-style) the
// loader expands them into a per-frame array of model_vert_c.
//
struct model_vert_c
{
	vec3_c pos;    // position in model-local space
	vec3_c normal; // surface normal (unit vector)
	vec2_c uv;     // texture coordinate

	model_vert_c() : pos(), normal(0, 0, 1), uv() { }

	model_vert_c(const vec3_c &p, const vec3_c &n, const vec2_c &t)
		: pos(p), normal(n), uv(t) { }
};


//------------------------------------------------------------------------
// model_tri_c – a triangle (three vertex indices into the body vertex list)
//------------------------------------------------------------------------
struct model_tri_c
{
	u16_t index[3]; // indices into model_body_c::verts[frame]

	model_tri_c() { index[0] = index[1] = index[2] = 0; }
	model_tri_c(u16_t a, u16_t b, u16_t c) { index[0]=a; index[1]=b; index[2]=c; }
};


//------------------------------------------------------------------------
// model_tex_c – one texture / skin reference
//------------------------------------------------------------------------
struct model_tex_c
{
	std::string name;   // path / lump name that was requested
	int         width;  // texel dimensions (0 = unknown)
	int         height;
	u8_t       *pixels; // RGBA pixel data; NULL = use engine texture cache

	model_tex_c()
		: name(), width(0), height(0), pixels(NULL) { }

	~model_tex_c() { delete[] pixels; }

	// non-copyable – textures own their pixel buffer
	model_tex_c(const model_tex_c &) = delete;
	model_tex_c &operator=(const model_tex_c &) = delete;
};


//------------------------------------------------------------------------
// model_frame_c – one animation frame
//------------------------------------------------------------------------
struct model_frame_c
{
	std::string name;  // frame name tag, e.g. "stand1", "run2"
	bbox3_c     bbox;  // axis-aligned bounding box for this frame

	// Per-body-part flattened vertex arrays (one entry per body_c).
	// Each entry is indexed as [body_index].
	// verts[b][v] is vertex v of body part b in this frame.
	// This vector is populated by the loader; the indices mirror the
	// body order in model_data_c::bodies.
	std::vector< std::vector<model_vert_c> > verts;

	model_frame_c() : name(), bbox(), verts() { }
};


//------------------------------------------------------------------------
// model_body_c – one sub-mesh / body part
//------------------------------------------------------------------------
//
// A model body part corresponds to a contiguous mesh with a single
// skin assignment.  MD2 models have a single body; MD3 models have
// one per surface; HL MDL models have explicit body parts.
//
class model_body_c
{
public:
	std::string name;   // body part name (e.g. "head", "torso", "legs")

	int skin_index;     // index into model_data_c::skins
	                    // -1 = no assigned skin

	std::vector<model_tri_c>  tris;   // triangle list (frame-independent)
	int                        num_verts_per_frame; // vertex count per frame

	model_body_c()
		: name(), skin_index(0), tris(), num_verts_per_frame(0) { }
};


//------------------------------------------------------------------------
// model_data_c – top-level model container
//------------------------------------------------------------------------
//
// The single object returned by every model loader.  Callers own the
// pointer and must delete it when done.
//
class model_data_c
{
public:
	std::string format_name; // "MD2", "MD3", "HLMDL", "MD5", …

	int fps;                 // suggested playback rate (frames per second);
	                         // 0 = unknown / not animated

	std::vector<model_body_c *> bodies; // body parts (owned)
	std::vector<model_tex_c *>  skins;  // skin / texture list (owned)
	std::vector<model_frame_c>  frames; // animation frames

	model_data_c() : format_name(), fps(10), bodies(), skins(), frames() { }

	~model_data_c()
	{
		for (model_body_c *b : bodies) delete b;
		for (model_tex_c  *t : skins)  delete t;
	}

	// Convenience helpers

	int NumFrames()  const { return (int)frames.size(); }
	int NumBodies()  const { return (int)bodies.size(); }
	int NumSkins()   const { return (int)skins.size(); }

	// Build a flat RGL_Vertex3Array for a given body/frame pair (useful for
	// immediate rendering without a hardware buffer cache).
	void BuildVertexArray(int body_idx, int frame_idx,
	                      RGL_Vertex3Array &out) const;

	// Lerp vertex positions between two frames (for smooth animation).
	// Writes the result into 'out'; out must already be sized correctly.
	void LerpVertices(int body_idx,
	                  int frame_a, int frame_b, float t,
	                  std::vector<model_vert_c> &out) const;

	// Compute a bounding box that covers all frames of a given body part.
	bbox3_c ComputeBounds(int body_idx) const;

private:
	model_data_c(const model_data_c &) = delete;
	model_data_c &operator=(const model_data_c &) = delete;
};

} // namespace epi

#endif /* __EPI_MODEL_DATA_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
