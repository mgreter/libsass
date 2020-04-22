#ifndef SASS_AST_HELPERS_H
#define SASS_AST_HELPERS_H

// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"
#include <algorithm>
#include <functional>
#include "util_string.hpp"
#include "string_utils.hpp"

namespace Sass {

  // ###########################################################################
  // ###########################################################################

  // easier to search with name
  const bool DELAYED = true;

  // ToDo: should this really be hardcoded
  // Note: most methods follow precision option
  const double NUMBER_EPSILON = 1e-12;

  // macro to test if numbers are equal within a small error margin
  #define NEAR_EQUAL(lhs, rhs) std::fabs(lhs - rhs) < NUMBER_EPSILON

  // ###########################################################################
  // We define various functions and functors here.
  // Functions satisfy the BinaryPredicate requirement
  // Functors are structs used for e.g. unordered_map
  // ###########################################################################

  // ###########################################################################
  // Implement compare and hashing operations for raw pointers
  // ###########################################################################

  const std::hash<std::size_t> hasher;

  template <class T>
  size_t PtrHashFn(const T* ptr) {
    return ((size_t)ptr) >> 3;
  }

  struct PtrHash {
    template <class T>
    size_t operator() (const T* ptr) const {
      return PtrHashFn(ptr);
    }
  };

  template <class T>
  bool PtrEqualityFn(const T* lhs, const T* rhs) {
    return lhs == rhs; // compare raw pointers
  }

  struct PtrEquality {
    template <class T>
    bool operator() (const T* lhs, const T* rhs) const {
      return PtrEqualityFn<T>(lhs, rhs);
    }
  };

  // ###########################################################################
  // Implement compare and hashing operations for AST Nodes
  // ###########################################################################

  // TODO: get rid of funtions and use ObjEquality<T>

  template <class T>
  // Hash the raw pointer instead of object
  size_t ObjPtrHashFn(const T& obj) {
    return PtrHashFn(obj.ptr());
  }

  struct ObjPtrHash {
    template <class T>
    // Hash the raw pointer instead of object
    size_t operator() (const T& obj) const {
      return ObjPtrHashFn(obj);
    }
  };

  template <class T>
  // Hash the object and its content
  size_t ObjHashFn(const T& obj) {
    return obj ? obj->hash() : 0;
  }

  struct ObjHash {
    template <class T>
    // Hash the object and its content
    size_t operator() (const T& obj) const {
      return ObjHashFn(obj);
    }
  };

  template <class T>
  // Hash the object behind pointer
  size_t PtrObjHashFn(const T* obj) {
    return obj ? obj->hash() : 0;
  }

  struct PtrObjHash {
    template <class T>
    // Hash the object behind pointer
    size_t operator() (const T* obj) const {
      return PtrObjHashFn(obj);
    }
  };

  template <class T>
  // Compare raw pointers to the object
  bool ObjPtrEqualityFn(const T& lhs, const T& rhs) {
    return PtrEqualityFn(lhs.ptr(), rhs.ptr());
  }

  struct ObjPtrEquality {
    template <class T>
    // Compare raw pointers to the object
    bool operator() (const T& lhs, const T& rhs) const {
      return ObjPtrEqualityFn<T>(lhs, rhs);
    }
  };

  template <class T>
  // Compare the objects behind the pointers
  bool PtrObjEqualityFn(const T* lhs, const T* rhs) {
    if (lhs == nullptr) return rhs == nullptr;
    else if (rhs == nullptr) return false;
    else return lhs == rhs || *lhs == *rhs;
  }

  struct PtrObjEquality {
    template <class T>
    // Compare the objects behind the pointers
    bool operator() (const T* lhs, const T* rhs) const {
      return PtrObjEqualityFn<T>(lhs, rhs);
    }
  };

  template <class T>
  // Compare the objects and its contents
  bool ObjEqualityFn(const T& lhs, const T& rhs) {
    return PtrObjEqualityFn(lhs.ptr(), rhs.ptr());
  }

  struct ObjEquality {
    template <class T>
    // Compare the objects and its contents
    bool operator() (const T& lhs, const T& rhs) const {
      return ObjEqualityFn<T>(lhs, rhs);
    }
  };

  // ###########################################################################
  // Implement ordering operations for AST Nodes
  // ###########################################################################

  template <class T>
  // Compare the objects behind pointers
  bool PtrObjLessThanFn(const T* lhs, const T* rhs) {
    if (lhs == nullptr) return rhs != nullptr;
    else if (rhs == nullptr) return false;
    else return lhs != rhs && *lhs < *rhs;
  }

  struct PtrObjLessThan {
    template <class T>
    // Compare the objects behind pointers
    bool operator() (const T* lhs, const T* rhs) const {
      return PtrObjLessThanFn<T>(lhs, rhs);
    }
  };

