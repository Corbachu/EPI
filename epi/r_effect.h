//------------------------------------------------------------------------
//  EPI Renderer – High-level Effect System
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
//  r_effect_c represents one named rendering "effect" composed of one or
//  more r_shader_c passes.  Effects bridge the gap between high-level game
//  code ("render this thing with the FUZZ effect") and the low-level fixed-
//  function GL state managed by r_shader_c.
//
//  Effects can be animated (Update() advances an internal time counter that
//  drives scroll offsets, alpha pulses, etc.) and queried for the number of
//  rendering passes they require.
//
//  Usage pattern:
//
//      r_effect_c fx(RFXTYPE_FUZZ, wall_tex);
//      // … each frame:
//      fx.Update(delta_time);
//      for (int p = 0; p < fx.NumPasses(); p++)
//      {
//          fx.BeginPass(p);
//          // … submit geometry …
//          fx.EndPass(p);
//      }
//
//------------------------------------------------------------------------

#ifndef __EPI_R_EFFECT_H__
#define __EPI_R_EFFECT_H__

#include <string>

#include "types.h"
#include "math_color.h"
#include "r_texcache.h"
#include "r_shader.h"

namespace epi
{

// ---------------------------------------------------------------------------
// Effect type enumeration
// ---------------------------------------------------------------------------
typedef enum
{
	RFXTYPE_NONE      = 0,

	// Single-pass / basic effects
	RFXTYPE_SOLID     = 1,  // opaque solid surface
	RFXTYPE_MASKED    = 2,  // alpha-tested masked surface
	RFXTYPE_TRANS     = 3,  // simple alpha-blended translucency

	// Classic EDGE-style effects
	RFXTYPE_FUZZ      = 4,  // Doom-style invisible/fuzz stippling
	RFXTYPE_SHADOW    = 5,  // blob shadow projected onto the floor
	RFXTYPE_COLORMAP  = 6,  // sector colormap tinting (2-unit pass)

	// Additive / glow
	RFXTYPE_ADDITIVE  = 7,  // particle / muzzle-flash additive blend
	RFXTYPE_GLOW      = 8,  // pulsing emissive glow (additive + time)

	// Environment / reflection
	RFXTYPE_ENVMAP    = 9,  // static sphere-map reflection

	// Water / animated surface
	RFXTYPE_WATER     = 10, // scrolling translucent water plane

	// ----------------------------------------------------------------
	// DITD (Alone in the Dark engine) specific effects
	// ----------------------------------------------------------------

	// Film-grain noise overlay: a small procedural noise texture is
	// rendered as a screen-covering quad (additive blend) with a
	// per-frame pseudo-random UV shift for animated static.
	// Use GenerateNoiseTex() to create the noise texture, then call
	// camera_c::PushScreenSpace() before submitting the screen quad.
	RFXTYPE_NOISE      = 11,

	// CRT-style horizontal scanline darkening overlay.  A 1×2 scanline
	// pattern texture is tiled across the screen with MULTIPLY-like
	// blending so every other pixel row is dimmed.  Invoke with
	// GenerateScanlineTex() and a full-screen quad.
	RFXTYPE_SCANLINE   = 12,

	// AITD room-transition dark zone: a solid semi-opaque black quad
	// is rendered without a texture to shade boundary areas between
	// pre-rendered rooms.  No texture is needed; supply nullptr.
	RFXTYPE_DARKZONE   = 13,

	// AITD pre-rendered 2D background layer: the supplied texture is
	// drawn as a full-screen REPLACE quad before any 3D geometry,
	// with depth test and depth write both disabled so 3D content
	// composites on top naturally.
	RFXTYPE_PRERENDERED_BG = 14,
}
r_effect_type_e;

// ---------------------------------------------------------------------------
// r_effect_c
// ---------------------------------------------------------------------------
class r_effect_c
{
public:
	// Construct a default (NONE) effect.
	r_effect_c();

	// Construct an effect of the given type referencing a primary texture.
	// For multi-texture effects supply 'tex2' as the secondary texture.
	r_effect_c(r_effect_type_e type,
	           const tex_entry_c *tex,
	           const tex_entry_c *tex2  = nullptr,
	           float              intensity = 1.0f);

	// Copy / assign
	r_effect_c(const r_effect_c &rhs);
	r_effect_c &operator=(const r_effect_c &rhs);

	~r_effect_c();

	// ---- type / configuration ----

	r_effect_type_e Type()      const { return type_; }
	float           Intensity() const { return intensity_; }
	float           Time()      const { return time_; }

	void SetType(r_effect_type_e t) { type_ = t; RebuildShaders(); }
	void SetIntensity(float v)      { intensity_ = v; RebuildShaders(); }

	// For fog effects – configure the fog parameters on all passes.
	void SetFog(r_fog_e mode, const color_c &col,
	            float density_or_end, float start = 0.0f);

	// ---- animation ----

	// Advance internal time by 'dt' seconds.  Updates scroll offsets and
	// alpha pulses.  Call once per rendered frame before BeginPass().
	void Update(float dt);

	// ---- rendering ----

	// Number of GL render passes required (usually 1, sometimes 2).
	int NumPasses() const;

	// Push GL state for pass 'p' (0-based).  Binds textures, sets blend, etc.
	void BeginPass(int p) const;

	// Pop GL state after rendering pass 'p'.
	void EndPass(int p) const;

	// ---- direct shader access ----

	// Returns the shader for pass 'p'.  Index must be < NumPasses().
	const r_shader_c &GetShader(int p) const;
	r_shader_c       &GetShader(int p);

private:
	r_effect_type_e type_;
	float           intensity_;
	float           time_;          // accumulated seconds since creation

	const tex_entry_c *tex_;        // primary texture
	const tex_entry_c *tex2_;       // secondary texture (or NULL)

	// Effects have at most 2 passes.
	r_shader_c shaders_[2];
	int        num_passes_;

	// Scroll state (animated effects)
	float scroll_s_;
	float scroll_t_;

	// Rebuild internal shaders_ from current type_ / intensity_ / tex_ state.
	void RebuildShaders();
};

}  // namespace epi

#endif  /* __EPI_R_EFFECT_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
