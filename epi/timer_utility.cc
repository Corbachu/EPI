//----------------------------------------------------------------------------
//  EPI Timer Utility – implementation
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
//  Platform clock backends:
//    • Windows  – QueryPerformanceCounter (QPC)
//    • Dreamcast– KallistiOS timer_ms_gettime64 / thd_sleep
//    • PS Vita  – sceKernelGetProcessTimeWide / sceKernelDelayThread
//    • Linux / macOS / other POSIX – clock_gettime(CLOCK_MONOTONIC)
//
//----------------------------------------------------------------------------

#include "epi.h"
#include "timer_utility.h"

#include <cstring>

// ---------------------------------------------------------------------------
// Platform-specific clock access
// ---------------------------------------------------------------------------

#if defined(_WIN32) || defined(_WIN64)
#   define EPI_TIMER_WIN32 1
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   include <windows.h>

#elif defined(_arch_dreamcast)
#   define EPI_TIMER_DREAMCAST 1
#   include <kos.h>

#elif defined(__vita__)
#   define EPI_TIMER_VITA 1
#   include <psp2/kernel/processmgr.h>

#else
// POSIX fallback (Linux, macOS, BSD, …)
#   define EPI_TIMER_POSIX 1
#   include <time.h>
#   if !defined(__APPLE__)
#       include <unistd.h>
#   else
#       include <unistd.h>
#   endif
#endif

namespace epi
{

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------

static int   s_tic_rate      = kDefaultTicRate;

// Epoch: absolute time at TimerInit(), stored in microseconds.
static u64_t s_epoch_us      = 0;

// Previous tick timestamp (µs) for delta computation.
static u64_t s_prev_us       = 0;

// Last frame-start timestamp (µs) – used by LimitFPS.
static u64_t s_frame_start_us = 0;

static float s_delta_time    = 0.0f;  // seconds
static float s_fps           = 0.0f;  // rolling average

// Accumulator for tic counting.
static u64_t s_tic_accum_us  = 0;   // leftover µs below one tic boundary
static int   s_tic_counter   = 0;   // total elapsed tics since init
static int   s_pending_tics  = 0;   // tics not yet consumed

// Rolling FPS buffer (16 entries).
static const int kFPSWindow = 16;
static float s_fps_samples[kFPSWindow];
static int   s_fps_idx      = 0;
static bool  s_fps_ready    = false;

// ---------------------------------------------------------------------------
// Platform: get current time in microseconds (monotonic)
// ---------------------------------------------------------------------------

static u64_t PlatformGetUS()
{
#if defined(EPI_TIMER_WIN32)
    static LARGE_INTEGER freq  = {};
    static bool          got_freq = false;
    if (!got_freq)
    {
        QueryPerformanceFrequency(&freq);
        got_freq = true;
    }
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    // Convert to microseconds carefully to avoid overflow.
    return (u64_t)((now.QuadPart * 1000000ULL) / freq.QuadPart);

#elif defined(EPI_TIMER_DREAMCAST)
    uint32 sec  = 0;
    uint32 usec = 0;
    timer_ms_gettime(&sec, &usec); // KallistiOS gives ms, so we convert
    return (u64_t)sec * 1000000ULL + (u64_t)usec * 1000ULL;

#elif defined(EPI_TIMER_VITA)
    // sceKernelGetProcessTimeWide returns microseconds since process start.
    return (u64_t)sceKernelGetProcessTimeWide();

#else
    // POSIX: CLOCK_MONOTONIC in nanoseconds → microseconds.
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64_t)ts.tv_sec * 1000000ULL + (u64_t)ts.tv_nsec / 1000ULL;
#endif
}

// ---------------------------------------------------------------------------
// Platform: sleep for n milliseconds
// ---------------------------------------------------------------------------

