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

#include "map-setup-x64.h"

namespace OS {

namespace x64 {

void MapSetup::MapNextVirtual() {
  if (pdtOffset == 0x200) {
    pdtOffset = 0;
    currentPDT = allocator->AllocPage();
    bzero((void *)currentPDT, 0x1000);
    ((uint64_t *)pdpt)[++pdptOffset] = currentPDT | 3;
  }
  ((uint64_t *)currentPDT)[pdtOffset++] = firstUnmappedVirtual | 0x183;
  firstUnmappedVirtual += 0x200000;
}

MapSetup::MapSetup(PageAllocator * _allocator) {
  allocator = _allocator;
  firstUnmappedVirtual = 0;
}

void MapSetup::Map() {
  size_t kernSize = KernelDataSize();
  
  // setup the initial state
  pml4 = allocator->AllocPage();
  pdpt = allocator->AllocPage();
  
  bzero((void *)pml4, 0x1000);
  bzero((void *)pdpt, 0x1000);
  
  uint64_t * pml4Buf = (uint64_t *)pml4;
  pml4Buf[0] = pdpt | 3;
  pdtOffset = 0x200;
  pdptOffset = -1;
  
  while (firstUnmappedVirtual < kernSize) {
    MapNextVirtual();
  }
}

VirtAddr MapSetup::GetFirstUnmapped() {
  return firstUnmappedVirtual;
}

PhysAddr MapSetup::GetPDPT() {
  return pdpt;
}

PhysAddr MapSetup::GetPML4() {
  return pml4;
}

}

}
