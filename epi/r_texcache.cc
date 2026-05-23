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

#include "epi.h"
#include "r_texcache.h"

// ---------------------------------------------------------------------------
// Platform GL / PVR headers
// ---------------------------------------------------------------------------
#if defined(_arch_dreamcast) || defined(DREAMCAST) || defined(PLATFORM_DREAMCAST)
#  define EPI_PLATFORM_DC 1
#  include <GL/gl.h>        // GLdc – OpenGL 1.x on KallistiOS
#  include <dc/pvr.h>       // PVR memory allocation
#  include <kos/img.h>      // kos_img_t
#elif defined(_WIN32) || defined(_WIN64)
#  include <windows.h>
#  include <GL/gl.h>
#elif defined(__APPLE__)
#  include <OpenGL/gl.h>
#else
#  include <GL/gl.h>
#endif

#include <cmath>
#include <cstring>
#include <cassert>

namespace epi
{

// ---------------------------------------------------------------------------
// Global singleton
// ---------------------------------------------------------------------------
tex_cache_c RGL_TexCache;

// ---------------------------------------------------------------------------
// tex_entry_c
// ---------------------------------------------------------------------------
tex_entry_c::tex_entry_c()
	: name(), width(0), height(0), format(TEXFMT_NONE), flags(0),
	  gl_id(0), pvr_ptr(nullptr), pvr_format(0),
	  size_bytes(0), last_use(0), ref_count(0)
{ }

tex_entry_c::~tex_entry_c()
{
	// GPU resources must have been freed by tex_cache_c before deletion.
	SYS_ASSERT(gl_id == 0);
	SYS_ASSERT(pvr_ptr == nullptr);
}

// ---------------------------------------------------------------------------
// tex_cache_c
// ---------------------------------------------------------------------------
tex_cache_c::tex_cache_c()
	: max_memory_(0), used_memory_(0), frame_counter_(0), initialised_(false)
{ }

tex_cache_c::~tex_cache_c()
{
	if (initialised_)
		Shutdown();
}

void tex_cache_c::Init(u32_t max_memory_bytes)
{
	if (initialised_)
		Shutdown();

	max_memory_  = max_memory_bytes;
	used_memory_ = 0;
	frame_counter_ = 0;
	initialised_ = true;
}

void tex_cache_c::Shutdown()
{
	Clear();
	initialised_ = false;
}

// ---------------------------------------------------------------------------
// Upload
// ---------------------------------------------------------------------------

// Estimate VRAM consumption for a given image and upload flags.
/*static*/ u32_t tex_cache_c::CalcSize(const image_data_c *img, u32_t flags)
{
	int bpp = img->bpp;

	// PVR-native packed formats are always 16-bit (2 bytes per texel).
#ifdef EPI_PLATFORM_DC
	bpp = 2;  // GLdc / PVR textures are always packed 16-bit on Dreamcast
#else
	(void)flags;  // flags may select mipmaps below; bpp stays as image bpp
#endif

	u32_t base = (u32_t)img->width * (u32_t)img->height * (u32_t)bpp;

	if (flags & TEXUPLOAD_MIPMAP)
	{
		// Mip chain ≈ 4/3 × base
		base = base + base / 3;
	}

	return base;
}

// ---------------------------------------------------------------------------
// Internal GL upload helper
// ---------------------------------------------------------------------------
void tex_cache_c::UploadToGL(tex_entry_c *entry, const image_data_c *img) const
{
	GLuint tex_id;
	glGenTextures(1, &tex_id);
	glBindTexture(GL_TEXTURE_2D, tex_id);

	// Filtering
	if (entry->flags & TEXUPLOAD_NEAREST)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
		                (entry->flags & TEXUPLOAD_MIPMAP) ?
		                GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	// Wrapping
	GLint wrap_s = (entry->flags & TEXUPLOAD_CLAMP_S) ?
	               GL_CLAMP_TO_EDGE : GL_REPEAT;
	GLint wrap_t = (entry->flags & TEXUPLOAD_CLAMP_T) ?
	               GL_CLAMP_TO_EDGE : GL_REPEAT;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_s);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_t);

	// Pixel data
	GLenum gl_fmt  = (img->bpp == 4) ? GL_RGBA : GL_RGB;
	GLenum gl_type = GL_UNSIGNED_BYTE;

	glTexImage2D(GL_TEXTURE_2D, 0, gl_fmt,
	             img->width, img->height, 0,
	             gl_fmt, gl_type, img->pixels);

	if (entry->flags & TEXUPLOAD_MIPMAP)
	{
#if defined(GL_GENERATE_MIPMAP)
		// GL 1.4 automatic mipmap generation via TexParameter
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
		// Re-upload with the hint set
		glTexImage2D(GL_TEXTURE_2D, 0, gl_fmt,
		             img->width, img->height, 0,
		             gl_fmt, gl_type, img->pixels);
#endif
	}

	glBindTexture(GL_TEXTURE_2D, 0);
	entry->gl_id = (u32_t)tex_id;

	// Choose format tag
	if (img->bpp == 4)
		entry->format = TEXFMT_RGBA8;
	else if (img->bpp == 3)
		entry->format = TEXFMT_RGB8;
	else
		entry->format = TEXFMT_ALPHA8;
}

