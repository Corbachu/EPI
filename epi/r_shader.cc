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

#include "epi.h"
#include "r_shader.h"

// ---------------------------------------------------------------------------
// Platform GL headers
// ---------------------------------------------------------------------------
#if defined(_arch_dreamcast) || defined(DREAMCAST) || defined(PLATFORM_DREAMCAST)
#  define EPI_PLATFORM_DC 1
#  include <GL/gl.h>
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

namespace epi
{

// ---------------------------------------------------------------------------
// r_shader_c
// ---------------------------------------------------------------------------
r_shader_c::r_shader_c()
	: blend(RBLEND_NONE), color(255, 255, 255), alpha(1.0f),
	  fog_enabled(false), depth_test(true), depth_write(true),
	  two_sided(false), alpha_threshold(0.5f)
{ }

// ---------------------------------------------------------------------------
// Apply blend state
// ---------------------------------------------------------------------------
void r_shader_c::ApplyBlend() const
{
	switch (blend)
	{
		default:
		case RBLEND_NONE:
			glDisable(GL_BLEND);
			glDisable(GL_ALPHA_TEST);
			break;

		case RBLEND_NORMAL:
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glDisable(GL_ALPHA_TEST);
			break;

		case RBLEND_ADD:
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			glDisable(GL_ALPHA_TEST);
			break;

		case RBLEND_MULTIPLY:
			glEnable(GL_BLEND);
			glBlendFunc(GL_DST_COLOR, GL_ZERO);
			glDisable(GL_ALPHA_TEST);
			break;

		case RBLEND_ALPHA_TEST:
			glDisable(GL_BLEND);
			glEnable(GL_ALPHA_TEST);
			glAlphaFunc(GL_GREATER, alpha_threshold);
			break;

		case RBLEND_SHADOW:
			glEnable(GL_BLEND);
			glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
			glDisable(GL_ALPHA_TEST);
			break;
	}
}

// ---------------------------------------------------------------------------
// Apply fog state
// ---------------------------------------------------------------------------
void r_shader_c::ApplyFog() const
{
	if (!fog_enabled || fog.mode == RFOG_NONE)
	{
		glDisable(GL_FOG);
		return;
	}

	glEnable(GL_FOG);

	// Fog color
	GLfloat fc[4] =
	{
		fog.color.r / 255.0f,
		fog.color.g / 255.0f,
		fog.color.b / 255.0f,
		fog.color.a / 255.0f
	};
	glFogfv(GL_FOG_COLOR, fc);

	switch (fog.mode)
	{
		case RFOG_LINEAR:
			glFogi(GL_FOG_MODE, GL_LINEAR);
			glFogf(GL_FOG_START, fog.start);
			glFogf(GL_FOG_END,   fog.end);
			break;

		case RFOG_EXP:
			glFogi(GL_FOG_MODE, GL_EXP);
			glFogf(GL_FOG_DENSITY, fog.density);
			break;

		case RFOG_EXP2:
			glFogi(GL_FOG_MODE, GL_EXP2);
			glFogf(GL_FOG_DENSITY, fog.density);
			break;

		default:
			break;
	}

	glHint(GL_FOG_HINT, GL_NICEST);
}

// ---------------------------------------------------------------------------
// Apply texture units
// ---------------------------------------------------------------------------
void r_shader_c::ApplyTexUnits() const
{
	for (int u = 0; u < MAX_UNITS; u++)
	{
		const r_tex_unit_c &unit = units[u];

// Multi-texture unit select -----------------------------------------------
#if defined(GL_ARB_multitexture) || defined(GL_VERSION_1_3)
		glActiveTexture(GL_TEXTURE0 + (GLenum)u);
		glClientActiveTexture(GL_TEXTURE0 + (GLenum)u);
#else
		if (u > 0) break;  // Only one unit available on very old hardware
#endif

		if (!unit.texture || !unit.texture->IsValid())
		{
			glDisable(GL_TEXTURE_2D);
			continue;
		}

		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, (GLuint)unit.texture->gl_id);

		// Override wrap modes if requested
		if (unit.clamp_s)
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		if (unit.clamp_t)
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		// Texture scroll via texture matrix
		if (unit.scroll_s != 0.0f || unit.scroll_t != 0.0f)
		{
			glMatrixMode(GL_TEXTURE);
			glLoadIdentity();
			glTranslatef(unit.scroll_s, unit.scroll_t, 0.0f);
			glMatrixMode(GL_MODELVIEW);
		}

		// Texture environment mode ----------------------------------------
		switch (unit.env_mode)
		{
			case RTEXENV_REPLACE:
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
				break;

			default:
			case RTEXENV_MODULATE:
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
				break;

			case RTEXENV_DECAL:
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
				break;

			case RTEXENV_ADD:
				// GL_ADD is part of core GL 1.3 (and ARB_texture_env_add).
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
				break;

			case RTEXENV_BLEND_COLOR:
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
				glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR,
				           unit.env_color);
				break;

#if defined(GL_COMBINE)
			case RTEXENV_COMBINE_MOD_ALPHA:
				// GL_COMBINE: color = tex*color, alpha = tex.a * color.a
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
				glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB,   GL_MODULATE);
				glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
				glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_RGB,   GL_TEXTURE);
				glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_RGB,   GL_PREVIOUS);
				glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_TEXTURE);
				glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_ALPHA, GL_PREVIOUS);
				break;

			case RTEXENV_COMBINE_PREV:
				// Pass-through: output = previous stage unchanged
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
				glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB,   GL_REPLACE);
				glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
				glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_RGB,   GL_PREVIOUS);
				glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_PREVIOUS);
				break;
