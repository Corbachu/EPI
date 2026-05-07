//----------------------------------------------------------------------------
//  Dreamcast EPI Memory Testing
//----------------------------------------------------------------------------
//
//  Copyright (c) 2002-2008  The EDGE Team.
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

#include "epi_memtest.h"
#include <cstdlib>
#include <cstring>
#include <new>
#include <cstdint>

namespace epi 
{

namespace {

inline std::uint32_t pattern_addr(std::uint32_t idx) {
    // simple address-based pattern
    return idx ^ 0xA5A5A5A5u;
}

bool test_pattern_word(std::uint32_t* words, std::size_t word_count, std::uint32_t pat) {
    for (std::size_t i = 0; i < word_count; ++i) {
        words[i] = pat;
    }
    // verify
    for (std::size_t i = 0; i < word_count; ++i) {
        if (words[i] != pat) return false;
    }
    return true;
}

bool test_walking_bits(std::uint32_t* words, std::size_t word_count, bool ones) {
    // walking 1s or 0s across 32-bit word
    for (int b = 0; b < 32; ++b) {
        std::uint32_t pat = (ones ? (1u << b) : ~(1u << b));
        // write
        for (std::size_t i = 0; i < word_count; ++i) words[i] = pat;
        // verify
        for (std::size_t i = 0; i < word_count; ++i) {
            if (words[i] != pat) return false;
        }
    }
    return true;
}

bool test_addr_pattern(std::uint32_t* words, std::size_t word_count) {
    // write address-based pattern
    for (std::size_t i = 0; i < word_count; ++i) {
        words[i] = pattern_addr(static_cast<std::uint32_t>(i));
    }
    // verify
    for (std::size_t i = 0; i < word_count; ++i) {
        if (words[i] != pattern_addr(static_cast<std::uint32_t>(i))) return false;
    }
    return true;
}

} // anonymous

bool memtest_buffer(void* buf, std::size_t size) {
    if (!buf || size < sizeof(std::uint32_t)) return false;
    std::size_t word_count = size / sizeof(std::uint32_t);
    std::uint32_t* words = static_cast<std::uint32_t*>(buf);

    // 1) Solid patterns
    if (!test_pattern_word(words, word_count, 0x00000000u)) return false;
    if (!test_pattern_word(words, word_count, 0xFFFFFFFFu)) return false;

    // 2) Alternating patterns
    if (!test_pattern_word(words, word_count, 0xAAAAAAAAu)) return false;
    if (!test_pattern_word(words, word_count, 0x55555555u)) return false;

    // 3) Walking ones
    if (!test_walking_bits(words, word_count, true)) return false;

    // 4) Walking zeros
    if (!test_walking_bits(words, word_count, false)) return false;

    // 5) Address pattern
    if (!test_addr_pattern(words, word_count)) return false;

    return true;
}

std::size_t detect_max_memory(std::size_t max_probe_bytes, std::size_t step_bytes) {
    if (step_bytes == 0) step_bytes = 1 * 1024 * 1024;
    if (max_probe_bytes < step_bytes) max_probe_bytes = step_bytes;

    std::size_t last_good = 0;
    std::size_t probe = step_bytes;

    // Use doubling until we exceed max_probe_bytes to speed up, then refine.
    // First, exponential growth phase
    while (probe <= max_probe_bytes) {
        void* buf = std::malloc(probe);
        if (!buf) break;
        // zero the buffer to avoid leaving garbage
        std::memset(buf, 0x00, probe);
        bool ok = memtest_buffer(buf, probe);
        std::free(buf);
        if (!ok) break;
        last_good = probe;
        // try doubling next, but cap to max_probe_bytes
        if (probe >= max_probe_bytes) break;
        std::size_t next = probe * 2;
        if (next > max_probe_bytes) next = max_probe_bytes;
        // if doubling doesn't increase (overflow), break
        if (next <= probe) break;
        probe = next;
    }

    // If we didn't reach max_probe_bytes, refine linearly from last_good + step_bytes
    std::size_t refine_start = last_good + step_bytes;
    for (std::size_t s = refine_start; s <= max_probe_bytes; s += step_bytes) {
        void* buf = std::malloc(s);
        if (!buf) break;
        std::memset(buf, 0x00, s);
        bool ok = memtest_buffer(buf, s);
        std::free(buf);
        if (!ok) break;
        last_good = s;
    }

    return last_good;
}

} // namespace epi