// ---------------------------------------------------------------------------
// Internal PVR upload helper (Dreamcast / KallistiOS only)
// ---------------------------------------------------------------------------
void tex_cache_c::UploadToPVR(tex_entry_c *entry,
                               const image_data_c *img) const
{
#ifdef EPI_PLATFORM_DC
	// Choose the best PVR texture format for the image.
	// PVR hardware requires power-of-two dimensions (caller's responsibility).

	bool has_alpha = (img->bpp == 4);
	u32_t pvr_fmt;
	int   packed_bpp = 2;

	if (has_alpha)
	{
		pvr_fmt = PVR_TXRFMT_ARGB4444;
		entry->format = TEXFMT_ARGB4444;
	}
	else
	{
		pvr_fmt = PVR_TXRFMT_RGB565;
		entry->format = TEXFMT_RGB565;
	}

	u32_t byte_count = (u32_t)img->width * (u32_t)img->height * (u32_t)packed_bpp;

	// Allocate PVR texture memory
	pvr_ptr_t mem = pvr_mem_malloc(byte_count);
	if (!mem)
	{
		I_Warning("r_texcache: pvr_mem_malloc(%u) failed for '%s'\n",
		          byte_count, entry->name.c_str());
		return;
	}

	// Pack pixels into the chosen PVR format
	u16_t *packed = new u16_t[img->width * img->height];
	if (has_alpha)
		const_cast<image_data_c *>(img)->PackARGB4444(packed);
	else
		const_cast<image_data_c *>(img)->PackRGB565(packed);

	// Optional: twiddle the texture for better cache coherence
	if (entry->flags & TEXUPLOAD_PVR_TWIDDLED)
	{
		pvr_fmt |= PVR_TXRFMT_TWIDDLED;
		// pvr_txr_load_ex handles twiddling via the KOS helper
		pvr_txr_load_ex(packed, mem,
		                (u32_t)img->width, (u32_t)img->height,
		                PVR_TXRLOAD_16BPP);
	}
	else
	{
		pvr_txr_load(packed, mem, byte_count);
	}

	delete[] packed;

	entry->pvr_ptr    = mem;
	entry->pvr_format = pvr_fmt;

	// Also expose via GL handle through GLdc's texture ID so callers can
	// bind with glBindTexture() as well.  We reuse UploadToGL for that.
	UploadToGL(entry, img);

#else
	// Non-Dreamcast: just use standard GL.
	UploadToGL(entry, img);
#endif
}

