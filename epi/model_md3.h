//----------------------------------------------------------------------------
//  EPI MD3 Model Loader (Quake 3 format)
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
//  Format reference: Quake 3 Arena MD3 specification
//
//  MD3 stores vertex positions as packed s16 values (xyz, normal).
//  Each surface carries its own per-frame vertex buffer, tex-coord
//  buffer, and index buffer.
//
//----------------------------------------------------------------------------

#ifndef __EPI_MODEL_MD3_H__
#define __EPI_MODEL_MD3_H__

#include "model_loader.h"

namespace epi
{

class MD3Loader : public model_loader_c
{
public:
	MD3Loader() { }
	virtual ~MD3Loader() { }

	virtual bool Probe(file_c *f) override;
	virtual model_data_c *Load(file_c *f) override;
	virtual const char *FormatName() const override { return "MD3"; }
};

} // namespace epi

#endif /* __EPI_MODEL_MD3_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
