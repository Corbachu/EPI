//------------------------------------------------------------------------
//  EPI Camera – 3D camera for EDGE and DITD
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
#include "camera.h"

#include <cmath>
#include <cstring>

#if defined(EPI_ENABLE_RGL)
#if defined(_arch_dreamcast) || defined(DREAMCAST) || defined(PLATFORM_DREAMCAST)
#  include <GL/gl.h>
#elif defined(_WIN32) || defined(_WIN64)
#  include <windows.h>
#  include <GL/gl.h>
#elif defined(__APPLE__)
#  include <OpenGL/gl.h>
#else
#  include <GL/gl.h>
#endif
#endif

namespace epi
{

// Default pitch limit: prevent gimbal lock near 90 degrees.
static const float kDefaultPitchLimit = 89.0f * (float)(3.14159265358979323846 / 180.0);

// Typical EDGE / Doom view bob constants.
static const float kBobAmplitude = 3.0f;      // world-unit vertical travel
static const float kBobPitchScale = 0.008f;   // radians per unit of vertical bob

// ===========================================================================
// cam_frustum_c
// ===========================================================================

void cam_frustum_c::ExtractFrom(const mat4_c &mvp)
{
	// Using the Gribb/Hartmann method (extract from rows of the combined
	// model-view-projection matrix stored column-major in mat4_c).
	//
	// Column-major layout (same as OpenGL):
	//   column j, row i  →  m[j*4 + i]
	//
	// Row vectors as seen in the standard notation:
	//   row 0: m[0], m[4], m[8],  m[12]
	//   row 1: m[1], m[5], m[9],  m[13]
	//   row 2: m[2], m[6], m[10], m[14]
	//   row 3: m[3], m[7], m[11], m[15]

	const float *m = mvp.m;

	// Left   :  row3 + row0
	normal[0] = vec3_c(m[3] + m[0], m[7] + m[4], m[11] + m[8]);
	d[0]      = m[15] + m[12];

	// Right  :  row3 - row0
	normal[1] = vec3_c(m[3] - m[0], m[7] - m[4], m[11] - m[8]);
	d[1]      = m[15] - m[12];

	// Bottom :  row3 + row1
	normal[2] = vec3_c(m[3] + m[1], m[7] + m[5], m[11] + m[9]);
	d[2]      = m[15] + m[13];

	// Top    :  row3 - row1
	normal[3] = vec3_c(m[3] - m[1], m[7] - m[5], m[11] - m[9]);
	d[3]      = m[15] - m[13];

	// Near   :  row3 + row2
	normal[4] = vec3_c(m[3] + m[2], m[7] + m[6], m[11] + m[10]);
	d[4]      = m[15] + m[14];

	// Far    :  row3 - row2
	normal[5] = vec3_c(m[3] - m[2], m[7] - m[6], m[11] - m[10]);
	d[5]      = m[15] - m[14];

	// Normalise each plane so d is the true signed distance.
	for (int i = 0; i < 6; i++)
	{
		float len = normal[i].Length();
		if (len > 1e-6f)
		{
			normal[i] *= (1.0f / len);
			d[i]       *= (1.0f / len);
		}
	}
}

bool cam_frustum_c::SphereVisible(const vec3_c &center, float radius) const
{
	for (int i = 0; i < 6; i++)
	{
		float dist = (normal[i] * center) + d[i];
		if (dist < -radius)
			return false;
	}
	return true;
}

bool cam_frustum_c::BBoxVisible(const bbox3_c &box) const
{
	// For each plane, find the "positive vertex" (furthest in the plane normal
	// direction) using the AABB corners.  If that vertex is behind the plane,
	// the entire box is outside.
	const vec3_c lo = box.Center() - vec3_c(box.Width()  * 0.5f,
	                                         box.Depth()  * 0.5f,
	                                         box.Height() * 0.5f);
	const vec3_c hi = lo + vec3_c(box.Width(), box.Depth(), box.Height());

	for (int i = 0; i < 6; i++)
	{
		// Positive vertex: pick corner that maximises dot product.
		vec3_c pv;
		pv.x = (normal[i].x >= 0) ? hi.x : lo.x;
		pv.y = (normal[i].y >= 0) ? hi.y : lo.y;
		pv.z = (normal[i].z >= 0) ? hi.z : lo.z;

		if ((normal[i] * pv) + d[i] < 0)
			return false;
	}
	return true;
}

// ===========================================================================
// camera_c – construction
// ===========================================================================

camera_c::camera_c()
	: pos_(0, 0, 0),
	  yaw_(0), pitch_(0), roll_(0),
	  prev_pos_(0, 0, 0),
	  prev_yaw_(0), prev_pitch_(0), prev_roll_(0),
	  proj_mode_(RCAM_PERSPECTIVE),
	  fov_y_(75.0f), aspect_(4.0f / 3.0f),
	  near_z_(1.0f), far_z_(16384.0f),
	  ortho_left_(-1), ortho_right_(1), ortho_bottom_(-1), ortho_top_(1),
	  pitch_limit_(kDefaultPitchLimit),
	  bob_z_(0.0f), bob_pitch_(0.0f),
	  forward_(0, 1, 0), right_(1, 0, 0), up_(0, 0, 1)
{
	// Initialise matrices to identity so any early read is safe.
	view_ = mat4_c::Identity();
	proj_ = mat4_c::Identity();
	vp_   = mat4_c::Identity();
}

camera_c::~camera_c()
{ }

camera_c::camera_c(const camera_c &rhs)
	: pos_(rhs.pos_),
	  yaw_(rhs.yaw_), pitch_(rhs.pitch_), roll_(rhs.roll_),
	  prev_pos_(rhs.prev_pos_),
	  prev_yaw_(rhs.prev_yaw_), prev_pitch_(rhs.prev_pitch_), prev_roll_(rhs.prev_roll_),
	  proj_mode_(rhs.proj_mode_),
	  fov_y_(rhs.fov_y_), aspect_(rhs.aspect_),
	  near_z_(rhs.near_z_), far_z_(rhs.far_z_),
	  ortho_left_(rhs.ortho_left_), ortho_right_(rhs.ortho_right_),
	  ortho_bottom_(rhs.ortho_bottom_), ortho_top_(rhs.ortho_top_),
	  pitch_limit_(rhs.pitch_limit_),
	  bob_z_(rhs.bob_z_), bob_pitch_(rhs.bob_pitch_),
	  forward_(rhs.forward_), right_(rhs.right_), up_(rhs.up_),
	  view_(rhs.view_), proj_(rhs.proj_), vp_(rhs.vp_),
	  frustum_(rhs.frustum_)
{ }

camera_c &camera_c::operator=(const camera_c &rhs)
{
	if (this == &rhs) return *this;
	pos_           = rhs.pos_;
	yaw_           = rhs.yaw_;
	pitch_         = rhs.pitch_;
	roll_          = rhs.roll_;
	prev_pos_      = rhs.prev_pos_;
	prev_yaw_      = rhs.prev_yaw_;
	prev_pitch_    = rhs.prev_pitch_;
	prev_roll_     = rhs.prev_roll_;
	proj_mode_     = rhs.proj_mode_;
	fov_y_         = rhs.fov_y_;
	aspect_        = rhs.aspect_;
	near_z_        = rhs.near_z_;
	far_z_         = rhs.far_z_;
	ortho_left_    = rhs.ortho_left_;
	ortho_right_   = rhs.ortho_right_;
	ortho_bottom_  = rhs.ortho_bottom_;
	ortho_top_     = rhs.ortho_top_;
	pitch_limit_   = rhs.pitch_limit_;
	bob_z_         = rhs.bob_z_;
	bob_pitch_     = rhs.bob_pitch_;
	forward_       = rhs.forward_;
	right_         = rhs.right_;
	up_            = rhs.up_;
	view_          = rhs.view_;
	proj_          = rhs.proj_;
	vp_            = rhs.vp_;
	frustum_       = rhs.frustum_;
	return *this;
}

// ===========================================================================
// Position and orientation
// ===========================================================================

void camera_c::SetPosition(const vec3_c &pos)
{
	pos_ = pos;
}

void camera_c::SetAngles(angle_c yaw, angle_c pitch, angle_c roll)
{
	yaw_   = yaw;
	roll_  = roll;

	// Clamp pitch if a limit is in effect.
	if (pitch_limit_ > 0.0f)
	{
		angle_c limit = angle_c::FromRadians((double)pitch_limit_);
		// Treat the signed range [-limit, +limit].  angle_c wraps at 2^32,
		// so map to signed 32-bit for comparison.
		s32_t p = (s32_t)pitch.Radians();  // just use the raw comparison via Radians

		float rad = (float)pitch.Radians();
		if (rad >  (float)(M_PI))  rad -= (float)(2.0 * M_PI);  // wrap to [-pi, pi]
		if (rad >  pitch_limit_)   rad  = pitch_limit_;
		if (rad < -pitch_limit_)   rad  = -pitch_limit_;
		pitch_ = angle_c::FromRadians((double)rad);
		(void)p;
	}
	else
	{
		pitch_ = pitch;
	}
}

void camera_c::LookAt(const vec3_c &target, const vec3_c &up_hint)
{
	vec3_c dir = target - pos_;
	float len = dir.Length();
	if (len < 1e-6f)
		return;

	dir *= (1.0f / len);

	// Derive yaw from the XY projection.
	yaw_ = angle_c::FromVector(dir.x, dir.y);

	// Pitch: asin(z) in the [-pi/2, pi/2] range.
	float raw_pitch = asinf(CLAMP(-1.0f, dir.z, 1.0f));
	if (pitch_limit_ > 0.0f)
		raw_pitch = CLAMP(-pitch_limit_, raw_pitch, pitch_limit_);
	pitch_ = angle_c::FromRadians((double)raw_pitch);

	roll_ = angle_c(0);

	(void)up_hint;  // up hint currently unused; incorporated via mat4_c::LookAt
}

// ===========================================================================
// Projection
// ===========================================================================

void camera_c::SetPerspective(float fov_y_degrees, float aspect,
                               float near_z, float far_z)
{
	proj_mode_ = RCAM_PERSPECTIVE;
	fov_y_     = fov_y_degrees;
	aspect_    = aspect;
	near_z_    = near_z;
	far_z_     = far_z;
}

void camera_c::SetOrthographic(float left, float right,
                                float bottom, float top,
                                float near_z, float far_z)
{
	proj_mode_     = RCAM_ORTHOGRAPHIC;
	ortho_left_    = left;
	ortho_right_   = right;
	ortho_bottom_  = bottom;
	ortho_top_     = top;
	near_z_        = near_z;
	far_z_         = far_z;
}

void camera_c::SetPitchLimit(float degrees)
{
	pitch_limit_ = (degrees > 0.0f)
	               ? degrees * (float)(3.14159265358979323846 / 180.0)
	               : 0.0f;
}

// ===========================================================================
// First-person movement helpers
// ===========================================================================

void camera_c::MoveForward(float dist)
{
	pos_ += forward_ * dist;
}

void camera_c::Strafe(float dist)
{
	pos_ += right_ * dist;
}

void camera_c::MoveUp(float dist)
{
	pos_.z += dist;
}

void camera_c::AdjustYaw(angle_c delta)
{
	yaw_ += delta;
}

void camera_c::AdjustPitch(angle_c delta)
{
	angle_c new_pitch = pitch_ + delta;

	if (pitch_limit_ > 0.0f)
	{
		float rad = (float)new_pitch.Radians();
		if (rad > (float)M_PI)  rad -= (float)(2.0 * M_PI);
		rad = CLAMP(-pitch_limit_, rad, pitch_limit_);
		pitch_ = angle_c::FromRadians((double)rad);
	}
	else
	{
		pitch_ = new_pitch;
	}
}

void camera_c::AdjustRoll(angle_c delta)
{
	roll_ += delta;
}

// ===========================================================================
// View bob
// ===========================================================================

void camera_c::UpdateViewBob(float bob_time, float bob_frac)
{
	// Classic Doom / EDGE view bob: cosine wave for Z, half-speed for pitch.
	// bob_time drives the phase; bob_frac scales the amplitude.
	float phase = bob_time * (float)(2.0 * 3.14159265358979323846) / 20.0f;  // ~20 tic cycle
	bob_z_     =  kBobAmplitude * bob_frac * cosf(phase);
	bob_pitch_ = -kBobPitchScale * bob_frac * cosf(phase);
}

void camera_c::ClearViewBob()
{
	bob_z_     = 0.0f;
	bob_pitch_ = 0.0f;
}

// ===========================================================================
// Smooth interpolation between game tics
// ===========================================================================

void camera_c::SnapshotPrev()
{
	prev_pos_   = pos_;
	prev_yaw_   = yaw_;
	prev_pitch_ = pitch_;
	prev_roll_  = roll_;
}

camera_c camera_c::Interpolate(float frac) const
{
	camera_c out = *this;

	out.pos_   = prev_pos_.Lerp(pos_, frac);
	out.yaw_   = prev_yaw_.Lerp(yaw_, frac);
	out.pitch_ = prev_pitch_.Lerp(pitch_, frac);
	out.roll_  = prev_roll_.Lerp(roll_, frac);

	// Copy bob as-is (already per-frame)
	out.bob_z_     = bob_z_;
	out.bob_pitch_ = bob_pitch_;

	return out;
}

// ===========================================================================
// Matrix assembly
// ===========================================================================

void camera_c::BuildDirectionVectors()
{
	// Build the basis vectors from yaw + pitch.
	// EDGE convention: +Y is forward on the horizon, +Z is up.
	//
	// forward = (cos(pitch)*cos(yaw), cos(pitch)*sin(yaw), sin(pitch))
	// right   = perpendicular in the XY plane
	// up      = cross(right, forward)

	float sp, cp, sy, cy, sr, cr;
	pitch_.SinCos(&sp, &cp);
	yaw_.SinCos(&sy, &cy);
	roll_.SinCos(&sr, &cr);

	// Account for view-bob pitch on top of the stored angle.
	if (bob_pitch_ != 0.0f)
	{
		float bp = (float)pitch_.Radians() + bob_pitch_;
		sp = sinf(bp);
		cp = cosf(bp);
	}

	// Forward direction (into the screen).
	forward_ = vec3_c(cp * cy, cp * sy, sp);

	// Right: rotate 90 degrees around Z (yaw only, no pitch lean).
	right_ = vec3_c(sy, -cy, 0.0f);   // = (cos(yaw-90), sin(yaw-90), 0)

	// Up: cross(right, forward)  — then apply roll if non-zero.
	up_ = right_.Cross(forward_);
	up_.MakeUnit();

	// Apply roll: rotate right and up around the forward axis.
	if (roll_ != angle_c(0))
	{
		// Rodrigues' rotation of 'right_' about 'forward_' by roll angle.
		vec3_c new_right = right_ * cr + up_ * sr;
		vec3_c new_up    = right_ * (-sr) + up_ * cr;
		right_ = new_right;
		up_    = new_up;
	}
}

void camera_c::BuildViewMatrix()
{
	// Eye position with view-bob vertical offset applied.
	vec3_c eye = pos_;
	eye.z += bob_z_;

	// Build a LookAt matrix from our derived direction vectors.
	// mat4_c::LookAt(eye, center, up) handles the full computation,
	// but since we already have the basis we can construct directly.
	//
	// View matrix = rotation_transpose * translation(-eye)
	//
	// Row-major construction (then stored column-major for GL):
	//   | right.x    right.y    right.z   -dot(right, eye)   |
	//   | up.x       up.y       up.z      -dot(up, eye)       |
	//   | -fwd.x    -fwd.y     -fwd.z     dot(fwd, eye)       |
	//   | 0          0          0          1                  |

	float rx = right_.x, ry = right_.y, rz = right_.z;
	float ux = up_.x,    uy = up_.y,    uz = up_.z;
	float fx = forward_.x, fy = forward_.y, fz = forward_.z;

	float tx = -(rx * eye.x + ry * eye.y + rz * eye.z);
	float ty = -(ux * eye.x + uy * eye.y + uz * eye.z);
	float tz =  (fx * eye.x + fy * eye.y + fz * eye.z);

	// Column-major storage: m[col*4 + row]
	float *m = view_.m;
	m[ 0] = rx;   m[ 4] = ry;   m[ 8] = rz;   m[12] = tx;
	m[ 1] = ux;   m[ 5] = uy;   m[ 9] = uz;   m[13] = ty;
	m[ 2] = -fx;  m[ 6] = -fy;  m[10] = -fz;  m[14] = tz;
	m[ 3] = 0.0f; m[ 7] = 0.0f; m[11] = 0.0f; m[15] = 1.0f;
}

void camera_c::BuildProjMatrix()
{
	if (proj_mode_ == RCAM_PERSPECTIVE)
	{
		proj_ = mat4_c::Perspective(
		    fov_y_ * (float)(3.14159265358979323846 / 180.0),
		    aspect_, near_z_, far_z_);
	}
	else
	{
		// Orthographic projection matrix.
		// Column-major layout for GL.
		float rml = ortho_right_  - ortho_left_;
		float tmb = ortho_top_    - ortho_bottom_;
		float fmn = far_z_        - near_z_;

		SYS_ASSERT(rml != 0 && tmb != 0 && fmn != 0);

		float *m = proj_.m;
		memset(m, 0, sizeof(float) * 16);
		m[ 0] =  2.0f / rml;
		m[ 5] =  2.0f / tmb;
		m[10] = -2.0f / fmn;
		m[12] = -(ortho_right_ + ortho_left_)   / rml;
		m[13] = -(ortho_top_   + ortho_bottom_) / tmb;
		m[14] = -(far_z_       + near_z_)        / fmn;
		m[15] =  1.0f;
	}
}

void camera_c::RebuildMatrices()
{
	BuildDirectionVectors();
	BuildViewMatrix();
	BuildProjMatrix();

	// Combined view-projection matrix (proj * view).
	vp_ = proj_ * view_;

	// Extract frustum planes from the combined matrix.
	frustum_.ExtractFrom(vp_);
}

// ===========================================================================
// Culling helpers
// ===========================================================================

bool camera_c::SphereVisible(const vec3_c &center, float radius) const
{
	return frustum_.SphereVisible(center, radius);
}

bool camera_c::BBoxVisible(const bbox3_c &box) const
{
	return frustum_.BBoxVisible(box);
}

float camera_c::DistSq(const vec3_c &pt) const
{
	float dx = pt.x - pos_.x;
	float dy = pt.y - pos_.y;
	float dz = pt.z - pos_.z;
	return dx*dx + dy*dy + dz*dz;
}

// ===========================================================================
// GL integration
// ===========================================================================

#if defined(EPI_ENABLE_RGL)

void camera_c::ApplyViewGL() const
{
	glLoadMatrixf(view_.m);
}

void camera_c::ApplyProjGL() const
{
	glLoadMatrixf(proj_.m);
}

void camera_c::ApplyGL() const
{
	glMatrixMode(GL_PROJECTION);
	ApplyProjGL();

	glMatrixMode(GL_MODELVIEW);
	ApplyViewGL();
}

#endif  // EPI_ENABLE_RGL

}  // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
