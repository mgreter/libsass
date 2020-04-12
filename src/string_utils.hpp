
#ifndef SASS_STRING_UTILS_H
#define SASS_UTIL_UTILS_H

// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

// Inspired by Java Apache Commons StringUtils

#include "character.hpp"

namespace Sass {
    namespace StringUtils {

      // Optimized version when you pass a static test you know is already lowercase
      bool equalsIgnoreCase(const sass::string& a, const char* b, size_t len);

      bool startsWith(const sass::string& str, const char* prefix, size_t len);
      bool startsWith(const sass::string& str, const sass::string& prefix);

      bool endsWith(const sass::string& str, const char* suffix, size_t len);
      bool endsWith(const sass::string& str, const sass::string& suffix);

      // Optimized version when you pass a static test you know is already lowercase
      bool startsWithIgnoreCase(const sass::string& str, const char* prefix, size_t len);
      bool startsWithIgnoreCase(const sass::string& str, const sass::string& prefix);

      // Optimized version when you pass a static test you know is already lowercase
      bool endsWithIgnoreCase(const sass::string& str, const char* suffix, size_t len);
      bool endsWithIgnoreCase(const sass::string& str, const sass::string& suffix);

      void makeTrimmed(sass::string& str);
      void makeLeftTrimmed(sass::string& str);
      void makeRightTrimmed(sass::string& str);

      void makeLowerCase(sass::string& str);
      void makeUpperCase(sass::string& str);

      sass::string toUpperCase(const sass::string& str);
      sass::string toLowerCase(const sass::string& str);

      sass::vector<sass::string> split(sass::string str, char delimiter, bool trim);
      sass::string join(const sass::vector<sass::string>& strings, const char* separator);

    }
}

#endif
