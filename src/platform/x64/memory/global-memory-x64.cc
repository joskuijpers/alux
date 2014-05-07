/*
 * Copyright (c) 2014, Alex Nichol and Alux contributors.
 * All rights reserved.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "global-memory-x64.h"

namespace OS {

static GlobalMap map;

GlobalMap & GlobalMap::GetGlobalMap() {
  return map;
}

void GlobalMap::InitializeGlobalMap(void * mbootPtr) {
  new(&map) GlobalMap(mbootPtr);
}

GlobalMap::GlobalMap(void * mbootPtr)
  : regions(mbootPtr),
    allocators(0x100000, 0x1000, 0x1000,
               regions.GetRegions(),
               regions.GetRegionCount()) {
  scratchLock = 0;
  bzero(scratchBitmap, sizeof(scratchBitmap));
}

static size_t PageTableSize(size_t size) {
  size_t ptCount = 1;
  size_t pdtCount = (size >> 21) + (size % (1L << 21) ? 1L : 0L);
  size_t pdptCount = (size >> 30) + (size % (1L << 30) ? 1L : 0L);
  size_t pml4Count = (size >> 39) + (size % (1L << 39) ? 1L : 0L);
  assert(pml4Count == 1);
  assert(pdptCount < 0x200);
  
  // we will also have a final PDT with a page table for temporary pages
  pdptCount++;
  pdtCount++;
  
  return (ptCount + pdtCount + pdptCount + pml4Count) << 12;
}

size_t GlobalMap::MemoryForKernel() {
  return 0x1000000; // TODO: use symbol here
}

size_t GlobalMap::MemoryForBitmaps() {
  size_t val = allocators.BitmapByteCount();
  if (val & 0xfff) {
    val += 0x1000 - (val & 0xfff);
  }
  return val;
}

size_t GlobalMap::MemoryForPageTables() {
  size_t size = MemoryForKernel() + MemoryForBitmaps();
  size_t pageTableSize = 0;
  while (1) {
    size_t newSize = PageTableSize(size + pageTableSize);
    if (newSize == pageTableSize) break;
    pageTableSize = newSize;
  }
  return pageTableSize;
}

void GlobalMap::Setup() {
  allocators.GenerateDescriptions();
  
  cout << "table memory=" << MemoryForPageTables()
    << " bitmap memory=" << MemoryForBitmaps()
    << " kernel memory=" << MemoryForKernel() << endl;
  
  size_t physicalSize = MemoryForPageTables() + MemoryForBitmaps()
    + MemoryForKernel();
  if (physicalSize & 0x1fffff) {
    physicalSize += 0x200000 - (physicalSize & 0x1fffff);
  }
  
  MapCreator creator(&regions, &allocators);
  creator.Map((uintptr_t)physicalSize, (uintptr_t)MemoryForKernel());

  pml4 = creator.GetPML4();
  pdpt = creator.GetPDPT();
  scratchTable = creator.ScratchPageTable();
}

void * GlobalMap::ReserveScratch(PhysAddr addr) {
  assert(!(addr & 0xfff));
  
  ScopeLock scope(&scratchLock);
  int scratchIdx = -1;
  
  // find a free bit and set it, and then set scratchIdx accordingly
  for (int i = 0; i < 8; i++) {
    if (!~scratchBitmap[i]) continue;
    
    int freeIndex;
    for (freeIndex = 0; freeIndex < 0x40; freeIndex++) {
      if (!(scratchBitmap[i] & (1L << freeIndex))) {
        break;
      }
    }
    scratchBitmap[i] |= (1L << freeIndex);
    scratchIdx = (i * 0x40) + freeIndex;
    
    break;
  }
  if (scratchIdx < 0) return NULL;
  
  scratchTable[scratchIdx] = 3 | addr;
  void * res = (void *)(0x8000000000L - (0x200L - scratchIdx) * 0x1000);
  __asm__("invlpg (%0)" : : "r" (res));
  return res;
}


/**
 * Release a scratch map that you made earlier.
 */
void GlobalMap::FreeScratchIndex(void * virt) {
  ScopeLock scope(&scratchLock);
  
  uintptr_t num = (uintptr_t)virt;
  num -= (0x8000000000L - 0x200000L);
  num >>= 12;
  
  int byteIndex = (int)(num >> 6);
  int bitIndex = (int)(num & 0x3f);
  scratchBitmap[byteIndex] ^= (1L << bitIndex);
}

AllocatorList & GlobalMap::GetAllocatorList() {
  return allocators;
}

}
