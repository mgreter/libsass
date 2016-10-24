#ifndef SASS_MEMORY_MANAGER_H
#define SASS_MEMORY_MANAGER_H

#include <vector>
#include <iostream>

namespace Sass {

  class Memory_Object {
  friend class Memory_Ptr;
  friend class Memory_Manager;
    long refcount;
  public:
    Memory_Object() { refcount = 0; };
    virtual ~Memory_Object() {};
  };


  class Memory_Ptr {
  private:
    Memory_Object* node;
  public:
    // the create constructor
    Memory_Ptr(Memory_Object* node)
    : node(node) {
      if (node) {
        node->refcount += 1;
        // std::cerr << "increase refcount, now at " << node->refcount << "\n";
      }
    };
    // the copy constructor
    Memory_Ptr(const Memory_Ptr& obj)
    : node(obj.node) {
      if (node) {
        node->refcount += 1;
        // std::cerr << "increase refcount, now at " << node->refcount << "\n";
      }
    }
    ~Memory_Ptr() {
      if (node) {
        node->refcount -= 1;
        // std::cerr << "decrease refcount, now at " << node->refcount << "\n";
        if (node->refcount == 1) {
          // delete and remove
        }
      }
    };
  public:
    Memory_Object* obj () {
      return node;
    };
    Memory_Object* obj () const {
      return node;
    };
    Memory_Object* operator-> () {
      return node;
    };
    bool isNull () const {
      return node == NULL;
    };

  };

  template < typename T >
  class Memory_Node : private Memory_Ptr {
  public:
    Memory_Node()
    : Memory_Ptr(NULL) {};
    Memory_Node(T* node)
    : Memory_Ptr(node) {};
    Memory_Node(const T& node)
    : Memory_Ptr(node) {};
    ~Memory_Node() {};
  public:
    T* operator& () {
      // should be save to cast it here
      return static_cast<T*>(this->obj());
    };
    T* operator-> () {
      // should be save to cast it here
      return static_cast<T*>(this->obj());
    };
    T* ptr () {
      // should be save to cast it here
      return static_cast<T*>(this->obj());
    };
    operator bool() const {
      return this->obj() != NULL;
    };
  };

  /////////////////////////////////////////////////////////////////////////////
  // A class for tracking allocations of AST_Node objects. The intended usage
  // is something like: Some_Node* n = new (mem_mgr) Some_Node(...);
  // Then, at the end of the program, the memory manager will delete all of the
  // allocated nodes that have been passed to it.
  // In the future, this class may implement a custom allocator.
  /////////////////////////////////////////////////////////////////////////////
  class Memory_Manager {
    std::vector<Memory_Object*> nodes;

  public:
    Memory_Manager(size_t size = 0);
    ~Memory_Manager();

    bool has(Memory_Object* np);
    Memory_Object* allocate(size_t size);
    void deallocate(Memory_Object* np);
    void remove(Memory_Object* np);
    void destroy(Memory_Object* np);
    Memory_Object* add(Memory_Object* np);

  };
}

///////////////////////////////////////////////////////////////////////////////
// Use macros for the allocation task, since overloading operator `new`
// has been proven to be flaky under certain compilers (see comment below).
///////////////////////////////////////////////////////////////////////////////

#define SASS_MEMORY_NEW(mgr, Class, ...)                                                 \
  (static_cast<Class*>(mgr.add(new (mgr.allocate(sizeof(Class))) Class(__VA_ARGS__))))   \

#define SASS_MEMORY_CREATE(mgr, Class, ...)                                                 \
  (static_cast<Class*>(mgr.add(new (mgr.allocate(sizeof(Class))) Class(__VA_ARGS__))))   \

#define SASS_MEMORY_OBJ(mgr, Class, ...)                                                 \
  Class##_Obj(static_cast<Class*>(mgr.add(new (mgr.allocate(sizeof(Class))) Class(__VA_ARGS__))))   \

#define SASS_MEMORY_CAST(Class, obj)   \
  (dynamic_cast<Class##_Ptr>(& obj))   \

#endif
