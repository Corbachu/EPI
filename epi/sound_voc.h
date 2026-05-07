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

#ifndef __EPI_SOUND_VOC_H__
#define __EPI_SOUND_VOC_H__

#include "file.h"
#include "sound_data.h"

namespace epi
{

bool VOC_Load(sound_data_c *buf, file_c *f);
// Decode the first Creative Voice data block from the given file stream,
// storing the results in signed 16-bit PCM.

} // namespace epi

#endif /* __EPI_SOUND_VOC_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab