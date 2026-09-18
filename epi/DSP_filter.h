//------------------------------------------------------------------------
//  EPI DSP Filter
//------------------------------------------------------------------------
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
//------------------------------------------------------------------------

#pragma once

namespace epi
{
    enum class FilterMode
    {
        LowPass,
        HighPass
    };

    class DSPFilter
    {
    public:
        DSPFilter();
        DSPFilter(FilterMode filter_mode, float cutoff_hz,
            float sample_period_seconds);

        float update(float input);
        float update(float input, float cutoff_hz,
            float sample_period_seconds);

        // Invalid configuration clears state and disables processing.
        bool reconfigure_filter(float cutoff_hz, float sample_period_seconds);

        // Supplying the current input avoids a transition impulse in high-pass mode.
        void set_mode(FilterMode new_mode, float initial_input = 0.0f);
        void reset(float initial_input = 0.0f);

        float get_output() const { return output_; }
        FilterMode get_mode() const { return mode_; }
        bool is_configured() const { return configured_; }

    private:
        float output_;
        float previous_input_;
        float decay_;
        FilterMode mode_;
        bool configured_;
    };

} // namespace epi
