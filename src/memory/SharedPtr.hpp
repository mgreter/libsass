#ifndef SASS_MEMORY_SHARED_PTR_H
#define SASS_MEMORY_SHARED_PTR_H

#include <vector>

namespace Sass {

#define DBG false
#define MEM true
// #define DEBUG_SHARED_PTR

  class SharedPtr;

  class SharedObj {
  protected:
  friend class SharedPtr;
  friend class Memory_Manager;
    long refcounter;
    bool detached;
  public:
    SharedObj();
    virtual ~SharedObj();
    long getRefCount() {
      return refcounter;
    }
  };


  class SharedPtr {
  private:
    SharedObj* node;
  private:
    inline void decRefCount();
    inline void incRefCount();
  public:
    // the empty constructor
    SharedPtr()
    : node(NULL) {};
    // the create constructor
    SharedPtr(SharedObj* ptr);
    // copy assignment operator
    SharedPtr& operator=(const SharedPtr& rhs);
    // move assignment operator
    SharedPtr& operator=(SharedPtr&& rhs);
    // the copy constructor
    SharedPtr(const SharedPtr& obj);
    // the move constructor
    // SharedPtr(SharedPtr&& obj);
    // destructor
    ~SharedPtr();
  public:
    inline SharedObj* obj () {
      return node;
    };
    inline SharedObj* obj () const {
      return node;
    };
    inline SharedObj* operator-> () {
      return node;
    };
    inline bool isNull () {
      return node == NULL;
    };
    inline bool isNull () const {
      return node == NULL;
    };
    inline SharedObj* detach() {
      node->detached = true;
      return node;
    };
    inline SharedObj* detach() const {
      if (node) {
        node->detached = true;
      }
      return node;
    };
    inline operator bool() {
      return node != NULL;
    };
    inline operator bool() const {
      return node != NULL;
    };

  };

  template < typename T >
  class SharedImpl : private SharedPtr {
  public:
    SharedImpl()
    : SharedPtr(NULL) {};
    SharedImpl(T* node)
    : SharedPtr(node) {};
    SharedImpl(T&& node)
    : SharedPtr(node) {};
    inline SharedImpl(const T& node)
    : SharedPtr(node) {};
    ~SharedImpl() {};
  public:
    inline T* operator& () {
      return static_cast<T*>(this->obj());
    };
    inline T* operator& () const {
      return static_cast<T*>(this->obj());
    };
    inline T& operator* () {
      return *static_cast<T*>(this->obj());
    };
    inline T& operator* () const {
      return *static_cast<T*>(this->obj());
    };
    inline T* operator-> () {
      return static_cast<T*>(this->obj());
    };
    inline T* operator-> () const {
      return static_cast<T*>(this->obj());
    };
    inline T* ptr () {
      return static_cast<T*>(this->obj());
    };
    inline T* survive() {
      if (this->obj() == NULL) return 0;
      return static_cast<T*>(this->detach());
    }
    inline operator bool() {
      return this->obj() != NULL;
    };
    inline operator bool() const {
      return this->obj() != NULL;
    };
  };

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_SHARED_PTR
  #define SASS_MEMORY_COPY(ptr) ptr->copy2(__FILE__, __LINE__)
  #define SASS_MEMORY_CLONE(ptr) ptr->clone2(__FILE__, __LINE__)
  #define SASS_MEMORY_NEW(Class, ...) new Class(__VA_ARGS__, __FILE__, __LINE__)
#else
  #define SASS_MEMORY_COPY(ptr) ptr->copy2()
  #define SASS_MEMORY_CLONE(ptr) ptr->clone2()
  #define SASS_MEMORY_NEW(Class, ...) new Class(__VA_ARGS__)
#endif

#define SASS_MEMORY_RETURN(obj) return obj.survive()
#define SASS_MEMORY_CAST(Class, obj) (dynamic_cast<Class##_Ptr>(&obj))
#define SASS_MEMORY_CAST_PTR(Class, ptr) (dynamic_cast<Class##_Ptr>(ptr))

#endif