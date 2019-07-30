#ifndef SASS_UTIL_STRING_H
#define SASS_UTIL_STRING_H

#include "sass.hpp"
#include <algorithm>
#include <vector>
#include <iterator>
#include <sstream>

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

  namespace Util {

    // ##########################################################################
    // Special case insensitive string matcher. We can optimize
    // the more general compare case quite a bit by requiring
    // consumers to obey some rules (lowercase and no space).
    // - `literal` must only contain lower case ascii characters
    // there is one edge case where this could give false positives:
    // test could contain a (non-ascii) char exactly 32 below literal
    // ##########################################################################
    bool equalsLiteral(const char* lit, const std::string& test);

    // ###########################################################################
    // Returns [name] without a vendor prefix.
    // If [name] has no vendor prefix, it's returned as-is.
    // ###########################################################################
    std::string unvendor(const std::string& name);

    // Locale-independent ASCII character routines.

    inline bool ascii_isalpha(unsigned char c) {
      return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    }

    inline bool ascii_isdigit(unsigned char c) {
      return (c >= '0' && c <= '9');
    }

    inline bool ascii_isalnum(unsigned char c) {
      return ascii_isalpha(c) || ascii_isdigit(c);
    }

    inline bool ascii_isascii(unsigned char c) { return c < 128; }

    inline bool ascii_isxdigit(unsigned char c) {
      return ascii_isdigit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
    }

    inline bool ascii_iswhitespace(unsigned char c) {
      return c == ' ' || c == '\t' || c == '\v' || c == '\f';
    }

    inline bool ascii_islinefeed(unsigned char c) {
      return c == '\r' || c == '\n';
    }

    inline bool ascii_isspace(unsigned char c) {
      return ascii_iswhitespace(c) || ascii_islinefeed(c);
    }

    inline char ascii_tolower(unsigned char c) {
      if (c >= 'A' && c <= 'Z') return c + 32;
      return c;
    }

    void ascii_str_tolower(std::string* s);
    std::string ascii_str_tolower(std::string s);

    inline char ascii_toupper(unsigned char c) {
      if (c >= 'a' && c <= 'z') return c - 32;
      return c;
    }

    void ascii_str_toupper(std::string* s);
    std::string ascii_str_toupper(std::string s);

    void ascii_str_rtrim(std::string& s);
    void ascii_str_ltrim(std::string& s);
    void ascii_str_trim(std::string& s);

    inline bool ascii_str_ends_with(const std::string& text, const std::string& trail)
    {
      // text must be bigger than check
      if (text.size() < trail.size()) {
        return false;
      }
      auto cur = text.end();
      auto chk = trail.end();
      while (chk > trail.begin()) {
        chk -= 1;
        cur -= 1;
        if (*chk != *cur) {
          return false;
        }
      }
      return true;
    }

    inline bool ascii_str_ends_with_insensitive(const std::string& text, const std::string& trail)
    {
      // text must be bigger than check
      if (text.size() < trail.size()) {
        return false;
      }
      auto cur = text.end();
      auto chk = trail.end();
      while (chk > trail.begin()) {
        chk -= 1;
        cur -= 1;
        if (ascii_toupper(*chk) != ascii_toupper(*cur)) {
          return false;
        }
      }
      return true;
    }

    inline bool ascii_str_starts_with_insensitive(const std::string& text, const std::string& prefix)
    {
      // text must be bigger than check
      if (text.size() < prefix.size()) {
        return false;
      }
      auto cur = text.begin();
      auto chk = prefix.begin();
      while (chk < prefix.end()) {
        if (ascii_tolower(*chk) != ascii_tolower(*cur)) {
          return false;
        }
        chk += 1;
        cur += 1;
      }
      return true;
    }

    inline void ascii_normalize_underscore(std::string& text)
    {
      std::replace(text.begin(), text.end(), '_', '-');
    }

    inline bool ascii_str_equals_ignore_separator(const std::string& text, const std::string& test)
    {
      // text must be bigger same size as test
      if (text.size() != test.size()) {
        return false;
      }
      auto cur = text.begin();
      auto chk = test.begin();
      while (chk < test.end()) {
        if (ascii_tolower(*chk) != ascii_tolower(*cur)) {
          if (*chk != '_' && *chk != '-' && *cur != '_' && *cur != '-') {
            return false;
          }
        }
        chk += 1;
        cur += 1;
      }
      return true;
    }

    inline size_t hash_ignore_separator(const std::string& obj) {
      size_t hash = 4603;
      for (const char chr : obj) {
        if (chr == '_' || chr == '-') {
          hash_combine(hash, '-');
        }
        else {
          hash_combine(hash, ascii_tolower(chr));
        }
      }
      return hash;
    }

    struct hashIgnoreSeparator {
      size_t operator() (const std::string& obj) const {
        return hash_ignore_separator(obj);
      }
    };

    struct equalsIgnoreSeparator {
      bool operator() (const std::string& lhs, const std::string& rhs) const {
        return ascii_str_equals_ignore_separator(lhs, rhs);
      }
    };

    // split string by delimiter
    std::vector<std::string> split_string(
      std::string str,
      char delimiter = PATH_SEP,
      bool trimSpace = false);
    // EO split_string

    std::string join_strings(
      std::vector<std::string>& strings,
      const char* const separator);

  }  // namespace Sass
}  // namespace Util
#endif  // SASS_UTIL_STRING_H
