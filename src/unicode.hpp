#ifndef SASS_UTF8_STRING_H
#define SASS_UTF8_STRING_H

#include <string>
#include "memory.hpp"
#include "utf8/checked.h"
#include "utf8/unchecked.h"

namespace Sass {
  namespace Unicode {

    // naming conventions:
    // bytes: raw byte offset (0 based)
    // position: code point offset (0 based)

    // function that will count the number of code points (utf-8 characters) from the beginning to the given end
    size_t codePointCount(const sass::string& utf8, size_t bytes);
    size_t codePointCount(const sass::string& utf8);

    // function that will return the byte offset of a code point in a
    size_t byteOffsetAtPosition(const sass::string& utf8, size_t position);

    sass::string utf8substr(sass::string& utf8, size_t start, size_t len);
    sass::string utf8replace(sass::string& utf8, size_t start, size_t len, const sass::string& insert);



    #ifdef _WIN32
    // functions to handle unicode paths on windows
    sass::string utf16to8(const sass::wstring& utf16);
    sass::wstring utf8to16(const sass::string& utf8);
    #endif

  }
}

#endif
