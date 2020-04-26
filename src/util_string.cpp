#include "util_string.hpp"
#include "character.hpp"

#include <cstring>
#include <iostream>
#include <algorithm>

namespace Sass {
  namespace Util {

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
