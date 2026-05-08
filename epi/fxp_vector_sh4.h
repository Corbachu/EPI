//------------------------------------------------------------------------
//  EPI Fixed-point Vector types (SH4-accelerated)
//------------------------------------------------------------------------
//
//  Copyright (c) 2004-2025  The EDGE Team.
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

#pragma once
#include "fxp_vector.h"
#if defined(DREAMCAST) && DITD_ENABLE_EPI_SH4_ACCEL
#include "../FitdLib/System/dc_fastmath.h"
#endif

// Enable via CMake:
//   - DITD_ENABLE_EPI_SH4_ACCEL=ON
#ifndef DITD_ENABLE_EPI_SH4_ACCEL
#define DITD_ENABLE_EPI_SH4_ACCEL 0
#endif

namespace epi::sh4 {

// ------------------------------------------------------------
// Fast fixed-point distance using SH-4 MAC + float sqrt
// ------------------------------------------------------------
static inline fix_c Dist2(fix_c x, fix_c y)
{
#if defined(DREAMCAST) && DITD_ENABLE_EPI_SH4_ACCEL
    // Convert to float
    float fx = fitd_int_to_float(x.v);
    float fy = fitd_int_to_float(y.v);

    // Use SH-4 fsqrt
    float d = fitd_sqrtf(fx*fx + fy*fy);

    // Convert back to fixed
    return fix_c(fitd_float_to_int_trunc(d));
#else
    return fxdist2(x, y);
#endif
}

static inline fix_c Dist3(fix_c x, fix_c y, fix_c z)
{
#if defined(DREAMCAST) && DITD_ENABLE_EPI_SH4_ACCEL
    float fx = fitd_int_to_float(x.v);
    float fy = fitd_int_to_float(y.v);
    float fz = fitd_int_to_float(z.v);

    float d = fitd_sqrtf(fx*fx + fy*fy + fz*fz);
    return fix_c(fitd_float_to_int_trunc(d));
#else
    return fxdist3(x, y, z);
#endif
}

// ------------------------------------------------------------
// Fast perpendicular distance using SH-4 reciprocal
// ------------------------------------------------------------
static inline fix_c PerpDist(fix_c x, fix_c y, fix_c px, fix_c py)
{
#if defined(DREAMCAST) && DITD_ENABLE_EPI_SH4_ACCEL
    float fx = fitd_int_to_float(x.v);
    float fy = fitd_int_to_float(y.v);
    float fpx = fitd_int_to_float(px.v);
    float fpy = fitd_int_to_float(py.v);

    float num = fpx * fy - fpy * fx;
    float den = fitd_sqrtf(fx*fx + fy*fy);

    float out = num * fitd_rcpf_fast2(den);
    return fix_c(fitd_float_to_int_trunc(out));
#else
    return (px * y - py * x) / fxdist2(x, y);
#endif
}

// ------------------------------------------------------------
// Fast along-track distance (dot/|v|) using SH-4 reciprocal
// ------------------------------------------------------------
static inline fix_c AlongDist(fix_c x, fix_c y, fix_c px, fix_c py)
{
#if defined(DREAMCAST) && DITD_ENABLE_EPI_SH4_ACCEL
    float fx = fitd_int_to_float(x.v);
    float fy = fitd_int_to_float(y.v);
    float fpx = fitd_int_to_float(px.v);
    float fpy = fitd_int_to_float(py.v);

    const float dot = fx * fpx + fy * fpy;
    const float den = fitd_sqrtf(fx * fx + fy * fy);

    // Match previous scalar behaviour (avoid div by zero blowups)
    if (den <= 0.0f)
        return fix_c(0);

    const float out = dot * fitd_rcpf_fast2(den);
    return fix_c(fitd_float_to_int_trunc(out));
#else
    return (px * x + py * y) / fxdist2(x, y);
#endif
}

static inline fix_c AlongDist3(fix_c x, fix_c y, fix_c z, fix_c px, fix_c py, fix_c pz)
{
#if defined(DREAMCAST) && DITD_ENABLE_EPI_SH4_ACCEL
    float fx = fitd_int_to_float(x.v);
    float fy = fitd_int_to_float(y.v);
    float fz = fitd_int_to_float(z.v);

    float fpx = fitd_int_to_float(px.v);
    float fpy = fitd_int_to_float(py.v);
    float fpz = fitd_int_to_float(pz.v);

    const float dot = fx * fpx + fy * fpy + fz * fpz;
    const float den = fitd_sqrtf(fx * fx + fy * fy + fz * fz);

    if (den <= 0.0f)
        return fix_c(0);

    const float out = dot * fitd_rcpf_fast2(den);
    return fix_c(fitd_float_to_int_trunc(out));
#else
    return (px * x + py * y + pz * z) / fxdist3(x, y, z);
#endif
}

} // namespace epi::sh4