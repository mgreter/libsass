#ifndef SASS_MEMORY_MANAGER_H
#define SASS_MEMORY_MANAGER_H

#include <vector>
#include <iostream>

namespace Sass {

  /////////////////////////////////////////////////////////////////////////////
  // A class for tracking allocations of AST_Node objects. The intended usage
  // is something like: Some_Node* n = new (mem_mgr) Some_Node(...);
  // Then, at the end of the program, the memory manager will delete all of the
  // allocated nodes that have been passed to it.
  // In the future, this class may implement a custom allocator.
  /////////////////////////////////////////////////////////////////////////////
  class Memory_Manager {
    std::vector<SharedObj*> nodes;

  public:
    Memory_Manager(size_t size = 0);
    ~Memory_Manager();

    bool has(SharedObj* np);
    SharedObj* allocate(size_t size);
    void deallocate(SharedObj* np);
    void remove(SharedObj* np);
    void destroy(SharedObj* np);
    SharedObj* add(SharedObj* np, std::string file = "", size_t line = std::string::npos);

  };
}

#endif
