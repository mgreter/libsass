#include "sass.hpp"
#include "sass.h"
#include "ast.hpp"
#include "util.hpp"
#include "util_string.hpp"
#include "constants.hpp"
#include "character.hpp"
#include "utf8/checked.h"

#include <cmath>
#include <stdint.h>
#if defined(_MSC_VER) && _MSC_VER >= 1800 && _MSC_VER < 1900 && defined(_M_X64)
#include <mutex>
#endif

namespace Sass {

  double round(double val, int precision)
  {
    // Disable FMA3-optimized implementation when compiling with VS2013 for x64 targets
    // See https://github.com/sass/node-sass/issues/1854 for details
    // FIXME: Remove this workaround when we switch to VS2015+
    #if defined(_MSC_VER) && _MSC_VER >= 1800 && _MSC_VER < 1900 && defined(_M_X64)
      static std::once_flag flag;
      std::call_once(flag, []() { _set_FMA3_enable(0); });
    #endif

    // https://github.com/sass/sass/commit/4e3e1d5684cc29073a507578fc977434ff488c93
    if (std::fmod(val, 1) - 0.5 > - std::pow(0.1, precision + 1)) return std::ceil(val);
    else if (std::fmod(val, 1) - 0.5 > std::pow(0.1, precision)) return std::floor(val);
    // work around some compiler issue
    // cygwin has it not defined in std
    using namespace std;
    return ::round(val);
  }

  /* Locale unspecific atof function. */
  double sass_strtod(const char *str)
  {
    char separator = *(localeconv()->decimal_point);
    if(separator != '.'){
      // The current locale specifies another
      // separator. convert the separator to the
      // one understood by the locale if needed
      const char *found = strchr(str, '.');
      if(found != NULL){
        // substitution is required. perform the substitution on a copy
        // of the string. This is slower but it is thread safe.
        char *copy = sass_copy_c_string(str);
        *(copy + (found - str)) = separator;
        double res = strtod(copy, NULL);
        free(copy);
        return res;
      }
    }

    return strtod(str, NULL);
  }

  // helper for safe access to c_ctx
  const char* safe_str (const char* str, const char* alt) {
    return str == NULL ? alt : str;
  }

  // 1. Removes whitespace after newlines.
  // 2. Replaces newlines with spaces.
  //
  // This method only considers LF and CRLF as newlines.
  sass::string string_to_output(const sass::string& str)
  {
    sass::string result;
    result.reserve(str.size());
    std::size_t pos = 0;
    while (true) {
      const std::size_t newline = str.find_first_of("\n\r", pos);
      if (newline == sass::string::npos) break;
      result.append(str, pos, newline - pos);
      if (str[newline] == '\r') {
        if (str[newline + 1] == '\n') {
          pos = newline + 2;
        } else {
          // CR without LF: append as-is and continue.
          result += '\r';
          pos = newline + 1;
          continue;
        }
      } else {
        pos = newline + 1;
      }
      result += ' ';
      const std::size_t non_space = str.find_first_not_of(" \f\n\r\t\v", pos);
      if (non_space != sass::string::npos) {
        pos = non_space;
      }
    }
    result.append(str, pos, sass::string::npos);
    return result;
  }

  // find best quote_mark by detecting if the string contains any single
  // or double quotes. When a single quote is found, we not we want a double
  // quote as quote_mark. Otherwise we check if the string cotains any double
  // quotes, which will trigger the use of single quotes as best quote_mark.
  char detect_best_quotemark(const char* s, char qm)
  {
    // ensure valid fallback quote_mark
    char quote_mark = qm && qm != '*' ? qm : '"';
    while (*s) {
      // force double quotes as soon
      // as one single quote is found
      if (*s == '\'') { return '"'; }
      // a single does not force quote_mark
      // maybe we see a double quote later
      else if (*s == '"') { quote_mark = '\''; }
      ++ s;
    }
    return quote_mark;
  }


