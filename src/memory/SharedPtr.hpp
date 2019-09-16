#ifndef SASS_MEMORY_SHARED_PTR_H
#define SASS_MEMORY_SHARED_PTR_H

// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "../sass.hpp"
#include "../allocator.hpp"

#include <cstddef>
#include <iostream>
#include <string>
#include <type_traits>
#include <vector>

#ifdef DEBUG_SHARED_PTR
#include <unordered_set>
#endif

// https://lokiastari.com/blog/2014/12/30/c-plus-plus-by-example-smart-pointer/index.html
// https://lokiastari.com/blog/2015/01/15/c-plus-plus-by-example-smart-pointer-part-ii/index.html
// https://lokiastari.com/blog/2015/01/23/c-plus-plus-by-example-smart-pointer-part-iii/index.html
// https://www.youtube.com/watch?v=LIb3L4vKZ7U - allocator composition (freelist and bucketizer)

namespace Sass {

  class SharedPtr;

  ///////////////////////////////////////////////////////////////////////////////
  // Use macros for the allocation task, since overloading operator `new`
  // has been proven to be flaky under certain compilers (see comment below).
  ///////////////////////////////////////////////////////////////////////////////

  template<typename T, typename... Args>
  inline T* SASS_MEMORY_NEW3(Args ...args) {
    void* buffer = Sass::allocateMem(sizeof(T));
    return new (buffer) T(std::forward<Args>(args)...);
  }

#ifdef DEBUG_SHARED_PTR

    #define SASS_MEMORY_NEW(Class, ...) \
      ((Class*)(SASS_MEMORY_NEW3<Class>(__VA_ARGS__))->trace(__FILE__, __LINE__)) \

    #define SASS_MEMORY_COPY(obj) \
      ((obj)->copy(__FILE__, __LINE__)) \

    #define SASS_MEMORY_CLONE(obj) \
      ((obj)->clone(__FILE__, __LINE__)) \

  #else

    #define SASS_MEMORY_NEW(Class, ...) \
      SASS_MEMORY_NEW3<Class>(__VA_ARGS__) \

    #define SASS_MEMORY_COPY(obj) \
      ((obj)->copy()) \

    #define SASS_MEMORY_CLONE(obj) \
      ((obj)->clone()) \

  #endif

  // SharedObj is the base class for all objects that can be stored as a shared object
  // It adds the reference counter and other values directly to the objects
  // This gives a slight overhead when directly used as a stack object, but has some
  // advantages for our code. It is safe to create two shared pointers from the same
  // objects, as the "control block" is directly attached to it. This would lead
  // to undefined behavior with std::shared_ptr. This also avoids the need to
  // allocate additional control blocks and/or the need to dereference two
  // pointers on each operation. This can be optimized in `std::shared_ptr`
  // too by using `std::make_shared` (where the control block and the actual
  // object are allocated in one continous memory block via one single call).
  class SharedObj {

   public:
    SharedObj() : refcount(0) {
      #ifdef DEBUG_SHARED_PTR
      this->objId = ++objCount;
      if (taint) all.emplace_back(this);
      #endif
    }
    virtual ~SharedObj() {
      #ifdef DEBUG_SHARED_PTR
      for (size_t i = 0; i < all.size(); i++) {
        if (all[i] == this) {
          all.erase(all.begin() + i);
          break;
        }
      }
      erased = true;
      #endif
    }


    #ifdef DEBUG_SHARED_PTR
    static void reportRefCounts() {
      std::cerr << "Max refcount: " <<
        SharedObj::maxRefCount << "\n";
    }
    static void dumpMemLeaks();
    SharedObj* trace(sass::string file, size_t line) {
      this->file = file;
      this->line = line;
      return this;
    }
    sass::string getDbgFile() { return file; }
    size_t getDbgLine() { return line; }
    void setDbg(bool dbg) { this->dbg = dbg; }
    size_t getRefCount() const { return refcount; }
    #endif

    static void setTaint(bool val) { taint = val; }
    virtual sass::string to_string() const = 0;
   protected:
    friend class SharedPtr;
    friend class MemoryPool;
  public:
  protected:
    uint32_t refcount;
  public:
    static bool taint;
#ifdef DEBUG_SHARED_PTR
    static size_t maxRefCount;
    sass::string file;
    size_t line;
  public:
    size_t objId;
  protected:
    bool dbg = false;
    bool erased = false;
    static size_t objCount;
    static sass::vector<SharedObj*> all;
    static std::unordered_set<size_t> deleted;
#endif
  };

