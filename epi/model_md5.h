//----------------------------------------------------------------------------
//  EPI MD5 Model Loader (Doom 3 / md5mesh + md5anim format)
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
//  Format reference: Doom 3 MD5 specification (id Software, text-based)
//
//  An MD5 model file (md5mesh) contains:
//    - A joint hierarchy (skeleton) with bind-pose transforms
//    - One or more meshes, each with:
//        - A shader/texture name
//        - Vertex list with (u, v) and weight range
//        - Triangle list
//        - Weight list: (joint_index, bias, pos_in_joint_space)
//
//  Vertex positions are computed from the skeleton by:
//      pos = sum_over_weights( joint_transform * weight.pos * weight.bias )
//
//  This loader integrates the md5_conv logic from EDGE to produce a
//  bind-pose model_data_c.  Animation (.md5anim) parsing is provided
//  separately via LoadAnim().
//
//----------------------------------------------------------------------------

#ifndef __EPI_MODEL_MD5_H__
#define __EPI_MODEL_MD5_H__

#include "model_loader.h"
#include "math_quaternion.h"

#include <string>
#include <vector>

namespace epi
{

//------------------------------------------------------------------------
// MD5 joint (bind-pose skeleton)
//------------------------------------------------------------------------
struct md5_joint_t
{
	std::string name;
	int         parent;   // -1 = root
	vec3_c      pos;      // bind-pose position
	quat_c      orient;   // bind-pose orientation
};

//------------------------------------------------------------------------
// MD5 mesh vertex (before skinning)
//------------------------------------------------------------------------
struct md5_vertex_t
{
	vec2_c uv;
	int    weight_start;
	int    weight_count;
};

//------------------------------------------------------------------------
// MD5 weight
//------------------------------------------------------------------------
struct md5_weight_t
{
	int    joint_index;
	float  bias;
	vec3_c pos;  // position in joint local space
};

//------------------------------------------------------------------------
// MD5 mesh (one surface of the model)
//------------------------------------------------------------------------
struct md5_mesh_t
{
	std::string              shader;
	std::vector<md5_vertex_t> verts;
	std::vector<model_tri_c>  tris;
	std::vector<md5_weight_t> weights;
};

//------------------------------------------------------------------------
// MD5 animation frame data
//------------------------------------------------------------------------
struct md5_anim_frame_t
{
	std::vector<md5_joint_t> joints; // skeleton in this frame
};

struct md5_anim_t
{
	std::string                 name;
	int                         frame_rate;
	std::vector<md5_anim_frame_t> frames;
};


//------------------------------------------------------------------------
// MD5Loader – implements model_loader_c
//------------------------------------------------------------------------
class MD5Loader : public model_loader_c
{
public:
	MD5Loader() { }
	virtual ~MD5Loader() { }

	virtual bool Probe(file_c *f) override;
	virtual model_data_c *Load(file_c *f) override;
	virtual const char *FormatName() const override { return "MD5"; }

	// Load an animation from a .md5anim file and expand it into
	// the model, adding one model_frame_c per animation frame.
	bool LoadAnim(file_c *f, model_data_c *mdl, const md5_anim_t &anim);

private:
	// Parse a null-terminated text buffer as an md5mesh file.
	bool ParseMesh(const char *text, size_t len,
	               std::vector<md5_joint_t> &joints,
	               std::vector<md5_mesh_t>  &meshes);

	// Skin a mesh using the given skeleton, producing one set of verts.
	void SkinMesh(const md5_mesh_t        &mesh,
	              const std::vector<md5_joint_t> &joints,
	              std::vector<model_vert_c>      &out);
};

} // namespace epi

#endif /* __EPI_MODEL_MD5_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
