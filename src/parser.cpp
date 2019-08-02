#include "parser.hpp"

#include "character.hpp"
#include "utf8/checked.h"

#include "error_handling.hpp"
#include "util_string.hpp"
#include "context.hpp"

namespace Sass {

  // Import some namespaces
  using namespace Charcode;
  using namespace Character;
  
  // create utf8 encoded string from charcodes
  sass::string StringFromCharCode(uint32_t character)
  {
    sass::string result;
    utf8::append(character,
      std::back_inserter(result));
    return result;
  }

  // create utf8 encoded string from charcodes
  sass::string StringFromCharCodes(sass::vector<uint32_t> characters)
  {
    sass::string result;
    for (uint32_t code : characters) {
      // std::cerr << "add code " << code << "\n";
      utf8::append(code, std::back_inserter(result));
    }
    // ebuggerstd::cerr << "FROM CHARCODES [" << result << "]\n";
    return result;
  }

  Parser::Parser(
    Context& context,
    SourceDataObj source) :
    context(context),
    disableEnvOptimization(false),
    scanner(*context.logger, source)
  {
    // context.varStack.push_back(&context.varRoot);
  }

  bool Parser::isIdentifier(sass::string text)
  {
    try {
      auto qwe = SASS_MEMORY_NEW(SourceFile,
        "asd", text.c_str(), -1);
      Parser p2(context, qwe);
      p2.identifier(); // try to parse
      return p2.scanner.isDone();
    }
    catch (Exception::InvalidSyntax&) {
      return false;
    }
  }

  // Consumes whitespace, including any comments.
  // void Parser::whitespace()
  // {
  //   do {
  //     whitespaceWithoutComments();
  //   } while (scanComment());
  // }

  // Like [whitespace], but returns whether any was consumed.
  // bool Parser::scanWhitespace()
  // {
  //   auto start = scanner.position;
  //   whitespace(); // consume spaces
  //   return scanner.position != start;
  // }

  // Consumes whitespace, but not comments.
  // void Parser::whitespaceWithoutComments()
  // {
  //   while (!scanner.isDone() && isWhitespace(scanner.peekChar())) {
  //     scanner.readChar();
  //   }
  // }

  // Consumes spaces and tabs.
  // void Parser::spaces()
  // {
  //   while (!scanner.isDone() && isSpaceOrTab(scanner.peekChar())) {
  //     scanner.readChar();
  //   }
  // }

  // Consumes and ignores a comment if possible.
  // Returns whether the comment was consumed.
  bool Parser::scanComment()
  {
    if (scanner.peekChar() != $slash) return false;

    auto next = scanner.peekChar(1);
    if (next == $slash) {
      silentComment();
      return true;
    }
    else if (next == $asterisk) {
      loudComment();
      return true;
    }
    else {
      return false;
    }
  }

  // Consumes and ignores a silent (Sass-style) comment.
  void Parser::silentComment()
  {
    scanner.expect("//");
    while (!scanner.isDone() && !isNewline(scanner.peekChar())) {
      scanner.readChar();
    }
  }

  // Consumes and ignores a loud (CSS-style) comment.
  void Parser::loudComment()
  {
    scanner.expect("/*");
    while (true) {
      auto next = scanner.readChar();
      if (next != $asterisk) continue;

      do {
        next = scanner.readChar();
      } while (next == $asterisk);
      if (next == $slash) break;
    }
  }

  // Consumes a plain CSS identifier. If [unit] is `true`, this 
  // doesn't parse a `-` followed by a digit. This ensures that 
  // `1px-2px` parses as subtraction rather than the unit `px-2px`.
  sass::string Parser::identifier(bool unit)
  {

    // NOTE: this logic is largely duplicated in StylesheetParser._interpolatedIdentifier
    // and isIdentifier in utils.dart. Most changes here should be mirrored there.

    Position start(scanner);
    // std::cerr << scanner.position << "\n";
    StringBuffer text;
    while (scanner.scanChar($dash)) {
      text.write($dash);
    }

    auto first = scanner.peekChar();
    // std::cerr << "GOT CHAR [" << (char)first << "]\n";
    if (first == $nul) {
      scanner.error("Expected identifier.",
        *context.logger, scanner.pstate(start));
    }
    else if (isNameStart(first)) {
     // std::cerr << "we start with name\n";
      text.write(scanner.readChar());
    }
    else if (first == $backslash) {
      text.write(escape(true)); // identifierStart: 
    }
    else {
      scanner.error("Expected identifier.",
        *context.logger, scanner.pstate(start));
    }

    _identifierBody(text, unit);

    // std::cerr << "rv identifier [" << text.toString() << "]\n";

    return text.toString();

  }

