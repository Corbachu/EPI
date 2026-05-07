//------------------------------------------------------------------------
//  libfastmem Dreamcast Shim
//------------------------------------------------------------------------
//
//  Copyright (c) 2025  The EDGE Team.
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

// Simple memory shim to centralize fastmem usage on Dreamcast
// Falls back to standard libc on other platforms

#pragma once

// NOTE: In DITD we keep FitdLib/System/dc_fastmem.h's fitd_memcpy/fitd_memset
// as the canonical Dreamcast wrappers (they handle SQ safety and small-copy
// overhead). This shim intentionally maps to libc so callers don't accidentally
// bypass those guards.

#include <string.h>

#define fm_memcpy memcpy
#define fm_memset memset
