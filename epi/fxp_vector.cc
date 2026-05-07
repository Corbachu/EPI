//------------------------------------------------------------------------
//  EPI Vector (point) types
//------------------------------------------------------------------------
//
//  Copyright (c) 2004-2026  The EDGE Team.
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
#include "fxp_vector.h"
#include "str_format.h"
#include "fxp_vector_sh4.h"

namespace epi {

//------------------------------------------------------------------------
// xvec2_c
//------------------------------------------------------------------------

fix_c xvec2_c::Length() const
{
#if DITD_ENABLE_EPI_SH4_ACCEL
    return epi::sh4::Dist2(x, y);
#else
    return fxdist2(x, y);
#endif
}

fix_c xvec2_c::PerpDist(const xvec2_c& point) const
{
#if DITD_ENABLE_EPI_SH4_ACCEL
    return epi::sh4::PerpDist(x, y, point.x, point.y);
#else
    return (point.x * y - point.y * x) / fxdist2(x, y);
#endif
}

fix_c xvec2_c::AlongDist(const xvec2_c& point) const
{
#if DITD_ENABLE_EPI_SH4_ACCEL
    return epi::sh4::AlongDist(x, y, point.x, point.y);
#else
    return (*this * point) / fxdist2(x, y);
#endif
}

std::string xvec2_c::ToStr(int precision) const
{
	return STR_Format("(%1.*f,%1.*f,%1.*f)",
				 precision, x.ToFloat(),
				 precision, y.ToFloat());
}

//------------------------------------------------------------------------
// xvec3_c
//------------------------------------------------------------------------

fix_c xvec3_c::Length() const
{
#if DITD_ENABLE_EPI_SH4_ACCEL
    return epi::sh4::Dist3(x, y, z);
#else
    return fxdist3(x, y, z);
#endif
}

fix_c xvec3_c::Slope() const
{
	fix_c dist (fxdist2(x, y));
	fix_c SM_Z (fxabs(z) >> 14);

	// prevent overflow or division by zero
	if (dist < SM_Z)
		dist = SM_Z;

	return z / dist;
}

fix_c xvec3_c::ApproxSlope() const
{
	fix_c ax (fxabs(x));
	fix_c ay (fxabs(y));

	// approximate distance
	fix_c dist ((ax > ay) ? (ax + ay >> 1) : (ay + ax >> 1));
	fix_c SM_Z (fxabs(z) >> 14);

	// prevent overflow or division by zero
	if (dist < SM_Z)
		dist = SM_Z;

	return z / dist;
}

fix_c xvec3_c::AlongDist(const xvec3_c& point) const
{
#if DITD_ENABLE_EPI_SH4_ACCEL
    return epi::sh4::AlongDist3(x, y, z, point.x, point.y, point.z);
#else
    return (*this * point) / Length();
#endif
}

xvec3_c xvec3_c::Cross(const xvec3_c& rhs) const
{
    return xvec3_c(y * rhs.z - z * rhs.y,
                   z * rhs.x - x * rhs.z,
                   x * rhs.y - y * rhs.x);
}

std::string xvec3_c::ToStr(int precision) const
{
    return STR_Format("(%1.*f,%1.*f,%1.*f)",
                      precision, x.ToFloat(),
                      precision, y.ToFloat(),
                      precision, z.ToFloat());
}

} // namespace epi