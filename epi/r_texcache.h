//------------------------------------------------------------------------
//  EPI Texture Cache – OpenGL and PVR/GLdc backends
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
//  Manages a pool of GPU textures identified by string names.  Textures
//  are uploaded from epi::image_data_c and evicted from the pool using a
//  least-recently-used policy when memory pressure is applied.
//
//  On Dreamcast (GLdc / KallistiOS) PVR texture memory is used directly;
//  on all other platforms standard OpenGL texture objects are used.
//
//  Texture handles:
//    • GL path   : u32_t  gl_id  – the GLuint texture name
//    • PVR path  : u32_t  pvr_fmt plus a void* pvr_ptr (pvr_ptr_t)
//
//  Both handles live inside tex_entry_c; callers bind via Bind() or
//  inspect via the public fields.
//
//------------------------------------------------------------------------

#ifndef __EPI_R_TEXCACHE_H__
#define __EPI_R_TEXCACHE_H__

#include <string>
#include <unordered_map>
#include <list>

#include "types.h"
#include "image_data.h"

namespace epi
{

// ---------------------------------------------------------------------------
// Texture pixel formats
// ---------------------------------------------------------------------------
typedef enum
{
	TEXFMT_NONE      = 0,
	TEXFMT_RGB8      = 1,   // 24-bit RGB
	TEXFMT_RGBA8     = 2,   // 32-bit RGBA
	TEXFMT_ALPHA8    = 3,   // 8-bit alpha-only (luminance)
	TEXFMT_PALETTE8  = 4,   // 8-bit paletted
	// PVR native packed formats
	TEXFMT_RGB565    = 5,   // PVR_TXRFMT_RGB565
	TEXFMT_ARGB1555  = 6,   // PVR_TXRFMT_ARGB1555
	TEXFMT_ARGB4444  = 7,   // PVR_TXRFMT_ARGB4444
}
tex_format_e;

// ---------------------------------------------------------------------------
// Upload flags
// ---------------------------------------------------------------------------
typedef enum
{
	TEXUPLOAD_NONE     = 0,
	TEXUPLOAD_MIPMAP   = (1 << 0),  // generate GL mipmaps after upload
	TEXUPLOAD_CLAMP_S  = (1 << 1),  // GL_CLAMP_TO_EDGE on s axis
	TEXUPLOAD_CLAMP_T  = (1 << 2),  // GL_CLAMP_TO_EDGE on t axis
	TEXUPLOAD_NEAREST  = (1 << 3),  // use GL_NEAREST filtering
	// PVR-specific
	TEXUPLOAD_PVR_TWIDDLED = (1 << 8),  // twiddle pixels before DMA upload
}
tex_upload_flags_e;

// ---------------------------------------------------------------------------
// tex_entry_c – one cached texture slot
// ---------------------------------------------------------------------------
struct tex_entry_c
{
	std::string   name;          // unique key (file path, lump name, …)
	int           width;         // pixel dimensions of the source image
	int           height;
	tex_format_e  format;        // pixel format that was uploaded
	u32_t         flags;         // tex_upload_flags_e bits used at upload time

	// GL handle – GLuint, stored as plain u32_t to avoid pulling in GL headers
	u32_t         gl_id;         // 0 = not uploaded / invalid

	// PVR handle – pvr_ptr_t is a void* in KallistiOS; store as pointer-width
	// integer so the header remains portable without KOS includes.
	void         *pvr_ptr;       // NULL when not on Dreamcast
	u32_t         pvr_format;    // PVR_TXRFMT_* packed value (DC only)

	u32_t         size_bytes;    // estimated VRAM / GPU memory consumed
	u32_t         last_use;      // frame counter at last Bind() call

	// Reference count – entries with ref_count > 0 are pinned (not evictable)
	int           ref_count;

	tex_entry_c();
	~tex_entry_c();

	// Returns true when the GL handle is valid
	bool IsValid() const { return gl_id != 0 || pvr_ptr != nullptr; }
};

// ---------------------------------------------------------------------------
// tex_cache_c – the central manager
// ---------------------------------------------------------------------------
class tex_cache_c
{
public:
	tex_cache_c();
	~tex_cache_c();

	// Initialise the cache.  'max_memory_bytes' is the soft VRAM limit
	// (0 = unlimited).  Must be called before any other method.
	void Init(u32_t max_memory_bytes = 0);

	// Release all textures and free internal state.
	void Shutdown();

