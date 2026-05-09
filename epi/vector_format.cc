//----------------------------------------------------------------------------
//  EPI Vector / Angle / Matrix Format Helpers  (implementation)
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

#include "epi.h"
#include "vector_format.h"

#include <cstdio>
#include <string>

namespace epi
{

std::string MatFormat(const mat3_c& m, int prec)
{
    char buf[512];
    snprintf(buf, sizeof(buf),
        "| %.*f  %.*f  %.*f |\n"
        "| %.*f  %.*f  %.*f |\n"
        "| %.*f  %.*f  %.*f |",
        prec, (double)m.m[0], prec, (double)m.m[3], prec, (double)m.m[6],
        prec, (double)m.m[1], prec, (double)m.m[4], prec, (double)m.m[7],
        prec, (double)m.m[2], prec, (double)m.m[5], prec, (double)m.m[8]);
    return buf;
}

std::string MatFormat(const mat4_c& m, int prec)
{
    char buf[768];
    snprintf(buf, sizeof(buf),
        "| %.*f  %.*f  %.*f  %.*f |\n"
        "| %.*f  %.*f  %.*f  %.*f |\n"
        "| %.*f  %.*f  %.*f  %.*f |\n"
        "| %.*f  %.*f  %.*f  %.*f |",
        prec, (double)m.m[0],  prec, (double)m.m[4],  prec, (double)m.m[8],  prec, (double)m.m[12],
        prec, (double)m.m[1],  prec, (double)m.m[5],  prec, (double)m.m[9],  prec, (double)m.m[13],
        prec, (double)m.m[2],  prec, (double)m.m[6],  prec, (double)m.m[10], prec, (double)m.m[14],
        prec, (double)m.m[3],  prec, (double)m.m[7],  prec, (double)m.m[11], prec, (double)m.m[15]);
    return buf;
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
