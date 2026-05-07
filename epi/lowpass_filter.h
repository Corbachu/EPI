//------------------------------------------------------------------------
//  EPI LowPass Filter
//------------------------------------------------------------------------
//
//  Copyright (c) 2004-2008  The EDGE Team.
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

#ifndef _lowpass_filter_h_
#define _lowpass_filter_h_

#include <cmath>

namespace epi
{
    class LowPassFilter
    {
    public:
        //constructors
        LowPassFilter();
        LowPassFilter(float iCutOffFrequency, float iDeltaTime);
        //functions
        float update(float input);
        float update(float input, float deltaTime, float cutoffFrequency);
        //get and configure funtions
        float getOutput() const{return output;}
        void setOutput(float v) { output = v; }
        void reconfigureFilter(float deltaTime, float cutoffFrequency);
    private:
        float output;
        float ePow;
    };
    
} // namespace epi

#endif //_lowpass_filter_h_