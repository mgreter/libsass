#include "parser_sass.hpp"

#include "character.hpp"
#include "utf8/checked.h"
#include "interpolation.hpp"

namespace Sass {

  // Import some namespaces
  using namespace Charcode;
  using namespace Character;

  Interpolation* SassParser::styleRuleSelector()
  {
    Offset start(scanner.offset);
    InterpolationBuffer buffer(scanner);
    do {
      buffer.addInterpolation(almostAnyValue());
      buffer.writeCharCode($lf);
    } while (buffer.trailingStringEndsWith(",") &&
      scanCharIf(isNewline));

    return buffer.getInterpolation(scanner.rawSpanFrom(start));
  }

  void SassParser::expectStatementSeparator(sass::string name) {
    if (!atEndOfStatement()) _expectNewline();
    if (_peekIndentation() <= _currentIndentation) return;
    sass::sstream strm;
    strm << "Nothing may be indented ";
    if (name.empty()) { strm << "here."; }
    else { strm << "beneath a " + name + "."; }
    whitespaceWithoutComments();
    scanner.error(
      strm.str(),
      *context.logger123,
      scanner.rawSpan());

      /*,
      position: _nextIndentationEnd.position*/
  }

  bool SassParser::atEndOfStatement()
  {
    uint8_t next;
    if (scanner.peekChar(next)) {
      return isNewline(next);
    }
    return true;
  }

  bool SassParser::lookingAtChildren()
  {
    return atEndOfStatement() &&
      _peekIndentation() > _currentIndentation;
  }

  void SassParser::importArgument(ImportRule* rule)
  {
    uint8_t next = scanner.peekChar();
    StringScannerState state(scanner.state());
    switch (next) {
    case $u:
    case $U:
      if (scanIdentifier("url")) {
        if (scanner.scanChar($lparen)) {
          scanner.resetState(state);
          StylesheetParser::importArgument(rule);
          return;
        }
        else {
          scanner.resetState(state);
        }
      }
      break;

    case $single_quote:
    case $double_quote:
      StylesheetParser::importArgument(rule);
      return;
    }

    Offset start(scanner.offset);
    StringScannerState state2(scanner.state());
    while (scanner.peekChar(next) &&
      next != $comma &&
      next != $semicolon &&
      !isNewline(next)) {
      scanner.readChar();
      next = scanner.peekChar();
    }

    sass::string url = scanner.substring(state2.position);

    if (_isPlainImportUrl(url)) {
      InterpolationObj itpl = SASS_MEMORY_NEW(
        Interpolation, scanner.relevantSpanFrom(start));
      auto str = SASS_MEMORY_NEW(SassString,
        scanner.relevantSpanFrom(start), url, true);
      // Must be an easier way to get quotes?
      str->value(str->to_string());
      itpl->append(str);
      rule->append(SASS_MEMORY_NEW(StaticImport,
        scanner.relevantSpanFrom(start), itpl));
    }
    else {

      resolveDynamicImport(rule, start, url);

    }

  }

  bool SassParser::scanElse(size_t ifIndentation)
  {
    if (_peekIndentation() != ifIndentation) return false;
    StringScannerState state(scanner.state());
    size_t startIndentation = _currentIndentation;
    size_t startNextIndentation = _nextIndentation;
    StringScannerState startNextIndentationEnd = _nextIndentationEnd;

    _readIndentation();
    if (scanner.scanChar($at) && scanIdentifier("else")) return true;

    scanner.resetState(state);
    _currentIndentation = startIndentation;
    _nextIndentation = startNextIndentation;
    _nextIndentationEnd = startNextIndentationEnd;
    return false;
  }

  sass::vector<StatementObj> SassParser::children(Statement* (StylesheetParser::* child)())
  {
    sass::vector<StatementObj> children;
    _whileIndentedLower(child, children);
    return children;
  }

  sass::vector<StatementObj> SassParser::statements(Statement* (StylesheetParser::* statement)())
  {
    uint8_t first = scanner.peekChar();
    if (first == $tab || first == $space) {
      scanner.error("Indenting at the beginning of the document is illegal.",
        *context.logger123, scanner.rawSpan());
        /*position: 0, length : scanner.position*/
    }

    sass::vector<StatementObj> statements;
    while (!scanner.isDone()) {
      Statement* child = _child(statement);
      if (child != nullptr) statements.emplace_back(child);
      size_t indentation = _readIndentation();
      SASS_ASSERT(indentation == 0, "indentation must be zero");
    }
    return statements;
  }

