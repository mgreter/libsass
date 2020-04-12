#include "util_string.hpp"
#include "character.hpp"

#include <cstring>
#include <iostream>
#include <algorithm>

namespace Sass {
  namespace Util {


  bool vendorMatch(const char* lit, const sass::string& test)
  {
    const char* str = test.c_str();
    // Must begin with exactly one dash, otherwise compare fully
    if (str[0] == '-' && str[1] != '-') {
      // Search next dash to finish the vendor prefix
      for (size_t pos = 2; str[pos] != 0; pos += 1) {
        if (str[pos] != '-') continue; // not found yet
        return std::strcmp(str + pos + 1, lit) == 0;
      }
    }
    return std::strcmp(str, lit) == 0;
  }

    // ###########################################################################
    // Returns [name] without a vendor prefix.
    // If [name] has no vendor prefix, it's returned as-is.
    // ###########################################################################
    sass::string unvendor(const sass::string& name)
    {
      if (name.size() < 2) return name;
      if (name[0] != '-') return name;
      if (name[1] == '-') return name;
      for (size_t i = 2; i < name.size(); i++) {
        if (name[i] == '-') return name.substr(i + 1);
      }
      return name;
    }
    // EO unvendor

  }
  // namespace Util

}
// namespace Sass