tex_entry_c *tex_cache_c::Upload(const std::string &name,
                                  const image_data_c *img,
                                  u32_t flags)
{
	SYS_ASSERT(initialised_);
	SYS_ASSERT(img);
	SYS_ASSERT(img->width > 0 && img->height > 0);

	// Return existing entry if already cached.
	tex_entry_c *existing = Find(name);
	if (existing)
		return existing;

	// Evict if we would exceed the budget.
	u32_t cost = CalcSize(img, flags);
	if (max_memory_ > 0 && used_memory_ + cost > max_memory_)
		Evict(max_memory_ - cost);

	tex_entry_c *entry = new tex_entry_c();
	entry->name      = name;
	entry->width     = img->width;
	entry->height    = img->height;
	entry->flags     = flags;
	entry->size_bytes = cost;
	entry->last_use  = frame_counter_;

#ifdef EPI_PLATFORM_DC
	UploadToPVR(entry, img);
#else
	UploadToGL(entry, img);
#endif

	if (!entry->IsValid())
	{
		I_Warning("r_texcache: upload failed for '%s'\n", name.c_str());
		delete entry;
		return nullptr;
	}

	entries_[name] = entry;
	lru_list_.push_back(entry);
	used_memory_ += cost;

	return entry;
}

// ---------------------------------------------------------------------------
// Find
// ---------------------------------------------------------------------------
tex_entry_c *tex_cache_c::Find(const std::string &name) const
{
	auto it = entries_.find(name);
	return (it != entries_.end()) ? it->second : nullptr;
}

// ---------------------------------------------------------------------------
// Bind / Unbind
// ---------------------------------------------------------------------------
void tex_cache_c::Bind(tex_entry_c *entry, int unit)
{
	if (!entry || !entry->IsValid())
		return;

#if defined(GL_ARB_multitexture) || defined(GL_VERSION_1_3)
	// Multi-texture select; on GLdc the ARB_multitexture extension is available.
	glActiveTexture(GL_TEXTURE0 + (GLenum)unit);
#else
	(void)unit;
#endif

	glBindTexture(GL_TEXTURE_2D, (GLuint)entry->gl_id);

	entry->last_use = frame_counter_;
	TouchLRU(entry);
}

void tex_cache_c::Unbind(int unit) const
{
#if defined(GL_ARB_multitexture) || defined(GL_VERSION_1_3)
	glActiveTexture(GL_TEXTURE0 + (GLenum)unit);
#else
	(void)unit;
#endif
	glBindTexture(GL_TEXTURE_2D, 0);
}

// ---------------------------------------------------------------------------
// Pin / Unpin
// ---------------------------------------------------------------------------
void tex_cache_c::Pin(tex_entry_c *entry)
{
	if (entry) entry->ref_count++;
}

void tex_cache_c::Unpin(tex_entry_c *entry)
{
	if (entry && entry->ref_count > 0) entry->ref_count--;
}

// ---------------------------------------------------------------------------
// Free
// ---------------------------------------------------------------------------
void tex_cache_c::DestroyEntry(tex_entry_c *entry)
{
	if (!entry) return;

	// Free GL texture object
	if (entry->gl_id != 0)
	{
		GLuint id = (GLuint)entry->gl_id;
		glDeleteTextures(1, &id);
		entry->gl_id = 0;
	}

#ifdef EPI_PLATFORM_DC
	// Free PVR texture memory (separate from GL memory on DC)
	if (entry->pvr_ptr)
	{
		pvr_mem_free(entry->pvr_ptr);
		entry->pvr_ptr = nullptr;
	}
#endif

	used_memory_ = (used_memory_ >= entry->size_bytes) ?
	               used_memory_ - entry->size_bytes : 0;
}

void tex_cache_c::Free(const std::string &name)
{
	auto it = entries_.find(name);
	if (it == entries_.end()) return;

	tex_entry_c *entry = it->second;
	lru_list_.remove(entry);
	entries_.erase(it);

	DestroyEntry(entry);
	delete entry;
}

