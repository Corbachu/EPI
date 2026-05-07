//----------------------------------------------------------------------------
//  Generic Sound Loader Helpers
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

#ifndef __EPI_SOUND_LOADER_H__
#define __EPI_SOUND_LOADER_H__

#include "epi.h"
#include "file.h"
#include "sound_data.h"

namespace epi
{

typedef enum
{
	SND_FMT_Unknown = 0,
	SND_FMT_WAV,
	SND_FMT_VOC,
	SND_FMT_OGG
}
sound_format_e;

sound_format_e Sound_DetectFormat(const byte *data, int length);

bool Sound_Load(sound_data_c *buf, file_c *f, bool preserve_stereo = true, sound_format_e *detected = nullptr);
bool Sound_LoadMemory(sound_data_c *buf, const byte *data, int length, bool preserve_stereo = true, sound_format_e *detected = nullptr);

} // namespace epi

#endif /* __EPI_SOUND_LOADER_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab