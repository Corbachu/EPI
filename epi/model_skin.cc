//------------------------------------------------------------------------
//  EPI AITD Model Skinning – implementation
//------------------------------------------------------------------------
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
//------------------------------------------------------------------------

#include "epi.h"
#include "model_skin.h"

#if defined(EPI_ENABLE_RGL)

#include "image_data.h"

#include <cassert>
#include <cstring>

namespace epi
{

// ---------------------------------------------------------------------------
// model_skin_c – construction
// ---------------------------------------------------------------------------

model_skin_c::model_skin_c()
	: model_(nullptr)
{ }

model_skin_c::~model_skin_c()
{ }

// ---------------------------------------------------------------------------
// Attach
// ---------------------------------------------------------------------------

void model_skin_c::Attach(const model_data_c *mdl)
{
	model_ = mdl;

	if (mdl)
	{
		resolved_ .assign(mdl->NumSkins(), nullptr);
		overrides_.assign(mdl->NumSkins(), nullptr);
	}
	else
	{
		resolved_ .clear();
		overrides_.clear();
	}
}

// ---------------------------------------------------------------------------
// ResolveAll – upload embedded skins; look up named skins
// ---------------------------------------------------------------------------

void model_skin_c::ResolveAll(tex_cache_c &cache, u32_t upload_flags)
{
	if (!model_)
		return;

	for (int i = 0; i < model_->NumSkins(); i++)
	{
		// Already resolved or overridden – skip.
		if (resolved_[i] || overrides_[i])
			continue;

		const model_tex_c *mt = model_->skins[i];
		if (!mt)
			continue;

		if (mt->pixels && mt->width > 0 && mt->height > 0)
		{
			// Embedded pixel data (flat-colour 1×1 RGBA tile, etc.).
			// Build a temporary image_data_c and upload via the cache.
			// tex_cache_c::Upload() is idempotent on name so repeat calls
			// at room-load time are free.
			image_data_c img(mt->width, mt->height, 4);
			std::memcpy(img.pixels, mt->pixels,
			            (size_t)(mt->width * mt->height * 4));

			resolved_[i] = cache.Upload(mt->name, &img, upload_flags);
		}
		else
		{
			// Named-only skin (aitd:tex:N:T).  Try to find it in the cache;
			// external code (PAK extractor, texture loader) is responsible
			// for uploading the real image beforehand.  Until then the slot
			// is NULL and rendering code should fall back gracefully.
			resolved_[i] = cache.Find(mt->name);
		}
	}
}

// ---------------------------------------------------------------------------
// SetOverride
// ---------------------------------------------------------------------------

void model_skin_c::SetOverride(int skin_idx, tex_entry_c *tex)
{
	if (skin_idx >= 0 && skin_idx < (int)overrides_.size())
		overrides_[skin_idx] = tex;
}

// ---------------------------------------------------------------------------
// GetTexture
// ---------------------------------------------------------------------------

tex_entry_c *model_skin_c::GetTexture(int skin_idx) const
{
	if (skin_idx < 0 || skin_idx >= (int)resolved_.size())
		return nullptr;

	if (overrides_[skin_idx])
		return overrides_[skin_idx];

	return resolved_[skin_idx];
}

// ---------------------------------------------------------------------------
// GetTextureForBody
// ---------------------------------------------------------------------------

tex_entry_c *model_skin_c::GetTextureForBody(int body_idx) const
{
	if (!model_)
		return nullptr;

	if (body_idx < 0 || body_idx >= model_->NumBodies())
		return nullptr;

	return GetTexture(model_->bodies[body_idx]->skin_index);
}

// ---------------------------------------------------------------------------
// BindForBody
// ---------------------------------------------------------------------------

void model_skin_c::BindForBody(tex_cache_c &cache, int body_idx, int unit) const
{
	tex_entry_c *tex = GetTextureForBody(body_idx);
	if (tex)
		cache.Bind(tex, unit);
}

}  // namespace epi

#endif  // EPI_ENABLE_RGL

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
