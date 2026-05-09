//----------------------------------------------------------------------------
//  EPI 3D Model Data – implementation
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
#include "model_data.h"

namespace epi
{

//------------------------------------------------------------------------
// model_data_c
//------------------------------------------------------------------------

void model_data_c::BuildVertexArray(int body_idx, int frame_idx,
                                    RGL_Vertex3Array &out) const
{
	SYS_ASSERT(body_idx  >= 0 && body_idx  < (int)bodies.size());
	SYS_ASSERT(frame_idx >= 0 && frame_idx < (int)frames.size());

	const model_body_c  &body  = *bodies[body_idx];
	const model_frame_c &frame = frames[frame_idx];

	SYS_ASSERT(body_idx < (int)frame.verts.size());

	const std::vector<model_vert_c> &fv = frame.verts[body_idx];

	out.Clear();
	out.Reserve(body.tris.size() * 3);

	for (const model_tri_c &tri : body.tris)
	{
		for (int k = 0; k < 3; k++)
		{
			u16_t vi = tri.index[k];
			SYS_ASSERT(vi < (u16_t)fv.size());

			const model_vert_c &mv = fv[vi];
			RGL_Vertex3f v;
			v.position = mv.pos;
			v.texcoord = mv.uv;
			// Color is left at default (white)
			out.Add(v);
		}
	}
}


void model_data_c::LerpVertices(int body_idx,
                                 int frame_a, int frame_b, float t,
                                 std::vector<model_vert_c> &out) const
{
	SYS_ASSERT(body_idx >= 0 && body_idx < (int)bodies.size());
	SYS_ASSERT(frame_a  >= 0 && frame_a  < (int)frames.size());
	SYS_ASSERT(frame_b  >= 0 && frame_b  < (int)frames.size());
	SYS_ASSERT(t >= 0.0f && t <= 1.0f);

	SYS_ASSERT(body_idx < (int)frames[frame_a].verts.size());
	SYS_ASSERT(body_idx < (int)frames[frame_b].verts.size());

	const std::vector<model_vert_c> &va = frames[frame_a].verts[body_idx];
	const std::vector<model_vert_c> &vb = frames[frame_b].verts[body_idx];

	SYS_ASSERT(va.size() == vb.size());

	out.resize(va.size());
	float inv_t = 1.0f - t;

	for (size_t i = 0; i < va.size(); i++)
	{
		out[i].pos    = va[i].pos    * inv_t + vb[i].pos    * t;
		out[i].normal = va[i].normal * inv_t + vb[i].normal * t;
		out[i].uv     = va[i].uv     * inv_t + vb[i].uv     * t;
	}
}


bbox3_c model_data_c::ComputeBounds(int body_idx) const
{
	SYS_ASSERT(body_idx >= 0 && body_idx < (int)bodies.size());

	bool first = true;
	bbox3_c box;

	for (const model_frame_c &frame : frames)
	{
		if (body_idx >= (int)frame.verts.size()) continue;
		const std::vector<model_vert_c> &fv = frame.verts[body_idx];
		for (const model_vert_c &mv : fv)
		{
			if (first)
			{
				box = bbox3_c(mv.pos);
				first = false;
			}
			else
			{
				box.Insert(mv.pos);
			}
		}
	}

	return box;
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
