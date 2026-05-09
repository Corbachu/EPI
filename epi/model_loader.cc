//----------------------------------------------------------------------------
//  EPI 3D Model Loader – factory implementation
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
#include "model_loader.h"

// Forward declarations from the concrete loaders
#include "model_md2.h"
#include "model_md3.h"
#include "model_hlmdl.h"
#include "model_md5.h"
#include "model_aitdbody.h"

namespace epi
{

model_loader_c *MDL_GetMD2Loader()       { static MD2Loader       l; return &l; }
model_loader_c *MDL_GetMD3Loader()       { static MD3Loader       l; return &l; }
model_loader_c *MDL_GetHLMDLLoader()     { static HLMDLLoader     l; return &l; }
model_loader_c *MDL_GetMD5Loader()       { static MD5Loader       l; return &l; }
model_loader_c *MDL_GetAITDBodyLoader()  { static AITDBodyLoader  l; return &l; }


model_data_c *MDL_Load(file_c *f, model_format_e fmt)
{
	SYS_ASSERT(f);

	if (fmt == MDL_FORMAT_AUTO)
	{
		// Probe each loader in order of likelihood
		model_loader_c *candidates[] =
		{
			MDL_GetMD2Loader(),
			MDL_GetMD3Loader(),
			MDL_GetHLMDLLoader(),
			MDL_GetMD5Loader(),
			NULL
		};

		for (int i = 0; candidates[i]; i++)
		{
			f->Seek(0, file_c::SEEKPOINT_START);
			if (candidates[i]->Probe(f))
			{
				f->Seek(0, file_c::SEEKPOINT_START);
				return candidates[i]->Load(f);
			}
		}

		I_Warning("MDL_Load: unrecognised model format\n");
		return NULL;
	}

	model_loader_c *loader = NULL;

	switch (fmt)
	{
		case MDL_FORMAT_MD2:      loader = MDL_GetMD2Loader();      break;
		case MDL_FORMAT_MD3:      loader = MDL_GetMD3Loader();      break;
		case MDL_FORMAT_HLMDL:    loader = MDL_GetHLMDLLoader();    break;
		case MDL_FORMAT_MD5:      loader = MDL_GetMD5Loader();      break;
		case MDL_FORMAT_AITDBODY: loader = MDL_GetAITDBodyLoader(); break;
		default:
			I_Warning("MDL_Load: unknown format id %d\n", (int)fmt);
			return NULL;
	}

	f->Seek(0, file_c::SEEKPOINT_START);
	return loader->Load(f);
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
