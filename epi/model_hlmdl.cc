//----------------------------------------------------------------------------
//  EPI HLMDL Loader – implementation
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

#include "epi.h"
#include "endianess.h"
#include "model_hlmdl.h"

#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <memory>
#include <vector>

namespace epi
{

//------------------------------------------------------------------------
// Binary layout structures
//------------------------------------------------------------------------

#define HLMDL_MAGIC_IDST  0x54534449  // "IDST"  (studio model)
#define HLMDL_MAGIC_IDSQ  0x51534449  // "IDSQ"  (studio sequence – separate file)
#define HLMDL_VERSION     10

#pragma pack(push, 1)

struct hlmdl_header_t
{
	s32_t magic;         // IDST
	s32_t version;       // 10
	char  name[64];

	s32_t data_length;

	float eye_pos[3];
	float hull_min[3];
	float hull_max[3];
	float view_bbmin[3];
	float view_bbmax[3];

	s32_t flags;

	s32_t num_bones;      s32_t ofs_bones;
	s32_t num_bonecontrollers; s32_t ofs_bonecontrollers;
	s32_t num_hitboxes;   s32_t ofs_hitboxes;
	s32_t num_seq;        s32_t ofs_seq;
	s32_t num_seqgroups;  s32_t ofs_seqgroups;

	s32_t num_textures;   s32_t ofs_textures;  s32_t ofs_texturedata;

	s32_t num_skins;      // skin count per family
	s32_t num_skingroups; s32_t ofs_skingroups;  // skin families

	s32_t num_bodyparts;  s32_t ofs_bodyparts;

	s32_t num_attachments; s32_t ofs_attachments;

	s32_t sound_table;
	s32_t sound_index;
	s32_t sound_groups;
	s32_t ofs_soundgroups;

	s32_t num_transitions; s32_t ofs_transitions;
};

struct hlmdl_bone_t
{
	char  name[32];
	s32_t parent;        // -1 = root
	s32_t flags;
	s32_t bonecontroller[6];
	float value[6];      // default joint values
	float scale[6];      // animation value scale factors
};

struct hlmdl_texture_t
{
	char  name[64];
	s32_t flags;
	s32_t width;
	s32_t height;
	s32_t ofs_data;   // offset from start of file to raw 8-bit indexed pixels
	                  // followed by a 768-byte (256 * RGB) palette
};

struct hlmdl_bodypart_t
{
	char  name[64];
	s32_t num_models;
	s32_t base;      // model index base (for picking variants)
	s32_t ofs_models;
};

struct hlmdl_model_t
{
	char  name[64];
	s32_t type;       // always 0
	float bounding_radius;
	s32_t num_mesh;   s32_t ofs_mesh;
	s32_t num_verts;  s32_t ofs_vert_info;  s32_t ofs_verts;
	s32_t num_norms;  s32_t ofs_norm_info;  s32_t ofs_norms;
	s32_t num_groups; s32_t ofs_groups;
};

struct hlmdl_mesh_t
{
	s32_t num_tris;    // number of triverts (NOT triangles – these are tri-strips/fans)
	s32_t ofs_tris;
	s32_t skin_ref;    // index into skin table
	s32_t num_norms;
	s32_t norm_ofs;
};

// A "trivert" in the HL format is (vertindex, normindex, s, t).
struct hlmdl_trivert_t
{
	s16_t vert_idx;
	s16_t norm_idx;
	s16_t s;
	s16_t t;
};

#pragma pack(pop)


class HlmdlReader
{
public:
	HlmdlReader(const std::vector<u8_t> &data, size_t limit)
		: data_(data), limit_(limit)
	{ }

	bool span(size_t offset, size_t count, size_t stride) const
	{
		return stride > 0 && offset <= limit_ && count <= (limit_ - offset) / stride;
	}

	bool read_s16(size_t offset, s16_t &value) const
	{
		if (!span(offset, 1, sizeof(value)))
			return false;

		std::memcpy(&value, data_.data() + offset, sizeof(value));
		value = EPI_LE_S16(value);
		return true;
	}

