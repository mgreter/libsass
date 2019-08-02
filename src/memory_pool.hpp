#ifndef SASS_MEMORY_POOL_H
#define SASS_MEMORY_POOL_H

#include <stdlib.h>
#include <iostream>
#include <algorithm>
#include <vector>

namespace Sass {

  // Move this to somewhere else, should be global
  constexpr size_t SASS_MEM_ALIGN = sizeof(unsigned int);

  inline static size_t align2(size_t n) {
    return (n + SASS_MEM_ALIGN - 1) & ~(SASS_MEM_ALIGN - 1);
  }

  // SIMPLE MEMORY-POOL ALLOCATOR WITH FREELIST ON TOP

  // This is a memory pool allocator with a free list on top.
  // We only allocate memory arenas from the system in specific
  // sizes (`SassAllocatorArenaSize`). Users claim memory fragments
  // of certain sizes from the pool. If the allocation is too big
  // to fit into our buckets, we use regular malloc/free instead.
  // Fragments are always aligned to `SassAllocatorAlignment`, so
  // max is `SassAllocatorBuckets` * `SassAllocatorAlignment`.

  // When the systems starts, we allocate the first arena and then
  // start to give out addresses to memory fragments. During that
  // we steadily increase `offset` until the current arena is full.
  // Once that happens we allocate a new arena and continue.
  // https://en.wikipedia.org/wiki/Memory_pool

  // Fragments that get deallocated are not really freed, we put
  // them on our freelist. For every bucket we have a pointer to
  // the first item for reuse. That item itself holds a pointer to
  // the previously free item (regular freelist implementation).
  // https://en.wikipedia.org/wiki/Free_list

  // On allocation calls we first check if there is any suitable
  // item on the freelist. If there is we pop it from the stack
  // and return it to the caller. Otherwise we have to take out
  // a new fragment from the current `arena` and increase `offset`.

  // Note that this is not thread safe. This is on purpose as we
  // want to use the memory pool in a thread local useage. In order
  // to get this thread safe you need to only allocate one pool
  // per thread. This can be achieved by using thread local PODs.
  // Simply create a pool on the first allocation and dispose
  // it once all allocations have been returned. E.g. by using:
  // static thread_local size_t allocations;
  // static thread_local MemoryPool* pool;


  // The size of the memory pool arenas in bytes.
  constexpr static size_t SassAllocatorArenaSize = 1024 * 256;

  // How many buckets should we have for the freelist
  // Determines when allocations go directly to malloc/free
  // For maximum size of managed items multiply by alignment
  constexpr static size_t SassAllocatorBuckets = 512;

  // The alignment for the memory fragments. Must be a multiple
  // of `SASS_MEM_ALIGN` and should not be too big (maybe 1 or 2)
  constexpr static size_t SassAllocatorAlignment = SASS_MEM_ALIGN * 2;

  // The number of bytes we use for our book-keeping before every
  // memory fragment. Needed to know to which bucket we belongs on
  // deallocations, or if it should go directly to the `free` call.
  constexpr size_t SassAllocatorBookSize = sizeof(unsigned int);

  // Bytes reserve for book-keeping on the arenas
  // Currently unused and for later optimization
  constexpr size_t SassAllocatorArenaHeadSize = 0;

  class MemoryPool {

    // Current arena we fill up
    // Considered a byte array
    char* arena;

    // Position into the lake
    size_t offset = std::string::npos;

    // A list of full arenas
    std::vector<void*> arenas;

    // One pointer for every bucket (zero init)
    void* freeList2[SassAllocatorBuckets] = {};

  public:

    // Static flag to enable the pool
    // static int enabled;

    // Static flag to signal exit phase
    // All memory has already been freed
    // static int isValid;

    // Default ctor
    MemoryPool() :
      // Wait for first allocation
      arena(nullptr),
      // Set to maximum value in order to
      // make an allocation on the first run
      offset(std::string::npos)
    {
      // std::cerr << "CREATED NEW MEMORY POOL\n";
    }

