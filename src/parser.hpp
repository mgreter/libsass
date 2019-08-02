#ifndef SASS_PARSER_H
#define SASS_PARSER_H

#include "sass.hpp"

// Parser has a great chance for stack overflow
// To fix this once and for all we need a different approach
// http://lambda-the-ultimate.org/node/1599
// Basically we should not call into recursion, but rather
// return some continuation bit, of course we still need to
// add our ast nodes somewhere (instead of returning as now)

#include <string>

#include "mapping.hpp"
#include "character.hpp"
#include "scanner_span.hpp"
#include "interpolation.hpp"
#include "error_handling.hpp"
#include "var_stack.hpp"

namespace Sass {

  class Parser {

  public:

    // Original source mappings. If defined, SourceSpan2s will
    // point to where the original source has come from. 
    // sass::vector<Mapping> srcmap;

    Context& context;

    bool disableEnvOptimization;

    /// Returns whether [string1] and [string2] are equal, ignoring ASCII case.
    bool equalsIgnoreCase(sass::string string1, sass::string string2)
    {
      if (string1.length() != string2.length()) return false;
      if (string1 == string2) return true;
      for (size_t i = 0; i < string1.length(); i++) {
        if (!Character::characterEqualsIgnoreCase(string1[i], string2[i])) {
          return false;
        }
      }
      return true;
    }

    Parser(
      Context& context,
      SourceDataObj source);

  public: // protected

    // The scanner that scans through the text being parsed.
    SpanScanner scanner;

    // The logger to use when emitting warnings.
    // Logger logger;

  public:

    // Returns whether [text] is a valid CSS identifier.
    bool isIdentifier(sass::string text);

    // Consumes whitespace, including any comments.
    virtual inline void whitespace()
    {
      do {
        whitespaceWithoutComments();
      } while (scanComment());
    }

    // Like [whitespace], but returns whether any was consumed.
    // bool scanWhitespace();

    // Consumes whitespace, but not comments.
    inline void whitespaceWithoutComments()
    {
      while (!scanner.isDone() && Character::isWhitespace(scanner.peekChar())) {
        scanner.readChar();
      }
    }

    // Consumes spaces and tabs.
    inline void spaces()
    {
      while (!scanner.isDone() && Character::isSpaceOrTab(scanner.peekChar())) {
        scanner.readChar();
      }
    }

    // Consumes and ignores a comment if possible.
    // Returns whether the comment was consumed.
    bool scanComment();

    // Consumes and ignores a silent (Sass-style) comment.
    virtual void silentComment();

    // Consumes and ignores a loud (CSS-style) comment.
    void loudComment();

    // Consumes a plain CSS identifier. If [unit] is `true`, this 
    // doesn't parse a `-` followed by a digit. This ensures that 
    // `1px-2px` parses as subtraction rather than the unit `px-2px`.
    sass::string identifier(bool unit = false);
    StringLiteral* identifierLiteral(bool unit = false);

    // Consumes a chunk of a plain CSS identifier after the name start.
    sass::string identifierBody();

    // Like [_identifierBody], but parses the body into the [text] buffer.
    void _identifierBody(StringBuffer& text, bool unit = false);

    // Consumes a plain CSS string. This returns the parsed contents of the 
    // string—that is, it doesn't include quotes and its escapes are resolved.
    sass::string string();

    // Consumes and returns a natural number.
    // That is, a non - negative integer.
    // Doesn't support scientific notation.
    double naturalNumber();

    // Consumes tokens until it reaches a top-level `";"`, `")"`, `"]"`,
    // or `"}"` and returns their contents as a string. If [allowEmpty]
    // is `false` (the default), this requires at least one token.
    sass::string declarationValue(bool allowEmpty = false);

    // Consumes a `url()` token if possible, and returns `null` otherwise.
    sass::string tryUrl();

    // Consumes a Sass variable name, and returns
    // its name without the dollar sign.
    sass::string variableName();

