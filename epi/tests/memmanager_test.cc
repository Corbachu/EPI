#include "epi/epi.h"
#include "epi/memmanager.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

int main()
{
    int failures = 0;
    epi::mem_manager_c manager(4096);
    std::vector<void*> allocations;

    for (std::size_t size = 1; size <= 63; size += 2)
    {
        void* allocation = manager.Alloc(size);
        if (!allocation || reinterpret_cast<std::uintptr_t>(allocation) % 8u != 0)
        {
            std::fprintf(stderr, "allocation of %zu bytes was null or misaligned\n", size);
            failures++;
            break;
        }
        std::memset(allocation, (int)size, size);
        allocations.push_back(allocation);
    }

    for (void* allocation : allocations)
        manager.Free(allocation);
    if (manager.UsedBytes() != 0)
    {
        std::fprintf(stderr, "free/coalesce left %u bytes in use\n", manager.UsedBytes());
        failures++;
    }

    unsigned char* original = static_cast<unsigned char*>(manager.Alloc(17));
    if (!original)
    {
        std::fprintf(stderr, "initial realloc fixture allocation failed\n");
        failures++;
    }
    else
    {
        for (int index = 0; index < 17; index++)
            original[index] = (unsigned char)(index + 1);

        unsigned char* enlarged = static_cast<unsigned char*>(manager.Realloc(original, 257));
        if (!enlarged || reinterpret_cast<std::uintptr_t>(enlarged) % 8u != 0)
        {
            std::fprintf(stderr, "realloc failed or returned a misaligned pointer\n");
            failures++;
        }
        else
        {
            for (int index = 0; index < 17; index++)
            {
                if (enlarged[index] != (unsigned char)(index + 1))
                {
                    std::fprintf(stderr, "realloc did not preserve payload bytes\n");
                    failures++;
                    break;
                }
            }
            unsigned char* impossible = static_cast<unsigned char*>(manager.Realloc(
                enlarged, std::numeric_limits<std::size_t>::max()));
            if (impossible != nullptr || enlarged[0] != 1)
            {
                std::fprintf(stderr, "overflowing realloc changed the live allocation\n");
                failures++;
            }
            manager.Free(enlarged);
            manager.Free(enlarged);
        }
    }

    int outside = 0;
    manager.Free(&outside);
    if (manager.UsedBytes() != 0)
    {
        std::fprintf(stderr, "invalid frees changed allocator accounting\n");
        failures++;
    }

    return failures == 0 ? 0 : 1;
}