    ~MemoryPool() {
      // The memory will not be used anymore
      // If you kill the pool to early go figure
      // std::cerr << "MEMORY POOL DESTRUCTOR\n";
      // Cant just throw them out, some might still want
      // to be destructed. Those will fail greatly.
      for (auto arena : arenas) {
        free(arena);
      }
      free(arena);

    }

    static void* mallocate(size_t size)
    {
      std::cerr << "Mallocate " << size << "\n";
      // Make sure we have enough space for us to
      // create the pointer to the free list later
      // Add the size needed for our book-keeping
      size = align2(SassAllocatorBookSize +
        std::max(sizeof(void*), size));
      // Allocate via malloc call
      char* buffer = (char*)malloc(size);
      // Check for valid result
      if (buffer == nullptr) {
        throw std::bad_alloc();
      }
      // Mark it for de-allocation via free
      ((unsigned int*)buffer)[0] = UINT_MAX;
      // Return pointer by skipping our book-keeping
      // std::cerr << "mallocate1 " << (void*)(buffer + SassAllocatorBookSize) << "\n";
      return (void*)(buffer + SassAllocatorBookSize);
    }

    void* allocate(size_t size)
    {

      // Make sure we have enough space for us to
      // create the pointer to the free list later
      // Add the size needed for our book-keeping
      size = align2(std::max(sizeof(void*), size)
        + SassAllocatorAlignment);
      // Size must be multiple of alignment
      size_t idx = size / SASS_MEM_ALIGN;
      // Everything bigger is allocated via malloc
      // Malloc is optimized for exactly this case
      if (idx >= SassAllocatorBuckets) {
        char* buffer = (char*)malloc(size);
        if (buffer == nullptr) {
          throw std::bad_alloc();
        }
        // Mark it for de-allocation via free
        ((unsigned int*)buffer)[0] = UINT_MAX;
        // Return pointer by skipping our book-keeping
        return (void*)(buffer + SassAllocatorAlignment);
      }
      // Check if we have space 
      else {
        // size_t max = std::min(idx + 4,
        //   SassAllocatorFreeBuckets);
        // while (idx < max) {
          // Get item from free list
        void*& free = freeList2[idx];
        // Do we have a free item?
        if (free != nullptr) {
          // Copy pointer to return
          void* ptr = free;
          // Update free list pointer
          // Updating free item is quite expensive
          // Fetching from ptr is not the problem
          // Probably due to cpu cache misses
          free = ((void**)ptr)[0];
          // std::cerr << "Got item for free\n";
          // Return poped item
          return ptr;
        }
        //  ++idx;
        // }
      }

      // size_t& memoryCtr = getMemoryCtr();
      // Check if we have enough space in the current bucket
      if (!arena || offset > SassAllocatorArenaSize - size) {
        if (arena) arenas.emplace_back(arena);
        arena = (char*)malloc(SassAllocatorArenaSize);
        if (arena == nullptr) throw std::bad_alloc();
        offset = SassAllocatorArenaHeadSize;
      }
      char* buffer = arena + offset;
      offset += size;
      // Set the index for this
     //  std::cerr << "Had to allocate it\n";
      ((unsigned int*)buffer)[0] = (unsigned int)idx;
      return (void*)(buffer + SassAllocatorAlignment);
    }

    void deallocate(void* ptr)
    {

      // Exit early if we are in exit phase
      // if (!MemoryPool::isValid) return;
      // std::cerr << "Deallocate stuff\n";

      // Rewind buffer from pointer
      char* buffer = (char*)ptr -
        SassAllocatorAlignment;

      // Get the index that was stored in the header
      unsigned int idx = ((unsigned int*)buffer)[0];

      if (idx != UINT_MAX) {
        // Assert that the variable has a valid index
        // SASS_ASSERT(idx < SassAllocatorFreeBuckets, "Invalid index");
        // Let memory point to previous item in free list
        ((void**)ptr)[0] = freeList2[idx];
        // Free list now points to our memory
        freeList2[idx] = (void*)ptr;
      }
      else {
        // Regular free call
        free(buffer);
      }

    }

  };

}

#endif
