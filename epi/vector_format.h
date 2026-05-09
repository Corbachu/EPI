//----------------------------------------------------------------------------
//  EPI Vector / Angle / Matrix Format Helpers
//----------------------------------------------------------------------------
//
//  Copyright (c) 2024-2026  The EDGE Team.
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
// Human-readable string formatting for all EPI math types.  Centralising
// these routines avoids scattering printf-style format strings throughout
// every math header and lets callers choose the precision they need.
//
//----------------------------------------------------------------------------

#ifndef __EPI_VECTOR_FORMAT_H__
#define __EPI_VECTOR_FORMAT_H__

#include <string>
#include "math_vector.h"
#include "math_angle.h"
#include "math_bbox.h"
#include "math_matrix.h"

namespace epi
{

//----------------------------------------------------------------------------
// vec2_c formatters
//----------------------------------------------------------------------------

// "(x, y)" with given decimal precision.
inline std::string VecFormat(const vec2_c& v, int prec = 3)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "(%.*f, %.*f)", prec, (double)v.x, prec, (double)v.y);
    return buf;
}

// Compact form "x y" suitable for log lines.
inline std::string VecFormatCompact(const vec2_c& v, int prec = 2)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "%.*f %.*f", prec, (double)v.x, prec, (double)v.y);
    return buf;
}

//----------------------------------------------------------------------------
// vec3_c formatters
//----------------------------------------------------------------------------

inline std::string VecFormat(const vec3_c& v, int prec = 3)
{
    char buf[160];
    snprintf(buf, sizeof(buf), "(%.*f, %.*f, %.*f)",
             prec, (double)v.x, prec, (double)v.y, prec, (double)v.z);
    return buf;
}

inline std::string VecFormatCompact(const vec3_c& v, int prec = 2)
{
    char buf[160];
    snprintf(buf, sizeof(buf), "%.*f %.*f %.*f",
             prec, (double)v.x, prec, (double)v.y, prec, (double)v.z);
    return buf;
}

//----------------------------------------------------------------------------
// vec4_c formatters
//----------------------------------------------------------------------------

inline std::string VecFormat(const vec4_c& v, int prec = 3)
{
    char buf[192];
    snprintf(buf, sizeof(buf), "(%.*f, %.*f, %.*f, %.*f)",
             prec, (double)v.x, prec, (double)v.y,
             prec, (double)v.z, prec, (double)v.w);
    return buf;
}

//----------------------------------------------------------------------------
// ivec_c formatter
//----------------------------------------------------------------------------

inline std::string VecFormat(const ivec_c& v)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "(%d, %d)", v.x, v.y);
    return buf;
}

//----------------------------------------------------------------------------
// angle_c formatters
//----------------------------------------------------------------------------

// Degrees with given precision, e.g. "90.000°"
inline std::string AngleFormatDeg(const angle_c& a, int prec = 1)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%.*f\xC2\xB0", prec, (double)a.Degrees());
    return buf;
}

// Radians representation.
inline std::string AngleFormatRad(const angle_c& a, int prec = 4)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%.*f rad", prec, a.Radians());
    return buf;
}

// Direction vector "(cos, sin)".
inline std::string AngleFormatVec(const angle_c& a, int prec = 4)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "(%.*f, %.*f)",
             prec, (double)a.getX(), prec, (double)a.getY());
    return buf;
}

//----------------------------------------------------------------------------
// bbox2_c / bbox3_c formatters
//----------------------------------------------------------------------------

inline std::string BBoxFormat(const bbox2_c& bb, int prec = 2)
{
    // Access lo/hi via the public Center/Area approach isn't available yet
    // so we leverage the existing ToStr member added in math_bbox.h.
    return bb.ToStr(prec);
}

inline std::string BBoxFormat(const bbox3_c& bb, int prec = 2)
{
    return bb.ToStr(prec);
}

//----------------------------------------------------------------------------
// mat3_c / mat4_c formatters
//----------------------------------------------------------------------------

// Pretty-print a 3x3 matrix as three rows.
std::string MatFormat(const mat3_c& m, int prec = 4);

// Pretty-print a 4x4 matrix as four rows.
std::string MatFormat(const mat4_c& m, int prec = 4);

} // namespace epi

#endif /* __EPI_VECTOR_FORMAT_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
