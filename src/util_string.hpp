#ifndef SASS_UTIL_STRING_H
#define SASS_UTIL_STRING_H

#include "sass.hpp"
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

    // ##########################################################################
    // Special case insensitive string matcher. We can optimize
    // the more general compare case quite a bit by requiring
    // consumers to obey some rules (lowercase and no space).
    // - `literal` must only contain lower case ASCII characters
    // there is one edge case where this could give false positives:
    // test could contain a (NON-ASCII) char exactly 32 below literal
    // ##########################################################################
    bool equalsLiteral(const char* lit, const sass::string& test);
    bool vendorMatch(const char* lit, const sass::string& test);

    // ###########################################################################
    // Returns [name] without a vendor prefix.
    // If [name] has no vendor prefix, it's returned as-is.
    // ###########################################################################
    sass::string unvendor(const sass::string& name);


    // Locale-independent ASCII character routines.

    // skip over 10xxxxxx and 01xxxxxx
    // count ASCII and initial utf8 bytes
    inline bool is_character(uint8_t character) {
      // 64 => initial utf8 byte
      // 128 => regular ASCII char
      // Ignore all `10xxxxxx` chars
      return (character & 192) != 128;
      // return (character & (128|64)) != 0;
    }

    char ascii_tolower(unsigned char c);

    void ascii_str_tolower(sass::string* s);
    sass::string ascii_str_tolower(sass::string s);

    char ascii_toupper(unsigned char c);

    void ascii_str_toupper(sass::string* s);
    sass::string ascii_str_toupper(sass::string s);

    void ascii_str_rtrim(sass::string& s);
    void ascii_str_ltrim(sass::string& s);
    void ascii_str_trim(sass::string& s);

    inline bool ascii_str_is_blank(const sass::string& text)
    {
      auto qwe = std::find_if_not(text.begin(), text.end(), isspace);
      return qwe == text.end();
    }

    inline bool ascii_str_ends_with(const sass::string& text, const sass::string& trail)
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

    inline bool ascii_str_ends_with_insensitive(const sass::string& text, const sass::string& trail)
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

    inline bool ascii_str_starts_with_insensitive(const sass::string& text, const sass::string& prefix)
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

    inline void ascii_normalize_underscore(sass::string& text)
    {
      std::replace(text.begin(), text.end(), '_', '-');
    }

    inline bool ascii_str_equals_ignore_separator(const sass::string& text, const sass::string& test)
    {
      // text must be bigger same size as test
      if (text.size() != test.size()) {
        return false;
      }
      auto cur = text.begin();
      auto chk = test.begin();
      while (chk < test.end()) {
        if (*chk != *cur && ascii_tolower(*chk) != ascii_tolower(*cur)) {
          if (*chk != '_' && *chk != '-' && *cur != '_' && *cur != '-') {
            return false;
          }
        }
        chk += 1;
        cur += 1;
      }
      return true;
    }

    inline size_t hash_ignore_separator(const sass::string& obj) {

      uint32_t hash = 0;

      for (auto chr : obj)
      {
        if (chr != '_' && chr != '-') {
          hash += chr;
        }
        else {
          hash += '-';
        }
        hash += (hash << 10);
        hash ^= (hash >> 6);
      }

      hash += (hash << 3);
      hash ^= (hash >> 11);
      hash += (hash << 15);

      return hash;
    }

    struct hashIgnoreSeparator {
      size_t operator() (const sass::string& obj) const {
        return hash_ignore_separator(obj);
      }
    };

    struct equalsIgnoreSeparator {
      bool operator() (const sass::string& lhs, const sass::string& rhs) const {
        return ascii_str_equals_ignore_separator(lhs, rhs);
      }
    };

    // split string by delimiter
    sass::vector<sass::string> split_string(
      sass::string str,
      char delimiter = PATH_SEP,
      bool trimSpace = false);
    // EO split_string

    sass::string join_strings(
      sass::vector<sass::string>& strings,
      const char* const separator);

  }
  // EO namespace Sass

}
// EO namespace Util

#endif  // SASS_UTIL_STRING_H
