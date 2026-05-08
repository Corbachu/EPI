//----------------------------------------------------------------------------
//  PS Vita EPI System Specifics
//----------------------------------------------------------------------------
//
//  Copyright (c) 2024-2025  The EDGE Team.
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
// Platform header for the Sony PlayStation Vita using VitaSDK
// (https://vitasdk.org/).  Activated when __vita__ or VITA is defined.
//
#ifndef __VITA_EPI_HEADER__
#define __VITA_EPI_HEADER__

// Sanity checking – only one platform header may be active at a time.
#ifdef __EPI_HEADER_SYSTEM_SPECIFIC__
#error "Two different system specific EPI headers included"
#else
#define __EPI_HEADER_SYSTEM_SPECIFIC__
#endif

// ---- Path / filesystem -------------------------------------------------------
#define DIRSEPARATOR '/'

// ---- Compiler helpers --------------------------------------------------------
#define GCCATTR(xyz) __attribute__ (xyz)

// ---- Case-insensitive string comparison (POSIX) ------------------------------
#include <strings.h>
#define stricmp  strcasecmp
#define strnicmp strncasecmp

// ---- Binary file mode (POSIX already treats text == binary) ------------------
#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifndef D_OK
#define D_OK X_OK
#endif

// ---- Vita-specific memory constants (in bytes) --------------------------------
// The Vita has 512 MiB of LPDDR3 system RAM shared between the OS and apps.
// Applications typically get ~320-400 MiB depending on the firmware version.
#define VITA_SYSTEM_RAM_BYTES  (512u * 1024u * 1024u)
// Safe working budget for an application (conservative estimate).
#define VITA_APP_RAM_BUDGET_BYTES (256u * 1024u * 1024u)

// ---- Vita storage path macros ------------------------------------------------
// Standard VitaSDK path prefixes used for resource/save-data access.
#define VITA_APP_PATH     "app0:"       // read-only application data
#define VITA_DATA_PATH    "ux0:data/"   // writable user data root
#define VITA_TEMP_PATH    "ux0:temp/"   // writable temporary storage

#endif /* __VITA_EPI_HEADER__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
