//------------------------------------------------------------------------
//  EDGE Type definitions
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

#ifndef __EPI_TYPE_H__
#define __EPI_TYPE_H__

#include <cstdint>
#include <cstddef>

// ---------------------------------------------------------------------------
// Fixed-width integer types – backed by <cstdint> for guaranteed sizes
// across all target platforms (x86-64, ARM Cortex-A9, SH-4, etc.).
// ---------------------------------------------------------------------------

typedef std::int8_t   s8_t;
typedef std::int16_t  s16_t;
typedef std::int32_t  s32_t;
typedef std::int64_t  s64_t;
typedef std::int64_t  i64_t;   // legacy alias

typedef std::uint8_t  u8_t;
typedef std::uint16_t u16_t;
typedef std::uint32_t u32_t;
typedef std::uint64_t u64_t;

// l32_t kept for backward compatibility (was 'long int', same width as s32_t)
typedef std::int32_t  l32_t;

typedef u8_t byte;

#endif  /*__EPI_TYPE_H__*/

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
