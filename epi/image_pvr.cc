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

#include "image_pvr.h"

#include <cstring>
#include <limits>

namespace epi
{

namespace
{

inline constexpr std::size_t kPvrHeaderSize = 16;
inline constexpr std::size_t kVqCodebookSize = 2048;
inline constexpr int kMinimumDimension = 8;
inline constexpr int kMaximumDimension = 1024;

// ~~マイウ~ 2026/08/08: 非整列ヘッダーをSH-4で直接参照せず、小端値を安全に読む。
u16_t read_u16_le(const byte* data)
{
    return static_cast<u16_t>(data[0]) |
        static_cast<u16_t>(static_cast<u16_t>(data[1]) << 8);
}

u32_t read_u32_le(const byte* data)
{
    return static_cast<u32_t>(data[0]) |
        (static_cast<u32_t>(data[1]) << 8) |
        (static_cast<u32_t>(data[2]) << 16) |
        (static_cast<u32_t>(data[3]) << 24);
}

u64_t read_u64_le(const byte* data)
{
    return static_cast<u64_t>(read_u32_le(data)) |
        (static_cast<u64_t>(read_u32_le(data + 4)) << 32);
}

bool fail(std::string* error, const char* message)
{
    if (error)
        *error = message;
    return false;
}

bool is_power_of_two(int value)
{
    return value > 0 && (value & (value - 1)) == 0;
}

int integer_log2(int value)
{
    int result = 0;
    while (value > 1)
    {
        value >>= 1;
        result++;
    }
    return result;
}

// ~~マイウ~ 2026/08/08: 長方形VQでもブロック座標をPowerVRのモートン順へ変換する。
std::size_t twiddled_index(int x, int y, int width, int height)
{
    const int width_bits = integer_log2(width);
    const int height_bits = integer_log2(height);
    std::size_t index = 0;

    for (int bit = 0; bit < 10; bit++)
    {
        if (bit < width_bits && bit < height_bits)
        {
            index |= static_cast<std::size_t>((y >> bit) & 1) << (bit * 2);
            index |= static_cast<std::size_t>((x >> bit) & 1) << (bit * 2 + 1);
        }
        else if (bit < width_bits)
        {
            index |= static_cast<std::size_t>((x >> bit) & 1) << (bit + height_bits);
        }
        else if (bit < height_bits)
        {
            index |= static_cast<std::size_t>((y >> bit) & 1) << (bit + width_bits);
        }
        else
        {
            break;
        }
    }

    return index;
}

void decode_rgb565(u16_t colour, byte* rgba)
{
    const byte red = static_cast<byte>((colour >> 11) & 31);
    const byte green = static_cast<byte>((colour >> 5) & 63);
    const byte blue = static_cast<byte>(colour & 31);

    rgba[0] = static_cast<byte>((red << 3) | (red >> 2));
    rgba[1] = static_cast<byte>((green << 2) | (green >> 4));
    rgba[2] = static_cast<byte>((blue << 3) | (blue >> 2));
    rgba[3] = 255;
}

} // namespace

std::unique_ptr<PvrImage> load_pvr(file_c& file, std::string* error)
{
    const int file_length = file.GetLength();
    if (file_length <= 0)
    {
        fail(error, "PVR file is empty");
        return nullptr;
    }

    std::vector<byte> bytes(static_cast<std::size_t>(file_length));
    if (!file.Seek(0, file_c::SEEKPOINT_START) ||
        file.Read(bytes.data(), bytes.size()) != bytes.size())
    {
        fail(error, "PVR file read was truncated");
        return nullptr;
    }

    return load_pvr(bytes.data(), bytes.size(), error);
}

std::unique_ptr<PvrImage> load_pvr(const byte* data, std::size_t length, std::string* error)
{
    if (error)
        error->clear();
    if (!data || length < kPvrHeaderSize)
    {
        fail(error, "PVR header is truncated");
        return nullptr;
    }

    std::size_t pvr_offset = 0;
    bool has_global_index = false;
    u64_t global_index = 0;

    if (std::memcmp(data, "GBIX", 4) == 0)
    {
        if (length < 16)
        {
            fail(error, "GBIX header is truncated");
            return nullptr;
        }

        const u32_t gbix_length = read_u32_le(data + 4);
        if (gbix_length < 8 || static_cast<std::size_t>(gbix_length) > length - 8)
        {
            fail(error, "GBIX length is invalid");
            return nullptr;
        }

        has_global_index = true;
        global_index = read_u64_le(data + 8);
        pvr_offset = 8 + static_cast<std::size_t>(gbix_length);
    }

    if (pvr_offset > length || length - pvr_offset < kPvrHeaderSize ||
        std::memcmp(data + pvr_offset, "PVRT", 4) != 0)
    {
        fail(error, "PVRT signature is missing");
        return nullptr;
    }

    const byte* header = data + pvr_offset;
    const u32_t declared_length = read_u32_le(header + 4);
    if (declared_length < 8 || static_cast<std::size_t>(declared_length) != length - pvr_offset - 8)
    {
        fail(error, "PVRT length does not match the file");
        return nullptr;
    }

    const u32_t type = read_u32_le(header + 8);
    if ((type & 0xFFFF0000u) != 0)
    {
        fail(error, "PVR type reserved bytes are non-zero");
        return nullptr;
    }

    const auto pixel_format = static_cast<PvrPixelFormat>(type & 0xFFu);
    const auto data_format = static_cast<PvrDataFormat>((type >> 8) & 0xFFu);
    if (pixel_format != PvrPixelFormat::kRgb565 || data_format != PvrDataFormat::kVq)
    {
        fail(error, "only RGB565 VQ PVR images are supported");
        return nullptr;
    }

    const int width = read_u16_le(header + 12);
    const int height = read_u16_le(header + 14);
    if (width < kMinimumDimension || height < kMinimumDimension ||
        width > kMaximumDimension || height > kMaximumDimension ||
        !is_power_of_two(width) || !is_power_of_two(height))
    {
        fail(error, "PVR dimensions must be power-of-two values from 8 through 1024");
        return nullptr;
    }
    if (width != height)
    {
        fail(error, "VQ PVR dimensions must be square");
        return nullptr;
    }

    const std::size_t pixel_count = static_cast<std::size_t>(width) *
        static_cast<std::size_t>(height);
    const std::size_t expected_payload_size = kVqCodebookSize + pixel_count / 4;
    if (expected_payload_size > std::numeric_limits<u32_t>::max() ||
        declared_length != 8 + expected_payload_size)
    {
        fail(error, "PVR RGB565 VQ payload size is invalid");
        return nullptr;
    }

    std::unique_ptr<PvrImage> image(new PvrImage());
    image->width_ = width;
    image->height_ = height;
    image->pixel_format_ = pixel_format;
    image->data_format_ = data_format;
    image->has_global_index_ = has_global_index;
    image->global_index_ = global_index;
    image->native_payload_.assign(header + kPvrHeaderSize,
        header + kPvrHeaderSize + expected_payload_size);
    return image;
}

bool PvrImage::decode_rgba(std::vector<byte>* rgba, std::string* error) const
{
    if (error)
        error->clear();
    if (!rgba)
        return fail(error, "RGBA output is null");

    const std::size_t pixel_count = static_cast<std::size_t>(width_) *
        static_cast<std::size_t>(height_);
    if (native_payload_.size() != kVqCodebookSize + pixel_count / 4)
        return fail(error, "owned PVR payload is inconsistent");

    rgba->assign(pixel_count * 4, 0);
    const byte* codebook = native_payload_.data();
    const byte* indices = codebook + kVqCodebookSize;
    const int block_width = width_ / 2;
    const int block_height = height_ / 2;

    for (int block_y = 0; block_y < block_height; block_y++)
    {
        for (int block_x = 0; block_x < block_width; block_x++)
        {
            const std::size_t index = twiddled_index(block_x, block_y,
                block_width, block_height);
            const byte* colours = codebook + static_cast<std::size_t>(indices[index]) * 8;
            const int pixel_x = block_x * 2;
            const int pixel_y = block_y * 2;

            decode_rgb565(read_u16_le(colours + 0),
                rgba->data() + (static_cast<std::size_t>(pixel_y) * width_ + pixel_x) * 4);
            decode_rgb565(read_u16_le(colours + 2),
                rgba->data() + (static_cast<std::size_t>(pixel_y + 1) * width_ + pixel_x) * 4);
            decode_rgb565(read_u16_le(colours + 4),
                rgba->data() + (static_cast<std::size_t>(pixel_y) * width_ + pixel_x + 1) * 4);
            decode_rgb565(read_u16_le(colours + 6),
                rgba->data() + (static_cast<std::size_t>(pixel_y + 1) * width_ + pixel_x + 1) * 4);
        }
    }

    return true;
}

} // namespace epi