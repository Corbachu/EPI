//------------------------------------------------------------------------
//  EPI Renderer – Pre-GL2 Shader / Fixed-Function State System
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
//  A "shader" in this system is a bundle of OpenGL 1.x / GLdc fixed-
//  function state that describes how a surface should be rendered.  No
//  GLSL programs are required; all effects are achieved via:
//
//    • GL_COMBINE texture combiners (up to 2 texture units)
//    • Alpha blending functions
//    • Fog parameters
//    • Per-vertex / constant color
//    • Depth/stencil write masks
//
//  This mirrors the design of the classic EDGE r_shader subsystem but is
//  self-contained inside EPI so it can be reused across engine targets
//  (desktop OpenGL 1.1-1.4, Dreamcast GLdc, PSP/Vita OpenGL ES).
//
//  Usage:
//      r_shader_c sh = r_shaders::Solid(texEntry);
//      sh.fog.mode = RFOG_LINEAR; sh.fog.start = 100; sh.fog.end = 600;
//      sh.Apply();
//      // … draw geometry …
//      r_shader_c::Reset();
//
//------------------------------------------------------------------------

#ifndef __EPI_R_SHADER_H__
#define __EPI_R_SHADER_H__

#include <string>

#include "types.h"
#include "math_color.h"
#include "r_texcache.h"

namespace epi
{

// ---------------------------------------------------------------------------
// Blend modes
// ---------------------------------------------------------------------------
typedef enum
{
	RBLEND_NONE        = 0,  // blending disabled (solid geometry)
	RBLEND_NORMAL      = 1,  // SRC_ALPHA, ONE_MINUS_SRC_ALPHA  (standard)
	RBLEND_ADD         = 2,  // SRC_ALPHA, ONE                  (additive)
	RBLEND_MULTIPLY    = 3,  // DST_COLOR, ZERO                 (multiply)
	RBLEND_ALPHA_TEST  = 4,  // blending off but alpha-test on  (masked)
	RBLEND_SHADOW      = 5,  // ZERO, ONE_MINUS_SRC_ALPHA       (blob shadow)
}
r_blend_e;

// ---------------------------------------------------------------------------
// Texture-environment (combiner) modes – GL_COMBINE or GL_TEXTURE_ENV_MODE
// ---------------------------------------------------------------------------
typedef enum
{
	RTEXENV_REPLACE     = 0,  // GL_REPLACE   – texture overrides vertex color
	RTEXENV_MODULATE    = 1,  // GL_MODULATE  – texture × vertex color (default)
	RTEXENV_DECAL       = 2,  // GL_DECAL     – alpha-based texture over color
	RTEXENV_ADD         = 3,  // GL_ADD       – texture + vertex color (GL 1.3)
	RTEXENV_BLEND_COLOR = 4,  // GL_BLEND     – mix with a constant GL_TEXTURE_ENV_COLOR
	// Advanced combiners (GL 1.3 / GL_ARB_texture_env_combine)
	RTEXENV_COMBINE_MOD_ALPHA = 5,  // color = tex*color, alpha = tex.a * color.a
	RTEXENV_COMBINE_PREV      = 6,  // pass through previous stage (noop second unit)
}
r_texenv_e;

// ---------------------------------------------------------------------------
// Fog mode
// ---------------------------------------------------------------------------
typedef enum
{
	RFOG_NONE     = 0,
	RFOG_LINEAR   = 1,  // linear falloff between 'start' and 'end' distances
	RFOG_EXP      = 2,  // exponential fog: density
	RFOG_EXP2     = 3,  // denser exponential fog: density²
}
r_fog_e;

// ---------------------------------------------------------------------------
// r_fog_params_c – all fog settings in one place
// ---------------------------------------------------------------------------
struct r_fog_params_c
{
	r_fog_e  mode;
	color_c  color;
	float    density;  // for EXP / EXP2
	float    start;    // for LINEAR
	float    end;      // for LINEAR

	r_fog_params_c()
		: mode(RFOG_NONE), color(128, 128, 128), density(0.01f),
		  start(0.0f), end(1000.0f)
	{ }
};

// ---------------------------------------------------------------------------
// r_tex_unit_c – state for one GL texture unit
// ---------------------------------------------------------------------------
struct r_tex_unit_c
{
	const tex_entry_c *texture;   // NULL = texture unit disabled
	r_texenv_e         env_mode;  // how texture is combined with vertex color
	float              env_color[4];  // constant for RTEXENV_BLEND_COLOR
	float              scroll_s;  // texture matrix S translation (animated tex)
	float              scroll_t;  // texture matrix T translation
	bool               clamp_s;   // override GL_TEXTURE_WRAP_S on bind
	bool               clamp_t;   // override GL_TEXTURE_WRAP_T on bind