  template <class T>
  // Compare the objects and its content
  bool ObjLessThanFn(const T& lhs, const T& rhs) {
    return PtrObjLessThanFn(lhs.ptr(), rhs.ptr());
  };

  struct ObjLessThan {
    template <class T>
    // Compare the objects and its content
    bool operator() (const T& lhs, const T& rhs) const {
      return ObjLessThanFn<T>(lhs, rhs);
    }
  };

  // ###########################################################################
  // Some STL helper functions
  // ###########################################################################

  // Check if all elements are equal
  template <class X, class Y,
    typename XT = typename X::value_type,
    typename YT = typename Y::value_type>
  bool ListEquality(const X& lhs, const Y& rhs,
    bool(*cmp)(const XT*, const YT*))
  {
    return lhs.size() == rhs.size() &&
      std::equal(lhs.begin(), lhs.end(),
        rhs.begin(), cmp);
  }

  // Return if Vector is empty
  template <class T>
  bool listIsEmpty(T* cnt) {
    return cnt && cnt->empty();
  }

  // Return if Vector is empty
  template <class T>
  bool listIsInvisible(T* cnt) {
    return cnt && (cnt->empty() || (cnt->is_invisible() && !cnt->isImpossible()));
  }

  // Erase items from vector that match predicate
  template<class T, class UnaryPredicate>
  void listEraseItemIf(T& vec, UnaryPredicate* predicate)
  {
    vec.erase(std::remove_if(vec.begin(), vec.end(), predicate), vec.end());
  }

  // Check that every item in `lhs` is also in `rhs`
  // Note: this works by comparing the raw pointers
  template <typename T>
  bool listIsSubsetOrEqual(const T& lhs, const T& rhs) {
    for (const auto& item : lhs) {
      if (std::find(rhs.begin(), rhs.end(), item) == rhs.end())
        return false;
    }
    return true;
  }

  // ##########################################################################
  // Returns whether [name] is the name of a pseudo-element
  // that can be written with pseudo-class syntax (CSS2 vs CSS3):
  // `:before`, `:after`, `:first-line`, or `:first-letter`
  // ##########################################################################
  inline bool isFakePseudoElement(const sass::string& name)
  {
    return StringUtils::equalsIgnoreCase(name, "after", 5)
      || StringUtils::equalsIgnoreCase(name, "before", 6)
      || StringUtils::equalsIgnoreCase(name, "first-line", 10)
      || StringUtils::equalsIgnoreCase(name, "first-letter", 12);
  }

  // ##########################################################################
  // Names of pseudo selectors that take selectors as arguments,
  // and that are subselectors of their arguments.
  // For example, `.foo` is a superselector of `:matches(.foo)`.
  // ##########################################################################
  inline bool isSubselectorPseudo(const sass::string& norm)
  {
    return StringUtils::equalsIgnoreCase(norm, "any", 3)
      || StringUtils::equalsIgnoreCase(norm, "matches", 7)
      || StringUtils::equalsIgnoreCase(norm, "nth-child", 9)
      || StringUtils::equalsIgnoreCase(norm, "nth-last-child", 14);
  }
  // EO isSubselectorPseudo

  // ###########################################################################
  // Pseudo-class selectors that take unadorned selectors as arguments.
  // ###########################################################################
  inline bool isSelectorPseudoClass(const sass::string& test)
  {
    return StringUtils::equalsIgnoreCase(test, "not", 3)
      || StringUtils::equalsIgnoreCase(test, "matches", 7)
      || StringUtils::equalsIgnoreCase(test, "current", 7)
      || StringUtils::equalsIgnoreCase(test, "any", 3)
      || StringUtils::equalsIgnoreCase(test, "has", 3)
      || StringUtils::equalsIgnoreCase(test, "host", 4)
      || StringUtils::equalsIgnoreCase(test, "host-context", 12);
  }
  // EO isSelectorPseudoClass

  // ###########################################################################
  // Pseudo-element selectors that take unadorned selectors as arguments.
  // ###########################################################################
  inline bool isSelectorPseudoElement(const sass::string& test)
  {
    return StringUtils::equalsIgnoreCase(test, "slotted", 7);
  }
  // EO isSelectorPseudoElement

  // ###########################################################################
  // Pseudo-element selectors that has binominals
  // ###########################################################################
  inline bool isSelectorPseudoBinominal(const sass::string& test)
  {
    return StringUtils::equalsIgnoreCase(test, "nth-child", 11)
      || StringUtils::equalsIgnoreCase(test, "nth-last-child", 14);
  }
  // EO isSelectorPseudoBinominal

  // ###########################################################################
  // ###########################################################################

}

#endif
