//------------------------------------------------------------------------
//  EPI AITD Model Skinning – resolves model skins into GPU textures
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
//
//  model_skin_c bridges the model layer (model_data_c / model_tex_c) and
//  the EPI render layer (tex_cache_c, r_shader_c, r_effect_c).
//
//  AITD body models carry two kinds of skin reference in model_data_c::skins:
//
//    1. Flat-colour skins   ("aitd:color:<N>")
//       The loader embeds a 1×1 RGBA texel derived from the 256-entry palette.
//       ResolveAll() uploads these to the texture cache (idempotent by name).
//
//    2. Named texture skins  ("aitd:tex:<color>:<subtype>")
//       The loader stores only the texture key; no pixel data is embedded.
//       ResolveAll() calls cache.Find() – if the image was already loaded
//       externally (e.g. via PAK extraction + cache.Upload()) it is found
//       immediately.  Otherwise the slot remains NULL until SetOverride()
//       supplies a real tex_entry_c.
//
//  Typical usage
//  -------------
//
//      // Load body + palette
//      epi::AITDBodyLoader ldr;
//      ldr.SetPalette(palette_rgb768);
//      epi::model_data_c *mdl = ldr.Load(body_file);
//
//      // Build the skin resolver (once per model instance)
//      epi::model_skin_c skin;
//      skin.Attach(mdl);
//      skin.ResolveAll(epi::RGL_TexCache);
//
//      // Optionally override a named-texture slot with a real image
//      epi::tex_entry_c *real_tex = epi::RGL_TexCache.Upload(
//              "aitd:tex:5:2", img_ptr);
//      skin.SetOverride(some_slot, real_tex);
//
//      // Render each body part
//      for (int bi = 0; bi < mdl->NumBodies(); bi++)
//      {
//          epi::r_effect_c fx(epi::RFXTYPE_SOLID,
//                             skin.GetTextureForBody(bi));
//          fx.BeginPass(0);
//          // ... submit geometry for body bi, frame fi ...
//          fx.EndPass(0);
//      }
//
//  Note: model_skin_c requires the EPI render layer (EPI_ENABLE_RGL).
//
//------------------------------------------------------------------------

#ifndef __EPI_MODEL_SKIN_H__
#define __EPI_MODEL_SKIN_H__

// model_skin_c depends on the EPI render layer which is gated behind
// EPI_ENABLE_RGL.  When the render layer is not compiled in, this header
// provides nothing.
#if defined(EPI_ENABLE_RGL)

#include <vector>

#include "model_data.h"
#include "r_texcache.h"

namespace epi
{

// ---------------------------------------------------------------------------
// model_skin_c
// ---------------------------------------------------------------------------
class model_skin_c
{
public:
	model_skin_c();
	~model_skin_c();

	// Attach to a loaded model.  The model_data_c must remain valid for the
	// entire lifetime of this object.  Clears all previously resolved /
	// overridden skin state.
	void Attach(const model_data_c *mdl);

	// Upload or locate all skins in 'cache':
	//   • Skins with embedded pixel data  → cache.Upload()  (idempotent)
	//   • Skins with name only            → cache.Find()    (NULL if absent)
	//
	// 'upload_flags' is passed to tex_cache_c::Upload() for embedded skins.
	// TEXUPLOAD_NEAREST is the right default for 1×1 flat-colour tiles.
	//
	// Safe to call multiple times – already-resolved slots are left unchanged.
	void ResolveAll(tex_cache_c &cache,
	                u32_t upload_flags = TEXUPLOAD_NEAREST);

	// Override skin slot 'skin_idx' with an externally provided texture.
	// Pass NULL to clear an override and revert to the resolved texture.
	void SetOverride(int skin_idx, tex_entry_c *tex);

	// Return the resolved (or overridden) tex_entry_c for 'skin_idx'.
	// Returns NULL when the slot has not been resolved yet.
	tex_entry_c *GetTexture(int skin_idx) const;

	// Convenience: return the texture assigned to body part 'body_idx'.
	// Equivalent to GetTexture(model_->bodies[body_idx]->skin_index).
	// Returns NULL for out-of-range indices or unresolved skins.
	tex_entry_c *GetTextureForBody(int body_idx) const;

	// Bind the texture for body part 'body_idx' to GL texture unit 'unit'
	// via 'cache'.  No-op when the texture is not yet resolved.
	void BindForBody(tex_cache_c &cache, int body_idx, int unit = 0) const;

	// Number of skin slots (mirrors model_->NumSkins()).
	int NumSkins() const { return (int)resolved_.size(); }

	// True when Attach() has been called with a non-NULL model.
	bool IsAttached() const { return model_ != nullptr; }

private:
	const model_data_c        *model_;
	std::vector<tex_entry_c *> resolved_;   // one per model_->skins[] slot
	std::vector<tex_entry_c *> overrides_;  // one per slot; NULL = use resolved_

	// non-copyable
	model_skin_c(const model_skin_c &) = delete;
	model_skin_c &operator=(const model_skin_c &) = delete;
};

}  // namespace epi

#endif  // EPI_ENABLE_RGL

#endif  // __EPI_MODEL_SKIN_H__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
