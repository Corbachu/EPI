//----------------------------------------------------------------------------
//  EPI Cross-Platform Input Abstraction
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
// Abstract input interface used by all EPI platform backends.
//
// Usage:
//   1. Call epi::input::Init() once after the platform is ready.
//   2. Call epi::input::Poll() every frame to sample hardware state.
//   3. Query button/axis/touch state via the accessor functions.
//   4. Call epi::input::Shutdown() on exit.
//
// Platform backends (input_dreamcast.cc, input_vita.cc, …) implement the
// functions declared here through a simple compile-time dispatch: only the
// source file matching the target platform is compiled into the project.
//
#ifndef __EPI_INPUT_H__
#define __EPI_INPUT_H__

#include "types.h"
#include <cstddef>

namespace epi { namespace input {

// ---------------------------------------------------------------------------
// Digital button bitmask – common across all supported platforms.
// Platforms that lack a button leave its bit permanently 0.
// ---------------------------------------------------------------------------
enum ButtonBit : u32_t
{
    BTN_UP        = (1u << 0),
    BTN_DOWN      = (1u << 1),
    BTN_LEFT      = (1u << 2),
    BTN_RIGHT     = (1u << 3),

    BTN_A         = (1u << 4),   // Dreamcast A  / Vita Cross
    BTN_B         = (1u << 5),   // Dreamcast B  / Vita Circle
    BTN_X         = (1u << 6),   // Dreamcast X  / Vita Square
    BTN_Y         = (1u << 7),   // Dreamcast Y  / Vita Triangle

    BTN_START     = (1u << 8),
    BTN_SELECT    = (1u << 9),   // Vita Select (no Dreamcast equivalent)

    BTN_L1        = (1u << 10),  // Dreamcast L  / Vita L
    BTN_R1        = (1u << 11),  // Dreamcast R  / Vita R
    BTN_L2        = (1u << 12),  // Vita L2  (not present on Dreamcast)
    BTN_R2        = (1u << 13),  // Vita R2  (not present on Dreamcast)
    BTN_L3        = (1u << 14),  // Vita L3 (left  stick click)
    BTN_R3        = (1u << 15),  // Vita R3 (right stick click)

    BTN_HOME      = (1u << 16),  // Vita PS button
};

// ---------------------------------------------------------------------------
// Analogue axes
// ---------------------------------------------------------------------------
struct AnalogAxes
{
    float left_x;    // left  stick X  [-1, +1]
    float left_y;    // left  stick Y  [-1, +1]
    float right_x;   // right stick X  [-1, +1]  (Vita only; 0 on Dreamcast)
    float right_y;   // right stick Y  [-1, +1]  (Vita only; 0 on Dreamcast)
    float l_trigger; // left  trigger  [ 0,  1]
    float r_trigger; // right trigger  [ 0,  1]
};

// ---------------------------------------------------------------------------
// Touch contact (PS Vita front / rear touch panels; unused on Dreamcast)
// ---------------------------------------------------------------------------
static constexpr std::size_t kMaxTouchPoints = 8;

struct TouchPoint
{
    float    x;       // normalised [0, 1], left → right
    float    y;       // normalised [0, 1], top  → bottom
    bool     active;  // true while finger is down
};

struct TouchState
{
    TouchPoint front[kMaxTouchPoints];
    TouchPoint rear [kMaxTouchPoints];
    std::size_t front_count;
    std::size_t rear_count;
};

// ---------------------------------------------------------------------------
// Motion / gyroscope (PS Vita only; values are 0 on Dreamcast)
// ---------------------------------------------------------------------------
struct MotionState
{
    float accel_x;   // accelerometer X (g)
    float accel_y;   // accelerometer Y (g)
    float accel_z;   // accelerometer Z (g)

    float gyro_x;    // gyroscope X (rad/s)
    float gyro_y;    // gyroscope Y (rad/s)
    float gyro_z;    // gyroscope Z (rad/s)
};

// ---------------------------------------------------------------------------
// Public interface – implemented per-platform
// ---------------------------------------------------------------------------

/**
 * Init – open hardware input devices and reset state.
 * Must be called once after the platform EPI::Init() has succeeded.
 * Returns true on success.
 */
bool Init(void);

/**
 * Shutdown – release input devices.
 */
void Shutdown(void);

/**
 * Poll – sample all hardware and update internal state.
 * Call once per frame before querying any state.
 */
void Poll(void);

/**
 * Buttons – returns bitmask of currently held buttons (ButtonBit flags).
 */
u32_t Buttons(void);

/**
 * ButtonsPressed – returns buttons that transitioned to pressed this frame.
 */
u32_t ButtonsPressed(void);

/**
 * ButtonsReleased – returns buttons that transitioned to released this frame.
 */
u32_t ButtonsReleased(void);

/**
 * Axes – fills *out with the current analogue axis state.
 */
void Axes(AnalogAxes* out);

/**
 * Touch – fills *out with the current touch panel state.
 * On platforms without a touch panel all counts are 0 and active is false.
 */
void Touch(TouchState* out);

/**
 * Motion – fills *out with the current motion sensor state.
 * On platforms without motion sensors all values are 0.
 */
void Motion(MotionState* out);

} } // namespace epi::input

#endif /* __EPI_INPUT_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