static void PlatformSleepMS(u32_t ms)
{
#if defined(EPI_TIMER_WIN32)
    Sleep((DWORD)ms);

#elif defined(EPI_TIMER_DREAMCAST)
    thd_sleep(ms);

#elif defined(EPI_TIMER_VITA)
    sceKernelDelayThread((SceUInt32)ms * 1000u);

#else
    struct timespec req;
    req.tv_sec  = (time_t)(ms / 1000u);
    req.tv_nsec = (long)((ms % 1000u) * 1000000L);
    nanosleep(&req, NULL);
#endif
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void TimerInit(int tic_rate)
{
    s_tic_rate        = (tic_rate > 0) ? tic_rate : kDefaultTicRate;
    s_epoch_us        = PlatformGetUS();
    s_prev_us         = s_epoch_us;
    s_frame_start_us  = s_epoch_us;
    s_delta_time      = 0.0f;
    s_fps             = (float)s_tic_rate; // sensible initial estimate
    s_tic_accum_us    = 0;
    s_tic_counter     = 0;
    s_pending_tics    = 0;
    s_fps_idx         = 0;
    s_fps_ready       = false;

    for (int i = 0; i < kFPSWindow; i++)
        s_fps_samples[i] = (float)s_tic_rate;
}

void TimerTick()
{
    u64_t now_us = PlatformGetUS();

    // Delta time.
    u64_t elapsed_us  = now_us - s_prev_us;
    s_delta_time      = (float)elapsed_us * 1e-6f;
    s_prev_us         = now_us;

    // Rolling FPS average.
    if (s_delta_time > 0.0f)
    {
        s_fps_samples[s_fps_idx] = 1.0f / s_delta_time;
        s_fps_idx = (s_fps_idx + 1) % kFPSWindow;
        if (s_fps_idx == 0) s_fps_ready = true;

        int  count = s_fps_ready ? kFPSWindow : s_fps_idx;
        float sum  = 0.0f;
        for (int i = 0; i < count; i++)
            sum += s_fps_samples[i];
        s_fps = sum / (float)count;
    }

    // Tic accounting.
    s_tic_accum_us += elapsed_us;
    u64_t us_per_tic = 1000000ULL / (u64_t)s_tic_rate;
    while (s_tic_accum_us >= us_per_tic)
    {
        s_tic_accum_us -= us_per_tic;
        s_tic_counter++;
        s_pending_tics++;
    }
}

u32_t GetTimeMS()
{
    u64_t elapsed = PlatformGetUS() - s_epoch_us;
    return (u32_t)(elapsed / 1000ULL);
}

u64_t GetTimeUS()
{
    return PlatformGetUS() - s_epoch_us;
}

int GetTic()
{
    return s_tic_counter;
}

int GetPendingTics()
{
    return s_pending_tics;
}

void ConsumeTic()
{
    if (s_pending_tics > 0)
        s_pending_tics--;
}

void ConsumeAllTics()
{
    s_pending_tics = 0;
}

float GetDeltaTime()
{
    return s_delta_time;
}

float GetFPS()
{
    return s_fps;
}

void SleepMS(u32_t ms)
{
    if (ms == 0)
    {
        YieldCPU();
        return;
    }
    PlatformSleepMS(ms);
}

void YieldCPU()
{
#if defined(EPI_TIMER_WIN32)
    Sleep(0);
#elif defined(EPI_TIMER_DREAMCAST)
    thd_pass();
#elif defined(EPI_TIMER_VITA)
    sceKernelDelayThread(0);
#else
    usleep(0);
#endif
}

void LimitFPS(int max_fps)
{
    if (max_fps <= 0)
        return;

    u32_t target_ms = 1000u / (u32_t)max_fps;
    u64_t now_us    = PlatformGetUS();
    u64_t elapsed   = now_us - s_frame_start_us;
    u32_t elapsed_ms = (u32_t)(elapsed / 1000ULL);

    if (elapsed_ms < target_ms)
        PlatformSleepMS(target_ms - elapsed_ms);

    s_frame_start_us = PlatformGetUS();
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
