//----------------------------------------------------------------------------
//  EPI HLMDL Loader (Half-Life 1 MDL format)
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
//  Format reference: Valve's Half-Life 1 MDL specification (version 10)
//
//  The HL MDL format is considerably more complex than MD2/MD3:
//    - Skeletal hierarchy (bones + bone controllers)
//    - Hitboxes
//    - Sequences (animations), each with a set of frames
//    - Body parts containing sub-models (meshes)
//    - Indexed/palettised textures stored inline
//    - Skin families (groups of texture swaps)
//    - Transition table for animation blending
//
//  This loader extracts the body parts and their base pose geometry into
//  the common model_data_c format.  Skeletal weighting and animation
//  sequences are not currently expanded – a future update can add full
//  skeletal support once model_data_c gains a bone/joint structure.
//
//----------------------------------------------------------------------------

#ifndef __EPI_MODEL_HLMDL_H__
#define __EPI_MODEL_HLMDL_H__

#include "model_loader.h"

namespace epi
{

class HLMDLLoader : public model_loader_c
{
public:
	HLMDLLoader() { }
	virtual ~HLMDLLoader() { }

	virtual bool Probe(file_c *f) override;
	virtual model_data_c *Load(file_c *f) override;
	virtual const char *FormatName() const override { return "HLMDL"; }
};

} // namespace epi

#endif /* __EPI_MODEL_HLMDL_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
