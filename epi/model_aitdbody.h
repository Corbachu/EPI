//----------------------------------------------------------------------------
//  EPI AITD Body Loader (Alone in the Dark body format)
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
//  Parses the binary body format used by Alone in the Dark (1992),
//  Alone in the Dark 2 / 3, Jack in the Dark, and Time Gate: Knight's
//  Chase (all by Infogrames).
//
//  Format reverse-engineered by yaz0r's FITD project and documented in
//  jmimu/AITD_PakEdit (GPLv2+).  This loader is an independent C++
//  reimplementation for the EPI model abstraction layer.
//
//  Binary layout of a raw body entry (all values little-endian):
//    u16  flags            (INFO_TRI=1, INFO_ANIM=2, INFO_TORTUE=4,
//                           INFO_OPTIMISE=8)
//    s16  zvx1,zvx2        bounding box X
//    s16  zvy1,zvy2        bounding box Y
//    s16  zvz1,zvz2        bounding box Z
//    u16  scratch_size     (bytes to skip – runtime scratch space)
//    u8   scratch[scratch_size]
//    u16  num_vertices
//    s16  vx[i], vy[i], vz[i]   (one per vertex, fixed-point integer coords)
//    --- if INFO_ANIM ---
//    u16  num_groups
//    u16  group_order[num_groups]   (byte-offset / entry-size)
//    group_entry × num_groups       (16 bytes each; +8 if INFO_OPTIMISE)
//    --- end if INFO_ANIM ---
//    u16  num_primitives
//    primitive_entry × num_primitives (variable-length per type)
//
//  Primitive types (stored as u8 type byte, then type-specific bytes):
//    0  Line        : subType(u8), color(u8), pad(u8), 2×vertRef(u16)
//    1  Poly        : count(u8), subType(u8), color(u8), count×vertRef(u16)
//    2  Point       : subType(u8), color(u8), pad(u8), 1×vertRef(u16)
//    3  Sphere      : subType(u8), color(u8), pad(u8), size(u16),
//                     1×vertRef(u16)
//    4  Disk        : (same layout as Sphere)
//    5  Cylinder    : (same layout as Sphere)
//    6  BigPoint    : same as Point
//    7  Zixel       : same as Point
//    8  PolyTex8    : count(u8), subType(u8), color(u8), count×vertRef(u16)
//    9  PolyTex9    : count(u8), subType(u8), color(u8), count×vertRef(u16),
//                     count×(u,v)(2×u8)
//   10  PolyTex10   : same as PolyTex9
//
//  Vertex references are stored as byte offsets into a
//  point-buffer-of-s16-triplets; divide by 6 to get vertex index.
//
//  Skin textures
//  -------------
//  AITD bodies use a 256-entry palette; flat-colour primitives reference
//  a palette index.  Call SetPalette() with the 768-byte RGB palette
//  before Load() so that each unique colour is materialised as a 1×1 RGBA
//  skin in the returned model_data_c::skins list.
//
//  Textured primitives (types 9 / 10) carry per-vertex (u, v) byte
//  coordinates (range 0–255).  The loader stores these in model_vert_c::uv
//  (mapped to [0,1]) and assigns a skin entry whose name encodes the
//  texture index as "aitd:tex:<color_byte>" so the engine can resolve it
//  through its own texture cache.
//
//----------------------------------------------------------------------------

#ifndef __EPI_MODEL_AITDBODY_H__
#define __EPI_MODEL_AITDBODY_H__

#include "model_loader.h"

namespace epi
{

class AITDBodyLoader : public model_loader_c
{
public:
	AITDBodyLoader();
	virtual ~AITDBodyLoader() { }

	// Supply a 256-entry RGB palette (768 bytes, R0 G0 B0 R1 G1 B1 …)
	// before calling Load().  Without this call the loader uses a grey ramp.
	void SetPalette(const u8_t *palette_rgb768);

	// model_loader_c interface
	//
	// Note: the AITD body format has no magic number; Probe() always returns
	// false so that the AUTO detector never mis-identifies a random binary.
	// Always request this loader explicitly with MDL_FORMAT_AITDBODY.
	virtual bool          Probe(file_c *f) override;
	virtual model_data_c *Load (file_c *f) override;
	virtual const char   *FormatName() const override { return "AITDBody"; }

private:
	u8_t palette_[768]; // 256 × RGB – initialised to greyscale ramp in ctor
};

} // namespace epi

#endif /* __EPI_MODEL_AITDBODY_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