  // Consumes a plain CSS identifier. If [unit] is `true`, this 
  // doesn't parse a `-` followed by a digit. This ensures that 
  // `1px-2px` parses as subtraction rather than the unit `px-2px`.
  StringLiteral* Parser::identifierLiteral(bool unit)
  {

    // NOTE: this logic is largely duplicated in StylesheetParser._interpolatedIdentifier
    // and isIdentifier in utils.dart. Most changes here should be mirrored there.

    Position start(scanner);
    // std::cerr << scanner.position << "\n";
    StringBuffer text;
    while (scanner.scanChar($dash)) {
      text.write($dash);
    }

    auto first = scanner.peekChar();
    // std::cerr << "GOT CHAR [" << (char)first << "]\n";
    if (first == $nul) {
      scanner.error("Expected identifier.",
        *context.logger, scanner.pstate(start));
    }
    else if (isNameStart(first)) {
      // std::cerr << "we start with name\n";
      text.write(scanner.readChar());
    }
    else if (first == $backslash) {
      text.write(escape(true)); // identifierStart: 
    }
    else {
      scanner.error("Expected identifier.",
        *context.logger, scanner.pstate(start));
    }

    _identifierBody(text, unit);

    // std::cerr << "rv identifier [" << text.toString() << "]\n";

    return SASS_MEMORY_NEW(StringLiteral,
      scanner.pstate9(start), text.toString());

  }

  // Consumes a chunk of a plain CSS identifier after the name start.
  sass::string Parser::identifierBody()
  {
    StringBuffer text;
    _identifierBody(text);
    if (text.empty()) {
      scanner.error(
        "Expected identifier body.",
        *context.logger, scanner.pstate());
    }
    return text.toString();
  }

  // Like [_identifierBody], but parses the body into the [text] buffer.
  void Parser::_identifierBody(StringBuffer& text, bool unit)
  {
    while (true) {
      uint8_t next = scanner.peekChar();
      if (next == $nul) {
        break;
      }
      else if (unit && next == $dash) {
        // Disallow `-` followed by a dot or a digit digit in units.
        uint8_t second = scanner.peekChar(1);
        if (second != $nul && (second == $dot || isDigit(second))) break;
        text.write(scanner.readChar());
      }
      else if (isName(next)) {
        text.write(scanner.readChar());
      }
      else if (next == $backslash) {
        text.write(escape());
      }
      else {
        break;
      }
    }
  }

  // Consumes a plain CSS string. This returns the parsed contents of the 
  // string—that is, it doesn't include quotes and its escapes are resolved.
  sass::string Parser::string()
  {
    // NOTE: this logic is largely duplicated in ScssParser._interpolatedString.
        // Most changes here should be mirrored there.

    uint8_t quote = scanner.readChar();
    if (quote != $single_quote && quote != $double_quote) {
      scanner.error("Expected string.",
        *context.logger, scanner.pstate());
        /*,
        position: quote == null ? scanner.position : scanner.position - 1*/
    }

    StringBuffer buffer;
    while (true) {
      uint8_t next = scanner.peekChar();
      if (next == quote) {
        scanner.readChar();
        break;
      }
      else if (next == $nul || isNewline(next)) {
        sass::sstream strm;
        strm << "Expected " << quote << ".";
        scanner.error(strm.str(),
          *context.logger,
          scanner.pstate());
      }
      else if (next == $backslash) {
        if (isNewline(scanner.peekChar(1))) {
          scanner.readChar();
          scanner.readChar();
        }
        else {
          buffer.writeCharCode(escapeCharacter());
        }
      }
      else {
        buffer.write(scanner.readChar());
      }
    }

    return buffer.toString();
  }

  // Consumes and returns a natural number.
  // That is, a non - negative integer.
  // Doesn't support scientific notation.
  double Parser::naturalNumber()
  {
    if (!isDigit(scanner.peekChar())) {
      scanner.error("Expected digit.",
        *context.logger, scanner.pstate());
    }
    uint8_t first = scanner.readChar();
    double number = asDecimal(first);
    while (isDigit(scanner.peekChar())) {
      number = asDecimal(scanner.readChar())
        + number * 10;
    }
    return number;
  }
  // EO naturalNumber

