//------------------------------------------------------------------------
//  RGL vertex helpers
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

#ifndef __EPI_RGL_VERTEX_H__
#define __EPI_RGL_VERTEX_H__

#include <vector>

#include "math_vector.h"

namespace epi
{

struct RGL_Vertex2f
{
	vec2_c position;
	vec2_c texcoord;
	vec4_c color;

	RGL_Vertex2f() : position(), texcoord(), color(1, 1, 1, 1) { }
	RGL_Vertex2f(const vec2_c& pos, const vec2_c& uv, const vec4_c& rgba)
		: position(pos), texcoord(uv), color(rgba) { }
};

struct RGL_Vertex3f
{
	vec3_c position;
	vec2_c texcoord;
	vec4_c color;

	RGL_Vertex3f() : position(), texcoord(), color(1, 1, 1, 1) { }
	RGL_Vertex3f(const vec3_c& pos, const vec2_c& uv, const vec4_c& rgba)
		: position(pos), texcoord(uv), color(rgba) { }
};

template <typename VertexT>
class RGL_VertexArray
{
private:
	std::vector<VertexT> vertices;

public:
	RGL_VertexArray() = default;

	void Clear() { vertices.clear(); }
	void Reserve(size_t count) { vertices.reserve(count); }

	size_t Size() const { return vertices.size(); }
	bool Empty() const { return vertices.empty(); }

	VertexT *Data() { return vertices.empty() ? nullptr : vertices.data(); }
	const VertexT *Data() const { return vertices.empty() ? nullptr : vertices.data(); }

	VertexT& operator[](size_t index) { return vertices[index]; }
	const VertexT& operator[](size_t index) const { return vertices[index]; }

	void Add(const VertexT& vertex) { vertices.push_back(vertex); }

	template <typename IterT>
	void Append(IterT first, IterT last)
	{
		vertices.insert(vertices.end(), first, last);
	}
};

using RGL_Vertex2Array = RGL_VertexArray<RGL_Vertex2f>;
using RGL_Vertex3Array = RGL_VertexArray<RGL_Vertex3f>;

}  // namespace epi

#endif  /* __EPI_RGL_VERTEX_H__ */
