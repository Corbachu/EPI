//----------------------------------------------------------------------------
//  WAV Format Sound Loading
//----------------------------------------------------------------------------
// 
//  Copyright (c) 2007-2008  The EDGE Team.
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

#ifndef __EPI_SOUND_WAV_H__
#define __EPI_SOUND_WAV_H__

#include "file.h"
#include "sound_data.h"

namespace epi
{

bool WAV_LoadEx(sound_data_c *buf, file_c *f, bool preserve_stereo);
// Decode WAV format sound data from the given file stream, optionally keeping
// stereo channels separate instead of folding them down to mono.

bool WAV_Load(sound_data_c *buf, file_c *f);
// Decode WAV format sound data from the given file stream,
// storing the results in the given sound_data_c object.
// Returns false if something went wrong.

void WAV_ApplyLowPass(sound_data_c *buf, float cutoff_hz);
// Apply a one-pole IIR low-pass filter to the decoded PCM data in-place.
// cutoff_hz is the -3 dB corner frequency; the sample rate is taken from
// buf->freq.  Useful for telephone-quality or muffled-underwater effects.

} // namespace epi

#endif /* __EPI_SOUND_WAV_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