  // SharedPtr is a intermediate (template-less) base class for SharedImpl.
  // ToDo: there should be a way to include this in SharedImpl and to get
  // ToDo: rid of all the static_cast that are now needed in SharedImpl.
  class SharedPtr {

  private:

    static const uint32_t SET_DETACHED_BITMASK = (uint32_t(1) << (sizeof(uint32_t) * 8 - 1));
    static const uint32_t UNSET_DETACHED_BITMASK = ~(uint32_t(1) << (sizeof(uint32_t) * 8 - 1));

  public:
    SharedPtr() : node(nullptr) {}
    SharedPtr(SharedObj* ptr) : node(ptr) { incRefCount(); }
    SharedPtr(const SharedPtr& obj) : SharedPtr(obj.node) {}
    SharedPtr(SharedPtr&& obj) : node(std::move(obj.node)) {
      obj.node = nullptr; // reset old node pointer
    }
    ~SharedPtr() {
      decRefCount();
    }

    SharedPtr& operator=(SharedObj* other_node) {
      if (node != other_node) {
        if (node) decRefCount();
        node = other_node;
        incRefCount();
      } else if (node != nullptr) {
        node->refcount &= UNSET_DETACHED_BITMASK;
      }
      return *this;
    }

    SharedPtr& operator=(const SharedPtr& obj) {
      return *this = obj.node;
    }

    void makeWeak() {
      // detach();
      decRefCount();
    }

    // Prevents all SharedPtrs from freeing this node until it is assigned to another SharedPtr.
    SharedObj* detach() {
      if (node != nullptr) {
        node->refcount |= SET_DETACHED_BITMASK;
      }
      #ifdef DEBUG_SHARED_PTR
      if (node && node->dbg) {
        std::cerr << "DETACHING NODE\n";
      }
      #endif 
      return node;
    }

    void clear() {
      if (node != nullptr) {
        decRefCount();
        node = nullptr;
      }
    }

    SharedObj* obj() const {
      #ifdef DEBUG_SHARED_PTR
      if (node && node->deleted.count(node->objId) == 1) {
        std::cerr << "ACCESSING DELETED " << node << "\n";
      }
      #endif
      return node;
    }
    SharedObj* operator->() const {
      #ifdef DEBUG_SHARED_PTR
      if (node && node->deleted.count(node->objId) == 1) {
        std::cerr << "ACCESSING DELETED " << node << "\n";
      }
      #endif
      return node;
    }
    bool isNull() const { return node == nullptr; }
    operator bool() const { return node != nullptr; }
    // Returns whether the caller is the only holder of this item
    bool isShared() const { return node && node->refcount > 1; }

   protected:
    SharedObj* node;
    // ##__declspec(noinline)
    inline void decRefCount() {
      if (node == nullptr) return;
      --node->refcount;
      #ifdef DEBUG_SHARED_PTR
      if (node->dbg) {
        std::cerr << "- " << node << " X " << ((node->refcount & SET_DETACHED_BITMASK) ? "detached " : "")
          << (node->refcount - (node->refcount  & SET_DETACHED_BITMASK)) << " (" << this << ") " << "\n";
      }
      #endif
      if (node->refcount == 0) {
        #ifdef DEBUG_SHARED_PTR
          if (node->dbg) {
            std::cerr << "DELETE NODE " << node << "\n";
          }
          // node->deleted.insert(node->objId);
        #endif
          node->~SharedObj();
          Sass::deallocateMem((void*)node, 1);
      }
      #ifdef DEBUG_SHARED_PTR
      else if (node->refcount & SET_DETACHED_BITMASK) {
        if (node->dbg) {
          std::cerr << "NODE EVAEDED DELETE " << node << "\n";
        }
      }
      #endif
    }
    void incRefCount() {
      if (node == nullptr) return;
      node->refcount &= UNSET_DETACHED_BITMASK;
      ++node->refcount;
      #ifdef DEBUG_SHARED_PTR
      if (SharedObj::maxRefCount < node->refcount) {
        SharedObj::maxRefCount = node->refcount;
      }
      if (node->dbg) {
        std::cerr << "+ " << node << " X " << node->refcount << " (" << this << ") " << "\n";
      }
      #endif
    }
  };

  template <class T>
  class SharedImpl : private SharedPtr {

