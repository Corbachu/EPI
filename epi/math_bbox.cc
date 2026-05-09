//------------------------------------------------------------------------
//  EPI Bounding Boxes
//------------------------------------------------------------------------
// 
//  Copyright (c) 2003-2008  The EDGE Team.
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
//------------------------------------------------------------------------

#include "epi.h"
#include "math_bbox.h"

#include <cstdio>

namespace epi
{

std::string bbox2_c::ToStr(int precision) const
{
    char buf[256];
    snprintf(buf, sizeof(buf),
             "[(%.*f,%.*f)..(%.*f,%.*f)]",
             precision, (double)lo.x, precision, (double)lo.y,
             precision, (double)hi.x, precision, (double)hi.y);
    return buf;
}

std::string bbox3_c::ToStr(int precision) const
{
    char buf[384];
    snprintf(buf, sizeof(buf),
             "[(%.*f,%.*f,%.*f)..(%.*f,%.*f,%.*f)]",
             precision, (double)lo.x, precision, (double)lo.y, precision, (double)lo.z,
             precision, (double)hi.x, precision, (double)hi.y, precision, (double)hi.z);
    return buf;
}

int bbox3_c::IntersectPlane(vec3_c loc, vec3_c face) const
{
	// determine the two points of the bounding box that are
	// nearest and furthest away from the plane (via the plane's
	// normal).  We only need to test these two points.

	vec3_c L(face.x < 0 ? hi.x : lo.x,
	         face.y < 0 ? hi.y : lo.y,
	         face.z < 0 ? hi.z : lo.z);

	vec3_c H(face.x < 0 ? lo.x : hi.x,
	         face.y < 0 ? lo.y : hi.y,
	         face.z < 0 ? lo.z : hi.z);

	// check the distances
	float L_dist = (L - loc) * face;

	if (L_dist >= 0)
		return HIT_INSIDE;

	float H_dist = (H - loc) * face;

	if (H_dist <= 0)
		return HIT_OUTSIDE;

	return HIT_PARTIAL;
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