	// Upload 'img' as a new texture named 'name'.  Returns the entry on
	// success; returns NULL and prints a warning on failure.  If a texture
	// with the same name already exists the existing entry is returned.
	tex_entry_c *Upload(const std::string &name,
	                    const image_data_c *img,
	                    u32_t flags = TEXUPLOAD_NONE);

	// Look up a cached entry.  Returns NULL when not found.
	// Does NOT update last_use.
	tex_entry_c *Find(const std::string &name) const;

	// Bind the texture for rendering on the given multi-texture unit.
	// Updates last_use to the current frame counter.
	void Bind(tex_entry_c *entry, int unit = 0);

	// Unbind all texture units (restores default GL state).
	void Unbind(int unit = 0) const;

	// Pin / unpin an entry so it is never evicted while pinned.
	void Pin(tex_entry_c *entry);
	void Unpin(tex_entry_c *entry);

	// Free a specific texture by name or by pointer.
	void Free(const std::string &name);
	void Free(tex_entry_c *entry);

	// Evict least-recently-used textures until used_memory <= target_bytes
	// (or the evictable pool is empty).
	void Evict(u32_t target_bytes = 0);

	// Free every texture in the cache.
	void Clear();

	// Advance the internal frame counter (call once per rendered frame).
	void Tick() { frame_counter_++; }

	// Getters
	u32_t GetUsedMemory()  const { return used_memory_; }
	u32_t GetMaxMemory()   const { return max_memory_; }
	int   GetCount()       const { return (int)entries_.size(); }
	u32_t GetFrameCounter() const { return frame_counter_; }

private:
	// Map: name → entry pointer
	std::unordered_map<std::string, tex_entry_c *> entries_;

	// LRU list – front = oldest (next candidate for eviction)
	// Holds raw pointers to entries_ values; kept in sync.
	std::list<tex_entry_c *> lru_list_;

	u32_t  max_memory_;
	u32_t  used_memory_;
	u32_t  frame_counter_;
	bool   initialised_;

	// Internal helpers
	void   UploadToGL(tex_entry_c *entry, const image_data_c *img) const;
	void   UploadToPVR(tex_entry_c *entry, const image_data_c *img) const;
	void   DestroyEntry(tex_entry_c *entry);
	void   TouchLRU(tex_entry_c *entry);

	// Estimate VRAM cost for an image
	static u32_t CalcSize(const image_data_c *img, u32_t flags);
};

// ---------------------------------------------------------------------------
// Global singleton – engine code may use this directly.
// Initialise with RGL_TexCache.Init() before use.
// ---------------------------------------------------------------------------
extern tex_cache_c RGL_TexCache;

// ---------------------------------------------------------------------------
// DITD procedural texture generators
//
// Both functions generate a CPU-side image_data_c, upload it via
// RGL_TexCache.Upload(), and return the resulting tex_entry_c.
// If a texture with the same 'name' already exists the cached entry
// is returned unchanged (idempotent – safe to call every level load).
//
// These are only meaningful when EPI_ENABLE_RGL is active.
// ---------------------------------------------------------------------------

// Generate an RGBA noise texture of the given power-of-two dimensions.
// Pixels have random grey intensity with full alpha; the caller controls
// the final opacity at render time via r_shaders::Noise() intensity.
// 'seed' seeds the LCG so deterministic frames can be generated.
//
// Typical use: once at startup (or room load) to create the grain atlas.
//   tex_entry_c *noise = epi::GenerateNoiseTex("ditd/noise", 32, 32);
//   r_effect_c grain(RFXTYPE_NOISE, noise, nullptr, 0.12f);
tex_entry_c *GenerateNoiseTex(const std::string &name,
                               int width = 32, int height = 32,
                               unsigned int seed = 0);

// Generate a 1-pixel-wide × height-pixel-tall RGBA scanline pattern.
// Rows alternate between fully transparent (pass-through) and semi-opaque
// black (darkened) to simulate a CRT raster.
// 'line_alpha' controls the darkness of the opaque rows [0–255].
//
// Typical use:
//   tex_entry_c *sl = epi::GenerateScanlineTex("ditd/scanline");
//   r_effect_c crt(RFXTYPE_SCANLINE, sl, nullptr, 0.35f);
tex_entry_c *GenerateScanlineTex(const std::string &name,
                                  int height = 2,
                                  u8_t line_alpha = 160);

}  // namespace epi

#endif  /* __EPI_R_TEXCACHE_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
