#ifndef SASS_ENVIRONMENT_H
#define SASS_ENVIRONMENT_H

// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include <map>
#include <string>
#include "ast_fwd_decl.hpp"
#include "ast_def_macros.hpp"
#include "ordered_map.hpp"
#include <unordered_map>
#include <unordered_set>
#include "allocator.hpp"

/* OPTIMIZATIONS */

// Each scope that opens a new env should have already an env populated
// Basically it should consist of all hoisted variables
// We want to pre-compile assignments and variable access
// This should probably be fully possible in parse step


// #include "../../parallel-hashmap/parallel_hashmap/phmap.h"

// #include "../../hopscotch-map/include/tsl/hopscotch_map.h"
// #include "../../hopscotch-map/include/tsl/hopscotch_set.h"
#include "ordered-map/ordered_map.h"
// #include "../../ordered-map/include/tsl/ordered_set.h"
// #include "robin_hood.hpp"

namespace std {
  template <>
  struct hash<Sass::EnvString> {
  public:
    inline size_t operator()(const Sass::EnvString& name) const
    {
      return name.hash();
    }
  };
};

namespace Sass {

  // tsl::hopscotch_map

  template<class T>
  // An unordered map seems faster than hopscotch
  using NormalizedMap = std::unordered_map<
    EnvString, T,
    SassIgnoreDashPair,
    SassAllocator<std::pair<const EnvString, T>>
  >;

  template<class T>
  using NormalizedOMap = tsl::ordered_map<
    EnvString, T,
    SassIgnoreDashPair,
    SassAllocator<std::pair<EnvString, T>>
  >;

  template<class T>
  using KeywordMapOlder = tsl::ordered_map<
    EnvString, T,
    SassIgnoreDashPair,
    SassAllocator<std::pair<EnvString, T>>,
    sass::vector<std::pair<EnvString, T>>
  >;

  using NormalizeSet = std::unordered_set<
    EnvString,
    SassIgnoreDashPair,
    SassAllocator<EnvString>
  >;

  template <typename T>
  class KeywordMapFast {

    using TYPE = sass::vector<std::pair<EnvString, T>>;

    sass::vector<std::pair<EnvString, T>> items;

  public:

    using iterator = typename TYPE::iterator;
    using const_iterator = typename TYPE::const_iterator;
    using reverse_iterator = typename TYPE::reverse_iterator;
    using const_reverse_iterator = typename TYPE::const_reverse_iterator;

    size_t size() const { return items.size(); }

    void clear() {
      items.clear();
    }

    size_t count(const EnvString& key) const {
      const_iterator cur = items.begin();
      const_iterator end = items.end();
      while (cur != end) {
        if (cur->first.norm() == key.norm()) {
          return true;
        }
        cur++;
      }
      return false;
    }

    void erase(const EnvString& key) {
      const_iterator cur = items.begin();
      const_iterator end = items.end();
      while (cur != end) {
        if (cur->first.norm() == key.norm()) {
          items.erase(cur);
          return;
        }
        cur++;
      }
    }

    bool empty() const {
      return items.empty();
    }

    void erase(iterator it) {
      items.erase(it);
    }

    const_iterator find(const EnvString& key) const {
      const_iterator cur = items.begin();
      const_iterator end = items.end();
      while (cur != end) {
        if (cur->first.norm() == key.norm()) {
          return cur;
        }
        cur++;
      }
      return end;
    }

    T& operator[](const EnvString& key)
    {
      iterator cur = items.begin();
      iterator end = items.end();
      while (cur != end) {
        if (cur->first.norm() == key.norm()) {
          return cur->second;
        }
        cur++;
      }
      // Append empty object
      items.push_back(
        std::make_pair(
          key, T{}));
      // Return newly added
      return items.back().second;
    }

    void reserve(size_t size) {
      items.reserve(size);
    }

    const T& at(const EnvString& key) const
    {
      const_iterator cur = items.begin();
      const_iterator end = items.end();
      while (cur != end) {
        if (cur->first.norm() == key.norm()) {
          return cur->second;
        }
        cur++;
      }
      // return newly added
      throw std::runtime_error("not found");
    }

    iterator find(const EnvString& key) {
      iterator cur = items.begin();
      iterator end = items.end();
      while (cur != end) {
        if (cur->first.norm() == key.norm()) {
          return cur;
        }
        cur++;
      }
      return end;
    }

    const_iterator end() const { return items.end(); }
    const_iterator begin() const { return items.begin(); }

  };

  template <typename T>
  using KeywordMap = KeywordMapFast<T>;

  sass::string pluralize(sass::string singular, size_t size, sass::string plural = "");

  sass::string toSentence(KeywordMap<ValueObj>& names, const sass::string& conjunction = "and");

}

#endif
