// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include <string>
#include <vector>
#include <cstdlib>
#include <cmath>

#include "unicode.hpp"

namespace Sass {
  namespace Unicode {

    // naming conventions:
    // offset: raw byte offset (0 based)
    // position: code point offset (0 based)
    // index: code point offset (1 based or negative)

    // function that will count the number of code points (utf-8 characters) from the given beginning to the given end
    size_t codePointCount(const sass::string& str, size_t bytes) {
      return utf8::distance(str.begin(), str.begin() + bytes);
    }

    size_t codePointCount(const sass::string& str) {
      return utf8::distance(str.begin(), str.end());
    }

    // function that will return the byte offset at a code point position
    size_t byteOffsetAtPosition(const sass::string& str, size_t position) {
      sass::string::const_iterator it = str.begin();
      utf8::advance(it, position, str.end());
      return std::distance(str.begin(), it);
    }

    // Returns utf8 aware substring.
    // Parameters are in code points.
    sass::string utf8substr(
      sass::string& utf8,
      size_t start,
      size_t len)
    {
      auto first = utf8.begin();
      utf8::advance(first,
        start, utf8.end());
      auto last = first;
      if (len != sass::string::npos) {
        utf8::advance(last,
          len, utf8.end());
      }
      else {
        last = utf8.end();
      }
      return sass::string(
        first, last);
    }
    // EO substr

    // Utf8 aware string replacement.
    // Parameters are in code points.
    // Inserted text must be valid utf8.
    sass::string utf8replace(
      sass::string& text,
      size_t start, size_t len,
      const sass::string& insert)
    {
      auto first = text.begin();
      utf8::advance(first,
        start, text.end());
      auto last = first;
      if (len != sass::string::npos) {
        utf8::advance(last,
          len, text.end());
      }
      else {
        last = text.end();
      }
      return text.replace(
        first, last,
        insert);
    }
    // EO replace


    #ifdef _WIN32

    // utf16 functions
    using std::wstring;

    // convert from utf16/wide string to utf8 string
    sass::string utf16to8(const sass::wstring& utf16)
    {
      sass::string utf8;
      // preallocate expected memory
      utf8.reserve(sizeof(utf16)/2);
      utf8::utf16to8(utf16.begin(), utf16.end(),
                     back_inserter(utf8));
      return utf8;
    }

    // convert from utf8 string to utf16/wide string
    sass::wstring utf8to16(const sass::string& utf8)
    {
      sass::wstring utf16;
      // preallocate expected memory
      utf16.reserve(codePointCount(utf8)*2);
      utf8::utf8to16(utf8.begin(), utf8.end(),
                     back_inserter(utf16));
      return utf16;
    }

    #endif

  }
}