  // Consumes tokens until it reaches a top-level `";"`, `")"`, `"]"`,
  // or `"}"` and returns their contents as a string. If [allowEmpty]
  // is `false` (the default), this requires at least one token.
  sass::string Parser::declarationValue(bool allowEmpty)
  {
    // NOTE: this logic is largely duplicated in
    // StylesheetParser._interpolatedDeclarationValue.
    // Most changes here should be mirrored there.

    sass::string url;
    StringBuffer buffer;
    bool wroteNewline = false;
    sass::vector<uint8_t> brackets;

    while (true) {
      uint8_t next = scanner.peekChar();
      switch (next) {
      case $backslash:
        buffer.write(escape(true));
        wroteNewline = false;
        break;

      case $double_quote:
      case $single_quote:
        buffer.write(rawText(&Parser::string));
        wroteNewline = false;
        break;

      case $slash:
        if (scanner.peekChar(1) == $asterisk) {
          buffer.write(rawText(&Parser::loudComment));
        }
        else {
          buffer.write(scanner.readChar());
        }
        wroteNewline = false;
        break;

      case $space:
      case $tab:
        if (wroteNewline || !isWhitespace(scanner.peekChar(1))) {
          buffer.write($space);
        }
        scanner.readChar();
        break;

      case $lf:
      case $cr:
      case $ff:
        if (!isNewline(scanner.peekChar(-1))) {
          buffer.write("\n");
        }
        scanner.readChar();
        wroteNewline = true;
        break;

      case $lparen:
      case $lbrace:
      case $lbracket:
        buffer.write(next);
        brackets.emplace_back(opposite(scanner.readChar()));
        wroteNewline = false;
        break;

      case $rparen:
      case $rbrace:
      case $rbracket:
        if (brackets.empty()) goto outOfLoop;
        buffer.write(next);
        scanner.expectChar(brackets.back());
        wroteNewline = false;
        brackets.pop_back();
        break;

      case $semicolon:
        if (brackets.empty()) goto outOfLoop;
        buffer.write(scanner.readChar());
        break;

      case $u:
      case $U:
        url = tryUrl() ;
        if (!url.empty()) {
          buffer.write(url);
        }
        else {
          buffer.write(scanner.readChar());
        }
        wroteNewline = false;
        break;

      default:
        if (next == $nul) goto outOfLoop;

        if (lookingAtIdentifier()) {
          buffer.write(identifier());
        }
        else {
          buffer.write(scanner.readChar());
        }
        wroteNewline = false;
        break;
      }
    }

  outOfLoop:

    if (!brackets.empty()) scanner.expectChar(brackets.back());
    if (!allowEmpty && buffer.empty()) scanner.error(
      "Expected token.", *context.logger, scanner.pstate());
    return buffer.toString();

  }
  // EO declarationValue

  // Consumes a `url()` token if possible, and returns `null` otherwise.
  sass::string Parser::tryUrl()
  {

    // NOTE: this logic is largely duplicated in ScssParser._tryUrlContents.
    // Most changes here should be mirrored there.

    LineScannerState2 state = scanner.state();
    if (!scanIdentifier("url")) return "";

    if (!scanner.scanChar($lparen)) {
      scanner.resetState(state);
      return "";
    }

    whitespace();

    // Match Ruby Sass's behavior: parse a raw URL() if possible, and if not
    // backtrack and re-parse as a function expression.
    uint8_t next;
    StringBuffer buffer;
    buffer.write("url(");
    while (true) {
      if (!scanner.peekChar(next)) {
        break;
      }
      if (next == $percent ||
          next == $ampersand ||
          next == $hash ||
          (next >= $asterisk && next <= $tilde) ||
          next >= 0x0080) {
        buffer.write(scanner.readChar());
      }
      else if (next == $backslash) {
        buffer.write(escape());
      }
      else if (isWhitespace(next)) {
        whitespace();
        if (scanner.peekChar() != $rparen) break;
      }
      else if (next == $rparen) {
        buffer.write(scanner.readChar());
        return buffer.toString();
      }
      else {
        break;
      }
    }

    scanner.resetState(state);
    return "";
  }
  // EO tryUrl

  using StackVariable = sass::string;

