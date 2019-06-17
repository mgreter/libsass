#include <iostream>

#include "sass.hpp"
#include "charcode.hpp"
#include "character.hpp"
#include "scanner_string.hpp"
#include "error_handling.hpp"
#include "utf8/checked.h"

namespace Sass {

  // Import some namespaces
  using namespace Charcode;
  using namespace Character;

  StringScanner::StringScanner(
    const char* content,
    const char* path,
    size_t srcid) :
    startpos(content),
    endpos(content + strlen(content)),
    position(content),
    sourceUrl(path),
    srcid(srcid),
    offset(srcid, 0, 0)
  {
    // consume BOM?

    auto invalid = utf8::find_invalid(content, endpos);
    if (invalid != endpos) {
      ParserState pstate(path, content, srcid);
      Offset start = Offset::init(content, invalid);
      pstate.line = start.line; pstate.column = start.column;
      throw Exception::InvalidUnicode(pstate, {});
    }
  }

  // Whether the scanner has completely consumed [string].
  bool StringScanner::isDone() const
  {
    return position >= endpos;
  }

  // Called whenever a character is consumed.
  // Used to update scanner line/column position.
  void StringScanner::consumedChar(uint8_t character)
  {
    if (character == $lf) {
      offset.line += 1;
      offset.column = 0;
    }
    else if (character < 128) {
      offset.column += 1;
    }
  }

  // Consumes a single character and returns its character
  // code. This throws a [FormatException] if the string has
  // been fully consumed. It doesn't affect [lastMatch].
  uint8_t StringScanner::readChar()
  {
    if (isDone()) _fail("more input");
    uint8_t ascii = *position;
    consumedChar(ascii);
    position += 1;
    return ascii;
  }

  // Returns the character code of the character [offset] away
  // from [position]. [offset] defaults to zero, and may be negative
  // to inspect already-consumed characters. This returns `null` if
  // [offset] points outside the string. It doesn't affect [lastMatch].
  uint8_t StringScanner::peekChar(size_t offset) const
  {
    const char* cur = position + offset;
    if (cur < startpos) {
      // std::cerr << "ScannerString1\n";
      return 0;
    }
    if (cur >= endpos) {
      // std::cerr << "ScannerString2\n";
      return 0;
    }
    return *cur;
  }

  bool StringScanner::peekChar(uint8_t& chr, size_t offset) const
  {
    const char* cur = position + offset;
    if (cur < startpos) {
      return false;
    }
    if (cur >= endpos) {
      return false;
    }
    chr = *cur;
    return true;
  }

  // If the next character in the string is [character], consumes it.
  // Returns whether or not [character] was consumed.
  bool StringScanner::scanChar(uint8_t character)
  {
    if (isDone()) return false;
    uint8_t ascii = *position;
    if (character != ascii) return false;
    consumedChar(character);
    position += 1;
    return true;
  }

  // If the next character in the string is [character], consumes it.
  // If [character] could not be consumed, throws a [FormatException]
  // describing the position of the failure. [name] is used in this
  // error as the expected name of the character being matched; if
  // it's `null`, the character itself is used instead.
  void StringScanner::expectChar(uint8_t character, std::string name)
  {
    if (scanChar(character)) return;

    if (name.empty()) {
      if (character == $backslash) {
        _fail("\"\\\"");
      }
      else if (character == $double_quote) {
        _fail("\"\\\"\"");
      }
      else {
        std::string msg("\"");
        msg += character;
        _fail(msg + "\"");
      }
    }

    _fail(name);
  }

  // If [pattern] matches at the current position of the string, scans forward
  // until the end of the match. Returns whether or not [pattern] matched.
  bool StringScanner::scan(std::string pattern)
  {
    const char* cur = position;
    for (uint8_t code : pattern) {
      if (isDone()) return false;
      uint8_t ascii = *cur;
      if (ascii != code) return false;
      consumedChar(ascii);
      cur += 1;
    }
    position = cur;
    return true;
  }

  void StringScanner::expect(std::string pattern, std::string name)
  {
    if (scan(pattern)) return;

    if (name.empty()) {
      /*
      if (pattern is RegExp) {
        var source = pattern.pattern;
        // if (!_slashAutoEscape) source = source.replaceAll("/", "\\/");
        _fail("/$source/");
      }
      else {
        // name = pattern.toString().replaceAll("\\", "\\\\").replaceAll('"', '\\"');
        _fail("\"$name\"");
      }
      */
      _fail(pattern);
    }
    _fail(name);
  }

  void StringScanner::expectDone()
  {
    if (isDone()) return;
    std::cerr << "no more input\n";
    exit(1);
  }

  bool StringScanner::matches(std::string pattern)
  {
    const char* cur = position;
    for (char chr : pattern) {
      if (chr != *cur) {
        return false;
      }
      cur += 1;
    }
    // position = cur;
    return true;
  }

  std::string StringScanner::substring(const char* start, const char* end)
  {
    if (end == 0) end = position;
    return std::string(start, end);
  }

  std::string StringScanner::substring(Position start, const char* end)
  {
    if (end == 0) end = position;
    return substring(start.position, end);
  }

  // Throws a [FormatException] describing that [name] is
// expected at the current position in the string.
  void StringScanner::_fail(std::string name) const
  {
    std::string msg("expected " + name + ".");
    throw Exception::InvalidSyntax("[pstate]", {}, msg);
    std::cerr << "fail " << msg << "\n"; exit(1);
  }

  void StringScanner::error(std::string msg) const
  {
    // traces.push_back(Backtrace(pstate));
    throw Exception::InvalidSyntax("[pstate]", {}, msg);
    // std::cerr << "error " << name << "\n"; exit(1);
  }

  void StringScanner::error(std::string msg, ParserState pstate) const
  {
    // traces.push_back(Backtrace(pstate));
    throw Exception::InvalidSyntax(pstate, {}, msg);
    // std::cerr << "error " << name << "\n"; exit(1);
  }

  bool StringScanner::hasLineBreak(const char* before) const
  {
    const char* cur = before;
    while (cur < endpos) {
      if (*cur == '\r') return true;
      if (*cur == '\n') return true;
      cur += 1;
    }
    return false;
  }

}
