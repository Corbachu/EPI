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
//
//  camera_c is a self-contained, general-purpose 3D camera that works in
//  both the EDGE (Doom-derived) and DITD engine contexts.  It supports:
//
//    • Doom-style Euler orientation (yaw / pitch / roll) via angle_c
//    • First-person player cameras with optional view-bob and sway
//    • Free / cinematic cameras with unconstrained orientation
//    • Perspective and orthographic projection matrices
//    • View frustum (6 planes) for coarse AABB / sphere culling
//    • Smooth temporal interpolation between simulation ticks
//    • Direct GL matrix push (pre-GL2 glLoadMatrixf / glMultMatrixf)
//      via the optional EPI_ENABLE_RGL build
//
//  Coordinate system (Quake / EDGE convention):
//    +X = east / right,  +Y = north / forward,  +Z = up
//
//  Angles (epi::angle_c) use BAM (binary angle measurement), where
//  Ang0 = east, Ang90 = north, etc.  Pitch of 0 = looking forward on
//  the horizon; positive pitch = looking up.
//
//  Usage (player camera):
//
//      epi::camera_c cam;
//      cam.SetPosition(vec3_c(100, 200, 48));
//      cam.SetAngles(angle_c(90), angle_c(0), angle_c(0)); // facing north
//      cam.SetPerspective(75.0f, 1.333f, 1.0f, 16384.0f);
//      cam.RebuildMatrices();
//
//      // Per-frame:
//      cam.ApplyGL();                          // push to GL
//      if (cam.SphereVisible(pos, radius)) { ... }
//
//  Usage (free camera, mouselook):
//
//      cam.MoveForward(speed * dt);
//      cam.Strafe(strafe * dt);
//      cam.AdjustYaw(angle_c::FromRadians(dx * sens));
//      cam.AdjustPitch(angle_c::FromRadians(dy * sens));
//      cam.RebuildMatrices();
//
//------------------------------------------------------------------------

#ifndef __EPI_CAMERA_H__
#define __EPI_CAMERA_H__

#include "types.h"
#include "math_angle.h"
#include "math_matrix.h"
#include "math_vector.h"
#include "math_bbox.h"

namespace epi
{

// ---------------------------------------------------------------------------
// Projection mode
// ---------------------------------------------------------------------------
typedef enum
{
	RCAM_PERSPECTIVE   = 0,   // standard 3D perspective projection
	RCAM_ORTHOGRAPHIC  = 1,   // parallel (orthographic) projection
}
cam_proj_e;

// ---------------------------------------------------------------------------
// camera_frustum_c – six half-space clip planes derived from the view-
// projection matrix.  Each plane is stored as (normal, d) where the
// equation is  normal·P + d >= 0  for points on the visible side.
// ---------------------------------------------------------------------------
struct cam_frustum_c
{
	// Planes: 0=left, 1=right, 2=bottom, 3=top, 4=near, 5=far
	vec3_c normal[6];
	float  d[6];

	// Returns true when the sphere is fully or partially inside all six
	// half-spaces (i.e. may be visible).
	bool SphereVisible(const vec3_c &center, float radius) const;

	// Returns true when any part of the axis-aligned box is inside the
	// frustum.  Slightly conservative (uses the 'max dot' AABB method).
	bool BBoxVisible(const bbox3_c &box) const;

	// Extract planes from a combined view-projection matrix (column-major,
	// as produced by mat4_c).
	void ExtractFrom(const mat4_c &mvp);
};

// ---------------------------------------------------------------------------
// camera_c – the main camera class
// ---------------------------------------------------------------------------
class camera_c
{
public:
	// ---- construction ----

	camera_c();
	~camera_c();

	// Copy / assign
	camera_c(const camera_c &rhs);
	camera_c &operator=(const camera_c &rhs);

	// ---- position and orientation ----

	void SetPosition(const vec3_c &pos);
	void SetAngles(angle_c yaw, angle_c pitch, angle_c roll = angle_c(0));

	// Convenience: point camera at a world-space target.
	// 'up_hint' is used to disambiguate when looking straight up/down.
	void LookAt(const vec3_c &target,
	            const vec3_c &up_hint = vec3_c(0, 0, 1));

	// Getters
	const vec3_c &GetPosition()   const { return pos_; }
	angle_c       GetYaw()        const { return yaw_; }
	angle_c       GetPitch()      const { return pitch_; }
	angle_c       GetRoll()       const { return roll_; }

	// Derived direction vectors (unit length; valid after RebuildMatrices).
	const vec3_c &GetForward()    const { return forward_; }
	const vec3_c &GetRight()      const { return right_; }
	const vec3_c &GetUp()         const { return up_; }

	// ---- projection ----

