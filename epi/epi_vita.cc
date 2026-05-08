//----------------------------------------------------------------------------
//  PS Vita EPI System Specifics
//----------------------------------------------------------------------------
//
//  Copyright (c) 2024-2025  The EDGE Team.
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
// PlayStation Vita platform backend for EPI.
//
// Memory layout used here:
//   - LPDDR3 system RAM:  512 MiB total (shared with kernel/firmware).
//   - Application budget: ~256 MiB (conservative; actual ceiling depends on
//     firmware and app type – extended memory apps can address ~400 MiB).
//   - Strategy: reserve kBaselineSystemHeapBytes for the CRT heap, then
//     hand the remainder to an EPI mem_manager_c extra pool.
//
// When building without VitaSDK (e.g. for host-side unit tests), the Vita
// memory constants are replaced by safe desktop defaults so that this TU
// compiles cleanly.
//
#include "memmanager.h"
#include "epi.h"
#include "epi_dual_memory.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

// ---------------------------------------------------------------------------
// VitaSDK headers (only available when cross-compiling for Vita)
// ---------------------------------------------------------------------------
#if defined(__vita__) || defined(VITA) || defined(PLATFORM_VITA)
#  include <psp2/kernel/processmgr.h>  // sceKernelGetFreeMemorySize / sceKernelTotalFreeMemSize
#  include <psp2/kernel/sysmem.h>      // sceKernelAllocMemBlock, SCE_KERNEL_MEMBLOCK_TYPE_USER_RW
#  define EPI_VITA_SDK_AVAILABLE 1
#else
#  define EPI_VITA_SDK_AVAILABLE 0
#endif

namespace
{
    constexpr std::size_t kMiB = 1024u * 1024u;

    // Reserve this much for the CRT heap / system overhead before creating
    // the extra EPI pool.
    constexpr std::size_t kBaselineSystemHeapBytes = 64u * kMiB;

    // Additional safety margin subtracted from the detected free RAM before
    // handing it to the pool (avoids OOM in OS allocators at runtime).
    constexpr std::size_t kExtraPoolSafetyReserveBytes = 16u * kMiB;

    // Minimum meaningful pool size; don't create one smaller than this.
    constexpr std::size_t kMinExtraPoolBytes = 4u * kMiB;

    // Only prefer the extra pool for allocations at or above this size.
    constexpr std::size_t kPreferExtraThresholdBytes = 4u * 1024u;

    // Magic tag written into every DualAlloc header so Free/Realloc can
    // identify which sub-allocator owns the block.
    constexpr std::uint32_t kDualAllocMagic = 0x56495441u; // 'VITA'

    enum class DualAllocSource : std::uint32_t
    {
        SYSTEM = 0,
        EXTRA  = 1,
    };

    struct DualAllocHeader
    {
        std::uint32_t magic;
        std::uint32_t source;
        std::uint32_t size;
        std::uint32_t reserved;
    };

    // ---------------------------------------------------------------------------
    // Query the Vita kernel for available free memory.
    // Falls back to a conservative compile-time constant when building without
    // VitaSDK (e.g. on a host Linux machine for tests).
    // ---------------------------------------------------------------------------
    static std::size_t query_vita_free_memory()
    {
#if EPI_VITA_SDK_AVAILABLE
        // sceKernelTotalFreeMemSize returns total free memory in bytes.
        SceSize free_bytes = sceKernelTotalFreeMemSize();
        return static_cast<std::size_t>(free_bytes);
#else
        // Fallback: assume 256 MiB app budget when SDK is not available.
        return 256u * kMiB;
#endif
    }

    static std::size_t align_down_mib(std::size_t bytes)
    {
        return bytes & ~(kMiB - 1u);
    }

    static std::size_t compute_extra_pool_bytes(std::size_t detected)
    {
        if (detected <= kBaselineSystemHeapBytes)
            return 0;

        std::size_t extra = detected - kBaselineSystemHeapBytes;
        if (extra <= kExtraPoolSafetyReserveBytes)
            return 0;

        extra -= kExtraPoolSafetyReserveBytes;
        extra  = align_down_mib(extra);

        if (extra < kMinExtraPoolBytes)
            return 0;

        return extra;
    }

    static bool should_try_extra_pool(epi::mem_manager_c* manager,
                                      std::size_t bytes,
                                      bool         preferExtra)
    {
        return preferExtra && manager != nullptr
               && bytes >= kPreferExtraThresholdBytes;
    }

    static DualAllocHeader* dual_header_from_ptr(void* ptr)
    {
        if (!ptr)
            return nullptr;
        return reinterpret_cast<DualAllocHeader*>(ptr) - 1;
    }
} // anonymous namespace

// ---------------------------------------------------------------------------
// epi namespace – Vita-specific Init / Shutdown and dual-memory helpers
// ---------------------------------------------------------------------------
namespace epi
{
    static bool          inited                 = false;
    static mem_manager_c* the_mem_manager       = nullptr;
    static std::size_t   detected_memory_bytes  = 0;
    static std::size_t   extra_pool_bytes        = 0;

    /**
     * Init
     *
     * Queries the Vita kernel for free RAM at runtime, carves out a safety
     * baseline for the CRT heap, then allocates an EPI extra pool from the
     * remainder.  On Vita hardware the typical outcome is an extra pool of
     * ~176 MiB (256 MB app budget − 64 MB baseline − 16 MB reserve).
     */
    bool Init(void)
    {
        Shutdown();

        detected_memory_bytes = query_vita_free_memory();

        // Defensive fallback if the kernel query returned 0 / garbage.
        const std::size_t fallback = 256u * kMiB;
        if (detected_memory_bytes < sizeof(void*))
            detected_memory_bytes = fallback;

        extra_pool_bytes = compute_extra_pool_bytes(detected_memory_bytes);

        if (extra_pool_bytes == 0)
        {
            inited = true;
            std::printf(
                "EPI[Vita]: detected=%u bytes, extra pool disabled (baseline-only path)\n",
                static_cast<unsigned int>(detected_memory_bytes));
            return true;
        }

        unsigned int mem_size = static_cast<unsigned int>(extra_pool_bytes);

        mem_manager_c* mm = new (std::nothrow) mem_manager_c(mem_size);
        if (mm && mm->TotalBytes() == 0)
        {
            delete mm;
            mm = nullptr;
        }

        if (mm)
        {
            the_mem_manager = mm;
            inited          = true;
            std::printf(
                "EPI[Vita]: detected=%u bytes, extra pool=%u bytes "
                "(baseline=%u reserve=%u)\n",
                static_cast<unsigned int>(detected_memory_bytes),
                mem_size,
                static_cast<unsigned int>(kBaselineSystemHeapBytes),
                static_cast<unsigned int>(kExtraPoolSafetyReserveBytes));
            return true;
        }
        else
        {
            inited           = false;
            extra_pool_bytes = 0;
            std::printf(
                "EPI[Vita]: Init failed (could not create extra pool, "
                "detected=%u bytes)\n",
                static_cast<unsigned int>(detected_memory_bytes));
            return false;
        }
    }

    /**
     * Shutdown – releases the extra pool and resets all state.
     */
    void Shutdown(void)
    {
        if (the_mem_manager)
        {
            delete the_mem_manager;
            the_mem_manager = nullptr;
        }
        detected_memory_bytes = 0;
        extra_pool_bytes       = 0;
        inited                 = false;
    }

    // ---- Dual-memory query helpers -------------------------------------------

    unsigned int GetDetectedMemoryBytes(void)
    {
        return static_cast<unsigned int>(detected_memory_bytes);
    }

    unsigned int GetExtraMemoryPoolBytes(void)
    {
        return static_cast<unsigned int>(extra_pool_bytes);
    }

