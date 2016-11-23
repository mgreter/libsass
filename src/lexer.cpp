#include "sass.hpp"
#include <cctype>
#include <cstddef>
#include <iostream>
#include <iomanip>
#include "lexer.hpp"
#include "constants.hpp"


namespace Sass {
  using namespace Constants;

  namespace Prelexer {

    //####################################
    // BASIC CHARACTER MATCHERS
    //####################################

    // Match standard control chars

    //####################################
    // implement some function that do exist in the standard
    // but those are locale aware which brought some trouble
    // this even seems to improve performance by quite a bit
    //####################################


    //####################################
    // BASIC CLASS MATCHERS
    //####################################


    // Match multiple ctype characters.
    const char* spaces(const char* src) { return one_plus<space>(src); }
    const char* digits(const char* src) { return one_plus<digit>(src); }
    const char* hyphens(const char* src) { return one_plus<hyphen>(src); }

    // Whitespace handling.
    const char* no_spaces(const char* src) { return negate< space >(src); }
    const char* optional_spaces(const char* src) { return zero_plus< space >(src); }

    // Match any single character.
    const char* any_char(const char* src) { return *src ? src + 1 : src; }

    // Match word boundary (zero-width lookahead).
    const char* word_boundary(const char* src) { return is_character(*src) || *src == '#' ? 0 : src; }

    // Match linefeed /(?:\n|\r\n?)/
    const char* re_linebreak(const char* src)
    {
      // end of file or unix linefeed return here
      if (*src == 0 || *src == '\n') return src + 1;
      // a carriage return may optionally be followed by a linefeed
      if (*src == '\r') return *(src + 1) == '\n' ? src + 2 : src + 1;
      // no linefeed
      return 0;
    }

    // Assert string boundaries (/\Z|\z|\A/)
    // This is a zero-width positive lookahead
    const char* end_of_line(const char* src)
    {
      // end of file or unix linefeed return here
      return *src == 0 || *src == '\n' || *src == '\r' ? src : 0;
    }

    // Assert end_of_file boundary (/\z/)
    // This is a zero-width positive lookahead
    const char* end_of_file(const char* src)
    {
      // end of file or unix linefeed return here
      return *src == 0 ? src : 0;
    }

  }
}
