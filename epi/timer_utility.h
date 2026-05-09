//----------------------------------------------------------------------------
//  EPI Timer Utility – high-resolution timing, tics, and framerate helpers
//----------------------------------------------------------------------------
//
//  Copyright (c) 2026  The EDGE Team.
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
//  Provides a thin, cross-platform wrapper around the platform clock and
//  exposes the time in several game-friendly units:
//
//    milliseconds (ms)   – u32_t, wraps after ~49 days
//    microseconds (us)   – u64_t, wraps after ~584,000 years
//    tics                – integer units at a configurable rate
//                          (default: TICRATE = 35 tics/sec, classic Doom)
//    delta-time (dt)     – floating-point seconds since last call
//    frames-per-second   – rolling average FPS
//
//  Usage pattern:
//
//    epi::TimerInit();                   // call once at startup
//
//    while (running)
//    {
//        epi::TimerTick();               // advance the internal clock
//
//        int   tic = epi::GetTic();      // current tic counter
//        float dt  = epi::GetDeltaTime();// seconds since last TimerTick()
//        float fps = epi::GetFPS();      // rolling average FPS
//
//        // limit to TICRATE (simulate fixed timestep):
//        while (epi::GetPendingTics() > 0)
//        {
//            epi::ConsumeTic();
//            UpdateGame();
//        }
//
//        RenderFrame(dt);
//    }
//
//----------------------------------------------------------------------------

#ifndef __EPI_TIMER_UTILITY_H__
#define __EPI_TIMER_UTILITY_H__

#include "types.h"

namespace epi
{

// ---------------------------------------------------------------------------
// Rate constants
// ---------------------------------------------------------------------------

static const int kDefaultTicRate = 35; // classic Doom tic rate

// ---------------------------------------------------------------------------
// Initialisation / per-frame tick
// ---------------------------------------------------------------------------

// Must be called once before any other timer function.
// Sets the tic rate (tics per second); use kDefaultTicRate for 35 Hz.
void TimerInit(int tic_rate = kDefaultTicRate);

// Advance the internal clock: update delta-time, tic counter, FPS average.
// Call exactly once per game loop iteration.
void TimerTick();

// ---------------------------------------------------------------------------
// Time queries (all valid after TimerInit(); most need at least one TimerTick)
// ---------------------------------------------------------------------------

// Elapsed milliseconds since TimerInit() (u32_t; wraps ~49 days).
u32_t GetTimeMS();

// Elapsed microseconds since TimerInit() (u64_t; effectively unbounded).
u64_t GetTimeUS();

// Current accumulated tic counter (advances at the tic rate).
int   GetTic();

// Number of tics that have elapsed since the last ConsumeTic() (or since
// TimerInit() if ConsumeTic() was never called).  Use this to drive a
// fixed-timestep simulation loop.
int   GetPendingTics();

// Consume one pending tic.  Decrement the pending counter by 1.
void  ConsumeTic();

// Consume all pending tics at once.
void  ConsumeAllTics();

// Seconds elapsed since the last TimerTick() call (floating point).
// Returns 0.0f on the very first tick.
float GetDeltaTime();

// Instantaneous frames-per-second (rolling 16-frame average).
float GetFPS();

// ---------------------------------------------------------------------------
// Convenience / sleep helpers
// ---------------------------------------------------------------------------

// Sleep for approximately the given number of milliseconds.
// The actual sleep may be longer due to OS scheduling granularity.
void SleepMS(u32_t ms);

// Yield the current timeslice (equivalent to SleepMS(0) on most platforms).
void YieldCPU();

// ---------------------------------------------------------------------------
// Frame-rate limiter helper
// ---------------------------------------------------------------------------

// Call at the end of each frame to enforce a maximum frame rate.
// If the frame finished faster than (1000 / max_fps) ms, the function
// sleeps for the remainder.  Has no effect if max_fps <= 0.
void LimitFPS(int max_fps);

} // namespace epi

#endif /* __EPI_TIMER_UTILITY_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
