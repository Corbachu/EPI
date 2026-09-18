#include "epi/epi.h"
#include "epi/model_hlmdl.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

static int warning_count = 0;

void I_Warning(const char *, ...)
{
    warning_count++;
}

class MemoryFile : public epi::file_c
{
public:
    explicit MemoryFile(const std::vector<u8_t> &data) : data_(data), position_(0) { }

    int GetLength() override { return (int)data_.size(); }
    int GetPosition() override { return position_; }

    unsigned int Read(void *destination, unsigned int size) override
    {
        unsigned int available = (unsigned int)data_.size() - (unsigned int)position_;
        unsigned int amount = std::min(size, available);
        std::memcpy(destination, data_.data() + position_, amount);
        position_ += (int)amount;
        return amount;
    }

    unsigned int Write(const void *, unsigned int) override { return 0; }

    bool Seek(int offset, int seekpoint) override
    {
        int base = 0;
        if (seekpoint == SEEKPOINT_CURRENT)
            base = position_;
        else if (seekpoint == SEEKPOINT_END)
            base = (int)data_.size();
        else if (seekpoint != SEEKPOINT_START)
            return false;

        int new_position = base + offset;
        if (new_position < 0 || new_position > (int)data_.size())
            return false;

        position_ = new_position;
        return true;
    }

private:
    const std::vector<u8_t> &data_;
    int position_;
};

static constexpr size_t kHeaderSize = 244;
static constexpr size_t kTextureOffset = kHeaderSize;
static constexpr size_t kSkinOffset = 324;
static constexpr size_t kBodypartOffset = 327;
static constexpr size_t kModelOffset = 403;
static constexpr size_t kMeshOffset = 515;
static constexpr size_t kVertexOffset = 535;
static constexpr size_t kNormalOffset = 571;
static constexpr size_t kCommandOffset = 583;
static constexpr size_t kFileSize = 611;

static void put_u16(std::vector<u8_t> &data, size_t offset, std::uint16_t value)
{
    data[offset + 0] = (u8_t)(value & 0xFFu);
    data[offset + 1] = (u8_t)(value >> 8);
}

static void put_s16(std::vector<u8_t> &data, size_t offset, std::int16_t value)
{
    put_u16(data, offset, (std::uint16_t)value);
}

static void put_u32(std::vector<u8_t> &data, size_t offset, std::uint32_t value)
{
    for (int byte = 0; byte < 4; byte++)
    data[offset + (size_t)byte] = (u8_t)(value >> (byte * 8));
}

static void put_s32(std::vector<u8_t> &data, size_t offset, std::int32_t value)
{
    put_u32(data, offset, (std::uint32_t)value);
}

static void put_float(std::vector<u8_t> &data, size_t offset, float value)
{
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    put_u32(data, offset, bits);
}

