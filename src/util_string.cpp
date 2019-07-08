#include "util_string.hpp"
#include "character.hpp"

#include <cstring>
#include <iostream>
#include <algorithm>

namespace Sass {
  namespace Util {

    const std::bitset<256> isAlphaTable(
      "00000000000000000000000000000000" // 255 - 224
      "00000000000000000000000000000000" // 223 - 192
      "00000000000000000000000000000000" // 192 - 160
      "00000000000000000000000000000000" // 159 - 128
      "00000111111111111111111111111110" // 127 - 96
      "00000111111111111111111111111110" // 95 - 64
      "00000000000000000000000000000000" // 63 - 32
      "00000000000000000000000000000000" // 31 - 0
    );

    const std::bitset<256> isDigitTable(
      "00000000000000000000000000000000" // 255 - 224
      "00000000000000000000000000000000" // 223 - 192
      "00000000000000000000000000000000" // 192 - 160
      "00000000000000000000000000000000" // 159 - 128
      "00000000000000000000000000000000" // 127 - 96
      "00000000000000000000000000000000" // 95 - 64
      "00000011111111110000000000000000" // 63 - 32
      "00000000000000000000000000000000" // 31 - 0
    );

    const std::bitset<256> isAlnumTable(
      "00000000000000000000000000000000" // 255 - 224
      "00000000000000000000000000000000" // 223 - 192
      "00000000000000000000000000000000" // 192 - 160
      "00000000000000000000000000000000" // 159 - 128
      "00000111111111111111111111111110" // 127 - 96
      "00000111111111111111111111111110" // 95 - 64
      "00000011111111110000000000000000" // 63 - 32
      "00000000000000000000000000000000" // 31 - 0
    );

    const std::bitset<256> isXdigitTable(
      "00000000000000000000000000000000" // 255 - 224
      "00000000000000000000000000000000" // 223 - 192
      "00000000000000000000000000000000" // 192 - 160
      "00000000000000000000000000000000" // 159 - 128
      "00000000000000000000000001111110" // 127 - 96
      "00000000000000000000000001111110" // 95 - 64
      "00000011111111110000000000000000" // 63 - 32
      "00000000000000000000000000000000" // 31 - 0
    );

    const std::bitset<256> isWhitespaceTable(
      "00000000000000000000000000000000" // 255 - 224
      "00000000000000000000000000000000" // 223 - 192
      "00000000000000000000000000000000" // 192 - 160
      "00000000000000000000000000000000" // 159 - 128
      "00000000000000000000000000000000" // 127 - 96
      "00000000000000000000000000000000" // 95 - 64
      "00000000000000000000000000000001" // 63 - 32
      "00000000000000000000101000000000" // 31 - 0
    );

    const std::bitset<256> isLinefeedTable(
      "00000000000000000000000000000000" // 255 - 224
      "00000000000000000000000000000000" // 223 - 192
      "00000000000000000000000000000000" // 192 - 160
      "00000000000000000000000000000000" // 159 - 128
      "00000000000000000000000000000000" // 127 - 96
      "00000000000000000000000000000000" // 95 - 64
      "00000000000000000000000000000000" // 63 - 32
      "00000000000000000011010000000000" // 31 - 0
    );

    const std::bitset<256> isSpaceTable(
      "00000000000000000000000000000000" // 255 - 224
      "00000000000000000000000000000000" // 223 - 192
      "00000000000000000000000000000000" // 192 - 160
      "00000000000000000000000000000000" // 159 - 128
      "00000000000000000000000000000000" // 127 - 96
      "00000000000000000000000000000000" // 95 - 64
      "00000000000000000000000000000001" // 63 - 32
      "00000000000000000011111000000000" // 31 - 0
    );






    // ##########################################################################
    // Special case insensitive string matcher. We can optimize
    // the more general compare case quite a bit by requiring
    // consumers to obey some rules (lowercase and no space).
    // - `literal` must only contain lower case ascii characters
    // there is one edge case where this could give false positives
    // test could contain a (non-ascii) char exactly 32 below literal
    // ##########################################################################
    bool equalsLiteral(const char* lit, const sass::string& test) {
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
    sass::string ascii_str_tolower(sass::string s) {
      ascii_str_tolower(&s);
      return s;
    }

    char ascii_toupper(unsigned char c) {
      if (Character::isAlphabetic(c)) return c & ~32;
      return c;
    }

    char ascii_tolower(unsigned char c) {
      if (Character::tblAlphabetic[c]) return c | 32;
      return c;
    }

    // operates directly on the passed string
    void ascii_str_tolower(sass::string* s) {
      for (auto& ch : *s) {
        unsigned char chr = ch;
        if (Character::tblAlphabetic[chr]) {
          ch |= 32;
        }
      }
    }

    // returns an uppercase copy
    sass::string ascii_str_toupper(sass::string s) {
      ascii_str_toupper(&s);
      return s;
    }

    // operates directly on the passed string
    void ascii_str_toupper(sass::string* s) {
      for (auto& ch : *s) {
        unsigned char chr = ch;
        if (Character::tblAlphabetic[chr]) {
          ch &= ~32;
        }
      }
    }

	  void ascii_str_rtrim(sass::string& s)
	  {
      auto it = std::find_if_not(s.rbegin(), s.rend(), Character::isWhitespace);
      s.erase(s.rend() - it);
    }

	  void ascii_str_ltrim(sass::string& s)
	  {
      if (s.begin() == s.end()) return;
      auto it = std::find_if_not(s.begin(), s.end(), Character::isWhitespace);
      s.erase(s.begin(), it);
    }

	  void ascii_str_trim(sass::string& s)
	  {
      ascii_str_ltrim(s);
      ascii_str_rtrim(s);
    }

    // split string by delimiter
    sass::vector<sass::string> split_string(sass::string str, char delimiter, bool trimSpace)
    {
      sass::vector<sass::string> paths;
      if (str.empty()) return paths;
      // find delimiter via prelexer (return zero at end)
      size_t start = 0, end = str.find_first_of(delimiter);
      // search until null delimiter
      while (end != sass::string::npos) {
        // add substring from current position to delimiter
        paths.emplace_back(str.substr(start, end - start));
        start = end + 1; // skip delimiter
        end = str.find_first_of(delimiter, start);
      }
      // add substring from current position to end
      paths.emplace_back(str.substr(start, end - start));
      // return back
      return paths;
    }

	sass::string join_strings(sass::vector<sass::string>& strings, const char* const separator)
	{
    size_t size = 0;
		switch (strings.size())
		{
		case 0:
			return "";
		case 1:
			return strings[0];
		default:
      for (size_t i = 1; i < strings.size(); i++) {
        size += strings[i].size() + 2;
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
