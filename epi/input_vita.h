//----------------------------------------------------------------------------
//  EPI PS Vita Input Backend
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
// PlayStation Vita input implementation using VitaSDK.
//
// Hardware features exposed:
//   • Digital buttons   – SceCtrlData.buttons bitmask
//   • Dual analogue sticks – lx/ly/rx/ry (0-255 unsigned, centre ≈ 128)
//   • Front touch panel – up to 8 contacts via SceTouchData
//   • Rear touch panel  – up to 8 contacts via SceTouchData
//   • Motion sensors    – accelerometer + gyroscope via SceMotionSensorState
//
// VitaSDK headers used:
//   <psp2/ctrl.h>    – SceCtrlData, sceCtrlReadBufferPositive
//   <psp2/touch.h>   – SceTouchData, sceTouchRead
//   <psp2/motion.h>  – SceMotionSensorState, sceMotionGetSensorState
//
#ifndef __EPI_INPUT_VITA_H__
#define __EPI_INPUT_VITA_H__

#include "input.h"

#if defined(__vita__) || defined(VITA) || defined(PLATFORM_VITA)

// VitaSDK controller / touch / motion APIs
#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <psp2/motion.h>

namespace epi { namespace input { namespace vita {

/**
 * MapVitaButtons
 *
 * Converts a VitaSDK 32-bit button bitmask (SceCtrlData.buttons) into the
 * portable EPI ButtonBit mask.
 */
u32_t MapVitaButtons(unsigned int vita_buttons);

/**
 * NormAxis
 *
 * Normalises a raw Vita analogue axis byte (0-255, centre ≈ 128) to the
 * [-1, +1] float range used by the EPI input interface.
 */
float NormAxis(unsigned char raw);

} } } // namespace epi::input::vita

#endif /* VITA */
#endif /* __EPI_INPUT_VITA_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
