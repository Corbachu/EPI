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
#include <new>

namespace epi
{

bool mem_manager_c::debug_ = false;

mem_manager_c::mem_manager_c(unsigned int total_bytes)
    : head_(nullptr), arena_(nullptr), total_bytes_(0), used_bytes_(0) {
    InitArena(total_bytes);
}

mem_manager_c::~mem_manager_c() {
    if (arena_) std::free(arena_);
    arena_ = nullptr;
    head_ = nullptr;
    total_bytes_ = used_bytes_ = 0;
}

void mem_manager_c::InitArena(unsigned int bytes) {
    if (bytes < 1024u) bytes = 1024u;
    // allocate with malloc so it works in freestanding builds that provide heap
    arena_ = std::malloc(bytes);
    if (!arena_) {
        head_ = nullptr;
        total_bytes_ = 0;
        used_bytes_ = 0;
        return;
    }
    total_bytes_ = bytes;
    used_bytes_ = 0;
    // create single free block covering entire arena
    head_ = reinterpret_cast<BlockHeader*>(arena_);
    head_->magic = MAGIC;
    head_->size = static_cast<std::uint32_t>(bytes - sizeof(BlockHeader));
    head_->next = nullptr;
    head_->prev = nullptr;
    head_->free = true;
    if (debug_) std::printf("mem_manager: arena %p size %u\n", arena_, total_bytes_);
}

std::size_t mem_manager_c::AlignUp(std::size_t v) {
    std::size_t a = ALIGNMENT;
    return (v + (a - 1)) & ~(a - 1);
}

mem_manager_c::BlockHeader* mem_manager_c::FindFit(std::size_t want) {
    for (BlockHeader* b = head_; b; b = b->next) {
        if (b->free && b->magic == MAGIC && b->size >= want) return b;
    }
    return nullptr;
}

void mem_manager_c::SplitBlock(BlockHeader* b, std::size_t want) {
    if (b->size < want + sizeof(BlockHeader) + ALIGNMENT) return;
    std::uint8_t* base = reinterpret_cast<std::uint8_t*>(b);
    std::uint8_t* newhdr = base + sizeof(BlockHeader) + want;
    BlockHeader* nb = reinterpret_cast<BlockHeader*>(newhdr);
    nb->magic = MAGIC;
    nb->size = static_cast<std::uint32_t>(b->size - want - sizeof(BlockHeader));
    nb->free = true;
    nb->next = b->next;
    nb->prev = b;
    if (b->next) b->next->prev = nb;
    b->next = nb;
    b->size = static_cast<std::uint32_t>(want);
}

void* mem_manager_c::Alloc(std::size_t bytes) {
    if (bytes == 0) bytes = 1;
    std::size_t want = AlignUp(bytes);
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
        b->size += sizeof(BlockHeader) + b->next->size;
        b->next = b->next->next;
        if (b->next) b->next->prev = b;
    }
    if (b->prev && b->prev->free && b->prev->magic == MAGIC) {
        b->prev->size += sizeof(BlockHeader) + b->size;
        b->prev->next = b->next;
        if (b->next) b->next->prev = b->prev;
    }
}

void* mem_manager_c::Realloc(void* ptr, std::size_t new_size) {
    if (!ptr) return Alloc(new_size);
    BlockHeader* h = HeaderFromPtr(ptr);
    if (!h || h->magic != MAGIC) return nullptr;
    if (new_size == 0) { Free(ptr); return nullptr; }
    std::size_t want = AlignUp(new_size);
    if (h->size >= want) return ptr;
    void* n = Alloc(new_size);
    if (!n) return nullptr;
    std::memcpy(n, ptr, h->size);
    Free(ptr);
    return n;
}

mem_manager_c::BlockHeader* mem_manager_c::HeaderFromPtr(void* p) const {
    if (!p) return nullptr;
    return reinterpret_cast<BlockHeader*>(
        reinterpret_cast<std::uint8_t*>(p) - sizeof(BlockHeader));
}

void* mem_manager_c::PtrFromHeader(BlockHeader* h) const {
    return reinterpret_cast<void*>(
        reinterpret_cast<std::uint8_t*>(h) + sizeof(BlockHeader));
}

unsigned int mem_manager_c::TotalBytes() const { return total_bytes_; }
unsigned int mem_manager_c::UsedBytes() const { return used_bytes_; }
unsigned int mem_manager_c::FreeBytes() const { return total_bytes_ - used_bytes_; }

void mem_manager_c::DumpStats() const {
    std::printf("mem_manager: total=%u used=%u free=%u\n",
                total_bytes_, used_bytes_, FreeBytes());
}

void mem_manager_c::SetDebug(bool on) { debug_ = on; }

};	// <-- epi namespace END

