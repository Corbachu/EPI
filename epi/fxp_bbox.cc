//------------------------------------------------------------------------
//  EPI Fixed-point Bounding Boxes
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

#include "fxp_bbox.h"
#include "str_format.h"

namespace epi
{

std::string xbox2_c::ToStr(int precision) const
{
	return STR_Format("[%1.*f:%1.*f %1.*f:%1.*f]",
			precision, static_cast<double>(lo.x.ToFloat()), precision, static_cast<double>(hi.x.ToFloat()),
			precision, static_cast<double>(lo.y.ToFloat()), precision, static_cast<double>(hi.y.ToFloat()));
}

std::string xbox3_c::ToStr(int precision) const
{
	return STR_Format("[%1.*f:%1.*f %1.*f:%1.*f %1.*f:%1.*f]",
			precision, static_cast<double>(lo.x.ToFloat()), precision, static_cast<double>(hi.x.ToFloat()),
			precision, static_cast<double>(lo.y.ToFloat()), precision, static_cast<double>(hi.y.ToFloat()),
			precision, static_cast<double>(lo.z.ToFloat()), precision, static_cast<double>(hi.z.ToFloat()));
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