void tex_cache_c::Free(tex_entry_c *entry)
{
	if (entry) Free(entry->name);
}

// ---------------------------------------------------------------------------
// Eviction
// ---------------------------------------------------------------------------
void tex_cache_c::Evict(u32_t target_bytes)
{
	// Walk the LRU list front-to-back (oldest first) and evict
	// unpinned entries until we are at or below target_bytes.
	auto it = lru_list_.begin();
	while (it != lru_list_.end())
	{
		if (max_memory_ == 0 || used_memory_ <= target_bytes)
			break;

		tex_entry_c *e = *it;
		if (e->ref_count > 0)
		{
			// Pinned – skip but keep traversing.
			++it;
			continue;
		}

		it = lru_list_.erase(it);
		entries_.erase(e->name);
		DestroyEntry(e);
		delete e;
	}
}

void tex_cache_c::Clear()
{
	for (auto &kv : entries_)
	{
		DestroyEntry(kv.second);
		delete kv.second;
	}
	entries_.clear();
	lru_list_.clear();
	used_memory_ = 0;
}

// ---------------------------------------------------------------------------
// TouchLRU – move an entry to the back (most-recently-used position)
// ---------------------------------------------------------------------------
void tex_cache_c::TouchLRU(tex_entry_c *entry)
{
	lru_list_.remove(entry);
	lru_list_.push_back(entry);
}

// ===========================================================================
// DITD procedural texture generators
// ===========================================================================

// Simple inline LCG – avoids pulling in math_random.h just for pixel fill.
static inline unsigned int NextLCG(unsigned int state)
{
	return state * 1664525u + 1013904223u;
}

tex_entry_c *GenerateNoiseTex(const std::string &name,
                               int width, int height,
                               unsigned int seed)
{
	// Return existing entry when already uploaded (idempotent).
	tex_entry_c *existing = RGL_TexCache.Find(name);
	if (existing)
		return existing;

	// Build RGBA noise: random grey value, full alpha.
	// The caller controls final opacity via the r_shaders::Noise intensity.
	image_data_c img(width, height, 4);

	unsigned int lcg = seed ^ 0xDEADBEEFu;
	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			lcg = NextLCG(lcg);
			u8_t grey  = (u8_t)(lcg & 0xFF);
			u8_t *p    = img.PixelAt(x, y);
			p[0] = grey;
			p[1] = grey;
			p[2] = grey;
			p[3] = 255;
		}
	}

	// Upload with repeat wrapping (tiled over the screen quad).
	return RGL_TexCache.Upload(name, &img, TEXUPLOAD_NEAREST);
}

tex_entry_c *GenerateScanlineTex(const std::string &name,
                                  int height,
                                  u8_t line_alpha)
{
	// Return existing entry when already uploaded (idempotent).
	tex_entry_c *existing = RGL_TexCache.Find(name);
	if (existing)
		return existing;

	// Build a 1×height RGBA image.
	// Row 0 (even): fully transparent – scene shows through.
	// Row 1 (odd):  black with line_alpha – raster darkening.
	// This 1-pixel wide strip tiles across the screen in S, and tiles
	// vertically in T to create the scanline pattern.
	image_data_c img(1, height, 4);
	img.Clear(0);

	for (int y = 0; y < height; y++)
	{
		u8_t *p = img.PixelAt(0, y);
		if (y & 1)
		{
			// Dark scanline row
			p[0] = 0;  p[1] = 0;  p[2] = 0;  p[3] = line_alpha;
		}
		else
		{
			// Transparent pass-through row
			p[0] = 0;  p[1] = 0;  p[2] = 0;  p[3] = 0;
		}
	}

	// Upload with nearest filtering and no mipmap; clamp S so the
	// 1-pixel width doesn't bleed, repeat T for vertical tiling.
	return RGL_TexCache.Upload(name, &img,
	                            TEXUPLOAD_NEAREST | TEXUPLOAD_CLAMP_S);
}

}  // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
