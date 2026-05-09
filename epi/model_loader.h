//----------------------------------------------------------------------------
//  EPI 3D Model Loader – abstract interface
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
//  Usage
//  -----
//    epi::file_c *f = ...;
//    epi::model_data_c *mdl = epi::MDL_Load(f, epi::MDL_FORMAT_AUTO);
//    if (mdl) { ... }
//
//  The factory function `MDL_Load` sniffs the file magic and picks the
//  right concrete loader automatically (MDL_FORMAT_AUTO), or you can
//  force a specific format.
//
//----------------------------------------------------------------------------

#ifndef __EPI_MODEL_LOADER_H__
#define __EPI_MODEL_LOADER_H__

#include "model_data.h"
#include "file.h"

namespace epi
{

//------------------------------------------------------------------------
// Format identifiers
//------------------------------------------------------------------------
enum model_format_e
{
	MDL_FORMAT_AUTO  = 0,  // detect from file magic / header
	MDL_FORMAT_MD2   = 1,  // Quake 2 (IDP2, v8)
	MDL_FORMAT_MD3   = 2,  // Quake 3 (IDP3, v15)
	MDL_FORMAT_HLMDL = 3,  // Half-Life 1 MDL (IDST, v10)
	MDL_FORMAT_MD5   = 4,  // Doom 3 MD5 (text, MD5Version 10)
};

//------------------------------------------------------------------------
// Abstract base loader
//------------------------------------------------------------------------
//
// Each format implements this interface.  Callers use MDL_Load() and
// never need to instantiate a concrete loader directly.
//
class model_loader_c
{
public:
	virtual ~model_loader_c() { }

	// Try to load a model from the given file.
	// Returns a newly-allocated model_data_c on success, NULL on failure.
	// The caller takes ownership of the returned object.
	virtual model_data_c *Load(file_c *f) = 0;

	// Quick probe: return true if this loader recognises the file header.
	// The file position must be at offset 0 on entry; it is left undefined
	// afterwards (the Load() call always seeks to 0 itself).
	virtual bool Probe(file_c *f) = 0;

	// Human-readable format name, e.g. "MD2", "MD3".
	virtual const char *FormatName() const = 0;
};

//------------------------------------------------------------------------
// Factory function
//------------------------------------------------------------------------
//
// Detect (or use the given hint) and load a 3D model from 'f'.
// Returns NULL and logs a warning on failure.
//
model_data_c *MDL_Load(file_c *f, model_format_e fmt = MDL_FORMAT_AUTO);

//------------------------------------------------------------------------
// Individual loader accessors (for callers that need format-specific
// loaders directly, e.g. for format-specific option structs)
//------------------------------------------------------------------------

model_loader_c *MDL_GetMD2Loader();
model_loader_c *MDL_GetMD3Loader();
model_loader_c *MDL_GetHLMDLLoader();
model_loader_c *MDL_GetMD5Loader();

} // namespace epi

#endif /* __EPI_MODEL_LOADER_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
