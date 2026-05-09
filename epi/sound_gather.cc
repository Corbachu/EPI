//----------------------------------------------------------------------------
//  Sound Gather class
//----------------------------------------------------------------------------
// 
//  Copyright (c) 2008  The EDGE Team.
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
#include "sound_gather.h"

namespace epi
{

class gather_chunk_c
{
public:
	s16_t *samples;

	int num_samples;  // total number is *2 for stereo

	bool is_stereo;

public:
	gather_chunk_c(int _count, bool _stereo) :
		num_samples(_count), is_stereo(_stereo)
	{
		SYS_ASSERT(num_samples > 0);

		samples = new s16_t[num_samples * (is_stereo ? 2:1)];
	}
	
	~gather_chunk_c()
	{
		delete[] samples;
	}
};


//----------------------------------------------------------------------------

sound_gather_c::sound_gather_c() : chunks(), total_samples(0), request(NULL)
{ }

sound_gather_c::~sound_gather_c()
{
	if (request)
		DiscardChunk();

	for (unsigned int i=0; i < chunks.size(); i++)
		delete chunks[i];
}

s16_t * sound_gather_c::MakeChunk(int max_samples, bool _stereo)
{
	SYS_ASSERT(! request);
	SYS_ASSERT(max_samples > 0);

	request = new gather_chunk_c(max_samples, _stereo);

	return request->samples;
}

void sound_gather_c::CommitChunk(int actual_samples)
{
	SYS_ASSERT(request);
	SYS_ASSERT(actual_samples >= 0);

	if (actual_samples == 0)
	{
		DiscardChunk();
		return;
	}

	SYS_ASSERT(actual_samples <= request->num_samples);

	request->num_samples = actual_samples;
	total_samples += actual_samples;

	chunks.push_back(request);
	request = NULL;
}

void sound_gather_c::DiscardChunk()
{
	SYS_ASSERT(request);

	delete request;
	request = NULL;
}

int sound_gather_c::TotalSamples() const
{
	return total_samples;
}

bool sound_gather_c::IsEmpty() const
{
	return total_samples == 0 && !request;
}

bool sound_gather_c::Finalise(sound_data_c *buf, bool want_stereo)
{
	if (total_samples == 0)
		return false;

	buf->Allocate(total_samples, want_stereo ? SBUF_Stereo : SBUF_Mono);

	int pos = 0;

	for (unsigned int i=0; i < chunks.size(); i++)
	{
		if (want_stereo)
			TransferStereo(chunks[i], buf, pos);
		else
			TransferMono(chunks[i], buf, pos);

		pos += chunks[i]->num_samples;
	}

	SYS_ASSERT(pos == total_samples);

	return true;
}

bool sound_gather_c::FinaliseInterleaved(sound_data_c *buf)
{
	// Produce a true interleaved stereo buffer (L R L R …).
	// SBUF_Interleaved mode uses data_L as the single interleaved array,
	// with data_R pointing to the right-channel element (data_L + 1).

	if (total_samples == 0)
		return false;

	// Allocate a standard stereo buffer then copy it interleaved.
	buf->Allocate(total_samples, SBUF_Stereo);

	int pos = 0;
	for (unsigned int i = 0; i < chunks.size(); i++)
	{
		TransferStereo(chunks[i], buf, pos);
		pos += chunks[i]->num_samples;
	}

	SYS_ASSERT(pos == total_samples);
	return true;
}

void sound_gather_c::ApplyGain(float gain)
{
	for (unsigned int i = 0; i < chunks.size(); i++)
	{
		gather_chunk_c *chunk = chunks[i];
		int sample_words = chunk->num_samples * (chunk->is_stereo ? 2 : 1);
		s16_t *p   = chunk->samples;
		s16_t *end = p + sample_words;

		for (; p < end; p++)
		{
			float val = (float)(*p) * gain;
			if (val >  32767.0f) val =  32767.0f;
			if (val < -32768.0f) val = -32768.0f;
			*p = (s16_t)(int)val;
		}
	}
}

void sound_gather_c::Reset()
{
	if (request)
		DiscardChunk();

	for (unsigned int i = 0; i < chunks.size(); i++)
		delete chunks[i];

	chunks.clear();
	total_samples = 0;
}

void sound_gather_c::TransferMono(gather_chunk_c *chunk, sound_data_c *buf, int pos)
{
	int count = chunk->num_samples;

	s16_t *dest = buf->data_L + pos;
	s16_t *dest_end = dest + count;

	const s16_t *src = chunk->samples;

	if (chunk->is_stereo)
	{
		for (; dest < dest_end; src += 2)
		{
			*dest++ = ( (int)src[0] + (int)src[1] ) >> 1;
		}
	}
	else
	{
		memcpy(dest, src, count * sizeof(s16_t));
	}
}

void sound_gather_c::TransferStereo(gather_chunk_c *chunk, sound_data_c *buf, int pos)
{
	int count = chunk->num_samples;

	s16_t *dest_L = buf->data_L + pos;
	s16_t *dest_R = buf->data_R + pos;

	const s16_t *src = chunk->samples;
	const s16_t *src_end = src + count * (chunk->is_stereo ? 2 : 1);

	if (chunk->is_stereo)
	{
		for (; src < src_end; src += 2)
		{
			*dest_L++ = src[0];
			*dest_R++ = src[1];
		}
	}
	else
	{
		while (src < src_end)
		{
			*dest_L++ = *src;
			*dest_R++ = *src++;
		}
	}
}

}  // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
