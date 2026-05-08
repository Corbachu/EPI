//----------------------------------------------------------------------------
//  EPI Dreamcast Input Backend
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
// Dreamcast input implementation using KallistiOS (KOS).
//
// KallistiOS controller API reference:
//   <dc/maple.h>          – maple bus enumeration
//   <dc/maple/controller.h> – cont_btn_state_t, cont_state_t
//
#ifndef __EPI_INPUT_DREAMCAST_H__
#define __EPI_INPUT_DREAMCAST_H__

// This header is only relevant when building for Dreamcast; including it on
// other platforms is a no-op thanks to the guard below.
#if defined(_arch_dreamcast) || defined(DREAMCAST) || defined(PLATFORM_DREAMCAST)

#include "input.h"

// KallistiOS headers – available only in the KOS cross-compilation
// environment.
#include <dc/maple.h>
#include <dc/maple/controller.h>

namespace epi { namespace input { namespace dreamcast {

// ---------------------------------------------------------------------------
// Dreamcast button-bit → EPI ButtonBit mapping
// ---------------------------------------------------------------------------
//
// KOS uses cont_btn_state_t flags (CONT_A, CONT_B, …).  We map them onto
// the portable EPI ButtonBit enum so higher-level code stays platform-agnostic.
//
// KOS flag          EPI bit
// ─────────────     ──────────
// CONT_DPAD_UP    → BTN_UP
// CONT_DPAD_DOWN  → BTN_DOWN
// CONT_DPAD_LEFT  → BTN_LEFT
// CONT_DPAD_RIGHT → BTN_RIGHT
// CONT_A          → BTN_A
// CONT_B          → BTN_B
// CONT_X          → BTN_X
// CONT_Y          → BTN_Y
// CONT_START      → BTN_START
// (no SELECT on Dreamcast)
// CONT_L          → BTN_L1
// CONT_R          → BTN_R1

/**
 * MapKOSButtons
 *
 * Converts a KOS 32-bit button bitfield (from cont_state_t::buttons) into the
 * EPI portable ButtonBit mask.  Exposed here so callers can perform manual
 * mapping if needed.
 */
u32_t MapKOSButtons(unsigned int kos_buttons);

} } } // namespace epi::input::dreamcast

#endif /* DREAMCAST */
#endif /* __EPI_INPUT_DREAMCAST_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
