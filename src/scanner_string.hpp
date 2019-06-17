#ifndef SASS_SCANNER_STRING_H
#define SASS_SCANNER_STRING_H

#include "utf8/core.h"
#include "position.hpp"

namespace Sass {

  // A class that scans through a string using [Pattern]s.
  class StringScanner {

  public:

    StringScanner(
      const char* startpos,
      const char* sourceUrl,
      size_t srcid);

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
    Position offset;

  public:

    // Whether the scanner has completely consumed [string].
    bool isDone() const;

    // Called whenever a character is consumed.
    // Used to update scanner line/column position.
    void consumedChar(uint8_t character);

    // Consumes a single character and returns its character
    // code. This throws a [FormatException] if the string has
    // been fully consumed. It doesn't affect [lastMatch].
    virtual uint8_t readChar();

    // Returns the character code of the character [offset] away
    // from [position]. [offset] defaults to zero, and may be negative
    // to inspect already-consumed characters. This returns `null` if
    // [offset] points outside the string. It doesn't affect [lastMatch].
    uint8_t peekChar(size_t offset = 0) const;

    bool peekChar(uint8_t& chr, size_t offset = 0) const;

    // If the next character in the string is [character], consumes it.
    // Returns whether or not [character] was consumed.
    virtual bool scanChar(uint8_t character);

    // If the next character in the string is [character], consumes it.
    // If [character] could not be consumed, throws a [FormatException]
    // describing the position of the failure. [name] is used in this
    // error as the expected name of the character being matched; if
    // it's `null`, the character itself is used instead.
    void expectChar(uint8_t character, std::string name = "");

    // If [pattern] matches at the current position of the string, scans forward
    // until the end of the match. Returns whether or not [pattern] matched.
    virtual bool scan(std::string pattern);

    // If [pattern] matches at the current position of the string, scans
    // forward until the end of the match. If [pattern] did not match,
    // throws a [FormatException] describing the position of the failure.
    // [name] is used in this error as the expected name of the pattern
    // being matched; if it's `null`, the pattern itself is used instead.
    void expect(std::string pattern, std::string name = "");

    // If the string has not been fully consumed,
    // this throws a [FormatException].
    void expectDone();

    // Returns whether or not [pattern] matches at the current position
    // of the string. This doesn't move the scan pointer forward.
    bool matches(std::string pattern);

    // Returns the substring of [string] between [start] and [end].
    // Unlike [String.substring], [end] defaults to [position]
    // rather than the end of the string.
    std::string substring(const char* start, const char* end = 0);
    std::string substring(Position start, const char* end = 0);


    // Throws a [FormatException] describing that [name] is
    // expected at the current position in the string.
    void _fail(std::string name) const;

    void error(std::string name) const;
    void error(std::string name, ParserState pstate) const;

    bool hasLineBreak(const char* before) const;

  };

}

#endif