  Statement* SassParser::_child(Statement* (StylesheetParser::* child)())
  {
    switch (scanner.peekChar()) {
      // Ignore empty lines.
    case $cr:
    case $lf:
    case $ff:
      return nullptr;

    case $dollar:
      return variableDeclaration();
      break;

    case $slash:
      switch (scanner.peekChar(1)) {
      case $slash:
        return _silentComment();
        break;
      case $asterisk:
        return _loudComment();
        break;
      default:
        return (this->*child)();
        break;
      }
      break;

    default:
      return (this->*child)();
      break;
    }
  }

  SilentComment* SassParser::_silentComment()
  {
    Offset start(scanner.offset);
    scanner.expect("//");
    StringBuffer buffer;
    size_t parentIndentation = _currentIndentation;

    do {
      sass::string commentPrefix = scanner.scanChar($slash) ? "///" : "//";

      while (true) {
        buffer.write(commentPrefix);

        // Skip the initial characters because we're already writing the
        // slashes.
        for (size_t i = commentPrefix.length();
          i < _currentIndentation - parentIndentation;
          i++) {
          buffer.writeCharCode($space);
        }

        while (!scanner.isDone() && !isNewline(scanner.peekChar())) {
          buffer.writeCharCode(scanner.readChar());
        }
        buffer.write("\n");

        if (_peekIndentation() < parentIndentation) goto endOfLoop;

        if (_peekIndentation() == parentIndentation) {
          // Look ahead to the next line to see if it starts another comment.
          if (scanner.peekChar(1 + parentIndentation) == $slash &&
            scanner.peekChar(2 + parentIndentation) == $slash) {
            _readIndentation();
          }
          break;
        }
        _readIndentation();
      }
    } while (scanner.scan("//"));

  endOfLoop:

    lastSilentComment = SASS_MEMORY_NEW(SilentComment,
      scanner.rawSpanFrom(start), buffer.toString());
    return lastSilentComment;
  }

  LoudComment* SassParser::_loudComment()
  {
    Offset start(scanner.offset);
    scanner.expect("/*");

    bool first = true;
    InterpolationBuffer buffer(scanner);
    buffer.write("/*");
    size_t parentIndentation = _currentIndentation;
    while (true) {
      if (first) {
        // If the first line is empty, ignore it.
        const char* beginningOfComment = scanner.position;
        spaces();
        if (isNewline(scanner.peekChar())) {
          _readIndentation();
          buffer.writeCharCode($space);
        }
        else {
          buffer.write(scanner.substring(beginningOfComment));
        }
      }
      else {
        buffer.write("\n");
        buffer.write(" * ");
      }
      first = false;

      for (size_t i = 3; i < _currentIndentation - parentIndentation; i++) {
        buffer.writeCharCode($space);
      }

      while (!scanner.isDone()) {
        uint8_t next = scanner.peekChar();
        switch (next) {
        case $lf:
        case $cr:
        case $ff:
          goto endOfLoop;

        case $hash:
          if (scanner.peekChar(1) == $lbrace) {
            buffer.add(singleInterpolation());
          }
          else {
            buffer.writeCharCode(scanner.readChar());
          }
          break;

        default:
          buffer.writeCharCode(scanner.readChar());
          break;
        }
      }

    endOfLoop:

      if (_peekIndentation() <= parentIndentation) break;

      // Preserve empty lines.
      while (_lookingAtDoubleNewline()) {
        _expectNewline();
        buffer.write("\n");
        buffer.write(" *");
      }

      _readIndentation();
    }

    if (!buffer.trailingStringEndsWith("*/")) {
      buffer.write(" */");
    }

    InterpolationObj itpl = buffer.getInterpolation(scanner.rawSpanFrom(start));
    return SASS_MEMORY_NEW(LoudComment, itpl->pstate(), itpl);
  }

  void SassParser::whitespace()
  {
    // This overrides whitespace consumption so that
    // it doesn't consume newlines or loud comments.
    while (!scanner.isDone()) {
      uint8_t next = scanner.peekChar();
      if (next != $tab && next != $space) break;
      scanner.readChar();
    }

    if (scanner.peekChar() == $slash && scanner.peekChar(1) == $slash) {
      silentComment();
    }
  }

