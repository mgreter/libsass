#ifndef SASS_UTIL_STRING_H
#define SASS_UTIL_STRING_H

#include "sass.hpp"
#include "character.hpp"
#include <algorithm>
#include <vector>
#include <bitset>
#include <iterator>
#include <sstream>
#include <iostream>

namespace Sass {

  //////////////////////////////////////////////////////////
  // `hash_combine` comes from boost (functional/hash):
  // http://www.boost.org/doc/libs/1_35_0/doc/html/hash/combine.html
  // Boost Software License - Version 1.0
  // http://www.boost.org/users/license.html
  template <typename T>
  void hash_combine(std::size_t& seed, const T& val)
  {
    seed ^= std::hash<T>()(val) + 0x9e3779b9
      + (seed << 6) + (seed >> 2);
  }
  //////////////////////////////////////////////////////////

  template <typename T>
  void hash_start(std::size_t& seed, const T& val)
  {
    seed = std::hash<T>()(val);
  }

  // Not sure if calling std::hash<size_t> has any overhead!?
  inline void hash_combine(std::size_t& seed, std::size_t val)
  {
    seed ^= val + 0x9e3779b9
      + (seed << 6) + (seed >> 2);
  }

  // Not sure if calling std::hash<size_t> has any overhead!?
  inline void hash_start(std::size_t& seed, std::size_t val)
  {
    seed = val;
  }

  namespace Util {

    // ###########################################################################
    // Returns [name] without a vendor prefix.
    // If [name] has no vendor prefix, it's returned as-is.
    // ###########################################################################
    sass::string unvendor(const sass::string& name);

  }
  // EO namespace Sass

}
// EO namespace Util

#endif  // SASS_UTIL_STRING_H
