//------------------------------------------------------------------------
//  EPI DSP Filter
//------------------------------------------------------------------------
//
//  Copyright (c) 2026  The EDGE Team.
//  Copyright (C) Jimmy van den Berg (original lowpass_filter implementation)
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
//------------------------------------------------------------------------
// 8/8/26 ~CA
// We re-wrote the lowpass_filter by the original author into a unified filter class
// that can be used for both low-pass and high-pass filtering. The original code was
// a bit messy and not very flexible, so we cleaned it up and made it more versatile.

#include "DSP_filter.h"

#include <cmath>
#include <cstdint>
#include <cstring>

namespace epi
{

namespace
{
constexpr float kPi = 3.14159265358979323846f;

bool is_positive_finite(float value)
{
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return (bits & 0x80000000u) == 0 &&
        (bits & 0x7fffffffu) != 0 &&
        (bits & 0x7f800000u) != 0x7f800000u;
}
} // namespace

DSPFilter::DSPFilter()
    : output_(0.0f),
      previous_input_(0.0f),
      decay_(0.0f),
      mode_(FilterMode::LowPass),
      configured_(false)
{}

DSPFilter::DSPFilter(FilterMode filter_mode, float cutoff_hz,
    float sample_period_seconds)
    : output_(0.0f),
      previous_input_(0.0f),
      decay_(0.0f),
      mode_(filter_mode),
      configured_(false)
{
    reconfigure_filter(cutoff_hz, sample_period_seconds);
}

float DSPFilter::update(float input)
{
    if (!configured_)
        return 0.0f;

    if (mode_ == FilterMode::LowPass)
        output_ += (input - output_) * (1.0f - decay_);
    else
    {
        output_ = decay_ * (output_ + input - previous_input_);
        previous_input_ = input;
    }

    return output_;
}

float DSPFilter::update(float input, float cutoff_hz,
    float sample_period_seconds)
{
    if (!reconfigure_filter(cutoff_hz, sample_period_seconds))
        return 0.0f;
    return update(input);
}

bool DSPFilter::reconfigure_filter(float cutoff_hz, float sample_period_seconds)
{
    if (!is_positive_finite(cutoff_hz) || !is_positive_finite(sample_period_seconds))
    {
        configured_ = false;
        decay_ = 0.0f;
        reset();
        return false;
    }

    decay_ = expf(-sample_period_seconds * 2.0f * kPi * cutoff_hz);
    if (decay_ >= 1.0f)
        decay_ = 0x1.fffffep-1f;
    configured_ = true;
    return true;
}

void DSPFilter::set_mode(FilterMode new_mode, float initial_input)
{
    if (mode_ == new_mode)
        return;

    mode_ = new_mode;
    reset(initial_input);
}

void DSPFilter::reset(float initial_input)
{
    previous_input_ = initial_input;
    output_ = mode_ == FilterMode::LowPass ? initial_input : 0.0f;
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab