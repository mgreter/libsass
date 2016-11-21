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
    std::vector<SharedObject*> nodes;

  public:
    Memory_Manager(size_t size = 0);
    ~Memory_Manager();

    bool has(SharedObject* np);
    SharedObject* allocate(size_t size);
    void deallocate(SharedObject* np);
    void remove(SharedObject* np);
    void destroy(SharedObject* np);
    SharedObject* add(SharedObject* np, std::string file = "", size_t line = std::string::npos);

  };
}

///////////////////////////////////////////////////////////////////////////////
// Use macros for the allocation task, since overloading operator `new`
// has been proven to be flaky under certain compilers (see comment below).
///////////////////////////////////////////////////////////////////////////////

#define SASS_MEMORY_NEW(mgr, Class, ...)                                                 \
  (static_cast<Class*>(mgr.add(new (mgr.allocate(sizeof(Class))) Class(__VA_ARGS__), __FILE__, __LINE__)))   \

#define SASS_MEMORY_CREATE(mgr, Class, ...)                                                 \
  (static_cast<Class*>(mgr.add(new (mgr.allocate(sizeof(Class))) Class(__VA_ARGS__), __FILE__, __LINE__)))   \

#define SASS_MEMORY_OBJ(mgr, Class, ...)                                                 \
  Class##_Obj(static_cast<Class*>(mgr.add(new (mgr.allocate(sizeof(Class))) Class(__VA_ARGS__), __FILE__, __LINE__)))   \

#define SASS_MEMORY_CAST(Class, obj)   \
  (dynamic_cast<Class##_Ptr>(& obj))   \

#define SASS_MEMORY_CAST_PTR(Class, ptr)   \
  (dynamic_cast<Class##_Ptr>(ptr))   \

#define SASS_MEMORY_COPY(mgr, Class, obj)   \
  (obj->copy2(mgr, __FILE__, __LINE__))   \

#define SASS_MEMORY_CLONE(mgr, Class, obj)   \
  (obj->clone2(mgr, __FILE__, __LINE__))   \

#endif
