//----------------------------------------------------------------------------
//  VOC Format Sound Loading
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

#include "epi.h"

#include <cmath>
#include <cstring>

#include "sound_voc.h"

namespace epi
{

bool VOC_Load(sound_data_c *buf, file_c *f)
{
	if (!buf || !f)
		return false;

	if (!f->Seek(0, file_c::SEEKPOINT_START))
		return false;

	const int size = f->GetLength();
	if (size < 32)
	{
		I_Warning("VOC Loader: file too small.\n");
		return false;
	}

	byte *data = f->LoadIntoMemory();
	if (!data)
	{
		I_Warning("VOC Loader: could not read file.\n");
		return false;
	}

	if (std::memcmp(data, "Creative Voice File", 19) != 0 || data[19] != 0x1A)
	{
		delete[] data;
		I_Warning("VOC Loader: bad Creative Voice header.\n");
		return false;
	}

	if (data[26] != 1)
	{
		delete[] data;
		I_Warning("VOC Loader: first block is not sound data.\n");
		return false;
	}

	const u32_t block_size = (u32_t)data[27] |
			((u32_t)data[28] << 8) |
			((u32_t)data[29] << 16);

	if (block_size < 2)
	{
		delete[] data;
		I_Warning("VOC Loader: invalid sound block size.\n");
		return false;
	}

	const int sample_count = (int)block_size - 2;
	if (32 + sample_count > size)
	{
		delete[] data;
		I_Warning("VOC Loader: truncated sound block.\n");
		return false;
	}

	const int sample_rate_div = (int)data[30];
	const int sample_rate = 1000000 / (256 - sample_rate_div);

	buf->freq = sample_rate;
	buf->Allocate(sample_count, SBUF_Mono);

	for (int i = 0; i < sample_count; i++)
	{
		const int sample = ((int)data[32 + i] - 128) << 8;
		buf->data_L[i] = (s16_t)sample;
	}

	delete[] data;
	return true;
}

void VOC_ApplyLowPass(sound_data_c *buf, float cutoff_hz)
{
	if (!buf || buf->length <= 0 || buf->freq <= 0)
		return;

	const float dt   = 1.0f / (float)buf->freq;
	const float ePow = 1.0f - expf(-dt * 2.0f * (float)M_PI * cutoff_hz);

	if (buf->data_L)
	{
		float out = (float)buf->data_L[0];
		for (int i = 1; i < buf->length; i++)
		{
			out += ((float)buf->data_L[i] - out) * ePow;
			buf->data_L[i] = (s16_t)(int)out;
		}
	}
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab