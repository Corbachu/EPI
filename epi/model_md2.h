//----------------------------------------------------------------------------
//  EPI MD2 Model Loader (Quake 2 format)
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
//  Format reference: Quake 2 MD2 specification (id Software)
//
//  Limits (hard-coded in Quake 2):
//    max vertices  per frame : 2048
//    max triangles           : 4096
//    max tex-coord pairs     : 4096
//    max animation frames    : 512
//    max skins               : 32
//
//  Vertex normals are stored as an index into a 162-entry precalculated
//  table (the Quake 2 anorm table).
//
//----------------------------------------------------------------------------

#ifndef __EPI_MODEL_MD2_H__
#define __EPI_MODEL_MD2_H__

#include "model_loader.h"

namespace epi
{

class MD2Loader : public model_loader_c
{
public:
	MD2Loader() { }
	virtual ~MD2Loader() { }

	virtual bool Probe(file_c *f) override;
	virtual model_data_c *Load(file_c *f) override;
	virtual const char *FormatName() const override { return "MD2"; }
};

} // namespace epi

#endif /* __EPI_MODEL_MD2_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