    bool HasExtraMemoryPool(void)
    {
        return the_mem_manager != nullptr && extra_pool_bytes > 0;
    }

    // ---- Dual-memory allocators ----------------------------------------------

    void* DualAlloc(unsigned int bytes, int preferExtra)
    {
        if (bytes == 0)
            bytes = 1;

        const std::size_t total =
            sizeof(DualAllocHeader) + static_cast<std::size_t>(bytes);

        void*            raw    = nullptr;
        DualAllocSource  source = DualAllocSource::SYSTEM;

        if (should_try_extra_pool(the_mem_manager, total, preferExtra != 0))
        {
            raw = the_mem_manager->Alloc(total);
            if (raw)
                source = DualAllocSource::EXTRA;
        }

        if (!raw)
            raw = std::malloc(total);
        if (!raw)
            return nullptr;

        DualAllocHeader* hdr = reinterpret_cast<DualAllocHeader*>(raw);
        hdr->magic    = kDualAllocMagic;
        hdr->source   = static_cast<std::uint32_t>(source);
        hdr->size     = static_cast<std::uint32_t>(bytes);
        hdr->reserved = 0;

        return reinterpret_cast<void*>(hdr + 1);
    }

    void DualFree(void* ptr)
    {
        if (!ptr)
            return;

        DualAllocHeader* hdr = dual_header_from_ptr(ptr);
        if (!hdr || hdr->magic != kDualAllocMagic)
        {
            std::free(ptr);
            return;
        }

        const DualAllocSource source =
            static_cast<DualAllocSource>(hdr->source);
        hdr->magic = 0;

        if (source == DualAllocSource::EXTRA && the_mem_manager)
        {
            the_mem_manager->Free(hdr);
            return;
        }

        std::free(hdr);
    }

    void* DualRealloc(void* ptr, unsigned int newSize, int preferExtra)
    {
        if (!ptr)
            return DualAlloc(newSize, preferExtra);

        if (newSize == 0)
        {
            DualFree(ptr);
            return nullptr;
        }

        DualAllocHeader* hdr = dual_header_from_ptr(ptr);
        if (!hdr || hdr->magic != kDualAllocMagic)
            return std::realloc(ptr, newSize);

        const std::size_t     total  =
            sizeof(DualAllocHeader) + static_cast<std::size_t>(newSize);
        const DualAllocSource source =
            static_cast<DualAllocSource>(hdr->source);

        if (source == DualAllocSource::EXTRA && the_mem_manager)
        {
            void* grown = the_mem_manager->Realloc(hdr, total);
            if (grown)
            {
                DualAllocHeader* newHdr =
                    reinterpret_cast<DualAllocHeader*>(grown);
                newHdr->magic    = kDualAllocMagic;
                newHdr->source   = static_cast<std::uint32_t>(DualAllocSource::EXTRA);
                newHdr->size     = static_cast<std::uint32_t>(newSize);
                newHdr->reserved = 0;
                return reinterpret_cast<void*>(newHdr + 1);
            }
        }
        else if (source == DualAllocSource::SYSTEM)
        {
            void* grown = std::realloc(hdr, total);
            if (grown)
            {
                DualAllocHeader* newHdr =
                    reinterpret_cast<DualAllocHeader*>(grown);
                newHdr->magic    = kDualAllocMagic;
                newHdr->source   = static_cast<std::uint32_t>(DualAllocSource::SYSTEM);
                newHdr->size     = static_cast<std::uint32_t>(newSize);
                newHdr->reserved = 0;
                return reinterpret_cast<void*>(newHdr + 1);
            }
        }

        // Fall back: alloc-copy-free
        void* newPtr = DualAlloc(newSize, preferExtra);
        if (!newPtr)
            return nullptr;

        const std::size_t copySize =
            std::min<std::size_t>(hdr->size, newSize);
        std::memcpy(newPtr, ptr, copySize);
        DualFree(ptr);
        return newPtr;
    }

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
