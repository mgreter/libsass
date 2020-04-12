#include "string_utils.hpp"

#include <stdio.h>
#include <string.h>

namespace Sass {
  namespace StringUtils {

    // Import from character
    using namespace Character;

    bool _equalsIgnoreCase(const char a, const char b) {
      return Character::characterEqualsIgnoreCase(a, b);
    }

    // Optimized version where we know one side is already lowercase
    bool _equalsIgnoreCaseConst(const char a, const char b) {
      return a == b || a == Character::toLowerCase(b);
    }

    bool startsWith(const sass::string& str, const char* prefix, size_t len) {
      return len <= str.size() && std::equal(prefix, prefix + len, str.begin());
    }

    bool startsWith(const sass::string& str, const sass::string& prefix) {
      return prefix.size() <= str.size() && std::equal(prefix.begin(), prefix.end(), str.begin());
    }

    bool endsWith(const sass::string& str, const char* suffix, size_t len) {
      return len <= str.size() && std::equal(suffix, suffix + len, str.end() - len);
    }

    bool endsWith(const sass::string& str, const sass::string& suffix) {
      return suffix.size() <= str.size() && std::equal(suffix.rbegin(), suffix.rend(), str.rbegin());
    }

    // This is an optimized version when you pass it static test you know is already lowercase
    bool startsWithIgnoreCase(const sass::string& str, const char* prefix, size_t len) {
      return len <= str.size() && std::equal(prefix, prefix + len, str.begin(), _equalsIgnoreCaseConst);
    }

    bool startsWithIgnoreCase(const sass::string& str, const sass::string& prefix) {
      return prefix.size() <= str.size() && std::equal(prefix.begin(), prefix.end(), str.begin(), _equalsIgnoreCase);
    }

    // This is an optimized version when you pass it static test you know is already lowercase
    bool endsWithIgnoreCase(const sass::string& str, const char* suffix, size_t len) {
      return len <= str.size() && std::equal(suffix, suffix + len, str.end() - len, _equalsIgnoreCaseConst);
    }

    bool endsWithIgnoreCase(const sass::string& str, const sass::string& suffix) {
      return suffix.size() <= str.size() && std::equal(suffix.rbegin(), suffix.rend(), str.rbegin(), _equalsIgnoreCase);
    }

    bool equalsIgnoreCase(const sass::string& a, const char* b, size_t len) {
      if (len != strlen(b)) std::cerr << "Fuck yeah " << b << " => " << strlen(b) << "\n";
      return len == a.size() && std::equal(b, b + len, a.begin(), _equalsIgnoreCaseConst);
    }

    void makeTrimmed(sass::string& str) {
      makeLeftTrimmed(str);
      makeRightTrimmed(str);
    }

    void makeLeftTrimmed(sass::string& str) {
      if (str.begin() != str.end()) {
        auto pos = std::find_if_not(
          str.begin(), str.end(),
          Character::isWhitespace);
        str.erase(str.begin(), pos);
      }
    }

    void makeRightTrimmed(sass::string& str) {
      if (str.begin() != str.end()) {
        auto pos = std::find_if_not(
          str.rbegin(), str.rend(),
          Character::isWhitespace);
        str.erase(str.rend() - pos);
      }
    }

    void makeLowerCase(sass::string& str) {
      for (char& character : str) {
        if (character >= $A && character <= $Z)
          character |= asciiCaseBit;
      }
    }

    void makeUpperCase(sass::string& str) {
      for (char& character : str) {
        if (character >= $a && character <= $z)
          character &= ~asciiCaseBit;
      }
    }

    sass::string toLowerCase(const sass::string& str) {
      sass::string rv(str);
      for (char& character : rv) {
        if (character >= $A && character <= $Z)
          character |= asciiCaseBit;
      }
      return rv;
    }

    sass::string toUpperCase(const sass::string& str) {
      sass::string rv(str);
      for (char& character : rv) {
        if (character >= $a && character <= $z)
          character &= ~asciiCaseBit;
      }
      return rv;
    }

    sass::vector<sass::string> split(sass::string str, char delimiter, bool trim)
    {
      sass::vector<sass::string> rv;
      if (trim) StringUtils::makeTrimmed(str);
      if (str.empty()) return rv;
      size_t start = 0, end = str.find_first_of(delimiter);
      // search until we are at the end
      while (end != sass::string::npos) {
        // add substring from current position to delimiter
        sass::string item(str.substr(start, end - start));
        if (trim) StringUtils::makeTrimmed(item);
        if (!trim || !item.empty()) rv.emplace_back(item);
        start = end + 1; // skip delimiter
        end = str.find_first_of(delimiter, start);
      }
      // add last substring from current position to end
      sass::string item(str.substr(start, end - start));
      if (trim) StringUtils::makeTrimmed(item);
      if (!trim || !item.empty()) rv.emplace_back(item);
      // return back
      return rv;
    }


    sass::string join(const sass::vector<sass::string>& strings, const char* separator)
    {
      switch (strings.size())
      {
      case 0:
        return "";
      case 1:
        return strings[0];
      default:
        size_t size = 0, sep_len = strlen(separator);
        for (size_t i = 1; i < strings.size(); i++) {
          size += strings[i].size() + sep_len;
        }
        sass::string os;
        os.reserve(size);
        os += strings[0];
        for (size_t i = 1; i < strings.size(); i++) {
          os += separator;
          os += strings[i];
        }
        return os;
      }
    }

  }
}
