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

#include "epi.h"
#include "r_effect.h"

#include <cmath>

namespace epi
{

// Scroll speeds (texture coordinate units per second) for animated effects.
static const float kWaterScrollS = 0.04f;
static const float kWaterScrollT = 0.02f;
static const float kFuzzScrollT  = 0.05f;
// Glow pulse: full cycle period in seconds
static const float kGlowPeriod   = 2.5f;
// Noise: reference frame-rate used to derive per-frame UV hash
static const float kNoiseFrameRate = 30.0f;

// ---------------------------------------------------------------------------
// Constructors / destructor
// ---------------------------------------------------------------------------
r_effect_c::r_effect_c()
	: type_(RFXTYPE_NONE), intensity_(1.0f), time_(0.0f),
	  tex_(nullptr), tex2_(nullptr), num_passes_(1),
	  scroll_s_(0.0f), scroll_t_(0.0f)
{
	RebuildShaders();
}

r_effect_c::r_effect_c(r_effect_type_e type,
                        const tex_entry_c *tex,
                        const tex_entry_c *tex2,
                        float              intensity)
	: type_(type), intensity_(intensity), time_(0.0f),
	  tex_(tex), tex2_(tex2), num_passes_(1),
	  scroll_s_(0.0f), scroll_t_(0.0f)
{
	RebuildShaders();
}

r_effect_c::r_effect_c(const r_effect_c &rhs)
	: type_(rhs.type_), intensity_(rhs.intensity_), time_(rhs.time_),
	  tex_(rhs.tex_), tex2_(rhs.tex2_), num_passes_(rhs.num_passes_),
	  scroll_s_(rhs.scroll_s_), scroll_t_(rhs.scroll_t_)
{
	shaders_[0] = rhs.shaders_[0];
	shaders_[1] = rhs.shaders_[1];
}

r_effect_c &r_effect_c::operator=(const r_effect_c &rhs)
{
	if (this == &rhs) return *this;
	type_       = rhs.type_;
	intensity_  = rhs.intensity_;
	time_       = rhs.time_;
	tex_        = rhs.tex_;
	tex2_       = rhs.tex2_;
	num_passes_ = rhs.num_passes_;
	scroll_s_   = rhs.scroll_s_;
	scroll_t_   = rhs.scroll_t_;
	shaders_[0] = rhs.shaders_[0];
	shaders_[1] = rhs.shaders_[1];
	return *this;
}

r_effect_c::~r_effect_c()
{ }

// ---------------------------------------------------------------------------
// SetFog – configure fog on all shader passes
// ---------------------------------------------------------------------------
void r_effect_c::SetFog(r_fog_e mode, const color_c &col,
                         float density_or_end, float start)
{
	for (int p = 0; p < num_passes_; p++)
	{
		shaders_[p].fog_enabled      = (mode != RFOG_NONE);
		shaders_[p].fog.mode         = mode;
		shaders_[p].fog.color        = col;
		shaders_[p].fog.start        = start;
		shaders_[p].fog.end          = density_or_end;  // LINEAR uses .end
		shaders_[p].fog.density      = density_or_end;  // EXP uses .density
	}
}

// ---------------------------------------------------------------------------
// Update – advance animation state
// ---------------------------------------------------------------------------
void r_effect_c::Update(float dt)
{
	time_ += dt;

	switch (type_)
	{
		case RFXTYPE_WATER:
			scroll_s_ += kWaterScrollS * dt;
			scroll_t_ += kWaterScrollT * dt;
			// Keep in [0,1) to avoid float precision loss over long play sessions
			if (scroll_s_ >= 1.0f) scroll_s_ -= 1.0f;
			if (scroll_t_ >= 1.0f) scroll_t_ -= 1.0f;
			shaders_[0].units[0].scroll_s = scroll_s_;
			shaders_[0].units[0].scroll_t = scroll_t_;
			break;

		case RFXTYPE_FUZZ:
			scroll_t_ += kFuzzScrollT * dt;
			if (scroll_t_ >= 1.0f) scroll_t_ -= 1.0f;
			shaders_[0].units[0].scroll_s = scroll_s_;
			shaders_[0].units[0].scroll_t = scroll_t_;
			break;

		case RFXTYPE_GLOW:
		{
			// Pulse the alpha with a sine wave
			float phase  = (time_ / kGlowPeriod) * (float)(2.0 * 3.14159265);
			float pulse  = 0.6f + 0.4f * sinf(phase);  // range [0.2, 1.0]
			shaders_[0].alpha = intensity_ * pulse;
			break;
		}

		case RFXTYPE_NOISE:
		{
			// Animate the noise texture with a pseudo-random UV offset so
			// the grain changes every rendered frame (DITD film grain).
			int ti = (int)(time_ * kNoiseFrameRate);
			float os = (float)((ti * 7 + 3) & 0xFF) / 255.0f;
			float ot = (float)((ti * 13 + 5) & 0xFF) / 255.0f;
			shaders_[0].units[0].scroll_s = os;
			shaders_[0].units[0].scroll_t = ot;
			// Subtle alpha variation for more organic feel
			shaders_[0].alpha = intensity_ * (0.8f + 0.2f * sinf(time_ * 4.7f));
			break;
		}

		default:
			break;
	}
}

// ---------------------------------------------------------------------------
// NumPasses
// ---------------------------------------------------------------------------
int r_effect_c::NumPasses() const
{
	return num_passes_;
}

// ---------------------------------------------------------------------------
// BeginPass / EndPass
// ---------------------------------------------------------------------------
void r_effect_c::BeginPass(int p) const
{
	SYS_ASSERT(p >= 0 && p < num_passes_);
	shaders_[p].Apply();
}

void r_effect_c::EndPass(int p) const
{
	SYS_ASSERT(p >= 0 && p < num_passes_);
	(void)p;
	r_shader_c::Reset();
}

// ---------------------------------------------------------------------------
// GetShader
// ---------------------------------------------------------------------------
const r_shader_c &r_effect_c::GetShader(int p) const
{
	SYS_ASSERT(p >= 0 && p < num_passes_);
	return shaders_[p];
}

r_shader_c &r_effect_c::GetShader(int p)
{
	SYS_ASSERT(p >= 0 && p < num_passes_);
	return shaders_[p];
}

// ---------------------------------------------------------------------------
// RebuildShaders – construct internal shader passes from current state
// ---------------------------------------------------------------------------
void r_effect_c::RebuildShaders()
{
	num_passes_ = 1;
	shaders_[0] = r_shader_c();
	shaders_[1] = r_shader_c();

	switch (type_)
	{
		default:
		case RFXTYPE_NONE:
			num_passes_ = 1;
			shaders_[0].units[0].texture = tex_;
			break;

		case RFXTYPE_SOLID:
			shaders_[0] = r_shaders::Solid(tex_);
			break;

		case RFXTYPE_MASKED:
			shaders_[0] = r_shaders::Masked(tex_);
			break;

		case RFXTYPE_TRANS:
			shaders_[0] = r_shaders::Translucent(tex_, intensity_);
			break;

		case RFXTYPE_FUZZ:
			shaders_[0] = r_shaders::Fuzzy(tex_);
			shaders_[0].alpha = intensity_ * 0.33f;
			break;

		case RFXTYPE_SHADOW:
			shaders_[0] = r_shaders::Shadow(tex_, intensity_);
			break;

		case RFXTYPE_COLORMAP:
			shaders_[0] = r_shaders::Colormap(tex_, tex2_);
			break;

		case RFXTYPE_ADDITIVE:
			shaders_[0] = r_shaders::Additive(tex_, intensity_);
			break;

		case RFXTYPE_GLOW:
			shaders_[0] = r_shaders::Additive(tex_, intensity_);
			shaders_[0].blend = RBLEND_ADD;
			shaders_[0].depth_write = false;
			break;

		case RFXTYPE_ENVMAP:
			shaders_[0] = r_shaders::EnvMap(tex_, tex2_, intensity_ * 0.3f);
			break;

		case RFXTYPE_WATER:
			// Translucent, scrolling, mildly additive
			shaders_[0] = r_shaders::Translucent(tex_, intensity_ * 0.7f);
			shaders_[0].fog_enabled = false;  // caller can override
			shaders_[0].units[0].scroll_s = 0.0f;
			shaders_[0].units[0].scroll_t = 0.0f;
			break;

		// ---- DITD-specific effects ----

		case RFXTYPE_NOISE:
			shaders_[0] = r_shaders::Noise(tex_, intensity_);
			break;

		case RFXTYPE_SCANLINE:
			shaders_[0] = r_shaders::Scanline(tex_, intensity_);
			break;

		case RFXTYPE_DARKZONE:
			shaders_[0] = r_shaders::DarkZone(intensity_);
			break;

		case RFXTYPE_PRERENDERED_BG:
			shaders_[0] = r_shaders::PrerenderedBG(tex_);
			break;
	}
}

}  // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