static std::vector<u8_t> make_valid_model()
{
    std::vector<u8_t> data(kFileSize, 0);

    put_u32(data, 0, 0x54534449u);
    put_s32(data, 4, 10);
    put_s32(data, 72, (std::int32_t)data.size());
    put_s32(data, 180, 1);
    put_s32(data, 184, (std::int32_t)kTextureOffset);
    put_s32(data, 192, 1);
    put_s32(data, 196, 1);
    put_s32(data, 200, (std::int32_t)kSkinOffset);
    put_s32(data, 204, 1);
    put_s32(data, 208, (std::int32_t)kBodypartOffset);

    std::fill_n(data.begin() + (std::ptrdiff_t)kTextureOffset, 64, (u8_t)'T');
    put_s32(data, kTextureOffset + 68, 64);
    put_s32(data, kTextureOffset + 72, 64);
    put_s16(data, kSkinOffset, 0);

    std::fill_n(data.begin() + (std::ptrdiff_t)kBodypartOffset, 64, (u8_t)'B');
    put_s32(data, kBodypartOffset + 64, 1);
    put_s32(data, kBodypartOffset + 72, (std::int32_t)kModelOffset);

    put_s32(data, kModelOffset + 72, 1);
    put_s32(data, kModelOffset + 76, (std::int32_t)kMeshOffset);
    put_s32(data, kModelOffset + 80, 3);
    put_s32(data, kModelOffset + 88, (std::int32_t)kVertexOffset);
    put_s32(data, kModelOffset + 92, 1);
    put_s32(data, kModelOffset + 100, (std::int32_t)kNormalOffset);

    put_s32(data, kMeshOffset + 0, 1);
    put_s32(data, kMeshOffset + 4, (std::int32_t)kCommandOffset);
    put_s32(data, kMeshOffset + 8, 0);

    const float positions[9] = {1.0f, 2.0f, 3.0f, 4.0f, 2.0f, 3.0f, 1.0f, 6.0f, 3.0f};
    for (int index = 0; index < 9; index++)
        put_float(data, kVertexOffset + (size_t)index * sizeof(float), positions[index]);
    put_float(data, kNormalOffset + 0, 0.0f);
    put_float(data, kNormalOffset + 4, 0.0f);
    put_float(data, kNormalOffset + 8, 1.0f);

    put_s16(data, kCommandOffset, 3);
    for (int index = 0; index < 3; index++)
    {
        size_t trivert_offset = kCommandOffset + 2 + (size_t)index * 8;
        put_s16(data, trivert_offset + 0, (std::int16_t)index);
        put_s16(data, trivert_offset + 2, 0);
        put_s16(data, trivert_offset + 4, (std::int16_t)(index * 16));
        put_s16(data, trivert_offset + 6, (std::int16_t)(index * 8));
    }
    put_s16(data, kCommandOffset + 26, 0);

    return data;
}

static bool rejected(const std::vector<u8_t> &data)
{
    MemoryFile file(data);
    epi::HLMDLLoader loader;
    epi::model_data_c *model = loader.Load(&file);
    delete model;
    return model == nullptr;
}

int main()
{
    int failures = 0;
    std::vector<u8_t> valid = make_valid_model();
    MemoryFile valid_file(valid);
    epi::HLMDLLoader loader;
    epi::model_data_c *model = loader.Load(&valid_file);
    if (!model || model->skins.size() != 1 || model->skins[0]->name.size() != 64 ||
        model->bodies.size() != 1 || model->bodies[0]->tris.size() != 1 ||
        model->frames.size() != 1 || model->frames[0].verts.size() != 1 ||
        model->frames[0].verts[0].size() != 3)
    {
        std::fprintf(stderr, "valid unaligned model was not decoded correctly\n");
        failures++;
    }
    delete model;

    std::vector<u8_t> truncated = valid;
    truncated.resize(kFileSize - 2);
    put_s32(truncated, 72, (std::int32_t)truncated.size());
    if (!rejected(truncated))
    {
        std::fprintf(stderr, "unterminated command stream was accepted\n");
        failures++;
    }

    std::vector<u8_t> bad_offset = valid;
    put_s32(bad_offset, 208, 10000);
    if (!rejected(bad_offset))
    {
        std::fprintf(stderr, "out-of-range body-part offset was accepted\n");
        failures++;
    }

    std::vector<u8_t> bad_index = valid;
    put_s16(bad_index, kCommandOffset + 2, 3);
    if (!rejected(bad_index))
    {
        std::fprintf(stderr, "out-of-range vertex index was accepted\n");
        failures++;
    }

    std::vector<u8_t> bad_triangle_count = valid;
    put_s32(bad_triangle_count, kMeshOffset, 2);
    if (!rejected(bad_triangle_count))
    {
        std::fprintf(stderr, "triangle-count mismatch was accepted\n");
        failures++;
    }

    std::vector<u8_t> bad_skin = valid;
    put_s32(bad_skin, kMeshOffset + 8, -1);
    if (!rejected(bad_skin))
    {
        std::fprintf(stderr, "negative skin reference was accepted\n");
        failures++;
    }

    std::vector<u8_t> non_finite = valid;
    put_float(non_finite, kVertexOffset, std::numeric_limits<float>::infinity());
    if (!rejected(non_finite))
    {
        std::fprintf(stderr, "non-finite vertex data was accepted\n");
        failures++;
    }

    if (warning_count != 6)
    {
        std::fprintf(stderr, "expected 6 malformed-input warnings, got %d\n", warning_count);
        failures++;
    }

    return failures == 0 ? 0 : 1;
}