#ifndef SASS_SCANNER_STRING_H
#define SASS_SCANNER_STRING_H

#include "utf8/core.h"
#include "source.hpp"
#include "logger.hpp"
#include "allocator.hpp"
#include "mapping.hpp"
#include "position.hpp"
#include "backtrace.hpp"
#include "source_span.hpp"
#include "source_state.hpp"
#include "util_string.hpp"
#include "charcode.hpp"
#include "character.hpp"
#include "strings.hpp"

namespace Sass {

  // Import some namespaces
  using namespace Charcode;
  using namespace Character;

  struct LineScannerState2 {
    const char* position;
    Offset offset;
  };

  // A class that scans through a string using [Pattern]s.
  class StringScanner {

  public:

    StringScanner(
      Logger& logger,
      SourceDataObj source);

    // The source associated with this scanner
    SourceDataObj source;

    // Next source mapping
    size_t nextMap;

    // The string being scanned through.
    const char* startpos;

    // The final position to scan to.
    const char* endpos;

    // The current position.
    const char* position;

    // The URL of the source of the string being scanned.
    // This is used for error reporting. It may be `null`,
    // indicating that the source URL is unknown or unavailable.
    const char* sourceUrl;

    // The global id for this input file.
    size_t srcid;

    // The current line/col offset
    Offset offset;

    // Last non whitespace position
    // Used to create parser state spans
    LineScannerState2 lastRelevant;

    // Attached logger
    Logger& logger;

  public:

    // Whether the scanner has completely consumed [string].
    bool isDone() const;

    // Called whenever a character is consumed.
    // Used to update scanner line/column position.
    inline void consumedChar(uint8_t character)
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
        offset.column += 1;
        break;
      default:
        // skip over 10xxxxxx and 01xxxxxx
        // count ascii and first utf8 bytes
        if (Util::is_character(character)) {
          // 64 => first utf8 byte
          // 128 => regular ascii char
          offset.column += 1;
          // sync lastRelevant position
          lastRelevant.offset = offset;
          lastRelevant.position = position + 1;
        }
        break;
      }
    }

    // Consumes a single character and returns its character
    // code. This throws a [FormatException] if the string has
    // been fully consumed. It doesn't affect [lastMatch].
    uint8_t readChar();

    // Returns the character code of the character [offset] away
    // from [position]. [offset] defaults to zero, and may be negative
    // to inspect already-consumed characters. This returns `null` if
    // [offset] points outside the string. It doesn't affect [lastMatch].
    uint8_t peekChar(size_t offset = 0) const;

    bool peekChar(uint8_t& chr, size_t offset = 0) const;

    // If the next character in the string is [character], consumes it.
    // Returns whether or not [character] was consumed.
    bool scanChar(uint8_t character);

    // If the next character in the string is [character], consumes it.
    // If [character] could not be consumed, throws a [FormatException]
    // describing the position of the failure. [name] is used in this
    // error as the expected name of the character being matched; if
    // it's `null`, the character itself is used instead.
    void expectChar(uint8_t character, const sass::string& name = Strings::empty);

    // If [pattern] matches at the current position of the string, scans forward
    // until the end of the match. Returns whether or not [pattern] matched.
    bool scan(const sass::string& pattern);

    // If [pattern] matches at the current position of the string, scans
    // forward until the end of the match. If [pattern] did not match,
    // throws a [FormatException] describing the position of the failure.
    // [name] is used in this error as the expected name of the pattern
    // being matched; if it's `null`, the pattern itself is used instead.
    void expect(const sass::string& pattern, const sass::string& name = Strings::empty);

    // If the string has not been fully consumed,
    // this throws a [FormatException].
    void expectDone();

    // Returns whether or not [pattern] matches at the current position
    // of the string. This doesn't move the scan pointer forward.
    bool matches(const sass::string& pattern);

    // Returns the substring of [string] between [start] and [end].
    // Unlike [String.substring], [end] defaults to [position]
    // rather than the end of the string.
    sass::string substring(const char* start, const char* end = 0);


    // Throws a [FormatException] describing that [name] is
    // expected at the current position in the string.
    void _fail(const sass::string& name) const;

    //    void error(sass::string name) const;

//    void error(sass::string name, SourceSpan pstate) const;

    void error(const sass::string& name,
      const Backtraces& traces,
      const SourceSpan& pstate) const;

    bool hasLineBreak(const char* before) const;

    LineScannerState2 state() {
      return { position, offset };
    }

    void resetState(const LineScannerState2& state);

    SourceSpan pstate(const LineScannerState2& start) {
      return source->adjustSourceSpan(
        SourceSpan(source, start.offset,
          Offset::distance(start.offset, offset)));
    }

    SourceSpan pstate(const Position& start,
      bool backupWhitespace = false);

    SourceSpan pstate(const Offset& start,
      bool backupWhitespace = false);

    SourceSpan pstate9();
    SourceSpan pstate9(const Offset& start);

    SourceSpan pstate43();

    SourceSpan pstate(const Position& start,
      const char* position);

    SourceSpan pstate() const;

    SourceState srcState() const;
    SourceSpan srcSpan(const Offset& start) const;

    SourceSpan spanFrom(const Offset& start);

  };

}

#endif
