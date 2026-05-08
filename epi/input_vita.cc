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
// The Vita exposes:
//   - SceCtrlData           (digital + analogue controller state)
//   - SceTouchData          (front and rear capacitive touch panels)
//   - SceMotionSensorState  (6-DOF IMU: 3-axis accelerometer + 3-axis gyro)
//
// All reads use a single-buffered "positive" mode so the latest hardware
// state is always returned without queuing.
//
#include "input.h"
#include "input_vita.h"

#if defined(__vita__) || defined(VITA) || defined(PLATFORM_VITA)

#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <psp2/motion.h>
#include <cstring>

namespace
{
    epi::u32_t g_cur_buttons  = 0;
    epi::u32_t g_prev_buttons = 0;

    epi::input::AnalogAxes  g_axes   = {};
    epi::input::TouchState  g_touch  = {};
    epi::input::MotionState g_motion = {};
}

namespace epi { namespace input {

// ---------------------------------------------------------------------------
// Public interface – Vita implementation
// ---------------------------------------------------------------------------

bool Init(void)
{
    // Enable analogue sticks (disabled by default in some firmware builds).
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);

    // Activate both front and rear touch panels.
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_BACK,  SCE_TOUCH_SAMPLING_STATE_START);

    // Activate the inertial measurement unit (IMU).
    sceMotionStartSampling();

    g_cur_buttons  = 0;
    g_prev_buttons = 0;
    std::memset(&g_axes,   0, sizeof(g_axes));
    std::memset(&g_touch,  0, sizeof(g_touch));
    std::memset(&g_motion, 0, sizeof(g_motion));

    return true;
}

void Shutdown(void)
{
    sceMotionStopSampling();
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_STOP);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_BACK,  SCE_TOUCH_SAMPLING_STATE_STOP);

    g_cur_buttons  = 0;
    g_prev_buttons = 0;
}

void Poll(void)
{
    g_prev_buttons = g_cur_buttons;

    // ---- Controller state ------------------------------------------------
    SceCtrlData ctrl = {};
    sceCtrlReadBufferPositive(0, &ctrl, 1);

    g_cur_buttons = vita::MapVitaButtons(ctrl.buttons);

    g_axes.left_x    =  vita::NormAxis(ctrl.lx);
    g_axes.left_y    = -vita::NormAxis(ctrl.ly);   // Y is inverted on hardware
    g_axes.right_x   =  vita::NormAxis(ctrl.rx);
    g_axes.right_y   = -vita::NormAxis(ctrl.ry);
    // Vita has no analogue triggers; use L1/R1 digital state as 0/1.
    g_axes.l_trigger = (ctrl.buttons & SCE_CTRL_L1) ? 1.0f : 0.0f;
    g_axes.r_trigger = (ctrl.buttons & SCE_CTRL_R1) ? 1.0f : 0.0f;

    // ---- Front touch panel -----------------------------------------------
    SceTouchData front_td = {};
    sceTouchRead(SCE_TOUCH_PORT_FRONT, &front_td, 1);
    g_touch.front_count = 0;
    for (unsigned i = 0; i < SCE_TOUCH_MAX_REPORT && i < kMaxTouchPoints; ++i)
    {
        const bool active = i < static_cast<unsigned>(front_td.reportNum);
        g_touch.front[i].active = active;
        if (active)
        {
            // Raw coords are in units of 1/2 pixel within a 1920×1088 space.
            g_touch.front[i].x = static_cast<float>(front_td.report[i].x) / 1920.0f;
            g_touch.front[i].y = static_cast<float>(front_td.report[i].y) / 1088.0f;
            ++g_touch.front_count;
        }
        else
        {
            g_touch.front[i].x = 0.0f;
            g_touch.front[i].y = 0.0f;
        }
    }

    // ---- Rear touch panel ------------------------------------------------
    SceTouchData rear_td = {};
    sceTouchRead(SCE_TOUCH_PORT_BACK, &rear_td, 1);
    g_touch.rear_count = 0;
    for (unsigned i = 0; i < SCE_TOUCH_MAX_REPORT && i < kMaxTouchPoints; ++i)
    {
        const bool active = i < static_cast<unsigned>(rear_td.reportNum);
        g_touch.rear[i].active = active;
        if (active)
        {
            g_touch.rear[i].x = static_cast<float>(rear_td.report[i].x) / 1920.0f;
            g_touch.rear[i].y = static_cast<float>(rear_td.report[i].y) / 1088.0f;
            ++g_touch.rear_count;
        }
        else
        {
            g_touch.rear[i].x = 0.0f;
            g_touch.rear[i].y = 0.0f;
        }
    }

    // ---- Motion sensors --------------------------------------------------
    SceMotionSensorState ms = {};
    sceMotionGetSensorState(&ms, 1);
    g_motion.accel_x = ms.accelerometer.x;
    g_motion.accel_y = ms.accelerometer.y;
    g_motion.accel_z = ms.accelerometer.z;
    g_motion.gyro_x  = ms.gyro.x;
    g_motion.gyro_y  = ms.gyro.y;
    g_motion.gyro_z  = ms.gyro.z;
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
// vita sub-namespace helpers
// ---------------------------------------------------------------------------
namespace epi { namespace input { namespace vita {

u32_t MapVitaButtons(unsigned int v)
{
    u32_t out = 0;

    if (v & SCE_CTRL_UP)        out |= BTN_UP;
    if (v & SCE_CTRL_DOWN)      out |= BTN_DOWN;
    if (v & SCE_CTRL_LEFT)      out |= BTN_LEFT;
    if (v & SCE_CTRL_RIGHT)     out |= BTN_RIGHT;

    if (v & SCE_CTRL_CROSS)     out |= BTN_A;
    if (v & SCE_CTRL_CIRCLE)    out |= BTN_B;
    if (v & SCE_CTRL_SQUARE)    out |= BTN_X;
    if (v & SCE_CTRL_TRIANGLE)  out |= BTN_Y;

    if (v & SCE_CTRL_START)     out |= BTN_START;
    if (v & SCE_CTRL_SELECT)    out |= BTN_SELECT;

    if (v & SCE_CTRL_L1)        out |= BTN_L1;
    if (v & SCE_CTRL_R1)        out |= BTN_R1;
    if (v & SCE_CTRL_L2)        out |= BTN_L2;
    if (v & SCE_CTRL_R2)        out |= BTN_R2;
    if (v & SCE_CTRL_L3)        out |= BTN_L3;
    if (v & SCE_CTRL_R3)        out |= BTN_R3;

    if (v & SCE_CTRL_PSBUTTON)  out |= BTN_HOME;

    return out;
}

float NormAxis(unsigned char raw)
{
    return (static_cast<float>(raw) - 128.0f) / 128.0f;
}

} } } // namespace epi::input::vita

#endif /* VITA */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
