//----------------------------------------------------------------------------
//  EPI MD2 Model Loader (Quake 2 format)
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
//  Format reference: Quake 2 MD2 specification (id Software)
//
//  Limits (hard-coded in Quake 2):
//    max vertices  per frame : 2048
//    max triangles           : 4096
//    max tex-coord pairs     : 4096
//    max animation frames    : 512
//    max skins               : 32
//
//  Vertex normals are stored as an index into a 162-entry precalculated
//  table (the Quake 2 anorm table).
//
//----------------------------------------------------------------------------

#ifndef __EPI_MODEL_MD2_H__
#define __EPI_MODEL_MD2_H__

#include "model_loader.h"

namespace epi
{

class MD2Loader : public model_loader_c
{
public:
	MD2Loader() { }
	virtual ~MD2Loader() { }

	virtual bool Probe(file_c *f) override;
	virtual model_data_c *Load(file_c *f) override;
	virtual const char *FormatName() const override { return "MD2"; }
};

// ---------------------------------------------------------------------------
// MD2 frame interpolation helpers
// ---------------------------------------------------------------------------
//
// These utilities drive per-frame vertex interpolation for MD2 animations.
// They complement model_data_c::LerpVertices() with MD2-specific semantics:
// named-frame lookup, time-to-parameter conversion, and a one-call
// "advance and lerp" interface suitable for a per-render-frame call.
//

// MD2AnimState – tracks playback state for a single MD2 animation clip.
//
// A "clip" is identified by a contiguous range of frame indices
// [first_frame, last_frame] within a model_data_c.  Call Advance() every
// tick / render frame with the elapsed time, then query LerpedFrame() for
// the interpolated vertex list.
//
struct MD2AnimState
{
	int   first_frame;  // first frame index of the clip (inclusive)
	int   last_frame;   // last  frame index of the clip (inclusive)
	float fps;          // playback speed in frames per second
	bool  loop;         // true = loop, false = clamp at last frame

	// Internal playback cursor (fractional frame position within clip)
	float cursor;       // [0, clip_length)

	MD2AnimState()
		: first_frame(0), last_frame(0), fps(10.0f), loop(true), cursor(0.0f)
	{ }

	MD2AnimState(int first, int last, float rate, bool do_loop)
		: first_frame(first), last_frame(last)
		, fps(rate > 0.0f ? rate : 10.0f)
		, loop(do_loop), cursor(0.0f)
	{ }

	// Advance the cursor by dt seconds.  Returns true while the animation
	// is still playing (always true for looping clips).
	bool Advance(float dt);

	// Current integer frame index (wraps / clamps within [first, last]).
	int  CurrentFrame()  const;

	// Next integer frame index (used as the second argument to LerpVertices).
	int  NextFrame()     const;

	// Interpolation parameter t in [0,1] for LerpVertices(…,t).
	float LerpParam()   const;

	// Helper: return the number of frames in the clip.
	int  ClipLength()   const { return last_frame - first_frame + 1; }
};

// MD2_FindFrameRange – scan a model's frame names to locate the first and
// last frames of a named animation clip (e.g. "stand", "run", "pain").
//
// MD2 frame names are typically stored as "stand01", "stand02", … so this
// function strips trailing digits and matches the prefix.
//
// Returns false if no matching frames are found; first/last are unchanged.
bool MD2_FindFrameRange(const model_data_c *mdl, const char *clip_name,
                        int &first_out, int &last_out);

// MD2_LerpFrame – convenience wrapper that combines Advance() + LerpVertices()
// into a single call.
//
// Advances 'state' by dt seconds and writes the interpolated vertex list for
// body part body_idx into 'out'.  Returns true while the animation is playing
// (always true for looping states).
bool MD2_LerpFrame(const model_data_c *mdl, int body_idx,
                   MD2AnimState &state, float dt,
                   std::vector<model_vert_c> &out);

} // namespace epi

#endif /* __EPI_MODEL_MD2_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
