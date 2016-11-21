#ifndef SASS_MEMORY_SHARED_PTR_H
#define SASS_MEMORY_SHARED_PTR_H

namespace Sass {

#define DBG false
#define MEM true
#define DEBUG_SHARED_PTR

  class SharedPtr;

  class SharedObject {
  protected:
  friend class SharedPtr;
  friend class Memory_Manager;
    static std::vector<SharedObject*> all;
    std::string allocated;
    size_t line;
    static bool taint;
    long refcounter;
    long refcount;
    bool dbg;
  public:
#ifdef DEBUG_SHARED_PTR
    static void debugEnd();
#endif
    SharedObject();
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
    virtual ~SharedObject();
    long getRefCount() {
      return refcounter;
    }
  };


  class SharedPtr {
  private:
    SharedObject* node;
  private:
    void decRefCount(std::string event);
    void incRefCount(std::string event);
  public:
    // the empty constructor
    SharedPtr()
    : node(NULL) {};
    // the create constructor
    SharedPtr(SharedObject* ptr);
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
    SharedObject* obj () {
      return node;
    };
    SharedObject* obj () const {
      return node;
    };
    SharedObject* operator-> () {
      return node;
    };
    bool isNull () {
      return node == NULL;
    };
    bool isNull () const {
      return node == NULL;
    };
    operator bool() {
      return node != NULL;
    };
    operator bool() const {
      return node != NULL;
    };

  };

  template < typename T >
  class Memory_Node : private SharedPtr {
  public:
    Memory_Node()
    : SharedPtr(NULL) {};
    Memory_Node(T* node)
    : SharedPtr(node) {};
    Memory_Node(T&& node)
    : SharedPtr(node) {};
    Memory_Node(const T& node)
    : SharedPtr(node) {};
    ~Memory_Node() {};
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
    operator bool() {
      return this->obj() != NULL;
    };
    operator bool() const {
      return this->obj() != NULL;
    };
  };

}

#endif