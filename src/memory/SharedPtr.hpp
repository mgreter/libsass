#ifndef SASS_MEMORY_SHARED_PTR_H
#define SASS_MEMORY_SHARED_PTR_H

#include <vector>

namespace Sass {

#define DBG false
#define MEM true
#define DEBUG_SHARED_PTR

  class SharedPtr;

  class SharedObj {
  protected:
  friend class SharedPtr;
  friend class Memory_Manager;
    static std::vector<SharedObj*> all;
    std::string allocated;
    size_t line;
    static bool taint;
    long refcounter;
    long refcount;
    bool detached;
    bool dbg;
  public:
#ifdef DEBUG_SHARED_PTR
    static void debugEnd();
#endif
    SharedObj();
    std::string getAllocation() {
      return allocated;
    }
    size_t getLine() {
      return line;
    }
  #ifdef DEBUG_SHARED_PTR
    void setDbg(bool val) {
      this->dbg = val;
    }
  #endif
    static void setTaint(bool val) {
      taint = val;
    }
    virtual ~SharedObj();
    long getRefCount() {
      return refcounter;
    }
  };


  class SharedPtr {
  private:
    SharedObj* node;
  private:
    void decRefCount(std::string event);
    void incRefCount(std::string event);
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
    SharedObj* obj () {
      return node;
    };
    SharedObj* obj () const {
      return node;
    };
    SharedObj* operator-> () {
      return node;
    };
    bool isNull () {
      return node == NULL;
    };
    bool isNull () const {
      return node == NULL;
    };
    SharedObj* detach() {
      node->detached = true;
      return node;
    };
    SharedObj* detach() const {
      if (node) {
        node->detached = true;
      }
      return node;
    };
    operator bool() {
      return node != NULL;
    };
    operator bool() const {
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
    SharedImpl(const T& node)
    : SharedPtr(node) {};
    ~SharedImpl() {};
  public:
    T* operator& () {
      return static_cast<T*>(this->obj());
    };
    T* operator& () const {
      return static_cast<T*>(this->obj());
    };
    T& operator* () {
      return *static_cast<T*>(this->obj());
    };
    T& operator* () const {
      return *static_cast<T*>(this->obj());
    };
    T* operator-> () {
      return static_cast<T*>(this->obj());
    };
    T* operator-> () const {
      return static_cast<T*>(this->obj());
    };
    T* ptr () {
      return static_cast<T*>(this->obj());
    };
    T* survive() {
      if (this->obj() == NULL) return 0;
      return static_cast<T*>(this->detach());
    }
    operator bool() {
      return this->obj() != NULL;
    };
    operator bool() const {
      return this->obj() != NULL;
    };
  };

}

#endif