	void SetPerspective(float fov_y_degrees, float aspect,
	                    float near_z, float far_z);
	void SetOrthographic(float left, float right,
	                     float bottom, float top,
	                     float near_z, float far_z);

	float GetFovY()   const { return fov_y_; }
	float GetAspect() const { return aspect_; }
	float GetNear()   const { return near_z_; }
	float GetFar()    const { return far_z_; }

	// ---- pitch clamping ----

	// Prevent the camera from looking more than 'limit' degrees above or
	// below the horizon.  Pass 0 to disable (default = 89.9 degrees).
	void SetPitchLimit(float degrees);

	// ---- first-person movement helpers ----

	// Move along the forward vector (positive = forward).
	void MoveForward(float dist);

	// Strafe perpendicular to forward (positive = right).
	void Strafe(float dist);

	// Move along world Z (positive = up).
	void MoveUp(float dist);

	// Rotate the camera.
	void AdjustYaw  (angle_c delta);
	void AdjustPitch(angle_c delta);
	void AdjustRoll (angle_c delta);

	// ---- view bob (player walking sway) ----

	// Call each tick when the player is on the ground and moving.
	// 'bob_time' is a monotonically increasing value (e.g. game tics).
	// 'bob_frac' is the fractional bob magnitude [0,1].
	// View-bob is added to the rendered view but NOT stored in pos_/pitch_.
	void UpdateViewBob(float bob_time, float bob_frac);

	// Disable view bobbing.
	void ClearViewBob();

	// ---- smooth interpolation ----

	// Store the current state as the "previous tick" snapshot so that
	// Interpolate() can blend between it and the current state.
	void SnapshotPrev();

	// Blend between the previous and current state by 'frac' [0,1].
	// Returns a new camera_c set to the interpolated pose.
	// The caller should call RebuildMatrices() on the result before rendering.
	camera_c Interpolate(float frac) const;

	// ---- matrix assembly ----

	// Rebuild view, projection, and combined VP matrices from current state.
	// Also refreshes the frustum planes and direction vectors.
	void RebuildMatrices();

	// Getters (valid after RebuildMatrices)
	const mat4_c           &GetViewMatrix()       const { return view_; }
	const mat4_c           &GetProjMatrix()        const { return proj_; }
	const mat4_c           &GetVPMatrix()          const { return vp_; }
	const cam_frustum_c    &GetFrustum()           const { return frustum_; }

	// ---- culling helpers (require up-to-date frustum) ----

	bool SphereVisible(const vec3_c &center, float radius) const;
	bool BBoxVisible  (const bbox3_c &box)                 const;

	// Returns the squared distance from the camera position to the point.
	float DistSq(const vec3_c &pt) const;

	// ---- GL integration (only compiled when EPI_ENABLE_RGL is defined) ----

#if defined(EPI_ENABLE_RGL)
	// Load the view matrix into the current GL MODELVIEW matrix stack.
	// Caller is responsible for calling glMatrixMode(GL_MODELVIEW) first.
	void ApplyViewGL() const;

	// Load the projection matrix into the current GL PROJECTION matrix stack.
	// Caller is responsible for calling glMatrixMode(GL_PROJECTION) first.
	void ApplyProjGL() const;

	// Convenience: set both matrices in the correct GL matrix mode.
	void ApplyGL() const;
#endif

private:
	// ---- current pose ----
	vec3_c  pos_;
	angle_c yaw_;
	angle_c pitch_;
	angle_c roll_;

	// ---- previous tick pose (for interpolation) ----
	vec3_c  prev_pos_;
	angle_c prev_yaw_;
	angle_c prev_pitch_;
	angle_c prev_roll_;

	// ---- projection parameters ----
	cam_proj_e proj_mode_;
	float      fov_y_;     // field-of-view in Y (degrees), perspective only
	float      aspect_;    // width / height, perspective only
	float      near_z_;
	float      far_z_;
	// Orthographic bounds
	float      ortho_left_, ortho_right_, ortho_bottom_, ortho_top_;

	// ---- pitch clamp ----
	float      pitch_limit_;   // radians (0 = disabled)

	// ---- view bob state ----
	float      bob_z_;         // vertical bob offset (world units)
	float      bob_pitch_;     // extra pitch from bobbing (radians)

	// ---- derived (rebuilt by RebuildMatrices) ----
	vec3_c     forward_;
	vec3_c     right_;
	vec3_c     up_;
	mat4_c     view_;
	mat4_c     proj_;
	mat4_c     vp_;
	cam_frustum_c frustum_;

	// ---- helpers ----
	void BuildDirectionVectors();
	void BuildViewMatrix();
	void BuildProjMatrix();
};

}  // namespace epi

#endif  /* __EPI_CAMERA_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
