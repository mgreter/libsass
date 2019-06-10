#include "sass.hpp"
#include "sass.h"
#include "ast.hpp"
#include "util.hpp"
#include "lexer.hpp"
#include "prelexer.hpp"
#include "debugger.hpp"
#include "constants.hpp"
#include "utf8/checked.h"

#include <cmath>
#include <stdint.h>
#if defined(_MSC_VER) && _MSC_VER >= 1800 && _MSC_VER < 1900 && defined(_M_X64)
#include <mutex>
#endif

namespace Sass {

  // Special case insensitive string matcher. We can optimize
  // the more general compare case quite a bit by requiring
  // consumers to obey some rules (lowercase and no space).
  // - `literal` must only contain lower case ascii characters
  // there is one edge case where this could give false positives
  // test could contain a (non-ascii) char exactly 32 below literal
  bool equalsLiteral(const std::string& literal, const std::string test) {
    // Work directly on characters
    const char* src = test.c_str();
    const char* lit = literal.c_str();
    // There is a small chance that the search string
    // Is longer than the rest of the string to look at
    while (*lit && (*src == *lit || *src + 32 == *lit)) {
      ++src, ++lit;
    }
    // True if literal is at end
    // If not test was too long
    return *lit == 0;
  }

  double round(double val, size_t precision)
  {
    // Disable FMA3-optimized implementation when compiling with VS2013 for x64 targets
    // See https://github.com/sass/node-sass/issues/1854 for details
    // FIXME: Remove this workaround when we switch to VS2015+
    #if defined(_MSC_VER) && _MSC_VER >= 1800 && _MSC_VER < 1900 && defined(_M_X64)
      static std::once_flag flag;
      std::call_once(flag, []() { _set_FMA3_enable(0); });
    #endif

    // https://github.com/sass/sass/commit/4e3e1d5684cc29073a507578fc977434ff488c93
    if (fmod(val, 1) - 0.5 > - std::pow(0.1, precision + 1)) return std::ceil(val);
    else if (fmod(val, 1) - 0.5 > std::pow(0.1, precision)) return std::floor(val);
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

  void free_string_array(char ** arr) {
    if(!arr)
        return;

    char **it = arr;
    while (it && (*it)) {
      free(*it);
      ++it;
    }

    free(arr);
  }

  char **copy_strings(const std::vector<std::string>& strings, char*** array, int skip) {
    int num = static_cast<int>(strings.size()) - skip;
    char** arr = (char**) calloc(num + 1, sizeof(char*));
    if (arr == 0)
      return *array = (char **)NULL;

    for(int i = 0; i < num; i++) {
      arr[i] = (char*) malloc(sizeof(char) * (strings[i + skip].size() + 1));
      if (arr[i] == 0) {
        free_string_array(arr);
        return *array = (char **)NULL;
      }
      std::copy(strings[i + skip].begin(), strings[i + skip].end(), arr[i]);
      arr[i][strings[i + skip].size()] = '\0';
    }

    arr[num] = 0;
    return *array = arr;
  }

  // read css string (handle multiline DELIM)
  std::string read_css_string(const std::string& str, bool css)
  {
    if (!css) return str;
    std::string out("");
    bool esc = false;
    for (auto i : str) {
      if (i == '\\') {
        esc = ! esc;
      } else if (esc && i == '\r') {
        continue;
      } else if (esc && i == '\n') {
        out.resize (out.size () - 1);
        esc = false;
        continue;
      } else {
        esc = false;
      }
      out.push_back(i);
    }
    // happens when parsing does not correctly skip
    // over escaped sequences for ie. interpolations
    // one example: foo\#{interpolate}
    // if (esc) out += '\\';
    return out;
  }

  // double escape all escape sequences
  // keep unescaped quotes and backslashes
  std::string evacuate_escapes(const std::string& str)
  {
    std::string out("");
    bool esc = false;
    for (auto i : str) {
      if (i == '\\' && !esc) {
        out += '\\';
        out += '\\';
        esc = true;
      } else if (esc && i == '"') {
        out += '\\';
        out += i;
        esc = false;
      } else if (esc && i == '\'') {
        out += '\\';
        out += i;
        esc = false;
      } else if (esc && i == '\\') {
        out += '\\';
        out += i;
        esc = false;
      } else {
        esc = false;
        out += i;
      }
    }
    // happens when parsing does not correctly skip
    // over escaped sequences for ie. interpolations
    // one example: foo\#{interpolate}
    // if (esc) out += '\\';
    return out;
  }

  // bell characters are replaced with spaces
  void newline_to_space(std::string& str)
  {
    std::replace(str.begin(), str.end(), '\n', ' ');
  }

  // 1. Removes whitespace after newlines.
  // 2. Replaces newlines with spaces.
  //
  // This method only considers LF and CRLF as newlines.
  std::string string_to_output(const std::string& str)
  {
    std::string result;
    result.reserve(str.size());
    std::size_t pos = 0;
    while (true) {
      const std::size_t newline = str.find_first_of("\n\r", pos);
      if (newline == std::string::npos) break;
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
      if (non_space != std::string::npos) {
        pos = non_space;
      }
    }
    result.append(str, pos, std::string::npos);
    return result;
  }

  std::string escape_string(const std::string& str)
  {
    std::string out;
    out.reserve(str.size());
    for (char c : str) {
      switch (c) {
        case '\n':
          out.append("\\n");
          break;
        case '\r':
          out.append("\\r");
          break;
        case '\f':
          out.append("\\f");
          break;
        default:
          out += c;
      }
    }
    return out;
  }

  std::string comment_to_compact_string(const std::string& text)
  {
    std::string str = "";
    size_t has = 0;
    char prev = 0;
    bool clean = false;
    for (auto i : text) {
      if (clean) {
        if (i == '\n') { has = 0; }
        else if (i == '\t') { ++ has; }
        else if (i == ' ') { ++ has; }
        else if (i == '*') {}
        else {
          clean = false;
          str += ' ';
          if (prev == '*' && i == '/') str += "*/";
          else str += i;
        }
      } else if (i == '\n') {
        clean = true;
      } else {
        str += i;
      }
      prev = i;
    }
    if (has) return str;
    else return text;
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

  std::string read_hex_escapes(const std::string& s)
  {

    std::string result;
    bool skipped = false;

    for (size_t i = 0, L = s.length(); i < L; ++i) {

      // implement the same strange ruby sass behavior
      // an escape sequence can also mean a unicode char
      if (s[i] == '\\' && !skipped) {

        // remember
        skipped = true;

        // escape length
        size_t len = 1;

        // parse as many sequence chars as possible
        // ToDo: Check if ruby aborts after possible max
        while (i + len < L && s[i + len] && isxdigit(s[i + len])) ++ len;

        if (len > 1) {

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
          for(size_t m = 0; m < 5 && u[m]; m++) result.push_back(u[m]);

          // skip some more chars?
          i += len - 1; skipped = false;

        }

        else {

          skipped = false;

          result.push_back(s[i]);

        }

      }

      else {

        result.push_back(s[i]);

      }

    }

    return result;

  }

  std::string unquote(const std::string& s, char* qd, bool keep_utf8_sequences, bool strict)
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

    std::string unq;
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
        while (i + len < L && s[i + len] && isxdigit(s[i + len])) ++ len;

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
      //   error("Unescaped delimiter in string to unquote found. [" + s + "]", ParserState("[UNQUOTE]"));
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

  std::string quote(const std::string& s, char q)
  {

    // autodetect with fallback to given quote
    q = detect_best_quotemark(s.c_str(), q);

    // return an empty quoted string
    if (s.empty()) return std::string(2, q ? q : '"');

    std::string quoted;
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
        // we hope we can remove this flag once we figure out
        // why ruby sass has these different output behaviors
        // gsub(/\n(?![a-fA-F0-9\s])/, "\\a").gsub("\n", "\\a ")
        using namespace Prelexer;
        if (alternatives <
          Prelexer::char_range<'a', 'f'>,
          Prelexer::char_range<'A', 'F'>,
          Prelexer::char_range<'0', '9'>,
          space
        >(it) != NULL) {
          quoted.push_back(' ');
        }
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

  bool peek_linefeed(const char* start)
  {
    using namespace Prelexer;
    using namespace Constants;
    return sequence <
             zero_plus <
               alternatives <
                 exactly <' '>,
                 exactly <'\t'>,
                 line_comment,
                 block_comment,
                 delimited_by <
                   slash_star,
                   star_slash,
                   false
                 >
               >
             >,
             re_linebreak
           >(start) != 0;
  }

  namespace Util {

    bool isPrintable(Ruleset* r, Sass_Output_Style style) {
      if (r == NULL) {
        return false;
      }

      Block_Obj b = r->block();

      SelectorList* sl = r->selector2();
      bool hasSelectors = sl ? sl->length() > 0 : false;

      if (!hasSelectors) {
        return false;
      }

      if (sl->isInvisible()) {
        // return false;
      }

      bool hasDeclarations = false;
      bool hasPrintableChildBlocks = false;
      for (size_t i = 0, L = b->length(); i < L; ++i) {
        Statement_Obj stm = b->at(i);
        if (Cast<Directive>(stm)) {
          return true;
        } else if (Declaration* d = Cast<Declaration>(stm)) {
          return isPrintable(d, style);
        } else if (Has_Block* p = Cast<Has_Block>(stm)) {
          Block_Obj pChildBlock = p->block();
          if (isPrintable(pChildBlock, style)) {
            hasPrintableChildBlocks = true;
          }
        } else if (Comment* c = Cast<Comment>(stm)) {
          // keep for uncompressed
          if (style != COMPRESSED) {
            hasDeclarations = true;
          }
          // output style compressed
          if (c->is_important()) {
            hasDeclarations = c->is_important();
          }
        } else {
          hasDeclarations = true;
        }

        if (hasDeclarations || hasPrintableChildBlocks) {
          return true;
        }
      }

      return false;
    }

    bool isPrintable(String_Constant* s, Sass_Output_Style style)
    {
      return ! s->value().empty();
    }

    bool isPrintable(String_Quoted* s, Sass_Output_Style style)
    {
      return true;
    }

    bool isPrintable(Declaration* d, Sass_Output_Style style)
    {
      Expression_Obj val = d->value();
      if (String_Quoted_Obj sq = Cast<String_Quoted>(val)) return isPrintable(sq.ptr(), style);
      if (String_Constant_Obj sc = Cast<String_Constant>(val)) return isPrintable(sc.ptr(), style);
      return true;
    }

    bool isPrintable(Supports_Block* f, Sass_Output_Style style) {
      if (f == NULL) {
        return false;
      }

      Block_Obj b = f->block();

      bool hasDeclarations = false;
      bool hasPrintableChildBlocks = false;
      for (size_t i = 0, L = b->length(); i < L; ++i) {
        Statement_Obj stm = b->at(i);
        if (Cast<Declaration>(stm) || Cast<Directive>(stm)) {
          hasDeclarations = true;
        }
        else if (Has_Block* b = Cast<Has_Block>(stm)) {
          Block_Obj pChildBlock = b->block();
          if (!b->is_invisible()) {
            if (isPrintable(pChildBlock, style)) {
              hasPrintableChildBlocks = true;
            }
          }
        }

        if (hasDeclarations || hasPrintableChildBlocks) {
          return true;
        }
      }

      return false;
    }

    bool isPrintable(Media_Block* m, Sass_Output_Style style)
    {
      if (m == 0) return false;
      Block_Obj b = m->block();
      if (b == 0) return false;
      for (size_t i = 0, L = b->length(); i < L; ++i) {
        Statement_Obj stm = b->at(i);
        if (Cast<Directive>(stm)) return true;
        else if (Cast<Declaration>(stm)) return true;
        else if (Comment * c = Cast<Comment>(stm)) {
          if (isPrintable(c, style)) {
            return true;
          }
        }
        else if (Ruleset * r = Cast<Ruleset>(stm)) {
          if (isPrintable(r, style)) {
            return true;
          }
        }
        else if (Supports_Block * f = Cast<Supports_Block>(stm)) {
          if (isPrintable(f, style)) {
            return true;
          }
        }
        else if (Media_Block * mb = Cast<Media_Block>(stm)) {
          if (isPrintable(mb, style)) {
            return true;
          }
        }
        else if (CssMediaRule * mb = Cast<CssMediaRule>(stm)) {
          if (isPrintable(mb, style)) {
            return true;
          }
        }
        else if (Has_Block * b = Cast<Has_Block>(stm)) {
          if (isPrintable(b->block(), style)) {
            return true;
          }
        }
      }
      return false;
    }


    bool isPrintable(CssMediaRule* m, Sass_Output_Style style)
    {
      if (m == 0) return false;
      Block_Obj b = m->block();
      if (b == 0) return false;
      for (size_t i = 0, L = b->length(); i < L; ++i) {
        Statement_Obj stm = b->at(i);
        if (Cast<Directive>(stm)) return true;
        else if (Cast<Declaration>(stm)) return true;
        else if (Comment * c = Cast<Comment>(stm)) {
          if (isPrintable(c, style)) {
            return true;
          }
        }
        else if (Ruleset * r = Cast<Ruleset>(stm)) {
          if (isPrintable(r, style)) {
            return true;
          }
        }
        else if (Supports_Block * f = Cast<Supports_Block>(stm)) {
          if (isPrintable(f, style)) {
            return true;
          }
        }
        else if (Media_Block * mb = Cast<Media_Block>(stm)) {
          if (isPrintable(mb, style)) {
            return true;
          }
        }
        else if (CssMediaRule * mb = Cast<CssMediaRule>(stm)) {
          if (isPrintable(mb, style)) {
            return true;
          }
        }
        else if (Has_Block * b = Cast<Has_Block>(stm)) {
          if (isPrintable(b->block(), style)) {
            return true;
          }
        }
      }
      return false;
    }


    bool isPrintable(Comment* c, Sass_Output_Style style)
    {
      // keep for uncompressed
      if (style != COMPRESSED) {
        return true;
      }
      // output style compressed
      if (c->is_important()) {
        return true;
      }
      // not printable
      return false;
    };

    bool isPrintable(Block_Obj b, Sass_Output_Style style) {
      if (!b) {
        return false;
      }

      for (size_t i = 0, L = b->length(); i < L; ++i) {
        Statement_Obj stm = b->at(i);
        if (Cast<Declaration>(stm) || Cast<Directive>(stm)) {
          return true;
        }
        else if (Comment* c = Cast<Comment>(stm)) {
          if (isPrintable(c, style)) {
            return true;
          }
        }
        else if (Ruleset* r = Cast<Ruleset>(stm)) {
          if (isPrintable(r, style)) {
            return true;
          }
        }
        else if (Supports_Block* f = Cast<Supports_Block>(stm)) {
          if (isPrintable(f, style)) {
            return true;
          }
        }
        else if (Media_Block * m = Cast<Media_Block>(stm)) {
          if (isPrintable(m, style)) {
            return true;
          }
        }
        else if (CssMediaRule * m = Cast<CssMediaRule>(stm)) {
          if (isPrintable(m, style)) {
            return true;
          }
        }
        else if (Has_Block* b = Cast<Has_Block>(stm)) {
          if (isPrintable(b->block(), style)) {
            return true;
          }
        }
      }

      return false;
    }

    bool isAscii(const char chr) {
      return unsigned(chr) < 128;
    }

  }
}
