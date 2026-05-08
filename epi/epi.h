//----------------------------------------------------------------------------
//  EDGE Platform Interface Header
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2008  The EDGE Team.
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

#ifndef __EDGE_PLATFORM_INTERFACE__
#define __EDGE_PLATFORM_INTERFACE__

#include "headers.h"
#include "types.h"
#include "macros.h"
#include "asserts.h"

#include <cstddef>

// ---------------------------------------------------------------------------
// Platform-specific header selection.
// The order matters: console SDKs define their own macros, check them first.
// ---------------------------------------------------------------------------
#if defined(_arch_dreamcast) || defined(DREAMCAST) || defined(PLATFORM_DREAMCAST)
#  include "epi_dreamcast.h"
#elif defined(__vita__) || defined(VITA) || defined(PLATFORM_VITA)
#  include "epi_vita.h"
#elif defined(_WIN32) || defined(_WIN64)
#  include "epi_win32.h"
#elif defined(__APPLE__)
#  include "epi_macosx.h"
#elif defined(__linux__) || defined(__unix__)
#  include "epi_linux.h"
#else
#  error "EPI: unsupported platform – add a new epi_<platform>.h"
#endif

// Optional third-party integrations (defined by build system)
#ifdef HAVE_COAL2
#  include "../COAL2/include/coal2.h"
#endif

#ifdef HAVE_PHYSFS
// PhysFS integration enabled
#endif

namespace epi
{
	// Base Functions
	bool Init(void);
	void Shutdown(void);
};

/* Important functions provided by Engine code */

void I_Error(const char *error,...) GCCATTR((format(printf, 1, 2)));
void I_Warning(const char *warning,...) GCCATTR((format(printf, 1, 2)));
void I_Printf(const char *message,...) GCCATTR((format(printf, 1, 2)));
void I_Debugf(const char *message,...) GCCATTR((format(printf, 1, 2)));

#endif /* __EDGE_PLATFORM_INTERFACE__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