	r_tex_unit_c()
		: texture(nullptr), env_mode(RTEXENV_MODULATE),
		  scroll_s(0.0f), scroll_t(0.0f),
		  clamp_s(false), clamp_t(false)
	{
		env_color[0] = env_color[1] = env_color[2] = env_color[3] = 1.0f;
	}
};

// ---------------------------------------------------------------------------
// r_shader_c – a complete fixed-function rendering configuration
// ---------------------------------------------------------------------------
class r_shader_c
{
public:
	static const int MAX_UNITS = 2;  // GL 1.x typically provides ≥2 units

	r_tex_unit_c   units[MAX_UNITS];  // texture unit configurations
	r_blend_e      blend;             // blend / alpha-test mode
	r_fog_params_c fog;               // fog settings (active when fog_enabled)
	color_c        color;             // constant color modulated onto geometry
	float          alpha;             // master alpha override [0,1]
	bool           fog_enabled;       // enable GL_FOG when applying
	bool           depth_test;        // GL_DEPTH_TEST enable
	bool           depth_write;       // glDepthMask
	bool           two_sided;         // disable GL_CULL_FACE
	float          alpha_threshold;   // alpha-test cutoff (for RBLEND_ALPHA_TEST)

	r_shader_c();

	// Apply this shader's state to the current GL context.
	// Saves no previous state – call Reset() afterward to restore defaults.
	void Apply() const;

	// Restore OpenGL fixed-function state to a clean default.
	// Call after rendering a batch that used Apply().
	static void Reset();

	// Convenience: return a copy with alpha adjusted.
	r_shader_c WithAlpha(float new_alpha) const;

private:
	void ApplyBlend()      const;
	void ApplyFog()        const;
	void ApplyTexUnits()   const;
};

// ---------------------------------------------------------------------------
// r_shaders – factory functions for common pre-built shader configurations
// ---------------------------------------------------------------------------
namespace r_shaders
{

// Solid opaque surface textured with 'tex'.
r_shader_c Solid(const tex_entry_c *tex);

// Alpha-tested (masked) surface – pixels with alpha < 128 are discarded.
r_shader_c Masked(const tex_entry_c *tex);

// Alpha-blended translucent surface.  'alpha' is the master transparency.
r_shader_c Translucent(const tex_entry_c *tex, float alpha = 0.5f);

// Additive blend – good for particles, glows, muzzle flashes.
r_shader_c Additive(const tex_entry_c *tex, float alpha = 1.0f);

// Doom-style fuzzy / invisible effect (stippled semi-transparency).
r_shader_c Fuzzy(const tex_entry_c *tex);

// Blob-shadow: the shadow texture darkens the surface beneath it.
r_shader_c Shadow(const tex_entry_c *tex, float intensity = 0.5f);

// Two-texture colormap pass: base texture in unit 0, colormap in unit 1.
// Used for EDGE-style sector colormap effects.
r_shader_c Colormap(const tex_entry_c *base, const tex_entry_c *cmap);

// Environment / sphere-map pass.  unit 0 = base texture,
// unit 1 = environment texture (MODULATE).
r_shader_c EnvMap(const tex_entry_c *base, const tex_entry_c *env,
                  float env_alpha = 0.3f);

// ---- DITD (Alone in the Dark engine) specific factories ----

// Film-grain / static noise overlay.  The noise texture (typically
// generated by GenerateNoiseTex()) is rendered additively at low alpha
// so it brightens random pixels, simulating analog film grain.
// Apply on a full-screen quad after the main 3D scene.
r_shader_c Noise(const tex_entry_c *noise_tex, float intensity = 0.15f);

// CRT scanline darkening overlay.  A 1×2 pattern texture (generated by
// GenerateScanlineTex()) is tiled over the screen; every other row is
// dimmed to simulate a CRT raster.  Rendered with normal alpha blend
// on a full-screen quad after the scene.
r_shader_c Scanline(const tex_entry_c *scanline_tex, float intensity = 0.4f);

// AITD room-transition dark zone.  Renders a solid semi-opaque black
// quad without any texture to shade boundary regions between pre-
// rendered background rooms.  Pass nullptr for tex.
r_shader_c DarkZone(float intensity = 0.6f);

// AITD pre-rendered 2D background layer.  The background texture is
// drawn with REPLACE (no vertex-color influence), no depth test, and no
// depth write – so 3D content rendered afterward composites on top.
// Use camera_c::PushScreenSpace() before submitting the screen quad.
r_shader_c PrerenderedBG(const tex_entry_c *bg_tex);

}  // namespace r_shaders

}  // namespace epi

#endif  /* __EPI_R_SHADER_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
