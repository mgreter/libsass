
#ifndef SASS_STRING_UTILS_H
#define SASS_STRING_UTILS_H

// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include "character.hpp"

namespace Sass {
  namespace StringUtils {

    // Optimized version when you pass a static test you know is already lowercase
    bool equalsIgnoreCase(const sass::string& a, const char* b, size_t len);

    bool startsWith(const sass::string& str, const sass::string& prefix);
    bool startsWith(const sass::string& str, const char* prefix, size_t len);

    bool endsWith(const sass::string& str, const sass::string& suffix);
    bool endsWith(const sass::string& str, const char* suffix, size_t len);

    bool startsWithIgnoreCase(const sass::string& str, const sass::string& prefix);
    // Optimized version when you pass `const char*` you know is already lowercase.
    bool startsWithIgnoreCase(const sass::string& str, const char* prefix, size_t len);

    bool endsWithIgnoreCase(const sass::string& str, const sass::string& suffix);
    // Optimized version when you pass `const char*` you know is already lowercase.
    bool endsWithIgnoreCase(const sass::string& str, const char* suffix, size_t len);

    // Make the passed string whitespace trimmed.
    void makeTrimmed(sass::string& str);

    // Trim the left side of passed string.
    void makeLeftTrimmed(sass::string& str);

    // Trim the right side of passed string.
    void makeRightTrimmed(sass::string& str);

    // Make the passed string lowercase.
    void makeLowerCase(sass::string& str);

    // Make the passed string uppercase.
    void makeUpperCase(sass::string& str);

    // Return new string converted to lowercase.
    sass::string toUpperCase(const sass::string& str);

    // Return new string converted to uppercase.
    sass::string toLowerCase(const sass::string& str);

    // Return list of strings split by `delimiter`.
    // Optionally `trim` all results (default behavior).
    sass::vector<sass::string> split(sass::string str, char delimiter, bool trim = true);

    // Return joined string from all passed strings, delimited by separator.
    sass::string join(const sass::vector<sass::string>& strings, const char* separator);

  }
}

#endif
