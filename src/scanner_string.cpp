#include <iostream>

#include "sass.hpp"
#include "charcode.hpp"
#include "character.hpp"
#include "util_string.hpp"
#include "scanner_string.hpp"
#include "error_handling.hpp"
#include "utf8/checked.h"
#include "logger.hpp"

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
    sourceUrl(source->getPath()),
    srcid(source->getSrcId()),
    offset(),
    lastRelevant(),
    logger(logger)
  {
    // consume BOM?

    // Check if we have source mappings
    // ToDo: Can we make this API better?
    if (source->hasMapping(0)) {
      nextMap = 0;
    }

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
  void StringScanner::expectChar(uint8_t character, const sass::string& name)
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
        sass::string msg("\"");
        msg += character;
        _fail(msg + "\"");
      }
    }

    _fail(name);
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

  bool StringScanner::matches(const sass::string& pattern)
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
    callStackFrame frame(logger, pstate());
    sass::string msg("expected " + name + ".");
    throw Exception::InvalidSyntax(
      pstate(), logger, msg);
    std::cerr << "fail " << msg << "\n"; exit(1);
  }
  /*
  void StringScanner::error(sass::string msg) const
  {
    // traces.emplace_back(Backtrace(pstate));
    throw Exception::InvalidSyntax("[pstateD2]", {}, msg);
    // std::cerr << "error " << name << "\n"; exit(1);
  }

  void StringScanner::error(sass::string msg, SourceSpan pstate) const
  {
    // traces.emplace_back(Backtrace(pstate));
    throw Exception::InvalidSyntax(pstate, {}, msg);
    // std::cerr << "error " << name << "\n"; exit(1);
  }
  */

  void StringScanner::error(
    const sass::string& message,
    const Backtraces& traces,
    const SourceSpan& pstate) const
  {
    throw Exception::InvalidSyntax(
      pstate, traces, message);
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

  SourceState StringScanner::srcState() const
  {
    return SourceState(source, offset);
  }

  SourceSpan StringScanner::srcSpan(const Offset& start) const
  {
    return SourceSpan(source, start, offset);
  }

}
