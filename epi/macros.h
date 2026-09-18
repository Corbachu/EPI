//------------------------------------------------------------------------
//  EDGE Macros
//------------------------------------------------------------------------
//
//  Copyright (c) 2003-2008  The EDGE Team.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public License
//  (LGPL) as published by the Free Software Foundation.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//------------------------------------------------------------------------

#ifndef __EPI_MACRO_H__
#define __EPI_MACRO_H__

// basic macros

#ifndef NULL
#define NULL  ((void*) 0)
#endif

#ifndef M_PI
#ifdef _arch_dreamcast
#define M_PI  3.14159265358979323846f
#else
#define M_PI  3.14159265358979323846
#endif
#endif

#ifndef M_ROOT2
#ifdef _arch_dreamcast
#define M_ROOT2  1.4142135623730950488f
#else
#define M_ROOT2  1.4142135623730950488
#endif
#endif

#ifndef MAX
#define MAX(a,b)  ((a) > (b) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b)  ((a) < (b) ? (a) : (b))
#endif

#ifndef ABS
#define ABS(a)  ((a) < 0 ? -(a) : (a))
#endif

#ifndef SGN
#define SGN(a)  ((a) < 0 ? -1 : (a) > 0 ? +1 : 0)
#endif

#ifndef I_ROUND
#define I_ROUND(x)  ((int) (((x) < 0.0f) ? ((x) - 0.5f) : ((x) + 0.5f)))
#endif

#ifndef CLAMP
#ifdef _arch_dreamcast
#define CLAMP( X, MIN, MAX )  ( (X)<(MIN) ? (MIN) : ((X)>(MAX) ? (MAX) : (X)) )
#else
#define CLAMP(low,x,high)  ((x) < (low) ? (low) : (x) > (high) ? (high) : (x))
#endif
#endif

#ifdef __cplusplus
#include <type_traits>

namespace epi
{
    template <typename A, typename B>
    constexpr typename std::common_type<A, B>::type Max(A a, B b)
    {
        using C = typename std::common_type<A, B>::type;
        return (C)a > (C)b ? (C)a : (C)b;
    }

    template <typename A, typename B>
    constexpr typename std::common_type<A, B>::type Min(A a, B b)
    {
        using C = typename std::common_type<A, B>::type;
        return (C)a < (C)b ? (C)a : (C)b;
    }

    template <typename T>
    constexpr T Abs(T a)
    {
        return a < (T)0 ? -a : a;
    }

    template <typename T>
    inline int IRound(T x)
    {
        const double d = (double)x;
        return (int)(d < 0.0 ? (d - 0.5) : (d + 0.5));
    }

    template <typename X, typename L, typename H>
    constexpr typename std::common_type<X, L, H>::type Clamp(X x, L low, H high)
    {
        using C = typename std::common_type<X, L, H>::type;
        return (C)x < (C)low ? (C)low : ((C)x > (C)high ? (C)high : (C)x);
    }
}

#undef MAX
#define MAX(a,b)  (epi::Max((a), (b)))

#undef MIN
#define MIN(a,b)  (epi::Min((a), (b)))

#undef ABS
#define ABS(a)  (epi::Abs((a)))

#undef I_ROUND
#define I_ROUND(x)  (epi::IRound((x)))

#ifdef _arch_dreamcast
#undef CLAMP
#define CLAMP(low,x,high)  (epi::Clamp((x), (low), (high)))
#else
#undef CLAMP
#define CLAMP(low,x,high)  (epi::Clamp((x), (low), (high)))
#endif

#endif  // __cplusplus

#define CHECK_SELF_ASSIGN(param)  \
    if (this == &param) return *this;

#endif  /* __EPI_MACRO_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