#else
			// Fallback when GL_COMBINE is not defined (very old GL):
			case RTEXENV_COMBINE_MOD_ALPHA:
			case RTEXENV_COMBINE_PREV:
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
				break;
#endif
		}
	}

	// Apply constant color + alpha to GL color (used by MODULATE etc.)
	glColor4f(color.r / 255.0f,
	           color.g / 255.0f,
	           color.b / 255.0f,
	           color.a / 255.0f * alpha);
}

// ---------------------------------------------------------------------------
// Apply – push full shader state to GL
// ---------------------------------------------------------------------------
void r_shader_c::Apply() const
{
	// Depth state
	if (depth_test)
		glEnable(GL_DEPTH_TEST);
	else
		glDisable(GL_DEPTH_TEST);

	glDepthMask(depth_write ? GL_TRUE : GL_FALSE);

	// Back-face culling
	if (two_sided)
		glDisable(GL_CULL_FACE);
	else
		glEnable(GL_CULL_FACE);

	ApplyBlend();
	ApplyFog();
	ApplyTexUnits();
}

// ---------------------------------------------------------------------------
// Reset – restore GL to a neutral default after a render pass
// ---------------------------------------------------------------------------
/*static*/ void r_shader_c::Reset()
{
	// Disable blending / alpha test
	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Fog off
	glDisable(GL_FOG);

	// Depth defaults
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);

	// Culling defaults
	glEnable(GL_CULL_FACE);

	// Reset all texture units to default state
	for (int u = MAX_UNITS - 1; u >= 0; u--)
	{
#if defined(GL_ARB_multitexture) || defined(GL_VERSION_1_3)
		glActiveTexture(GL_TEXTURE0 + (GLenum)u);
		glClientActiveTexture(GL_TEXTURE0 + (GLenum)u);
#else
		if (u > 0) continue;
#endif
		glDisable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);

		// Reset texture matrix
		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();

		// Reset env mode
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}

	glMatrixMode(GL_MODELVIEW);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

// ---------------------------------------------------------------------------
// WithAlpha
// ---------------------------------------------------------------------------
r_shader_c r_shader_c::WithAlpha(float new_alpha) const
{
	r_shader_c copy = *this;
	copy.alpha = new_alpha;
	return copy;
}