  // Consumes a Sass variable name, and returns
  // its name without the dollar sign.
  StackVariable Parser::variableName()
  {
    scanner.expectChar($dollar);
    sass::string name("$" + identifier());
    // this is also called for parameters
    // context.varRoot.hoistVariable(name);
    // dart sass removes the dollar
    return name;
  }

  // Consumes an escape sequence and returns the text that defines it.
  // If [identifierStart] is true, this normalizes the escape sequence
  // as though it were at the beginning of an identifier.
  sass::string Parser::escape(bool identifierStart)
  {
    scanner.expectChar($backslash);
    uint32_t value = 0;
    uint8_t first, next;
    if (!scanner.peekChar(first)) {
      return "";
    }
    else if (isNewline(first)) {
      scanner.error("Expected escape sequence.",
        *context.logger, scanner.pstate());
      return "";
    }
    else if (isHex(first)) {
      for (uint8_t i = 0; i < 6; i++) {
        scanner.peekChar(next);
        if (next == $nul || !isHex(next)) break;
        value *= 16;
        value += asHex(scanner.readChar());
      }

      scanCharIf(isWhitespace);
    }
    else {
      value = scanner.readChar();
    }

    if (identifierStart ? isNameStart(value) : isName(value)) {
      return StringFromCharCode(value);
    }
    else if (value <= 0x1F ||
      value == 0x7F ||
      (identifierStart && isDigit(value))) {
      StringBuffer buffer;
      buffer.write($backslash);
      if (value > 0xF) buffer.write(hexCharFor(value >> 4));
      buffer.write(hexCharFor(value & 0xF));
      buffer.write($space);
      return buffer.toString();
    }
    else {
      return StringFromCharCodes({ $backslash, value });
    }
  }
  // EO tryUrl

  // Consumes an escape sequence and returns the character it represents.
  uint32_t Parser::escapeCharacter()
  {
    // See https://drafts.csswg.org/css-syntax-3/#consume-escaped-code-point.

    uint8_t first, next;
    scanner.expectChar($backslash);
    if (!scanner.peekChar(first)) {
      return 0xFFFD;
    }
    else if (isNewline(first)) {
      scanner.error("Expected escape sequence.",
        *context.logger, scanner.pstate());
      return 0;
    }
    else if (isHex(first)) {
      uint32_t value = 0;
      for (uint8_t i = 0; i < 6; i++) {
        if (!scanner.peekChar(next) || !isHex(next)) break;
        value = (value << 4) + asHex(scanner.readChar());
      }
      if (isWhitespace(scanner.peekChar())) {
        scanner.readChar();
      }

      if (value == 0 ||
        (value >= 0xD800 && value <= 0xDFFF) ||
        value >= 0x10FFFF) {
        return 0xFFFD;
      }
      else {
        return value;
      }
    }
    else {
      return scanner.readChar();
    }
  }

  // Consumes the next character if it matches [condition].
  // Returns whether or not the character was consumed.
  bool Parser::scanCharIf(bool(*condition)(uint8_t character))
  {
    uint8_t next = scanner.peekChar();
    if (!condition(next)) return false;
    scanner.readChar();
    return true;
  }

  // Consumes the next character if it's equal
  // to [letter], ignoring ASCII case.
  bool Parser::scanCharIgnoreCase(uint32_t letter)
  {
    if (!equalsLetterIgnoreCase(letter, scanner.peekChar())) return false;
    scanner.readChar();
    return true;
  }

  // Consumes the next character and asserts that
  // it's equal to [letter], ignoring ASCII case.
  void Parser::expectCharIgnoreCase(uint32_t letter)
  {
    SourceSpan span = scanner.pstate();
    uint8_t actual = scanner.readChar();
    if (equalsLetterIgnoreCase(letter, actual)) return;

    sass::string msg = "Expected \"";
    utf8::append(letter, std::back_inserter(msg));
    scanner.error(msg + "\".",
      *context.logger, span);

      // position: actual == null ? scanner.position : scanner.position - 1*/);
  }

  // Returns whether the scanner is immediately before a number. This follows [the CSS algorithm].
  // [the CSS algorithm]: https://drafts.csswg.org/css-syntax-3/#starts-with-a-number
  bool Parser::lookingAtNumber() const
  {
    uint8_t first, second, third;
    if (!scanner.peekChar(first)) return false;
    if (isDigit(first)) return true;

    if (first == $dot) {
      return scanner.peekChar(second, 1)
        && isDigit(second);
    }
    else if (first == $plus || first == $minus) {
      if (!scanner.peekChar(second, 1)) return false;
      if (isDigit(second)) return true;
      if (second != $dot) return false;

      return scanner.peekChar(third, 2)
        && isDigit(third);
    }
    else {
      return false;
    }
  }
  // EO lookingAtNumber

