#include "util_string.hpp"
#include "character.hpp"

#include <iostream>
#include <algorithm>

namespace Sass {
  namespace Util {

    // ##########################################################################
    // Special case insensitive string matcher. We can optimize
    // the more general compare case quite a bit by requiring
    // consumers to obey some rules (lowercase and no space).
    // - `literal` must only contain lower case ascii characters
    // there is one edge case where this could give false positives
    // test could contain a (non-ascii) char exactly 32 below literal
    // ##########################################################################
    bool equalsLiteral(const char* lit, const std::string& test) {
      // Work directly on characters
      const char* src = test.c_str();
      // There is a small chance that the search string
      // Is longer than the rest of the string to look at
      while (*lit && (*src == *lit || *src + 32 == *lit)) {
        ++src, ++lit;
      }
      // True if literal is at end
      // If not test was too long
      return *lit == 0;
    }

    // returns a lowercase copy
    std::string ascii_str_tolower(std::string s) {
      ascii_str_tolower(&s);
      return s;
    }

    // operates directly on the passed string
    void ascii_str_tolower(std::string* s) {
      for (auto& ch : *s) {
        ch = ascii_tolower(static_cast<unsigned char>(ch));
      }
    }

    // returns an uppercase copy
    std::string ascii_str_toupper(std::string s) {
      ascii_str_toupper(&s);
      return s;
    }

    // operates directly on the passed string
    void ascii_str_toupper(std::string* s) {
      for (auto& ch : *s) {
        ch = ascii_toupper(static_cast<unsigned char>(ch));
      }
    }

	  void ascii_str_rtrim(std::string& s)
	  {
      auto it = std::find_if_not(s.rbegin(), s.rend(), Character::isWhitespace);
      s.erase(s.rend() - it);
    }

	  void ascii_str_ltrim(std::string& s)
	  {
      if (s.begin() == s.end()) return;
      auto it = std::find_if_not(s.begin(), s.end(), Character::isWhitespace);
      s.erase(s.begin(), it);
    }

	  void ascii_str_trim(std::string& s)
	  {
      ascii_str_ltrim(s);
      ascii_str_rtrim(s);
    }

    // split string by delimiter
    std::vector<std::string> split_string(std::string str, char delimiter, bool trimSpace)
    {
      std::vector<std::string> paths;
      if (str.empty()) return paths;
      // find delimiter via prelexer (return zero at end)
      size_t start = 0, end = str.find_first_of(delimiter);
      // search until null delimiter
      while (end != std::string::npos) {
        // add substring from current position to delimiter
        paths.push_back(str.substr(start, end - start));
        start = end + 1; // skip delimiter
        end = str.find_first_of(delimiter, start);
      }
      // add substring from current position to end
      paths.push_back(str.substr(start, end - start));
      // return back
      return paths;
    }


    // ###########################################################################
    // Returns [name] without a vendor prefix.
    // If [name] has no vendor prefix, it's returned as-is.
    // ###########################################################################
    std::string unvendor(const std::string& name)
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
