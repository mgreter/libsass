// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include <iostream>

#include "charcode.hpp"
#include "character.hpp"
#include "utf8/checked.h"
#include "error_handling.hpp"
#include "scanner_string.hpp"

namespace Sass {

  // Import some namespaces
  using namespace Charcode;
  using namespace Character;

  StringScanner::StringScanner(
    Logger& logger,
    SourceDataObj source) :
    source(source),
    nextMap(sass::string::npos),
    startpos(source->begin()),
    endpos(source->end()),
    position(source->begin()),
    sourceUrl(source->getAbsPath()),
    srcid(source->getSrcId()),
    offset(),
    relevant(),
    logger(logger)
  {
    // consume BOM?

    // Check if we have source mappings
    // ToDo: Can we make this API better?
    //if (source->hasMapping(0)) {
    //  nextMap = 0;
    //}

    // This can use up to 3% runtime (mostly under 1%)
    auto invalid = utf8::find_invalid(startpos, endpos);
    if (invalid != endpos) {
      SourceSpan pstate(source);
      Offset start(startpos, invalid);
      pstate.position.line = start.line;
      pstate.position.column = start.column;
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
    switch (character) {
    case $lf:
      offset.line += 1;
      offset.column = 0;
      break;
    case $space:
    case $tab:
    case $vt:
    case $ff:
    case $cr:
      offset.column += 1;
      break;
    default:
      // skip over 10xxxxxx and 01xxxxxx
      // count ASCII and first utf8 bytes
      if (Character::isCharacter(character)) {
        // 64 => first utf8 byte
        // 128 => regular ASCII char
        offset.column += 1;
        // sync relevant position
        relevant = offset;
      }
      break;
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
    if (cur < startpos || cur >= endpos) {
      return 0;
    }
    return *cur;
  }

  // Same as above, but stores next char into passed variable.
  bool StringScanner::peekChar(uint8_t& chr, size_t offset) const
  {
    const char* cur = position + offset;
    if (cur < startpos || cur >= endpos) {
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
  // it's empty, the character itself is used instead.
  void StringScanner::expectChar(uint8_t character, const sass::string& name)
  {
    if (!scanChar(character)) {
      if (name.empty()) {
        if (character == $double_quote) {
          _fail("\"\\\"\"");
        }
        else {
          sass::string msg("\"");
          msg += character;
          _fail(msg + "\"");
        }
      }
      _fail(name);
    }
  }

  // If [pattern] matches at the current position of the string, scans forward
  // until the end of the match. Returns whether or not [pattern] matched.
  bool StringScanner::scan(const sass::string& pattern)
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

  void StringScanner::expect(const sass::string& pattern, const sass::string& name)
  {
    if (!scan(pattern)) {
      if (name.empty()) {
        _fail(pattern);
      }
      _fail(name);
    }
  }

  void StringScanner::expectDone()
  {
    if (isDone()) return;
    std::cerr << "no more input\n";
    exit(1);
  }

  bool StringScanner::matches(const sass::string& pattern)
  {
    const char* cur = position;
    for (char chr : pattern) {
      if (chr != *cur) {
        return false;
      }
      cur += 1;
    }
    return true;
  }

  sass::string StringScanner::substring(const char* start, const char* end)
  {
    if (end == 0) end = position;
    return sass::string(start, end);
  }

  // Throws a [FormatException] describing that [name] is
  // expected at the current position in the string.
  void StringScanner::_fail(
    const sass::string& name) const
  {
    SourceSpan span(relevantSpan());
    callStackFrame frame(logger, span);
    sass::string msg("expected " + name + ".");
    throw Exception::InvalidSyntax(
      logger, msg);
    std::cerr << "fail " << msg << "\n"; exit(1);
  }

  void StringScanner::error(
    const sass::string& message,
    const BackTraces& traces,
    const SourceSpan& pstate) const
  {
    callStackFrame frame(logger, pstate);
    throw Exception::InvalidSyntax(
      traces, message);
  }

  bool StringScanner::hasLineBreak(
    const char* before) const
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