  // Returns whether the scanner is immediately before a plain CSS identifier.
  // If [forward] is passed, this looks that many characters forward instead.
  // This is based on [the CSS algorithm][], but it assumes all backslashes start escapes.
  // [the CSS algorithm]: https://drafts.csswg.org/css-syntax-3/#would-start-an-identifier
  bool Parser::lookingAtIdentifier(size_t forward) const
  {
    // See also [ScssParser._lookingAtInterpolatedIdentifier].
    uint8_t first, second, third;
    if (!scanner.peekChar(first, forward)) return false;
    if (isNameStart(first) || first == $backslash) return true;
    if (first != $dash) return false;

    if (!scanner.peekChar(second, forward + 1)) return false;
    if (isNameStart(second) || second == $backslash) return true;
    if (second != $dash) return false;

    return scanner.peekChar(third, forward + 2)
      && isNameStart(third);
  }
  // EO lookingAtIdentifier

  // Returns whether the scanner is immediately before a sequence
  // of characters that could be part of a plain CSS identifier body.
  bool Parser::lookingAtIdentifierBody()
  {
    uint8_t next;
    return scanner.peekChar(next)
      && (isName(next) || next == $backslash);
  }
  // EO lookingAtIdentifierBody

  // Consumes an identifier if its name exactly matches [text].
  bool Parser::scanIdentifier(sass::string text)
  {
    if (!lookingAtIdentifier()) return false;

    LineScannerState2 state = scanner.state();
    for (size_t i = 0; i < text.size(); i++) {
      uint8_t next = text[i]; // cast needed
      if (scanCharIgnoreCase(next)) continue;
      scanner.resetState(state);
      return false;
    }

    if (!lookingAtIdentifierBody()) return true;
    scanner.resetState(state);
    return false;
  }
  // EO scanIdentifier

  // Consumes an identifier and asserts that its name exactly matches [text].
  void Parser::expectIdentifier(sass::string text, sass::string name)
  {
    SourceSpan start(scanner.pstate());
    if (name.empty()) name = "\"" + text + "\"";
    for (size_t i = 0; i < text.size(); i++) {
      uint8_t next = text[i]; // cast needed
      if (scanCharIgnoreCase(next)) continue;
      scanner.error("Expected " + name + ".",
        *context.logger, scanner.pstate(start.position));
    }
    if (!lookingAtIdentifierBody()) return;
    scanner.error("Expected " + name + ".",
      *context.logger, scanner.pstate(start.position));
  }

  // Prints a warning to standard error, associated with [span].

  void Parser::warn(sass::string message, SourceSpan pstate) {
    warning(message, *context.logger, pstate);
  }

  // Throws an error associated with [span].

  void Parser::error(sass::string message, SourceSpan pstate) {
    callStackFrame frame(*context.logger, Backtrace(pstate));
    throw Exception::InvalidSyntax(pstate, *context.logger, message);
  }
  void Parser::error(sass::string message, Backtraces& traces, SourceSpan pstate) {
    callStackFrame frame(traces, Backtrace(pstate));
    throw Exception::InvalidSyntax(pstate, traces, message);
  }
  // EO expectIdentifier

  // If [position] is separated from the previous non-whitespace character
  // in `scanner.string` by one or more newlines, returns the offset of the
  // last separating newline. Otherwise returns [position]. This helps avoid 
  // missing token errors pointing at the next closing bracket rather than
  // the line where the problem actually occurred.
  // const char* Parser::_firstNewlineBefore(const char* position)
  // {
  //   const char* lastNewline = 0;
  //   const char* index = position - 1;
  //   while (index >= scanner.startpos) {
  //     if (!isWhitespace(*index)) {
  //       return lastNewline == 0 ?
  //         position : lastNewline;
  //     }
  //     if (isNewline(*index)) {
  //       lastNewline = index;
  //     }
  //     index -= 1;
  //   }
  // 
  //   // If the document *only* contains whitespace
  //   // before [position], always return [position].
  //   return position;
  // }
  // EO _firstNewlineBefore

}