  sass::string unquote(const sass::string& s, char* qd, bool keep_utf8_sequences, bool strict)
  {

    // not enough room for quotes
    // no possibility to unquote
    if (s.length() < 2) return s;

    char q;
    bool skipped = false;

    // this is no guarantee that the unquoting will work
    // what about whitespace before/after the quote_mark?
    if      (*s.begin() == '"'  && *s.rbegin() == '"')  q = '"';
    else if (*s.begin() == '\'' && *s.rbegin() == '\'') q = '\'';
    else                                                return s;

    sass::string unq;
    unq.reserve(s.length()-2);

    for (size_t i = 1, L = s.length() - 1; i < L; ++i) {

      // implement the same strange ruby sass behavior
      // an escape sequence can also mean a unicode char
      if (s[i] == '\\' && !skipped) {
        // remember
        skipped = true;

        // skip it
        // ++ i;

        // if (i == L) break;

        // escape length
        size_t len = 1;

        // parse as many sequence chars as possible
        // ToDo: Check if ruby aborts after possible max
        while (i + len < L && s[i + len] && Character::isHex(static_cast<unsigned char>(s[i + len]))) ++ len;

        // hex string?
        if (keep_utf8_sequences) {
          unq.push_back(s[i]);
        } else if (len > 1) {

          // convert the extracted hex string to code point value
          // ToDo: Maybe we could do this without creating a substring
          uint32_t cp = strtol(s.substr (i + 1, len - 1).c_str(), NULL, 16);

          if (s[i + len] == ' ') ++ len;

          // assert invalid code points
          if (cp == 0) cp = 0xFFFD;
          // replace bell character
          // if (cp == '\n') cp = 32;

          // use a very simple approach to convert via utf8 lib
          // maybe there is a more elegant way; maybe we shoud
          // convert the whole output from string to a stream!?
          // allocate memory for utf8 char and convert to utf8
          unsigned char u[5] = {0,0,0,0,0}; utf8::append(cp, u);
          for(size_t m = 0; m < 5 && u[m]; m++) unq.push_back(u[m]);

          // skip some more chars?
          i += len - 1; skipped = false;

        }


      }
      // check for unexpected delimiter
      // be strict and throw error back
      // else if (!skipped && q == s[i]) {
      //   // don't be that strict
      //   return s;
      //   // this basically always means an internal error and not users fault
      //   error("Unescaped delimiter in string to unquote found. [" + s + "]", SourceSpan("[UNQUOTE]"));
      // }
      else {
        if (strict && !skipped) {
          if (s[i] == q) return s;
        }
        skipped = false;
        unq.push_back(s[i]);
      }

    }
    if (skipped) { return s; }
    if (qd) *qd = q;
    return unq;

  }

  sass::string quote(const sass::string& s, char q)
  {

    // autodetect with fallback to given quote
    q = detect_best_quotemark(s.c_str(), q);

    // return an empty quoted string
    if (s.empty()) return sass::string(2, q ? q : '"');

    sass::string quoted;
    quoted.reserve(s.length()+2);
    quoted.push_back(q);

    const char* it = s.c_str();
    const char* end = it + strlen(it) + 1;
    while (*it && it < end) {
      const char* now = it;

      if (*it == q) {
        quoted.push_back('\\');
      } else if (*it == '\\') {
        quoted.push_back('\\');
      }

      int cp = utf8::next(it, end);

      // in case of \r, check if the next in sequence
      // is \n and then advance the iterator and skip \r
      if (cp == '\r' && it < end && utf8::peek_next(it, end) == '\n') {
        cp = utf8::next(it, end);
      }

      if (cp == '\n') {
        quoted.push_back('\\');
        quoted.push_back('a');
      } else if (cp < 127) {
        quoted.push_back((char) cp);
      } else {
        while (now < it) {
          quoted.push_back(*now);
          ++ now;
        }
      }
    }

    quoted.push_back(q);
    return quoted;
  }

  bool is_hex_doublet(double n)
  {
    return n == 0x00 || n == 0x11 || n == 0x22 || n == 0x33 ||
           n == 0x44 || n == 0x55 || n == 0x66 || n == 0x77 ||
           n == 0x88 || n == 0x99 || n == 0xAA || n == 0xBB ||
           n == 0xCC || n == 0xDD || n == 0xEE || n == 0xFF ;
  }

  bool is_color_doublet(double r, double g, double b)
  {
    return is_hex_doublet(r) && is_hex_doublet(g) && is_hex_doublet(b);
  }
  
}
