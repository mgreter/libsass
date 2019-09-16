#ifndef SASS_ALLOCATOR_H
#define SASS_ALLOCATOR_H

#include <iostream>
#include <cstddef>
#include <memory>
#include <vector>
#include <unordered_map>
#include <scoped_allocator>
#include <vector>
#include <limits>
#include <mutex>
#include <algorithm>

#include "settings.hpp"
#include "randomize.hpp"

#include <functional>
#include "MurmurHash2.hpp"

namespace Sass {

  void deallocateMem(void* ptr, size_t size);
  void* allocateMem(size_t size);
}

// namespace Sass {

  template<typename T>
  struct RemoveConst
  {
    typedef T value_type;
  };

  template<typename T>
  struct RemoveConst<const T>
  {
    typedef T value_type;
  };

  template <class T>
  class Allocator {
  public:
    // type definitions
    typedef RemoveConst<T>              Base;
    typedef typename Base::value_type   value_type;
    typedef value_type* pointer;
    typedef const value_type* const_pointer;
    typedef value_type& reference;
    typedef const value_type& const_reference;
    typedef std::size_t                 size_type;
    typedef std::ptrdiff_t              difference_type;

    // rebind allocator to type U
    template <class U>
    struct rebind {
      typedef Allocator<U> other;
    };

    // return address of values
    pointer address(reference value) const {
      return &value;
    }
    const_pointer address(const_reference value) const {
      return &value;
    }

    /* constructors and destructor
    * - nothing to do because the allocator has no state
    */
    Allocator() throw() {
    }
    Allocator(const Allocator&) throw() {
    }
    template <class U>
    Allocator(const Allocator<U>&) throw() {
    }
    ~Allocator() throw() {
    }

    // return maximum number of elements that can be allocated
    size_type max_size() const throw() {
      return std::numeric_limits<std::size_t>::max() / sizeof(T);
    }

    // allocate but don't initialize num elements of type T
    pointer allocate(size_type num, const void* other = nullptr) {
      if (other != nullptr) std::cerr << "Other not null?\n";
      // std::cerr << "allocate, now at " << (Sass::memPtr / 1024.0 / 1024.0) << "\n";
      return (pointer)(Sass::allocateMem(num * sizeof(T)));

      // std::cerr << "allocated\n";
      return (pointer)malloc(num * sizeof(T));
    }

    // initialize elements of allocated storage p with value value
    void construct(pointer p, const T& value) {
      // initialize memory with placement new
      new((void*)p)T(value);
    }

    // destroy elements of initialized storage p
    void destroy(pointer p) {
      // destroy objects by calling their destructor
      p->~T();
    }

    // deallocate storage p of deleted elements
    void deallocate(pointer p, size_type num) {
      Sass::deallocateMem(p, num);
      // free(p);
    }
  };

// }

#ifdef SASS_CUSTOM_ALLOCATOR
template <typename T> using SassAllocator = Allocator<T>;
#else
template <typename T> using SassAllocator = std::allocator<T>;
#endif

namespace Sass {


  namespace sass {

#ifdef SASS_CUSTOM_ALLOCATOR
    template <typename T> using vector = std::vector<T, SassAllocator<T>>;
    // Create new specialized types from basic string and stream types
    typedef std::basic_string<char, std::char_traits<char>, SassAllocator<char>> string;
    typedef std::basic_stringstream<char, std::char_traits<char>, SassAllocator<char>> sstream;
#else
    typedef std::string string;
    typedef std::stringstream sstream;
#endif

  }

}


// Make sure hash
#ifdef SASS_CUSTOM_ALLOCATOR

template <class T, class U>
bool operator==(const SassAllocator<T>&, const SassAllocator<U>&) { return true; }
template <class T, class U>
bool operator!=(const SassAllocator<T>&, const SassAllocator<U>&) { return false; }

namespace std {

 
  // GCC seems to need this specialization
  template <> class hash<Sass::sass::string> {
  public:
    inline size_t operator()(
      const Sass::sass::string& name) const
    {
      return MurmurHash2(
        (void*)name.c_str(),
        (int)name.size(),
        0xdeadbeef);
      // Not thread safe!
      // Sass::getHashSeed());
    }
  };

};
#endif

#endif
