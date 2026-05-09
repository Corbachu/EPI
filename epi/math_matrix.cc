//------------------------------------------------------------------------
//  EPI Matrix types
//------------------------------------------------------------------------
//
//  Copyright (c) 2004-2008  The EDGE Team.
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

#include <cmath>
#include <cstring>

#include "math_matrix.h"
#include "math_vector.h"
#include "str_format.h"


namespace epi
{

mat3_c::mat3_c()
{
	m[0]=1;  m[3]=0;  m[6]=0;
	m[1]=0;  m[4]=1;  m[7]=0;
	m[2]=0;  m[5]=0;  m[8]=1;
}

mat3_c::mat3_c(const mat3_c& rhs)
{
	memcpy(m, rhs.m, sizeof(m));
}

std::string mat3_c::ToStr() const
{
	return STR_Format(
		"\n(%1.4f  %1.4f  %1.4f)"
	    "\n(%1.4f  %1.4f  %1.4f)"
	    "\n(%1.4f  %1.4f  %1.4f)",
		m[0], m[3], m[6],
		m[1], m[4], m[7],
		m[2], m[5], m[8]);
}

mat3_c& mat3_c::Negate()
{
	for (int i = 0; i < 9; i++)
		m[i] = -m[i];

	return *this;
}

mat3_c& mat3_c::Transpose()
{
	swap(1, 3); swap(2, 6); swap(5, 7);

	return *this;
}

mat3_c& mat3_c::operator= (const mat3_c& rhs)
{
	CHECK_SELF_ASSIGN(rhs);

	memcpy(m, rhs.m, sizeof(m));

	return *this;
}

mat3_c& mat3_c::operator+= (const mat3_c& rhs)
{
	for (int i = 0; i < 9; i++)
		m[i] += rhs.m[i];

	return *this;
}

mat3_c& mat3_c::operator-= (const mat3_c& rhs)
{
	for (int i = 0; i < 9; i++)
		m[i] -= rhs.m[i];

	return *this;
}

mat3_c& mat3_c::operator*= (const mat3_c& rhs)
{
	float tmp[9];

	for (int x = 0; x < 3; x++)
	for (int y = 0; y < 3; y++)
	{
		tmp[x*3+y] = m[    y] * rhs.m[x*3  ] +
		             m[1*3+y] * rhs.m[x*3+1] +
		             m[2*3+y] * rhs.m[x*3+2];
	}

	memcpy(m, tmp, sizeof(m));

	return *this;
}

mat3_c& mat3_c::operator*= (float scale)
{
	for (int i = 0; i < 9; i++)
		m[i] *= scale;

	return *this;
}

mat3_c& mat3_c::operator/= (float scale)
{
	for (int i = 0; i < 9; i++)
		m[i] /= scale;

	return *this;
}

//------------------------------------------------------------------------

mat4_c::mat4_c()
{
	m[0]=1;  m[4]=0;  m[8] =0;  m[12]=0;
	m[1]=0;  m[5]=1;  m[9] =0;  m[13]=0;
	m[2]=0;  m[6]=0;  m[10]=1;  m[14]=0;
	m[3]=0;  m[7]=0;  m[11]=0;  m[15]=1;
}

mat4_c::mat4_c(const mat4_c& rhs)
{
	memcpy(m, rhs.m, sizeof(m));
}

mat4_c::mat4_c(const mat3_c& rhs, float w)
{
	for (int x = 0; x < 4; x++)
	for (int y = 0; y < 4; y++)
	{
		m[x*4+y] = (x < 3 && y < 3) ? rhs.m[x*3+y] : 0.0;
	}

	m[15] = w;
}

std::string mat4_c::ToStr() const
{
	return STR_Format(
		"\n(%1.4f  %1.4f  %1.4f  %1.4f)"
	    "\n(%1.4f  %1.4f  %1.4f  %1.4f)"
	    "\n(%1.4f  %1.4f  %1.4f  %1.4f)"
	    "\n(%1.4f  %1.4f  %1.4f  %1.4f)",
		m[0], m[4], m[8],  m[12],
		m[1], m[5], m[9],  m[13],
		m[2], m[6], m[10], m[14],
		m[3], m[7], m[11], m[15]);
}

mat4_c& mat4_c::Negate()
{
	for (int i = 0; i < 16; i++)
		m[i] = -m[i];

	return *this;
}

mat4_c& mat4_c::Transpose()
{
	swap(1, 4); swap(2, 8);  swap(3,  12);
	swap(6, 9); swap(7, 13); swap(11, 14);

	return *this;
}

mat4_c& mat4_c::SetOrigin(const vec3_c& rhs)
{
	m[12] = rhs.x;
	m[13] = rhs.y;
	m[14] = rhs.z;

	return *this;
}

mat4_c& mat4_c::SetOrigin(const vec3_c& rhs, float w)
{
	SetOrigin(rhs);
	m[15] = w;

	return *this;
}

mat4_c& mat4_c::SetOrigin(const vec4_c& rhs)
{
	m[12] = rhs.x;
	m[13] = rhs.y;
	m[14] = rhs.z;
	m[15] = rhs.w;

	return *this;
}

mat4_c& mat4_c::operator= (const mat4_c& rhs)
{
	CHECK_SELF_ASSIGN(rhs);

	memcpy(m, rhs.m, sizeof(m));

	return *this;
}

mat4_c& mat4_c::operator+= (const mat4_c& rhs)
{
	for (int i = 0; i < 16; i++)
		m[i] += rhs.m[i];

	return *this;
}

mat4_c& mat4_c::operator-= (const mat4_c& rhs)
{
	for (int i = 0; i < 16; i++)
		m[i] -= rhs.m[i];

	return *this;
}

mat4_c& mat4_c::operator*= (const mat4_c& rhs)
{
	float tmp[16];

	for (int x = 0; x < 4; x++)
	for (int y = 0; y < 4; y++)
	{
		tmp[x*4+y] = m[    y] * rhs.m[x*4  ] +
		             m[1*4+y] * rhs.m[x*4+1] +
		             m[2*4+y] * rhs.m[x*4+2] +
					 m[3*4+y] * rhs.m[x*4+3];
	}

	memcpy(m, tmp, sizeof(m));

	return *this;
}

mat4_c mat4_c::operator* (const mat4_c& rhs) const
{
	float tmp[16];

	for (int x = 0; x < 4; x++)
	for (int y = 0; y < 4; y++)
	{
		tmp[x*4+y] = m[    y] * rhs.m[x*4  ] +
		             m[1*4+y] * rhs.m[x*4+1] +
		             m[2*4+y] * rhs.m[x*4+2] +
					 m[3*4+y] * rhs.m[x*4+3];
	}
	
	mat4_c newmat;
	memcpy(newmat.m, tmp, sizeof(m));

	return newmat;
}

mat4_c& mat4_c::operator*= (float scale)
{
	for (int i = 0; i < 16; i++)
		m[i] *= scale;

	return *this;
}

mat4_c& mat4_c::operator/= (float scale)
{
	for (int i = 0; i < 16; i++)
		m[i] /= scale;

	return *this;
}

//------------------------------------------------------------------------
// mat3_c factory methods
//------------------------------------------------------------------------

mat3_c mat3_c::Identity()
{
	return mat3_c();  // default constructor already sets identity
}

mat3_c mat3_c::Scale(float sx, float sy)
{
	mat3_c r;
	r.m[0] = sx;
	r.m[4] = sy;
	return r;
}

mat3_c mat3_c::Rotate(float radians)
{
	float c = cosf(radians);
	float s = sinf(radians);
	mat3_c r;
	// Column-major layout:
	// ( c  -s   0 )
	// ( s   c   0 )
	// ( 0   0   1 )
	r.m[0] =  c;  r.m[3] = -s;  r.m[6] = 0;
	r.m[1] =  s;  r.m[4] =  c;  r.m[7] = 0;
	r.m[2] =  0;  r.m[5] =  0;  r.m[8] = 1;
	return r;
}

mat3_c mat3_c::Translation(float tx, float ty)
{
	mat3_c r;
	r.m[6] = tx;
	r.m[7] = ty;
	return r;
}

float mat3_c::Det() const
{
	return m[0] * (m[4]*m[8] - m[7]*m[5])
	     - m[3] * (m[1]*m[8] - m[7]*m[2])
	     + m[6] * (m[1]*m[5] - m[4]*m[2]);
}

bool mat3_c::Inverse(mat3_c& out) const
{
	float det = Det();
	if (det == 0.0f) return false;
	float inv = 1.0f / det;

	out.m[0] =  (m[4]*m[8] - m[5]*m[7]) * inv;
	out.m[3] = -(m[3]*m[8] - m[5]*m[6]) * inv;
	out.m[6] =  (m[3]*m[7] - m[4]*m[6]) * inv;

	out.m[1] = -(m[1]*m[8] - m[2]*m[7]) * inv;
	out.m[4] =  (m[0]*m[8] - m[2]*m[6]) * inv;
	out.m[7] = -(m[0]*m[7] - m[1]*m[6]) * inv;

	out.m[2] =  (m[1]*m[5] - m[2]*m[4]) * inv;
	out.m[5] = -(m[0]*m[5] - m[2]*m[3]) * inv;
	out.m[8] =  (m[0]*m[4] - m[1]*m[3]) * inv;

	return true;
}

//------------------------------------------------------------------------
// mat4_c factory methods
//------------------------------------------------------------------------

mat4_c mat4_c::Identity()
{
	return mat4_c();
}

mat4_c mat4_c::Scale(float sx, float sy, float sz)
{
	mat4_c r;
	r.m[0]  = sx;
	r.m[5]  = sy;
	r.m[10] = sz;
	return r;
}

mat4_c mat4_c::Translation(float tx, float ty, float tz)
{
	mat4_c r;
	r.m[12] = tx;
	r.m[13] = ty;
	r.m[14] = tz;
	return r;
}

mat4_c mat4_c::Translation(const vec3_c& t)
{
	return Translation(t.x, t.y, t.z);
}

mat4_c mat4_c::RotateX(float radians)
{
	float c = cosf(radians);
	float s = sinf(radians);
	mat4_c r;
	r.m[5]  =  c;  r.m[9]  = -s;
	r.m[6]  =  s;  r.m[10] =  c;
	return r;
}

mat4_c mat4_c::RotateY(float radians)
{
	float c = cosf(radians);
	float s = sinf(radians);
	mat4_c r;
	r.m[0]  =  c;  r.m[8]  =  s;
	r.m[2]  = -s;  r.m[10] =  c;
	return r;
}

mat4_c mat4_c::RotateZ(float radians)
{
	float c = cosf(radians);
	float s = sinf(radians);
	mat4_c r;
	r.m[0] =  c;  r.m[4] = -s;
	r.m[1] =  s;  r.m[5] =  c;
	return r;
}

mat4_c mat4_c::Perspective(float fov_y_rad, float aspect, float near_z, float far_z)
{
	mat4_c r;
	memset(r.m, 0, sizeof(r.m));
	float f = 1.0f / tanf(fov_y_rad * 0.5f);
	r.m[0]  = f / aspect;
	r.m[5]  = f;
	r.m[10] = (far_z + near_z) / (near_z - far_z);
	r.m[11] = -1.0f;
	r.m[14] = (2.0f * far_z * near_z) / (near_z - far_z);
	return r;
}

mat4_c mat4_c::LookAt(const vec3_c& eye, const vec3_c& center, const vec3_c& up)
{
	// Compute orthonormal basis
	vec3_c f = (center - eye);
	float flen = sqrtf(f.x*f.x + f.y*f.y + f.z*f.z);
	if (flen > 0.0f) { f.x /= flen; f.y /= flen; f.z /= flen; }

	// right = normalize(cross(f, up))
	vec3_c s;
	s.x = f.y*up.z - f.z*up.y;
	s.y = f.z*up.x - f.x*up.z;
	s.z = f.x*up.y - f.y*up.x;
	float slen = sqrtf(s.x*s.x + s.y*s.y + s.z*s.z);
	if (slen > 0.0f) { s.x /= slen; s.y /= slen; s.z /= slen; }

	// u = cross(s, f)
	vec3_c u;
	u.x = s.y*f.z - s.z*f.y;
	u.y = s.z*f.x - s.x*f.z;
	u.z = s.x*f.y - s.y*f.x;

	mat4_c r;
	r.m[0]  =  s.x;   r.m[4]  =  s.y;   r.m[8]  =  s.z;   r.m[12] = -(s.x*eye.x + s.y*eye.y + s.z*eye.z);
	r.m[1]  =  u.x;   r.m[5]  =  u.y;   r.m[9]  =  u.z;   r.m[13] = -(u.x*eye.x + u.y*eye.y + u.z*eye.z);
	r.m[2]  = -f.x;   r.m[6]  = -f.y;   r.m[10] = -f.z;   r.m[14] =  (f.x*eye.x + f.y*eye.y + f.z*eye.z);
	r.m[3]  =  0.0f;  r.m[7]  =  0.0f;  r.m[11] =  0.0f;  r.m[15] =  1.0f;
	return r;
}

float mat4_c::Det() const
{
	// Cofactor expansion along first column
	float c0 = m[5] *(m[10]*m[15] - m[11]*m[14])
	          - m[9] *(m[6] *m[15] - m[7] *m[14])
	          + m[13]*(m[6] *m[11] - m[7] *m[10]);

	float c1 = m[1] *(m[10]*m[15] - m[11]*m[14])
	          - m[9] *(m[2] *m[15] - m[3] *m[14])
	          + m[13]*(m[2] *m[11] - m[3] *m[10]);

	float c2 = m[1] *(m[6] *m[15] - m[7] *m[14])
	          - m[5] *(m[2] *m[15] - m[3] *m[14])
	          + m[13]*(m[2] *m[7]  - m[3] *m[6] );

	float c3 = m[1] *(m[6] *m[11] - m[7] *m[10])
	          - m[5] *(m[2] *m[11] - m[3] *m[10])
	          + m[9] *(m[2] *m[7]  - m[3] *m[6] );

	return m[0]*c0 - m[4]*c1 + m[8]*c2 - m[12]*c3;
}

bool mat4_c::Inverse(mat4_c& out) const
{
	float tmp[16];

	tmp[0]  =  m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15] + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
	tmp[4]  = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15] - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
	tmp[8]  =  m[4]*m[9] *m[15] - m[4]*m[11]*m[13] - m[8]*m[5]*m[15] + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
	tmp[12] = -m[4]*m[9] *m[14] + m[4]*m[10]*m[13] + m[8]*m[5]*m[14] - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];

	tmp[1]  = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15] - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
	tmp[5]  =  m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15] + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
	tmp[9]  = -m[0]*m[9] *m[15] + m[0]*m[11]*m[13] + m[8]*m[1]*m[15] - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
	tmp[13] =  m[0]*m[9] *m[14] - m[0]*m[10]*m[13] - m[8]*m[1]*m[14] + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];

	tmp[2]  =  m[1]*m[6] *m[15] - m[1]*m[7] *m[14] - m[5]*m[2]*m[15] + m[5]*m[3]*m[14] + m[13]*m[2]*m[7]  - m[13]*m[3]*m[6];
	tmp[6]  = -m[0]*m[6] *m[15] + m[0]*m[7] *m[14] + m[4]*m[2]*m[15] - m[4]*m[3]*m[14] - m[12]*m[2]*m[7]  + m[12]*m[3]*m[6];
	tmp[10] =  m[0]*m[5] *m[15] - m[0]*m[7] *m[13] - m[4]*m[1]*m[15] + m[4]*m[3]*m[13] + m[12]*m[1]*m[7]  - m[12]*m[3]*m[5];
	tmp[14] = -m[0]*m[5] *m[14] + m[0]*m[6] *m[13] + m[4]*m[1]*m[14] - m[4]*m[2]*m[13] - m[12]*m[1]*m[6]  + m[12]*m[2]*m[5];

	tmp[3]  = -m[1]*m[6] *m[11] + m[1]*m[7] *m[10] + m[5]*m[2]*m[11] - m[5]*m[3]*m[10] - m[9] *m[2]*m[7]  + m[9] *m[3]*m[6];
	tmp[7]  =  m[0]*m[6] *m[11] - m[0]*m[7] *m[10] - m[4]*m[2]*m[11] + m[4]*m[3]*m[10] + m[8] *m[2]*m[7]  - m[8] *m[3]*m[6];
	tmp[11] = -m[0]*m[5] *m[11] + m[0]*m[7] *m[9]  + m[4]*m[1]*m[11] - m[4]*m[3]*m[9]  - m[8] *m[1]*m[7]  + m[8] *m[3]*m[5];
	tmp[15] =  m[0]*m[5] *m[10] - m[0]*m[6] *m[9]  - m[4]*m[1]*m[10] + m[4]*m[2]*m[9]  + m[8] *m[1]*m[6]  - m[8] *m[2]*m[5];

	float det = m[0]*tmp[0] + m[1]*tmp[4] + m[2]*tmp[8] + m[3]*tmp[12];
	if (det == 0.0f) return false;

	float inv = 1.0f / det;
	for (int i = 0; i < 16; i++)
		out.m[i] = tmp[i] * inv;

	return true;
}

}  // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
