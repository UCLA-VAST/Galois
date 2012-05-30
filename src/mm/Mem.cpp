/** Memory allocator implementation -*- C++ -*-
 * @file
 * @section License
 *
 * Galois, a framework to exploit amorphous data-parallelism in irregular
 * programs.
 *
 * Copyright (C) 2011, The University of Texas at Austin. All rights reserved.
 * UNIVERSITY EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES CONCERNING THIS
 * SOFTWARE AND DOCUMENTATION, INCLUDING ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR ANY PARTICULAR PURPOSE, NON-INFRINGEMENT AND WARRANTIES OF
 * PERFORMANCE, AND ANY WARRANTY THAT MIGHT OTHERWISE ARISE FROM COURSE OF
 * DEALING OR USAGE OF TRADE.  NO WARRANTY IS EITHER EXPRESS OR IMPLIED WITH
 * RESPECT TO THE USE OF THE SOFTWARE OR DOCUMENTATION. Under no circumstances
 * shall University be liable for incidental, special, indirect, direct or
 * consequential damages or loss of profits, interruption of business, or
 * related expenses which may arise from use of Software or Documentation,
 * including but not limited to those resulting from defects in Software and/or
 * Documentation, or loss or inaccuracy of data of any kind.
 *
 * @section Description
 *
 * Strongly inspired by heap layers:
 *  http://www.heaplayers.org/
 * FSB is modified from:
 *  http://warp.povusers.org/FSBAllocator/
 *
 * @author Andrew Lenharth <andrewl@lenharth.org>
 */
#include "Galois/Runtime/mm/Mem.h"
#include "Galois/Runtime/Threads.h"
#include "Galois/Runtime/Support.h"

#include <map>

#ifdef GALOIS_NUMA
#include <numa.h>
#endif

using namespace GaloisRuntime;
using namespace MM;
using namespace LL;

//Anchor the class
SystemBaseAlloc::SystemBaseAlloc() {}
SystemBaseAlloc::~SystemBaseAlloc() {}

PtrLock<SizedAllocatorFactory, true> SizedAllocatorFactory::instance;
#ifndef USEMALLOC
SizedAllocatorFactory::SizedAlloc* 
SizedAllocatorFactory::getAllocatorForSize(size_t size) {
  lock.lock();
  SizedAlloc*& retval = allocators[size];
  if (!retval)
    retval = new SizedAlloc;
  lock.unlock();
 
  return retval;
}

SizedAllocatorFactory::~SizedAllocatorFactory() {
  for (AllocatorsTy::iterator it = allocators.begin(), end = allocators.end();
      it != end; ++it) {
    delete it->second;
  }
}
#endif

void* GaloisRuntime::MM::largeAlloc(size_t len) {
  void* data = 0;
#if defined GALOIS_NUMA_OLD
  nodemask_t nm = numa_no_nodes;
  unsigned int num = GaloisRuntime::ThreadPool::getActiveThreads();
  for (unsigned y = 0; y < num; ++y)
    nodemask_set(&nm, y/4);
  data = numa_alloc_interleaved_subset(len, &nm);
#elif defined GALOIS_NUMA
  bitmask* nm = numa_allocate_nodemask();
  unsigned int num = GaloisRuntime::ThreadPool::getActiveThreads();
  for (unsigned y = 0; y < num; ++y)
    numa_bitmask_setbit(nm, y/4);
  data = numa_alloc_interleaved_subset(len, nm);
  numa_free_nodemask(nm);
#else
  data = malloc(len);
#endif
  if (!data)
    abort();
  return data;
}

void GaloisRuntime::MM::largeFree(void* m, size_t len) {
#ifdef GALOIS_NUMA
  numa_free(m, len);
#else
  free(m);
#endif
}