  public:
    SharedImpl() : SharedPtr(nullptr) {}

    template <class U>
    SharedImpl(U* node) :
      SharedPtr(static_cast<T*>(node)) {}

    template <class U>
    SharedImpl(const SharedImpl<U>& impl) :
      SharedImpl(impl.ptr()) {}

    template <class U>
    SharedImpl<T>& operator=(U *rhs) {
      return static_cast<SharedImpl<T>&>(
        SharedPtr::operator=(static_cast<T*>(rhs)));
    }

    template <class U>
    SharedImpl<T>& operator=(const SharedImpl<U>& rhs) {
      return static_cast<SharedImpl<T>&>(
        SharedPtr::operator=(static_cast<const SharedImpl<T>&>(rhs)));
    }

    operator sass::string() const {
      if (node) return node->to_string();
      return "null";
    }

    using SharedPtr::isNull;
    using SharedPtr::operator bool;
    operator T*() const { return static_cast<T*>(this->obj()); }
    operator T&() const { return *static_cast<T*>(this->obj()); }
    T& operator* () const { return *static_cast<T*>(this->obj()); };
    T* operator-> () const { return static_cast<T*>(this->obj()); };
    T* ptr () const { return static_cast<T*>(this->obj()); };
    T* detach() { return static_cast<T*>(SharedPtr::detach()); }
    void clear() { return SharedPtr::clear(); }
    bool isShared() { return SharedPtr::isShared(); }
    void makeWeak() { return SharedPtr::makeWeak(); }
  };

  // Comparison operators, based on:
  // https://en.cppreference.com/w/cpp/memory/unique_ptr/operator_cmp

  template<class T1, class T2>
  bool operator==(const SharedImpl<T1>& x, const SharedImpl<T2>& y) {
    return x.ptr() == y.ptr();
  }

  template<class T1, class T2>
  bool operator!=(const SharedImpl<T1>& x, const SharedImpl<T2>& y) {
    return x.ptr() != y.ptr();
  }

  template<class T1, class T2>
  bool operator<(const SharedImpl<T1>& x, const SharedImpl<T2>& y) {
    using CT = typename std::common_type<T1*, T2*>::type;
    return std::less<CT>()(x.get(), y.get());
  }

  template<class T1, class T2>
  bool operator<=(const SharedImpl<T1>& x, const SharedImpl<T2>& y) {
    return !(y < x);
  }

  template<class T1, class T2>
  bool operator>(const SharedImpl<T1>& x, const SharedImpl<T2>& y) {
    return y < x;
  }

  template<class T1, class T2>
  bool operator>=(const SharedImpl<T1>& x, const SharedImpl<T2>& y) {
    return !(x < y);
  }

  template <class T>
  bool operator==(const SharedImpl<T>& x, std::nullptr_t) noexcept {
    return x.isNull();
  }

  template <class T>
  bool operator==(std::nullptr_t, const SharedImpl<T>& x) noexcept {
    return x.isNull();
  }

  template <class T>
  bool operator!=(const SharedImpl<T>& x, std::nullptr_t) noexcept {
    return !x.isNull();
  }

  template <class T>
  bool operator!=(std::nullptr_t, const SharedImpl<T>& x) noexcept {
    return !x.isNull();
  }

  template <class T>
  bool operator<(const SharedImpl<T>& x, std::nullptr_t) {
    return std::less<T*>()(x.get(), nullptr);
  }

  template <class T>
  bool operator<(std::nullptr_t, const SharedImpl<T>& y) {
    return std::less<T*>()(nullptr, y.get());
  }

  template <class T>
  bool operator<=(const SharedImpl<T>& x, std::nullptr_t) {
    return !(nullptr < x);
  }

  template <class T>
  bool operator<=(std::nullptr_t, const SharedImpl<T>& y) {
    return !(y < nullptr);
  }

  template <class T>
  bool operator>(const SharedImpl<T>& x, std::nullptr_t) {
    return nullptr < x;
  }

  template <class T>
  bool operator>(std::nullptr_t, const SharedImpl<T>& y) {
    return y < nullptr;
  }

  template <class T>
  bool operator>=(const SharedImpl<T>& x, std::nullptr_t) {
    return !(x < nullptr);
  }

  template <class T>
  bool operator>=(std::nullptr_t, const SharedImpl<T>& y) {
    return !(nullptr < y);
  }

}  // namespace Sass

#endif