  void SassParser::_expectNewline()
  {
    switch (scanner.peekChar()) {
    case $semicolon:
      scanner.error("semicolons aren't allowed in the indented syntax.",
        *context.logger123, scanner.rawSpan());
      return;
    case $cr:
      scanner.readChar();
      if (scanner.peekChar() == $lf) scanner.readChar();
      return;
    case $lf:
    case $ff:
      scanner.readChar();
      return;
    default:
      scanner.error("expected newline.",
        *context.logger123, scanner.rawSpan());
    }
  }

  bool SassParser::_lookingAtDoubleNewline()
  {
    uint8_t next = scanner.peekChar();
    uint8_t nextChar = scanner.peekChar(1);
    switch (next) {
    case $cr:
      if (nextChar == $lf) return isNewline(scanner.peekChar(2));
      return nextChar == $cr || nextChar == $ff;
    case $lf:
    case $ff:
      return isNewline(scanner.peekChar(1));
    default:
      return false;
    }
  }

  void SassParser::_whileIndentedLower(Statement* (StylesheetParser::* child)(), sass::vector<StatementObj>& children)
  {
    size_t parentIndentation = _currentIndentation;
    size_t childIndentation = sass::string::npos;
    while (_peekIndentation() > parentIndentation) {
      size_t indentation = _readIndentation();
      if (childIndentation == sass::string::npos) {
        childIndentation = indentation;
      }
      if (childIndentation != indentation) {
        sass::sstream msg;
        msg << "Inconsistent indentation, expected "
          << childIndentation << " spaces.";
        scanner.error(msg.str(),
          *context.logger123,
          scanner.rawSpan());

          /*,
          position: scanner.position - scanner.column,
          length : scanner.column*/
      }
      children.emplace_back(_child(child));
    }

  }

  size_t SassParser::_readIndentation()
  {
    if (_nextIndentation == sass::string::npos) {
      _peekIndentation();
    }
    _currentIndentation = _nextIndentation;
    scanner.resetState(_nextIndentationEnd);
    _nextIndentation = sass::string::npos;
    // What does this mean, where is it used?
    // _nextIndentationEnd = null; ToDo
    return _currentIndentation;

  }

  size_t SassParser::_peekIndentation()
  {
    if (_nextIndentation != sass::string::npos) {
      return _nextIndentation;
    }

    if (scanner.isDone()) {
      _nextIndentation = 0;
      _nextIndentationEnd = scanner.state();
      return 0;
    }

    StringScannerState start = scanner.state();
    if (!scanCharIf(isNewline)) {
      scanner.error("Expected newline.",
        *context.logger123, scanner.rawSpan());
        /* position: scanner.position*/
    }

    bool containsTab;
    bool containsSpace;
    do {
      containsTab = false;
      containsSpace = false;
      _nextIndentation = 0;

      while (true) {
        uint8_t next = scanner.peekChar();
        if (next == $space) {
          containsSpace = true;
        }
        else if (next == $tab) {
          containsTab = true;
        }
        else {
          break;
        }
        _nextIndentation++;
        scanner.readChar();
      }

      if (scanner.isDone()) {
        _nextIndentation = 0;
        _nextIndentationEnd = scanner.state();
        scanner.resetState(start);
        return 0;
      }
    } while (scanCharIf(isNewline));

    _checkIndentationConsistency(containsTab, containsSpace);

    if (_nextIndentation > 0) {
      if (_spaces == Sass_Indent_Type::AUTO) {
        _spaces = containsSpace ?
          Sass_Indent_Type::SPACES :
          Sass_Indent_Type::TABS;
      }
    }
    _nextIndentationEnd = scanner.state();
    scanner.resetState(start);
    return _nextIndentation;
  }

  void SassParser::_checkIndentationConsistency(bool containsTab, bool containsSpace)
  {
    if (containsTab) {
      if (containsSpace) {
        scanner.error("Tabs and spaces may not be mixed.",
          *context.logger123, scanner.rawSpan());
          /* position: scanner.position - scanner.column,
          length : scanner.column*/
      }
      else if (indentWithSpaces()) {
        scanner.error("Expected spaces, was tabs.",
          *context.logger123, scanner.rawSpan());
          /* position: scanner.position - scanner.column,
          length : scanner.column*/
      }
    }
    else if (containsSpace && indentWithTabs()) {
      scanner.error("Expected tabs, was spaces.",
        *context.logger123, scanner.rawSpan());
        /* position: scanner.position - scanner.column, length : scanner.column*/
    }
  }

}
