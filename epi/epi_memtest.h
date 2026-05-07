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

#ifndef EPI_MEMTEST_H
#define EPI_MEMTEST_H

#include <cstddef>
#include <cstdint>

namespace epi 
{

/**
 * Run a set of memtests on the provided buffer.
 *
 * @param buf Pointer to memory buffer (must be at least size bytes).
 * @param size Size in bytes.
 * @return true if all tests passed, false on first failure.
 */
bool memtest_buffer(void* buf, std::size_t size);

/**
 * Detect the maximum usable contiguous memory (in bytes) by allocating
 * progressively larger buffers and running memtests on them.
 *
 * @param max_probe_bytes Upper bound to probe (default 256 MiB).
 * @param step_bytes Allocation step (default 1 MiB).
 * @return Largest size in bytes that allocated and passed memtest.
 */
std::size_t detect_max_memory(std::size_t max_probe_bytes = 256 * 1024 * 1024,
                              std::size_t step_bytes = 1 * 1024 * 1024);

} // namespace epi

#endif // EPI_MEMTEST_H


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab