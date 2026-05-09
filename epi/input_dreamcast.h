//----------------------------------------------------------------------------
//  EPI Dreamcast Input Backend
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
// Dreamcast input implementation using KallistiOS (KOS).
//
// KallistiOS controller API reference:
//   <dc/maple.h>             – maple bus enumeration
//   <dc/maple/controller.h>  – cont_btn_state_t, cont_state_t
//   <dc/maple/purupuru.h>    – Purupuru (Jump Pack / rumble) device
//   <dc/maple/vmu.h>         – Visual Memory Unit (VMU) I/O
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
#include <dc/maple/purupuru.h>
#include <dc/maple/vmu.h>

#include <cstdint>

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

// ---------------------------------------------------------------------------
// Purupuru (Jump Pack / Dreamcast Rumble Pack) support
// ---------------------------------------------------------------------------

// Attempt to find a connected Purupuru device on any Maple sub-port.
// Returns the maple_device_t pointer or nullptr if none found.
maple_device_t* FindPurupuru();

/**
 * RumbleStart
 *
 * Begin a rumble effect on the first connected Purupuru device.
 *
 * @param intensity   0–7  (0 = no effect, 7 = maximum)
 * @param include_motor  true to vibrate the main motor, false for the
 *                       micro (buzzer-style) motor only.
 *
 * This function maps the intensity onto the KOS purupuru_effect_t fields.
 * It is a no-op if no Purupuru device is connected.
 */
void RumbleStart(int intensity = 4, bool include_motor = true);

/**
 * RumbleStop
 *
 * Stop any ongoing rumble effect immediately.
 */
void RumbleStop();

// ---------------------------------------------------------------------------
// VMU (Visual Memory Unit) support
// ---------------------------------------------------------------------------
//
// The VMU is a peripheral that plugs into the controller's sub-port.
// It exposes a 128 × 64 monochrome LCD and a speaker, plus 200 "blocks"
// of VRAM-backed flash storage.
//
// VMU+ (enhanced VMU) uses the same maple function mask (MAPLE_FUNC_LCD +
// MAPLE_FUNC_MEMCARD) but supports colour displays.  We detect it by
// checking for the extended capability bit reported in the device_info.

/**
 * VMUDisplayBitmap
 *
 * Write a 128×64 bit-packed (1 bit per pixel) monochrome bitmap to the
 * VMU LCD of the first connected VMU found on sub-port 0 of port A.
 *
 * The bitmap array must be at least 128*64/8 = 1024 bytes long.
 * Bit 0 of byte 0 corresponds to the top-left pixel.
 *
 * Returns true on success, false if no VMU is connected.
 */
bool VMUDisplayBitmap(const uint8_t *bitmap_1bpp);

/**
 * VMUDisplayText
 *
 * Write a short text string (up to ~16 characters) to the VMU LCD using a
 * simple built-in proportional font.  Longer strings are clipped.
 *
 * Returns true on success, false if no VMU is connected.
 */
bool VMUDisplayText(const char *text);

/**
 * VMUClearDisplay
 *
 * Clear the VMU LCD to all-black.
 *
 * Returns true on success, false if no VMU is connected.
 */
bool VMUClearDisplay();

/**
 * VMUSaveBlock
 *
 * Write up to 'size' bytes from 'data' into the VMU flash at block
 * 'block_index'.  The data is padded with zeroes to fill a complete 512-byte
 * block.  Returns true on success.
 */
bool VMUSaveBlock(int block_index, const void *data, int size);

/**
 * VMULoadBlock
 *
 * Read a 512-byte block from the VMU flash at 'block_index' into 'out'.
 * 'out' must point to at least 512 bytes of storage.
 * Returns true on success.
 */
bool VMULoadBlock(int block_index, void *out);

/**
 * VMUGetFreeBlocks
 *
 * Returns the number of free 512-byte storage blocks on the first connected
 * VMU, or -1 if no VMU is connected.
 */
int VMUGetFreeBlocks();

// ---------------------------------------------------------------------------
// VMU+ (enhanced VMU) detection
// ---------------------------------------------------------------------------

/**
 * VMUPlusDetected
 *
 * Returns true if the connected VMU reports VMU+ extended capabilities
 * (colour LCD support).  Returns false for a standard VMU or if no VMU is
 * connected.
 */
bool VMUPlusDetected();

/**
 * VMUPlusDisplayRGB
 *
 * Write a 128×64 RGB565 framebuffer to a VMU+ display.
 * 'pixels' must be at least 128 * 64 * 2 bytes (one u16_t per pixel).
 *
 * On standard VMUs this function falls back to a 1-bit dithered bitmap.
 *
 * Returns true on success.
 */
bool VMUPlusDisplayRGB(const uint16_t *pixels_rgb565);

} } } // namespace epi::input::dreamcast

#endif /* DREAMCAST */
#endif /* __EPI_INPUT_DREAMCAST_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
