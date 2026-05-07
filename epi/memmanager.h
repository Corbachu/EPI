//----------------------------------------------------------------------------
//  EDGE Memory Manager Header
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2005  The EDGE Team.
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
//
// This replaces the classic Z_Zone code from the original Doom Source.
//
#ifndef __EPI_MEMORY_MANAGER_HEADER__
#define __EPI_MEMORY_MANAGER_HEADER__

#include "epi.h"
#include "arrays.h"	
#include "errors.h"	

#include <new>

#include <cstddef>
#include <cstdint>

namespace epi
{

class mem_manager_c 
{
public:
    explicit mem_manager_c(unsigned int total_bytes);
    ~mem_manager_c();

    void* Alloc(std::size_t bytes);
    void  Free(void* ptr);
    void* Realloc(void* ptr, std::size_t new_size);

    unsigned int TotalBytes() const;
    unsigned int UsedBytes() const;
    unsigned int FreeBytes() const;
    void DumpStats() const;

    // Debug control
    static void SetDebug(bool on);

private:
    struct BlockHeader 
    {
        std::uint32_t magic;
        std::uint32_t size;    // payload size in bytes
        BlockHeader* next;
        BlockHeader* prev;
        bool free;
    };

    BlockHeader* head_;
    void* arena_;
    unsigned int total_bytes_;
    unsigned int used_bytes_;

    static constexpr std::uint32_t MAGIC = 0x45504930u; // 'E' 'P' 'I' '0' in ASCII
    static constexpr std::size_t ALIGNMENT = 8u;

    static bool debug_;

    void InitArena(unsigned int bytes);
    void Coalesce(BlockHeader* b);
    static std::size_t AlignUp(std::size_t v);
    BlockHeader* FindFit(std::size_t size);
    void SplitBlock(BlockHeader* b, std::size_t want);
    BlockHeader* HeaderFromPtr(void* p) const;
    void* PtrFromHeader(BlockHeader* h) const;
};

}  // namespace epi

#endif /* __EPI_MEMORY_MANAGER__ */
//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