// ===========================================================================
// r_shaders factories
// ===========================================================================
namespace r_shaders
{

r_shader_c Solid(const tex_entry_c *tex)
{
	r_shader_c sh;
	sh.units[0].texture  = tex;
	sh.units[0].env_mode = RTEXENV_REPLACE;
	sh.blend = RBLEND_NONE;
	return sh;
}

r_shader_c Masked(const tex_entry_c *tex)
{
	r_shader_c sh;
	sh.units[0].texture  = tex;
	sh.units[0].env_mode = RTEXENV_MODULATE;
	sh.blend             = RBLEND_ALPHA_TEST;
	sh.alpha_threshold   = 0.5f;
	return sh;
}

r_shader_c Translucent(const tex_entry_c *tex, float alpha)
{
	r_shader_c sh;
	sh.units[0].texture  = tex;
	sh.units[0].env_mode = RTEXENV_MODULATE;
	sh.blend             = RBLEND_NORMAL;
	sh.alpha             = alpha;
	sh.depth_write       = false;
	return sh;
}

r_shader_c Additive(const tex_entry_c *tex, float alpha)
{
	r_shader_c sh;
	sh.units[0].texture  = tex;
	sh.units[0].env_mode = RTEXENV_MODULATE;
	sh.blend             = RBLEND_ADD;
	sh.alpha             = alpha;
	sh.depth_write       = false;
	return sh;
}

r_shader_c Fuzzy(const tex_entry_c *tex)
{
	// Classic Doom fuzz: low alpha, additive-like translucency, no depth write.
	r_shader_c sh;
	sh.units[0].texture  = tex;
	sh.units[0].env_mode = RTEXENV_MODULATE;
	sh.blend             = RBLEND_NORMAL;
	sh.alpha             = 0.33f;
	sh.depth_write       = false;
	sh.two_sided         = true;
	return sh;
}

r_shader_c Shadow(const tex_entry_c *tex, float intensity)
{
	r_shader_c sh;
	sh.units[0].texture  = tex;
	sh.units[0].env_mode = RTEXENV_MODULATE;
	sh.blend             = RBLEND_SHADOW;
	sh.alpha             = intensity;
	sh.color             = color_c(0, 0, 0);
	sh.depth_test        = false;
	sh.depth_write       = false;
	sh.two_sided         = true;
	return sh;
}

r_shader_c Colormap(const tex_entry_c *base, const tex_entry_c *cmap)
{
	// Unit 0: base texture (MODULATE with vertex color = sector light)
	// Unit 1: colormap texture (MODULATE to tint final color)
	r_shader_c sh;
	sh.units[0].texture  = base;
	sh.units[0].env_mode = RTEXENV_MODULATE;
	sh.units[1].texture  = cmap;
	sh.units[1].env_mode = RTEXENV_COMBINE_MOD_ALPHA;
	sh.blend             = RBLEND_NONE;
	return sh;
}

r_shader_c EnvMap(const tex_entry_c *base, const tex_entry_c *env,
                  float env_alpha)
{
	r_shader_c sh;
	sh.units[0].texture  = base;
	sh.units[0].env_mode = RTEXENV_MODULATE;
	sh.units[1].texture  = env;
	sh.units[1].env_mode = RTEXENV_BLEND_COLOR;
	sh.units[1].env_color[0] = sh.units[1].env_color[1] =
	sh.units[1].env_color[2] = env_alpha;
	sh.units[1].env_color[3] = 1.0f;
	sh.blend = RBLEND_NONE;
	return sh;
}

r_shader_c Noise(const tex_entry_c *noise_tex, float intensity)
{
	// Additive blend over the existing framebuffer; low alpha so the
	// grain brightens individual pixels subtly (DITD film grain).
	r_shader_c sh;
	sh.units[0].texture  = noise_tex;
	sh.units[0].env_mode = RTEXENV_MODULATE;
	// Tile the small noise texture across the screen
	sh.units[0].clamp_s  = false;
	sh.units[0].clamp_t  = false;
	sh.blend             = RBLEND_ADD;
	sh.alpha             = intensity;
	sh.depth_test        = false;
	sh.depth_write       = false;
	sh.two_sided         = true;
	sh.fog_enabled       = false;
	return sh;
}

r_shader_c Scanline(const tex_entry_c *scanline_tex, float intensity)
{
	// Normal alpha blend: the scanline texture has alternating transparent
	// and semi-opaque dark rows – every other scanline is dimmed.
	r_shader_c sh;
	sh.units[0].texture  = scanline_tex;
	sh.units[0].env_mode = RTEXENV_MODULATE;
	sh.units[0].clamp_s  = false;
	sh.units[0].clamp_t  = false;  // tile vertically
	sh.color             = color_c(0, 0, 0);
	sh.blend             = RBLEND_NORMAL;
	sh.alpha             = intensity;
	sh.depth_test        = false;
	sh.depth_write       = false;
	sh.two_sided         = true;
	sh.fog_enabled       = false;
	return sh;
}

r_shader_c DarkZone(float intensity)
{
	// No texture – render a solid black quad with normal alpha blend.
	// Used to shade AITD room boundary transition areas.
	r_shader_c sh;
	sh.units[0].texture  = nullptr;
	sh.color             = color_c(0, 0, 0);
	sh.blend             = RBLEND_NORMAL;
	sh.alpha             = intensity;
	sh.depth_test        = false;
	sh.depth_write       = false;
	sh.two_sided         = true;
	sh.fog_enabled       = false;
	return sh;
}

r_shader_c PrerenderedBG(const tex_entry_c *bg_tex)
{
	// Full-screen 2D background: REPLACE so vertex color does not tint it;
	// depth test and depth write both off so 3D geometry can overdraw freely.
	r_shader_c sh;
	sh.units[0].texture  = bg_tex;
	sh.units[0].env_mode = RTEXENV_REPLACE;
	sh.units[0].clamp_s  = true;
	sh.units[0].clamp_t  = true;
	sh.blend             = RBLEND_NONE;
	sh.depth_test        = false;
	sh.depth_write       = false;
	sh.two_sided         = true;
	sh.fog_enabled       = false;
	return sh;
}

}  // namespace r_shaders

}  // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