	bool read_s32(size_t offset, s32_t &value) const
	{
		if (!span(offset, 1, sizeof(value)))
			return false;

		std::memcpy(&value, data_.data() + offset, sizeof(value));
		value = EPI_LE_S32(value);
		return true;
	}

	bool read_float(size_t offset, float &value) const
	{
		u32_t bits = 0;
		if (!span(offset, 1, sizeof(bits)))
			return false;

		std::memcpy(&bits, data_.data() + offset, sizeof(bits));
		bits = EPI_LE_U32(bits);
		std::memcpy(&value, &bits, sizeof(value));
		return true;
	}

	bool read_name(size_t offset, size_t width, std::string &value) const
	{
		if (!span(offset, width, 1))
			return false;

		size_t length = 0;
		while (length < width && data_[offset + length] != 0)
			length++;

		value.assign(reinterpret_cast<const char *>(data_.data() + offset), length);
		return true;
	}

private:
	const std::vector<u8_t> &data_;
	size_t limit_;
};


//------------------------------------------------------------------------
// HLMDLLoader::Probe
//------------------------------------------------------------------------

bool HLMDLLoader::Probe(file_c *f)
{
	s32_t magic = 0;
	if (f->Read(&magic, 4) != 4)
		return false;
	magic = EPI_LE_S32(magic);
	return (magic == HLMDL_MAGIC_IDST);
}


//------------------------------------------------------------------------
// HLMDLLoader::Load
//------------------------------------------------------------------------
//
// Reads body-part geometry (first sub-model of each body part) into the
// common model_data_c format as a single static frame.
//
model_data_c *HLMDLLoader::Load(file_c *f)
{
	if (!f->Seek(0, file_c::SEEKPOINT_END))
		return NULL;

	int file_len = f->GetLength();
	if (file_len < (int)sizeof(hlmdl_header_t) ||
		!f->Seek(0, file_c::SEEKPOINT_START))
	{
		I_Warning("HLMDL: file too small\n");
		return NULL;
	}

	std::vector<u8_t> raw((size_t)file_len);
	if (f->Read(raw.data(), (unsigned)file_len) != (unsigned)file_len)
	{
		I_Warning("HLMDL: failed to read file\n");
		return NULL;
	}

	HlmdlReader physical_reader(raw, raw.size());
	s32_t magic = 0;
	s32_t version = 0;
	s32_t data_length = 0;
	if (!physical_reader.read_s32(offsetof(hlmdl_header_t, magic), magic) ||
		!physical_reader.read_s32(offsetof(hlmdl_header_t, version), version) ||
		!physical_reader.read_s32(offsetof(hlmdl_header_t, data_length), data_length))
	{
		I_Warning("HLMDL: truncated header\n");
		return NULL;
	}
	if (magic != HLMDL_MAGIC_IDST)
	{
		I_Warning("HLMDL: bad magic\n");
		return NULL;
	}
	if (version != HLMDL_VERSION)
	{
		I_Warning("HLMDL: unsupported version %ld\n", static_cast<long>(version));
		return NULL;
	}
	if (data_length < (s32_t)sizeof(hlmdl_header_t) || data_length > file_len)
	{
		I_Warning("HLMDL: invalid declared file length\n");
		return NULL;
	}

	HlmdlReader reader(raw, (size_t)data_length);
	s32_t num_bodyparts = 0;
	s32_t ofs_bodyparts = 0;
	s32_t num_textures = 0;
	s32_t ofs_textures = 0;
	s32_t num_skinrefs = 0;
	s32_t num_skin_families = 0;
	s32_t ofs_skin_families = 0;
	if (!reader.read_s32(offsetof(hlmdl_header_t, num_bodyparts), num_bodyparts) ||
		!reader.read_s32(offsetof(hlmdl_header_t, ofs_bodyparts), ofs_bodyparts) ||
		!reader.read_s32(offsetof(hlmdl_header_t, num_textures), num_textures) ||
		!reader.read_s32(offsetof(hlmdl_header_t, ofs_textures), ofs_textures) ||
		!reader.read_s32(offsetof(hlmdl_header_t, num_skins), num_skinrefs) ||
		!reader.read_s32(offsetof(hlmdl_header_t, num_skingroups), num_skin_families) ||
		!reader.read_s32(offsetof(hlmdl_header_t, ofs_skingroups), ofs_skin_families))
	{
		I_Warning("HLMDL: truncated header tables\n");
		return NULL;
	}

	if (num_bodyparts <= 0 || ofs_bodyparts < 0 ||
		num_textures <= 0 || ofs_textures < 0 ||
		num_skinrefs <= 0 || num_skin_families <= 0 || ofs_skin_families < 0 ||
		!reader.span((size_t)ofs_bodyparts, (size_t)num_bodyparts,
		             sizeof(hlmdl_bodypart_t)) ||
		!reader.span((size_t)ofs_textures, (size_t)num_textures,
		             sizeof(hlmdl_texture_t)) ||
		(size_t)num_skinrefs > std::numeric_limits<size_t>::max() /
		                           (size_t)num_skin_families ||
		!reader.span((size_t)ofs_skin_families,
		             (size_t)num_skinrefs * (size_t)num_skin_families, sizeof(s16_t)))
	{
		I_Warning("HLMDL: invalid model tables\n");
		return NULL;
	}

	std::unique_ptr<model_data_c> mdl(new model_data_c());
	mdl->format_name = "HLMDL";
	mdl->fps = 0;

	for (int ti = 0; ti < num_textures; ti++)
	{
		size_t texture_offset = (size_t)ofs_textures +
		                        (size_t)ti * sizeof(hlmdl_texture_t);
		std::unique_ptr<model_tex_c> texture(new model_tex_c());
		s32_t width = 0;
		s32_t height = 0;
		if (!reader.read_name(texture_offset + offsetof(hlmdl_texture_t, name),
		                      sizeof(hlmdl_texture_t::name), texture->name) ||
			!reader.read_s32(texture_offset + offsetof(hlmdl_texture_t, width), width) ||
			!reader.read_s32(texture_offset + offsetof(hlmdl_texture_t, height), height) ||
			width <= 0 || height <= 0)
		{
			I_Warning("HLMDL: invalid texture table\n");
			return NULL;
		}

		texture->width = width;
		texture->height = height;
		mdl->skins.push_back(texture.get());
		texture.release();
	}

	std::vector<int> default_skin_family((size_t)num_skinrefs);
	for (int skin_ref = 0; skin_ref < num_skinrefs; skin_ref++)
	{
		s16_t texture_index = 0;
		if (!reader.read_s16((size_t)ofs_skin_families +
		                     (size_t)skin_ref * sizeof(s16_t), texture_index) ||
			texture_index < 0 || texture_index >= num_textures)
		{
			I_Warning("HLMDL: invalid default skin family\n");
			return NULL;
		}
		default_skin_family[(size_t)skin_ref] = texture_index;
	}

	mdl->frames.resize(1);
	mdl->frames[0].name = "base";
	bool bbox_initialised = false;

	for (int bp = 0; bp < num_bodyparts; bp++)
	{
		size_t bodypart_offset = (size_t)ofs_bodyparts +
		                         (size_t)bp * sizeof(hlmdl_bodypart_t);
		std::string bodypart_name;
		s32_t num_models = 0;
		s32_t ofs_models = 0;
		if (!reader.read_name(bodypart_offset + offsetof(hlmdl_bodypart_t, name),
		                      sizeof(hlmdl_bodypart_t::name), bodypart_name) ||
			!reader.read_s32(bodypart_offset + offsetof(hlmdl_bodypart_t, num_models),
			                 num_models) ||
			!reader.read_s32(bodypart_offset + offsetof(hlmdl_bodypart_t, ofs_models),
			                 ofs_models) ||
			num_models < 0 || ofs_models < 0 ||
			!reader.span((size_t)ofs_models, (size_t)num_models, sizeof(hlmdl_model_t)))
		{
			I_Warning("HLMDL: invalid body-part table\n");
			return NULL;
		}
		if (num_models == 0)
			continue;

		size_t model_offset = (size_t)ofs_models;
		s32_t num_meshes = 0;
		s32_t ofs_meshes = 0;
		s32_t num_vertices = 0;
		s32_t ofs_vertices = 0;
		s32_t num_normals = 0;
		s32_t ofs_normals = 0;
		if (!reader.read_s32(model_offset + offsetof(hlmdl_model_t, num_mesh), num_meshes) ||
			!reader.read_s32(model_offset + offsetof(hlmdl_model_t, ofs_mesh), ofs_meshes) ||
			!reader.read_s32(model_offset + offsetof(hlmdl_model_t, num_verts), num_vertices) ||
			!reader.read_s32(model_offset + offsetof(hlmdl_model_t, ofs_verts), ofs_vertices) ||
			!reader.read_s32(model_offset + offsetof(hlmdl_model_t, num_norms), num_normals) ||
			!reader.read_s32(model_offset + offsetof(hlmdl_model_t, ofs_norms), ofs_normals) ||
			num_meshes < 0 || num_vertices < 0 || num_normals < 0 ||
			ofs_meshes < 0 || ofs_vertices < 0 || ofs_normals < 0 ||
			!reader.span((size_t)ofs_meshes, (size_t)num_meshes, sizeof(hlmdl_mesh_t)) ||
			!reader.span((size_t)ofs_vertices, (size_t)num_vertices, sizeof(float) * 3) ||
			!reader.span((size_t)ofs_normals, (size_t)num_normals, sizeof(float) * 3))
		{
			I_Warning("HLMDL: invalid sub-model tables\n");
			return NULL;
		}
		if (num_meshes == 0 || num_vertices == 0)
			continue;

		for (int mi = 0; mi < num_meshes; mi++)
		{
			size_t mesh_offset = (size_t)ofs_meshes + (size_t)mi * sizeof(hlmdl_mesh_t);
			s32_t num_triangles = 0;
			s32_t ofs_commands = 0;
			s32_t skin_ref = 0;
			if (!reader.read_s32(mesh_offset + offsetof(hlmdl_mesh_t, num_tris),
			                     num_triangles) ||
				!reader.read_s32(mesh_offset + offsetof(hlmdl_mesh_t, ofs_tris),
				                 ofs_commands) ||
				!reader.read_s32(mesh_offset + offsetof(hlmdl_mesh_t, skin_ref), skin_ref) ||
				num_triangles < 0 || ofs_commands < 0 ||
				skin_ref < 0 || skin_ref >= num_skinrefs)
			{
				I_Warning("HLMDL: invalid mesh table\n");
				return NULL;
			}
			if (num_triangles == 0)
				continue;

			std::unique_ptr<model_body_c> body(new model_body_c());
			body->name = bodypart_name + "_mesh" + std::to_string(mi);
			body->skin_index = default_skin_family[(size_t)skin_ref];
			float inverse_width = 1.0f / (float)mdl->skins[body->skin_index]->width;
			float inverse_height = 1.0f / (float)mdl->skins[body->skin_index]->height;

			std::vector<model_vert_c> canonical_vertices;
			std::vector<model_tri_c> triangles;
			size_t command_offset = (size_t)ofs_commands;
			int emitted_triangles = 0;
			bool terminated = false;

			while (!terminated)
			{
				s16_t command = 0;
				if (!reader.read_s16(command_offset, command))
				{
					I_Warning("HLMDL: truncated triangle command stream\n");
					return NULL;
				}
				command_offset += sizeof(command);
				if (command == 0)
				{
					terminated = true;
					continue;
				}

				bool is_fan = command < 0;
				int vertex_count = is_fan ? -(int)command : (int)command;
				int command_triangles = vertex_count - 2;
				if (vertex_count < 3 || command_triangles > num_triangles - emitted_triangles ||
					!reader.span(command_offset, (size_t)vertex_count,
					             sizeof(hlmdl_trivert_t)))
				{
					I_Warning("HLMDL: invalid triangle command\n");
					return NULL;
				}

				std::vector<u16_t> strip;
				strip.reserve((size_t)vertex_count);
				for (int vi = 0; vi < vertex_count; vi++)
				{
					size_t trivert_offset = command_offset +
					                        (size_t)vi * sizeof(hlmdl_trivert_t);
					s16_t vertex_index = 0;
					s16_t normal_index = 0;
					s16_t texture_s = 0;
					s16_t texture_t = 0;
					if (!reader.read_s16(trivert_offset + offsetof(hlmdl_trivert_t, vert_idx),
					                     vertex_index) ||
						!reader.read_s16(trivert_offset + offsetof(hlmdl_trivert_t, norm_idx),
						                 normal_index) ||
						!reader.read_s16(trivert_offset + offsetof(hlmdl_trivert_t, s), texture_s) ||
						!reader.read_s16(trivert_offset + offsetof(hlmdl_trivert_t, t), texture_t) ||
						vertex_index < 0 || vertex_index >= num_vertices ||
						normal_index < 0 || normal_index >= num_normals ||
						canonical_vertices.size() > std::numeric_limits<u16_t>::max())
					{
						I_Warning("HLMDL: invalid triangle vertex\n");
						return NULL;
					}

					float position[3];
					float normal[3];
					size_t position_offset = (size_t)ofs_vertices +
					                         (size_t)vertex_index * sizeof(float) * 3;
					size_t normal_offset = (size_t)ofs_normals +
					                       (size_t)normal_index * sizeof(float) * 3;
					bool scalars_valid = true;
					for (int component = 0; component < 3; component++)
					{
						scalars_valid = scalars_valid &&
							reader.read_float(position_offset +
							                  (size_t)component * sizeof(float),
							                  position[component]) &&
							reader.read_float(normal_offset +
							                  (size_t)component * sizeof(float),
							                  normal[component]);
					}
					if (!scalars_valid)
					{
						I_Warning("HLMDL: truncated vertex data\n");
						return NULL;
					}
					for (int component = 0; component < 3; component++)
					{
						if (!std::isfinite(position[component]) ||
							!std::isfinite(normal[component]))
						{
							I_Warning("HLMDL: non-finite vertex data\n");
							return NULL;
						}
					}

					model_vert_c vertex;
					vertex.pos = vec3_c(position[0], position[1], position[2]);
					vertex.normal = vec3_c(normal[0], normal[1], normal[2]);
					vertex.uv = vec2_c((float)texture_s * inverse_width,
					                   (float)texture_t * inverse_height);
					strip.push_back((u16_t)canonical_vertices.size());
					canonical_vertices.push_back(vertex);
				}
				command_offset += (size_t)vertex_count * sizeof(hlmdl_trivert_t);

				if (is_fan)
				{
					for (int index = 2; index < vertex_count; index++)
						triangles.push_back(model_tri_c(strip[0], strip[index - 1],
						                                    strip[index]));
				}
				else
				{
					for (int index = 2; index < vertex_count; index++)
					{
						if (index & 1)
							triangles.push_back(model_tri_c(strip[index - 1], strip[index - 2],
							                                    strip[index]));
						else
							triangles.push_back(model_tri_c(strip[index - 2], strip[index - 1],
							                                    strip[index]));
					}
				}
				emitted_triangles += command_triangles;
			}

			if (emitted_triangles != num_triangles)
			{
				I_Warning("HLMDL: triangle count does not match command stream\n");
				return NULL;
			}

			body->tris = std::move(triangles);
			body->num_verts_per_frame = (int)canonical_vertices.size();
			int body_index = (int)mdl->bodies.size();
			mdl->bodies.push_back(body.get());
			body.release();

			model_frame_c &frame = mdl->frames[0];
			frame.verts.push_back(std::move(canonical_vertices));
			for (const model_vert_c &vertex : frame.verts[(size_t)body_index])
			{
				if (!bbox_initialised)
				{
					frame.bbox = bbox3_c(vertex.pos);
					bbox_initialised = true;
				}
				else
				{
					frame.bbox.Insert(vertex.pos);
				}
			}
		}
	}

	if (mdl->bodies.empty())
	{
		I_Warning("HLMDL: no renderable meshes\n");
		return NULL;
	}

	return mdl.release();
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
