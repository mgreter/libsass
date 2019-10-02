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

    // It seems thread_local variable might be discharged!?
    // But the destructors of e.g. static strings is still
    // called, although their memory was discharged too.
    // Fine with me as long as address sanitizer is happy.
    if (pool == nullptr) { return; }
    if (allocations == 0) { return; }

    pool->deallocate(ptr);
    // ToDo: change to empty()
    if (--allocations == 0) {
      delete pool;
      pool = nullptr;
    }
    return;
  }

  void* allocateMem(size_t size)
  {
    if (pool == nullptr) {
      pool = new MemoryPool();
    }
    allocations++;
    return pool->allocate(size);
  }

}
