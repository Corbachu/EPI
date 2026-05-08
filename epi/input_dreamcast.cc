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
// Dreamcast input implementation backed by KallistiOS (KOS).
//
// The Dreamcast uses a Maple bus with up to 4 ports (A-D); each port can
// host one main device plus two sub-peripheral slots.  Standard controllers
// report digital buttons plus two analogue thumb-sticks / triggers via the
// cont_state_t structure returned by maple_enum_type().
//
// This backend always reads from port A (player 1) and maps the result to
// the portable epi::input interface.  It falls back to all-zero state if
// no controller is connected.
//
#include "input.h"
#include "input_dreamcast.h"

#if defined(_arch_dreamcast) || defined(DREAMCAST) || defined(PLATFORM_DREAMCAST)

#include <dc/maple.h>
#include <dc/maple/controller.h>
#include <cstring>

namespace
{
    // Current and previous button masks (EPI format)
    epi::u32_t g_cur_buttons  = 0;
    epi::u32_t g_prev_buttons = 0;

    epi::input::AnalogAxes g_axes  = {};
    // Dreamcast has no touch / motion hardware; keep zero structs.
    epi::input::TouchState  g_touch  = {};
    epi::input::MotionState g_motion = {};

    // Normalise an unsigned 8-bit analogue value (0-255) to [-1, +1].
    static float axis_norm(unsigned char v)
    {
        return (static_cast<float>(v) - 128.0f) / 128.0f;
    }

    // Normalise an unsigned 8-bit trigger value (0-255) to [0, +1].
    static float trigger_norm(unsigned char v)
    {
        return static_cast<float>(v) / 255.0f;
    }
}

namespace epi { namespace input {

// ---------------------------------------------------------------------------
// Public interface implementation
// ---------------------------------------------------------------------------

bool Init(void)
{
    g_cur_buttons  = 0;
    g_prev_buttons = 0;
    std::memset(&g_axes,   0, sizeof(g_axes));
    std::memset(&g_touch,  0, sizeof(g_touch));
    std::memset(&g_motion, 0, sizeof(g_motion));
    return true;  // KOS initialises the Maple bus during arch_init()
}

void Shutdown(void)
{
    g_cur_buttons  = 0;
    g_prev_buttons = 0;
}

void Poll(void)
{
    g_prev_buttons = g_cur_buttons;

    // Find the first connected standard controller on any Maple port.
    maple_device_t* dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    if (!dev)
    {
        g_cur_buttons = 0;
        std::memset(&g_axes, 0, sizeof(g_axes));
        return;
    }

    cont_state_t* st = reinterpret_cast<cont_state_t*>(
        maple_dev_status(dev));
    if (!st)
    {
        g_cur_buttons = 0;
        std::memset(&g_axes, 0, sizeof(g_axes));
        return;
    }

    // Map KOS button bits → EPI button bits.
    g_cur_buttons = dreamcast::MapKOSButtons(
        static_cast<unsigned int>(st->buttons));

    // Analogue left stick (joyx, joyy) and triggers (rtrig, ltrig).
    g_axes.left_x    =  axis_norm(static_cast<unsigned char>(st->joyx));
    g_axes.left_y    = -axis_norm(static_cast<unsigned char>(st->joyy)); // Y is inverted
    g_axes.right_x   = 0.0f;
    g_axes.right_y   = 0.0f;
    g_axes.l_trigger = trigger_norm(static_cast<unsigned char>(st->ltrig));
    g_axes.r_trigger = trigger_norm(static_cast<unsigned char>(st->rtrig));
}

u32_t Buttons(void)         { return g_cur_buttons; }
u32_t ButtonsPressed(void)  { return  g_cur_buttons & ~g_prev_buttons; }
u32_t ButtonsReleased(void) { return ~g_cur_buttons &  g_prev_buttons; }

void Axes(AnalogAxes* out)
{
    if (out) *out = g_axes;
}

void Touch(TouchState* out)
{
    if (out) *out = g_touch;
}

void Motion(MotionState* out)
{
    if (out) *out = g_motion;
}

} } // namespace epi::input

// ---------------------------------------------------------------------------
// dreamcast sub-namespace helpers
// ---------------------------------------------------------------------------
namespace epi { namespace input { namespace dreamcast {

u32_t MapKOSButtons(unsigned int kos)
{
    u32_t out = 0;

    if (kos & CONT_DPAD_UP)    out |= BTN_UP;
    if (kos & CONT_DPAD_DOWN)  out |= BTN_DOWN;
    if (kos & CONT_DPAD_LEFT)  out |= BTN_LEFT;
    if (kos & CONT_DPAD_RIGHT) out |= BTN_RIGHT;

    if (kos & CONT_A)          out |= BTN_A;
    if (kos & CONT_B)          out |= BTN_B;
    if (kos & CONT_X)          out |= BTN_X;
    if (kos & CONT_Y)          out |= BTN_Y;

    if (kos & CONT_START)      out |= BTN_START;
    if (kos & CONT_L)          out |= BTN_L1;
    if (kos & CONT_R)          out |= BTN_R1;

    return out;
}

} } } // namespace epi::input::dreamcast

#endif /* DREAMCAST */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
