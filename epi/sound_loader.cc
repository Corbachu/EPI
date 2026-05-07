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

#include <cstring>

#include "sound_loader.h"

#include "file_memory.h"
#include "sound_voc.h"
#include "sound_wav.h"

namespace epi
{

sound_format_e Sound_DetectFormat(const byte *data, int length)
{
	if (!data || length <= 0)
		return SND_FMT_Unknown;

	if (length >= 12 && std::memcmp(data, "RIFF", 4) == 0 && std::memcmp(data + 8, "WAVE", 4) == 0)
		return SND_FMT_WAV;

	if (length >= 20 && std::memcmp(data, "Creative Voice File", 19) == 0)
		return SND_FMT_VOC;

	if (length >= 4 && std::memcmp(data, "OggS", 4) == 0)
		return SND_FMT_OGG;

	return SND_FMT_Unknown;
}

bool Sound_Load(sound_data_c *buf, file_c *f, bool preserve_stereo, sound_format_e *detected)
{
	if (!buf || !f)
		return false;

	const int start_pos = f->GetPosition();
	byte header[32];
	const unsigned int got = f->Read(header, sizeof(header));

	if (!f->Seek(start_pos, file_c::SEEKPOINT_START))
		return false;

	const sound_format_e format = Sound_DetectFormat(header, (int)got);
	if (detected)
		*detected = format;

	switch (format)
	{
		case SND_FMT_WAV:
			return WAV_LoadEx(buf, f, preserve_stereo);

		case SND_FMT_VOC:
			return VOC_Load(buf, f);

		default:
			return false;
	}
}

bool Sound_LoadMemory(sound_data_c *buf, const byte *data, int length, bool preserve_stereo, sound_format_e *detected)
{
	if (!buf || !data || length <= 0)
		return false;

	mem_file_c mem(data, length, false);
	return Sound_Load(buf, &mem, preserve_stereo, detected);
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab