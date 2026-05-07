//----------------------------------------------------------------------------
//  Dreamcast Dual-Memory Helpers
//----------------------------------------------------------------------------

#ifndef __EPI_DUAL_MEMORY_H__
#define __EPI_DUAL_MEMORY_H__

namespace epi
{
	unsigned int GetDetectedMemoryBytes(void);
	unsigned int GetExtraMemoryPoolBytes(void);
	bool HasExtraMemoryPool(void);
	void* DualAlloc(unsigned int bytes, int preferExtra = 1);
	void* DualRealloc(void* ptr, unsigned int newSize, int preferExtra = 1);
	void DualFree(void* ptr);
}

#endif