    // Consumes an escape sequence and returns the text that defines it.
    // If [identifierStart] is true, this normalizes the escape sequence
    // as though it were at the beginning of an identifier.
    sass::string escape(bool identifierStart = false);

    // Consumes an escape sequence and returns the character it represents.
    uint32_t escapeCharacter();

    // Consumes the next character if it matches [condition].
    // Returns whether or not the character was consumed.
    bool scanCharIf(bool (*condition)(uint8_t character));

    // Consumes the next character if it's equal
    // to [letter], ignoring ASCII case.
    bool scanCharIgnoreCase(uint32_t letter);

    // Consumes the next character and asserts that
    // it's equal to [letter], ignoring ASCII case.
    void expectCharIgnoreCase(uint32_t letter);

    // Returns whether the scanner is immediately before a number. This follows [the CSS algorithm].
    // [the CSS algorithm]: https://drafts.csswg.org/css-syntax-3/#starts-with-a-number
    bool lookingAtNumber() const;

    // Returns whether the scanner is immediately before a plain CSS identifier.
    // If [forward] is passed, this looks that many characters forward instead.
    // This is based on [the CSS algorithm][], but it assumes all backslashes start escapes.
    // [the CSS algorithm]: https://drafts.csswg.org/css-syntax-3/#would-start-an-identifier
    bool lookingAtIdentifier(size_t forward = 0) const;

    // Returns whether the scanner is immediately before a sequence
    // of characters that could be part of a plain CSS identifier body.
    bool lookingAtIdentifierBody();

    // Consumes an identifier if its name exactly matches [text].
    bool scanIdentifier(sass::string text);

    // Consumes an identifier and asserts that its name exactly matches [text].
    void expectIdentifier(sass::string text, sass::string name = "");

    // Runs [consumer] and returns the source text that it consumes.
    // sass::string rawText(sass::string(Parser::*)());
    // sass::string rawText(void(Parser::*)());

    // ToDo: make template to ignore return type
    template <typename T, typename X>
    sass::string rawText(T(X::* consumer)())
    {
      const char* start = scanner.position;
      // We need to clean up after ourself
      (static_cast<X*>(this)->*consumer)();
      return scanner.substring(start);
    }

    // Prints a warning to standard error, associated with [span].
    void warn(sass::string message, SourceSpan pstate);

    // Prints a warning to standard error, associated with [span].
    // void warn(sass::string message /* , FileSpan span */) {
    //   // throw Exception::InvalidSyntax("[pstate]", {}, message);
    //   std::cerr << "Warn: " << message << "\n";
    //   // logger.warn(message, span: span);
    // }

    // Throws an error associated with [span].
    // void error(sass::string message /*, FileSpan span */) {
    //   throw Exception::InvalidSyntax("[pstate]", {}, message);
    //   std::cerr << "Error: " << message << "\n";
    //   // throw StringScannerException(message, span, scanner.string);
    // }

    // Throws an error associated with [span].
    void error(sass::string message, SourceSpan pstate /*, FileSpan span */);

    void error(sass::string message, Backtraces& traces, SourceSpan pstate /*, FileSpan span */);

    // Prints a source span highlight of the current location being scanned.
    // If [message] is passed, prints that as well. This is
    // intended for use when debugging parser failures.
    void debug(sass::string message) {
      std::cerr << "DEBUG: " << message << "\n";
      if (message.empty()) {
        // print(scanner.emptySpan.highlight(color: true));
      }
      else {
        // print(scanner.emptySpan.message(message.toString(), color: true));
      }
    }

    // If [position] is separated from the previous non-whitespace character
    // in `scanner.string` by one or more newlines, returns the offset of the
    // last separating newline. Otherwise returns [position]. This helps avoid 
    // missing token errors pointing at the next closing bracket rather than
    // the line where the problem actually occurred.
    // const char* _firstNewlineBefore(const char* position);

  };

}

#endif
