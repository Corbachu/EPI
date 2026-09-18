//------------------------------------------------------------------------
//  EPI PVR Image Tests
//------------------------------------------------------------------------

#include "epi/image_pvr.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace
{

void append_u16(std::vector<byte>* bytes, u16_t value)
{
    bytes->push_back(static_cast<byte>(value));
    bytes->push_back(static_cast<byte>(value >> 8));
}

void append_u32(std::vector<byte>* bytes, u32_t value)
{
    bytes->push_back(static_cast<byte>(value));
    bytes->push_back(static_cast<byte>(value >> 8));
    bytes->push_back(static_cast<byte>(value >> 16));
    bytes->push_back(static_cast<byte>(value >> 24));
}

void append_u64(std::vector<byte>* bytes, u64_t value)
{
    append_u32(bytes, static_cast<u32_t>(value));
    append_u32(bytes, static_cast<u32_t>(value >> 32));
}

std::vector<byte> make_pvr(bool gbix = false)
{
    constexpr int width = 8;
    constexpr int height = 8;
    constexpr std::size_t payload_size = 2048 + width * height / 4;
    std::vector<byte> bytes;

    if (gbix)
    {
        bytes.insert(bytes.end(), {'G', 'B', 'I', 'X'});
        append_u32(&bytes, 8);
        append_u64(&bytes, 0x1122334455667788ULL);
    }

    bytes.insert(bytes.end(), {'P', 'V', 'R', 'T'});
    append_u32(&bytes, static_cast<u32_t>(8 + payload_size));
    append_u32(&bytes, 0x00000301);
    append_u16(&bytes, width);
    append_u16(&bytes, height);
    bytes.resize(bytes.size() + payload_size, 0);

    // ~~マイウ~ 2026/08/08: 2x2コードブック順とRGB565の全域展開を一つのセルで検証する。
    const std::size_t payload_offset = bytes.size() - payload_size;
    const u16_t colours[4] = {0xF800, 0x07E0, 0x001F, 0xFFFF};
    for (int index = 0; index < 4; index++)
    {
        bytes[payload_offset + index * 2] = static_cast<byte>(colours[index]);
        bytes[payload_offset + index * 2 + 1] = static_cast<byte>(colours[index] >> 8);
    }

    const u16_t yellow = 0xFFE0;
    for (int index = 0; index < 4; index++)
    {
        bytes[payload_offset + 8 + index * 2] = static_cast<byte>(yellow);
        bytes[payload_offset + 8 + index * 2 + 1] = static_cast<byte>(yellow >> 8);
    }
    bytes[payload_offset + 2048 + 2] = 1;
    return bytes;
}

bool expect(bool condition, const char* message)
{
    if (condition)
        return true;
    std::fprintf(stderr, "image_pvr_test: %s\n", message);
    return false;
}

bool rejects(std::vector<byte> bytes, const char* expected_error)
{
    std::string error;
    auto image = epi::load_pvr(bytes.data(), bytes.size(), &error);
    return expect(!image && error == expected_error, expected_error);
}

} // namespace

int main(int argc, char** argv)
{
    std::string error;
    std::vector<byte> bytes = make_pvr(true);
    auto image = epi::load_pvr(bytes.data(), bytes.size(), &error);
    if (!expect(image != nullptr, error.c_str()) ||
        !expect(image->width() == 8 && image->height() == 8, "dimensions were not preserved") ||
        !expect(image->pixel_format() == epi::PvrPixelFormat::kRgb565, "pixel format mismatch") ||
        !expect(image->data_format() == epi::PvrDataFormat::kVq, "data format mismatch") ||
        !expect(image->has_global_index(), "GBIX was not recognised") ||
        !expect(image->global_index() == 0x1122334455667788ULL, "GBIX value mismatch") ||
        !expect(image->native_payload_size() == 2064, "native payload size mismatch"))
    {
        return 1;
    }

    bytes.assign(bytes.size(), 0);
    if (!expect(image->native_payload()[0] == 0x00 && image->native_payload()[1] == 0xF8,
        "PVR image did not retain ownership of its payload"))
    {
        return 1;
    }

    std::vector<byte> rgba;
    if (!expect(image->decode_rgba(&rgba, &error), error.c_str()) ||
        !expect(rgba.size() == 8 * 8 * 4, "decoded RGBA size mismatch") ||
        !expect(rgba[0] == 255 && rgba[1] == 0 && rgba[2] == 0 && rgba[3] == 255,
            "top-left codebook colour mismatch") ||
        !expect(rgba[8 * 4] == 0 && rgba[8 * 4 + 1] == 255, "bottom-left codebook colour mismatch") ||
        !expect(rgba[4] == 0 && rgba[5] == 0 && rgba[6] == 255, "top-right codebook colour mismatch") ||
        !expect(rgba[8 * 4 + 4] == 255 && rgba[8 * 4 + 5] == 255 && rgba[8 * 4 + 6] == 255,
            "bottom-right codebook colour mismatch") ||
        !expect(rgba[2 * 4] == 255 && rgba[2 * 4 + 1] == 255 && rgba[2 * 4 + 2] == 0,
            "Morton block index mismatch"))
    {
        return 1;
    }

    std::vector<byte> malformed = make_pvr();
    malformed.resize(15);
    if (!rejects(malformed, "PVR header is truncated"))
        return 1;

    malformed = make_pvr();
    malformed[0] = 'X';
    if (!rejects(malformed, "PVRT signature is missing"))
        return 1;

    malformed = make_pvr();
    malformed[8] = 0x00;
    if (!rejects(malformed, "only RGB565 VQ PVR images are supported"))
        return 1;

    malformed = make_pvr();
    malformed[12] = 7;
    malformed[13] = 0;
    if (!rejects(malformed, "PVR dimensions must be power-of-two values from 8 through 1024"))
        return 1;

    malformed = make_pvr();
    malformed[12] = 16;
    if (!rejects(malformed, "VQ PVR dimensions must be square"))
        return 1;

    malformed = make_pvr();
    malformed.pop_back();
    if (!rejects(malformed, "PVRT length does not match the file"))
        return 1;

    for (int index = 1; index < argc; index++)
    {
        std::ifstream input(argv[index], std::ios::binary);
        std::vector<byte> file_bytes((std::istreambuf_iterator<char>(input)),
            std::istreambuf_iterator<char>());
        auto file_image = epi::load_pvr(file_bytes.data(), file_bytes.size(), &error);
        if (!expect(input.good() || input.eof(), "unable to read production PVR") ||
            !expect(file_image != nullptr, error.c_str()))
        {
            std::fprintf(stderr, "image_pvr_test: failed file: %s\n", argv[index]);
            return 1;
        }
        std::printf("image_pvr_test: valid %s (%dx%d)\n", argv[index],
            file_image->width(), file_image->height());
    }

    std::puts("image_pvr_test: all tests passed");
    return 0;
}