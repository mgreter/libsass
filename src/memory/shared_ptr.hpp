#ifndef SASS_MEMORY_SHARED_PTR_HPP
#define SASS_MEMORY_SHARED_PTR_HPP

// sass.hpp must go before all system headers
// to get the __EXTENSIONS__ fix on Solaris.
#include "../capi_sass.hpp"
#include "allocator.hpp"

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

  template <class T>
  class SharedPtr;

  ///////////////////////////////////////////////////////////////////////////
  // Use macros for the allocation task, since overloading operator `new`
  // has been proven to be flaky under certain compilers. This allows us
  // to track memory issues better by adding the source location for every
  // memory allocation, so once it leaks we know where it was created.
  ///////////////////////////////////////////////////////////////////////////

  #ifdef DEBUG_SHARED_PTR

    // Macro to call the constructor
    // Attach file/line in debug mode
    #define SASS_MEMORY_NEW(Class, ...) \
      ((Class*)(new Class(__VA_ARGS__))->trace(__FILE__, __LINE__)) \

    // Macro to call the constructor
    // Attach file/line in debug mode
    #define SASS_MEMORY_NEW_DBG(Class, ...) \
      ((Class*)(new Class(__VA_ARGS__))->trace(file, line)) \

    // Macro to call the constructor
    // Attach file/line in debug mode
    #define SASS_MEMORY_NEW(Class, ...) \
      ((Class*)(new Class(__VA_ARGS__))->trace(__FILE__, __LINE__)) \

    // Copy object and zero children
    // The children array is empty
    #define SASS_MEMORY_RESECT(obj) \
      ((obj)->copy(__FILE__, __LINE__, true)) \

    // Full copy but same children
    // Children are the same reference
    #define SASS_MEMORY_COPY(obj) \
      ((obj)->copy(__FILE__, __LINE__, false)) \

  #else

    // Macro to call the constructor
    // Attach file/line in debug mode
    #define SASS_MEMORY_NEW(Class, ...) \
      new Class(__VA_ARGS__) \

    // Macro to call the constructor
    // Attach file/line in debug mode
    #define SASS_MEMORY_NEW_DBG(Class, ...) \
      new Class(__VA_ARGS__) \

    // Copy object and zero children
    // The children array is empty
    #define SASS_MEMORY_RESECT(obj) \
      ((obj)->copy(true)) \

    // Full copy but same children
    // Children are the same reference
    #define SASS_MEMORY_COPY(obj) \
      ((obj)->copy(false)) \

  #endif

  // RefCounted is the base class for all objects that can be stored as a shared object
  // It adds the reference counter and other values directly to the objects
  // This gives a slight overhead when directly used as a stack object, but has some
  // advantages for our code. It is safe to create two shared pointers from the same
  // objects, as the "control block" is directly attached to it. This would lead
  // to undefined behavior with std::shared_ptr. This also avoids the need to
  // allocate additional control blocks and/or the need to dereference two
  // pointers on each operation. This can be optimized in `std::shared_ptr`
  // too by using `std::make_shared` (where the control block and the actual
  // object are allocated in one continuous memory block via one single call).
  class RefCounted {

   public:
    RefCounted() : refcount(0) {
      #ifdef DEBUG_SHARED_PTR
      this->objId = ++objCount;
      if (taint) {
        all.emplace_back(this);
      }
      #endif
    }
    virtual ~RefCounted() {
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
        RefCounted::maxRefCount << "\n";
    }
    static void dumpMemLeaks();
    RefCounted* trace(sass::string file, size_t line) {
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

    #ifdef SASS_CUSTOM_ALLOCATOR
    inline void* operator new(size_t nbytes) {
      return allocateMem(nbytes);
    }
    inline void operator delete(void* ptr) {
      return deallocateMem(ptr);
    }
    #endif

   protected:
    friend class MemoryPool;
  public:
  public:
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
    static sass::vector<RefCounted*> all;
    static std::unordered_set<size_t> deleted;
#endif
  };

  // SharedPtr is a intermediate (template-less) base class for SharedPtr.
  // ToDo: there should be a way to include this in SharedPtr and to get
  // ToDo: rid of all the static_cast that are now needed in SharedPtr.
  template <class T>
  class SharedPtr {

  protected:

    RefCounted* node;

  private:

    static const uint32_t SET_DETACHED_BITMASK = (uint32_t(1) << (sizeof(uint32_t) * 8 - 1));
    static const uint32_t UNSET_DETACHED_BITMASK = ~(uint32_t(1) << (sizeof(uint32_t) * 8 - 1));

  public:
    SharedPtr() : node(nullptr) {}
    SharedPtr(RefCounted* ptr) : node(ptr) { incRefCount(); }
    SharedPtr(const SharedPtr<T>& obj) : SharedPtr(obj.node) {}
    SharedPtr(SharedPtr<T>&& obj) noexcept : node(std::move(obj.node)) {
      obj.node = nullptr; // reset old node pointer
    }
    virtual ~SharedPtr() {
      decRefCount();
    }

    SharedPtr<T>& operator=(RefCounted* other_node)
    {
      if (node != other_node) {
        if (node) decRefCount();
        node = other_node;
        incRefCount();
      } else if (node != nullptr) {
        node->refcount &= UNSET_DETACHED_BITMASK;
      }
      return *this;
    }

    SharedPtr<T>& operator=(SharedPtr<T>&& obj) noexcept
    {
      if (node != obj.node) {
        if (node) decRefCount();
        node = obj.node;
        obj.node = nullptr;
      }
      else if (node != nullptr) {
        node->refcount &= UNSET_DETACHED_BITMASK;
      }
      return *this;
    }

    SharedPtr<T>& operator=(const SharedPtr<T>& obj)
    {
      return *this = obj.node;
    }

    // Prevents all SharedPtrs from freeing this node until it is assigned to another SharedPtr.
    T* detach() {
      if (node != nullptr) {
        node->refcount |= SET_DETACHED_BITMASK;
      }
      #ifdef DEBUG_SHARED_PTR
      if (node && node->dbg) {
        std::cerr << "DETACHING NODE\n";
      }
      #endif
      return static_cast<T*>(node);
    }

    void clear() {
      if (node != nullptr) {
        decRefCount();
        node = nullptr;
      }
    }

    T* ptr() const {
      #ifdef DEBUG_SHARED_PTR
      if (node && node->deleted.count(node->objId) == 1) {
        std::cerr << "ACCESSING DELETED " << node << "\n";
      }
      #endif
      return static_cast<T*>(node);
    }

    T* operator->() const {
      #ifdef DEBUG_SHARED_PTR
      if (node && node->deleted.count(node->objId) == 1) {
        std::cerr << "ACCESSING DELETED " << node << "\n";
      }
      #endif
      return static_cast<T*>(node);
    }

    operator T* () const { return ptr(); }
    operator T& () const { return *ptr(); }
    T& operator* () const { return *ptr(); };

    bool isNull() const { return node == nullptr; }
    operator bool() const { return node != nullptr; }

  protected:

    // ##__declspec(noinline)
    inline void decRefCount() noexcept {
      if (node == nullptr) return;
      #ifdef DEBUG_SHARED_PTR
      if (node->dbg) {
        std::cerr << "- " << node << " X " << ((node->refcount & SET_DETACHED_BITMASK) ? "detached " : "")
          << (node->refcount - (node->refcount  & SET_DETACHED_BITMASK)) << " (" << this << ") " << "\n";
      }
      #endif
      if (--node->refcount == 0) {
        #ifdef DEBUG_SHARED_PTR
          if (node->dbg) {
            std::cerr << "DELETE NODE " << node << "\n";
          }
          // node->deleted.insert(node->objId);
        #endif
        delete node;
      }
      #ifdef DEBUG_SHARED_PTR
      else if (node->refcount & SET_DETACHED_BITMASK) {
        if (node->dbg) {
          std::cerr << "NODE EVADED DELETE " << node << "\n";
        }
      }
      #endif
    }
    void incRefCount() noexcept {
      if (node == nullptr) return;
      node->refcount &= UNSET_DETACHED_BITMASK;
      ++node->refcount;
      #ifdef DEBUG_SHARED_PTR
      if (RefCounted::maxRefCount < node->refcount) {
        RefCounted::maxRefCount = node->refcount;
      }
      if (node->dbg) {
        std::cerr << "+ " << node << " X " << node->refcount << " (" << this << ") " << "\n";
      }
      #endif
    }

    // template <class U>
    // SharedPtr<T>& operator=(U *rhs) {
    //   return static_cast<SharedPtr<T>&>(
    //     SharedPtr<T>::operator=(static_cast<T*>(rhs)));
    // }
    // 
    // template <class U>
    // SharedPtr<T>& operator=(SharedPtr<U>&& rhs) {
    //   return static_cast<SharedPtr<T>&>(
    //     SharedPtr<T>::operator=(std::move(static_cast<SharedPtr<T>&>(rhs))));
    // }
    // 
    // template <class U>
    // SharedPtr<T>& operator=(const SharedPtr<U>& rhs) {
    //   return static_cast<SharedPtr<T>&>(
    //     SharedPtr<T>::operator=(static_cast<const SharedPtr<T>&>(rhs)));
    // }

  };

  // Comparison operators, based on:
  // https://en.cppreference.com/w/cpp/memory/unique_ptr/operator_cmp

  template<class T1, class T2>
  bool operator==(const SharedPtr<T1>& x, const SharedPtr<T2>& y) {
    return x.ptr() == y.ptr();
  }

  template<class T1, class T2>
  bool operator!=(const SharedPtr<T1>& x, const SharedPtr<T2>& y) {
    return x.ptr() != y.ptr();
  }

  template<class T1, class T2>
  bool operator<(const SharedPtr<T1>& x, const SharedPtr<T2>& y) {
    using CT = typename std::common_type<T1*, T2*>::type;
    return std::less<CT>()(x.get(), y.get());
  }

  template<class T1, class T2>
  bool operator<=(const SharedPtr<T1>& x, const SharedPtr<T2>& y) {
    return !(y < x);
  }

  template<class T1, class T2>
  bool operator>(const SharedPtr<T1>& x, const SharedPtr<T2>& y) {
    return y < x;
  }

  template<class T1, class T2>
  bool operator>=(const SharedPtr<T1>& x, const SharedPtr<T2>& y) {
    return !(x < y);
  }

  template <class T>
  bool operator==(const SharedPtr<T>& x, std::nullptr_t) noexcept {
    return x.isNull();
  }

  template <class T>
  bool operator==(std::nullptr_t, const SharedPtr<T>& x) noexcept {
    return x.isNull();
  }

  template <class T>
  bool operator!=(const SharedPtr<T>& x, std::nullptr_t) noexcept {
    return !x.isNull();
  }

  template <class T>
  bool operator!=(std::nullptr_t, const SharedPtr<T>& x) noexcept {
    return !x.isNull();
  }

  template <class T>
  bool operator<(const SharedPtr<T>& x, std::nullptr_t) {
    return std::less<T*>()(x.get(), nullptr);
  }

  template <class T>
  bool operator<(std::nullptr_t, const SharedPtr<T>& y) {
    return std::less<T*>()(nullptr, y.get());
  }

  template <class T>
  bool operator<=(const SharedPtr<T>& x, std::nullptr_t) {
    return !(nullptr < x);
  }

  template <class T>
  bool operator<=(std::nullptr_t, const SharedPtr<T>& y) {
    return !(y < nullptr);
  }

  template <class T>
  bool operator>(const SharedPtr<T>& x, std::nullptr_t) {
    return nullptr < x;
  }

  template <class T>
  bool operator>(std::nullptr_t, const SharedPtr<T>& y) {
    return y < nullptr;
  }

  template <class T>
  bool operator>=(const SharedPtr<T>& x, std::nullptr_t) {
    return !(x < nullptr);
  }

  template <class T>
  bool operator>=(std::nullptr_t, const SharedPtr<T>& y) {
    return !(nullptr < y);
  }

}  // namespace Sass

#endif
