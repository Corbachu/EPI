//----------------------------------------------------------------------------
//  EDGE Memory Manager
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2002  The EDGE Team.
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
#include "epi.h"
#include "arrays.h"
//#include "memalloc.h"
#include "memmanager.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>

namespace epi
{

bool mem_manager_c::debug_ = false;

mem_manager_c::mem_manager_c(unsigned int total_bytes)
    : head_(nullptr), allocation_(nullptr), arena_(nullptr), total_bytes_(0), used_bytes_(0) {
    InitArena(total_bytes);
}

mem_manager_c::~mem_manager_c() {
    if (allocation_) std::free(allocation_);
    allocation_ = nullptr;
    arena_ = nullptr;
    head_ = nullptr;
    total_bytes_ = used_bytes_ = 0;
}

void mem_manager_c::InitArena(unsigned int bytes) {
    if (bytes < 1024u) bytes = 1024u;
    if ((std::size_t)bytes > std::numeric_limits<std::size_t>::max() - (ALIGNMENT - 1u))
        return;

    // malloc remains the backing allocator, while the pool preserves its stronger 8-byte contract.
    allocation_ = std::malloc((std::size_t)bytes + ALIGNMENT - 1u);
    if (!allocation_) {
        head_ = nullptr;
        total_bytes_ = 0;
        used_bytes_ = 0;
        return;
    }

    std::uintptr_t raw_address = reinterpret_cast<std::uintptr_t>(allocation_);
    std::uintptr_t aligned_address = (raw_address + ALIGNMENT - 1u) & ~(ALIGNMENT - 1u);
    arena_ = reinterpret_cast<void*>(aligned_address);
    total_bytes_ = bytes;
    used_bytes_ = 0;
    // Create one free block; the padded header keeps every payload and split aligned.
    head_ = new (arena_) BlockHeader();
    head_->magic = MAGIC;
    head_->size = static_cast<std::uint32_t>(bytes - HEADER_SIZE);
    head_->next = nullptr;
    head_->prev = nullptr;
    head_->free = true;
    if (debug_) std::printf("mem_manager: arena %p size %u\n", arena_, total_bytes_);
}

std::size_t mem_manager_c::AlignUp(std::size_t v) {
    if (v > std::numeric_limits<std::size_t>::max() - (ALIGNMENT - 1u))
        return 0;
    return (v + ALIGNMENT - 1u) & ~(ALIGNMENT - 1u);
}

mem_manager_c::BlockHeader* mem_manager_c::FindFit(std::size_t want) {
    for (BlockHeader* b = head_; b; b = b->next) {
        if (b->free && b->magic == MAGIC && b->size >= want) return b;
    }
    return nullptr;
}

void mem_manager_c::SplitBlock(BlockHeader* b, std::size_t want) {
    if (want > b->size || b->size - want < HEADER_SIZE + ALIGNMENT) return;
    std::uint8_t* base = reinterpret_cast<std::uint8_t*>(b);
    void* new_header_address = base + HEADER_SIZE + want;
    BlockHeader* nb = new (new_header_address) BlockHeader();
    nb->magic = MAGIC;
    nb->size = static_cast<std::uint32_t>(b->size - want - HEADER_SIZE);
    nb->free = true;
    nb->next = b->next;
    nb->prev = b;
    if (b->next) b->next->prev = nb;
    b->next = nb;
    b->size = static_cast<std::uint32_t>(want);
}

void* mem_manager_c::Alloc(std::size_t bytes) {
    if (bytes == 0) bytes = 1;
    if (bytes > std::numeric_limits<std::uint32_t>::max()) return nullptr;
    std::size_t want = AlignUp(bytes);
    if (want == 0 || want > std::numeric_limits<std::uint32_t>::max()) return nullptr;
    BlockHeader* b = FindFit(want);
    if (!b) return nullptr;
    SplitBlock(b, want);
    b->free = false;
    used_bytes_ += static_cast<unsigned int>(b->size);
    void* p = PtrFromHeader(b);
    if (debug_) std::printf("mem_manager: alloc %zu -> %p\n", bytes, p);
    return p;
}

void mem_manager_c::Free(void* ptr) {
    if (!ptr) return;
    BlockHeader* h = HeaderFromPtr(ptr);
    if (!h || h->magic != MAGIC) {
        if (debug_) std::printf("mem_manager: invalid free %p\n", ptr);
        return;
    }
    if (h->free) {
        if (debug_) std::printf("mem_manager: double free %p\n", ptr);
        return;
    }
    h->free = true;
    used_bytes_ -= static_cast<unsigned int>(h->size);
    Coalesce(h);
    if (debug_) std::printf("mem_manager: free %p\n", ptr);
}

void mem_manager_c::Coalesce(BlockHeader* b) {
    if (b->next && b->next->free && b->next->magic == MAGIC) {
        b->size += HEADER_SIZE + b->next->size;
        b->next = b->next->next;
        if (b->next) b->next->prev = b;
    }
    if (b->prev && b->prev->free && b->prev->magic == MAGIC) {
        b->prev->size += HEADER_SIZE + b->size;
        b->prev->next = b->next;
        if (b->next) b->next->prev = b->prev;
    }
}

void* mem_manager_c::Realloc(void* ptr, std::size_t new_size) {
    if (!ptr) return Alloc(new_size);
    BlockHeader* h = HeaderFromPtr(ptr);
    if (!h || h->magic != MAGIC || h->free) return nullptr;
    if (new_size == 0) { Free(ptr); return nullptr; }
    std::size_t want = AlignUp(new_size);
    if (want == 0 || want > std::numeric_limits<std::uint32_t>::max()) return nullptr;
    if (h->size >= want) return ptr;
    void* n = Alloc(new_size);
    if (!n) return nullptr;
    std::memcpy(n, ptr, h->size);
    Free(ptr);
    return n;
}

mem_manager_c::BlockHeader* mem_manager_c::HeaderFromPtr(void* p) const {
    if (!p || !arena_) return nullptr;

    std::uintptr_t arena_address = reinterpret_cast<std::uintptr_t>(arena_);
    std::uintptr_t pointer_address = reinterpret_cast<std::uintptr_t>(p);
    if (pointer_address < arena_address + HEADER_SIZE ||
        pointer_address >= arena_address + total_bytes_ ||
        (pointer_address - arena_address) % ALIGNMENT != 0)
        return nullptr;

    for (BlockHeader* block = head_; block; block = block->next) {
        if (PtrFromHeader(block) == p) return block;
    }
    return nullptr;
}

void* mem_manager_c::PtrFromHeader(BlockHeader* h) const {
    return reinterpret_cast<void*>(
        reinterpret_cast<std::uint8_t*>(h) + HEADER_SIZE);
}

unsigned int mem_manager_c::TotalBytes() const { return total_bytes_; }
unsigned int mem_manager_c::UsedBytes() const { return used_bytes_; }
unsigned int mem_manager_c::FreeBytes() const {
    unsigned int free_bytes = 0;
    for (BlockHeader* block = head_; block; block = block->next) {
        if (block->free && block->magic == MAGIC) free_bytes += block->size;
    }
    return free_bytes;
}

void mem_manager_c::DumpStats() const {
    std::printf("mem_manager: total=%u used=%u free=%u\n",
                total_bytes_, used_bytes_, FreeBytes());
}

void mem_manager_c::SetDebug(bool on) { debug_ = on; }

};	// <-- epi namespace END

