//------------------------------------------------------------------------
//  Dreamcast PVR Image Handling
//------------------------------------------------------------------------
//
//  Copyright (c) 2017-2026  The EDGE Team.
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

#pragma once

#include "file.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace epi
{

enum class PvrPixelFormat : u8_t
{
    kArgb1555 = 0x00,
    kRgb565 = 0x01,
    kArgb4444 = 0x02,
    kYuv422 = 0x03,
    kBump = 0x04,
    kPalette4 = 0x05,
    kPalette8 = 0x06
};

enum class PvrDataFormat : u8_t
{
    kSquareTwiddled = 0x01,
    kSquareTwiddledMipmapped = 0x02,
    kVq = 0x03,
    kVqMipmapped = 0x04,
    kRectangle = 0x09,
    kRectangularStride = 0x0B,
    kRectangularTwiddled = 0x0D,
    kSmallVq = 0x10,
    kSmallVqMipmapped = 0x11
};

class PvrImage
{
public:
    int width() const { return width_; }
    int height() const { return height_; }
    PvrPixelFormat pixel_format() const { return pixel_format_; }
    PvrDataFormat data_format() const { return data_format_; }
    bool has_global_index() const { return has_global_index_; }
    u64_t global_index() const { return global_index_; }
    const byte* native_payload() const { return native_payload_.data(); }
    std::size_t native_payload_size() const { return native_payload_.size(); }

    /// Decodes the RGB565 VQ payload to top-to-bottom RGBA8 pixels.
    bool decode_rgba(std::vector<byte>* rgba, std::string* error = nullptr) const;

private:
    friend std::unique_ptr<PvrImage> load_pvr(const byte*, std::size_t, std::string*);

    int width_ = 0;
    int height_ = 0;
    PvrPixelFormat pixel_format_ = PvrPixelFormat::kRgb565;
    PvrDataFormat data_format_ = PvrDataFormat::kVq;
    bool has_global_index_ = false;
    u64_t global_index_ = 0;
    std::vector<byte> native_payload_;
};

/// Loads and owns a validated RGB565 VQ PVR image, including its native payload.
std::unique_ptr<PvrImage> load_pvr(file_c& file, std::string* error = nullptr);

/// Parses PVR bytes. The returned image owns a copy independent of the source buffer.
std::unique_ptr<PvrImage> load_pvr(const byte* data, std::size_t length,
    std::string* error = nullptr);

} // namespace epi