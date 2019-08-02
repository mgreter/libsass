#include "sass.hpp"
#include "allocator.hpp"
#include "memory_pool.hpp"

#if defined (_MSC_VER) // Visual studio
#define thread_local __declspec( thread )
#elif defined (__GCC__) // GCC
#define thread_local __thread
#endif

namespace Sass {

  // Only use PODs for thread_local
  // Objects get unpredicable init order
  static thread_local MemoryPool* pool;
  static thread_local size_t allocations;

  void deallocateMem(void* ptr, size_t size)
  {
    pool->deallocate(ptr);
    if (--allocations == 0) {
      delete pool;
      pool = nullptr;
    }
    return;
  }

  void* allocateMem(size_t size)
  {
    if (allocations++ == 0) {
      pool = new MemoryPool();
    }
    return pool->allocate(size);
  }

}
