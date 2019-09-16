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
    Position start(scanner);
    InterpolationBuffer buffer(scanner);
    do {
      buffer.addInterpolation(almostAnyValue());
      buffer.writeCharCode($lf);
    } while (buffer.trailingStringEndsWith(",") &&
      scanCharIf(isNewline));

    return buffer.getInterpolation(scanner.pstate(start));
  }

  void SassParser::expectStatementSeparator(sass::string name) {
    if (!atEndOfStatement()) _expectNewline();
    if (_peekIndentation() <= _currentIndentation) return;
    sass::sstream strm;
    strm << "Nothing may be indented ";
    if (name.empty()) { strm << "here."; }
    else { strm << "beneath a " + name + "."; }
    whitespaceWithoutComments();
    callStackFrame frame(*context.logger,
      Backtrace(scanner.pstate43()));
    scanner.error(
      strm.str(),
      *context.logger,
      scanner.pstate43());

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

  ImportBase* SassParser::importArgument()
  {
    uint8_t next = scanner.peekChar();
    LineScannerState2 state(scanner.state());
    switch (next) {
    case $u:
    case $U:
      if (scanIdentifier("url")) {
        if (scanner.scanChar($lparen)) {
          // std::cerr << "reset state 4\n";
          scanner.resetState(state);
          return StylesheetParser::importArgument();
        }
        else {
          // std::cerr << "reset state 5\n";
          scanner.resetState(state);
        }
      }
      break;

    case $single_quote:
    case $double_quote:
      return StylesheetParser::importArgument();
    }

    Position start(scanner);
    LineScannerState2 state2(scanner.state());
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
        Interpolation, scanner.pstate9(start));
      auto str = SASS_MEMORY_NEW(String_Constant,
        scanner.pstate9(start), url, true);
      // Must be an easier way to get quotes?
      str->value(str->to_string());
      itpl->append(str);
      return SASS_MEMORY_NEW(StaticImport,
        scanner.pstate9(start), itpl);
    }

    return SASS_MEMORY_NEW(DynamicImport,
      SourceSpan::fake("[pstateT1]"), url);

  }

  bool SassParser::scanElse(size_t ifIndentation)
  {
    if (_peekIndentation() != ifIndentation) return false;
    LineScannerState2 state(scanner.state());
    size_t startIndentation = _currentIndentation;
    size_t startNextIndentation = _nextIndentation;
    LineScannerState2 startNextIndentationEnd = _nextIndentationEnd;

    _readIndentation();
    if (scanner.scanChar($at) && scanIdentifier("else")) return true;

    // std::cerr << "reset state 6\n";
    scanner.resetState(state);
    _currentIndentation = startIndentation;
    _nextIndentation = startNextIndentation;
    _nextIndentationEnd = startNextIndentationEnd;
    return false;
  }

  sass::vector<StatementObj> SassParser::children(Statement* (StylesheetParser::* child)())
  {
    // std::cerr << "SassParser::children\n";
    sass::vector<StatementObj> children;
    _whileIndentedLower(child, children);
    return children;
  }

  sass::vector<StatementObj> SassParser::statements(Statement* (StylesheetParser::* statement)())
  {
    // std::cerr << "SassParser::statements\n";
    uint8_t first = scanner.peekChar();
    if (first == $tab || first == $space) {
      scanner.error("Indenting at the beginning of the document is illegal.",
        *context.logger, scanner.pstate());
        /*position: 0, length : scanner.position*/
    }

    sass::vector<StatementObj> statements;
    while (!scanner.isDone()) {
      // std::cerr << "SassParser::statements loop\n";
      Statement* child = _child(statement);
      // std::cerr << "SassParser::statements loop out\n";
      if (child != nullptr) statements.emplace_back(child);
//      size_t indentation = _readIndentation();
      SASS_ASSERT(indentation == 0, "indentation must be zero");
    }
    return statements;
  }

  Statement* SassParser::_child(Statement* (StylesheetParser::* child)())
  {
    // std::cerr << "SassParser::_child\n";
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
      // std::cerr << "Call child parser\n";
      return (this->*child)();
      break;
    }
  }

  SilentComment* SassParser::_silentComment()
  {
    Position start(scanner);
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
      scanner.pstate(start), buffer.toString());
    return lastSilentComment;
  }

  LoudComment* SassParser::_loudComment()
  {
	Position start(scanner);
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

    InterpolationObj itpl = buffer.getInterpolation(scanner.pstate(start));
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
        *context.logger, scanner.pstate());
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
        *context.logger, scanner.pstate());
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
          *context.logger,
          scanner.pstate());

          /*,
          position: scanner.position - scanner.column,
          length : scanner.column*/
      }
      children.emplace_back(_child(child));
    }

  }

  size_t SassParser::_readIndentation()
  {
    // std::cerr << "_readIndentation " << _nextIndentation << "\n";
    if (_nextIndentation == sass::string::npos) {
      _peekIndentation();
    }
    _currentIndentation = _nextIndentation;
    // std::cerr << "reset state 1\n";
    scanner.resetState(_nextIndentationEnd);
    _nextIndentation = sass::string::npos;
    // What does this mean, where is it used?
    // _nextIndentationEnd = null; ToDo
    return _currentIndentation;

  }

  size_t SassParser::_peekIndentation()
  {
    // std::cerr << "_peekIndentation\n";
    if (_nextIndentation != sass::string::npos) {
      return _nextIndentation;
    }

    if (scanner.isDone()) {
      _nextIndentation = 0;
      _nextIndentationEnd = scanner.state();
      return 0;
    }

    LineScannerState2 start = scanner.state();
    if (!scanCharIf(isNewline)) {
      scanner.error("Expected newline.",
        *context.logger, scanner.pstate());
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
        // std::cerr << "reset state 2\n";
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
    // std::cerr << "reset state 3\n";
    scanner.resetState(start);
    return _nextIndentation;
  }

  void SassParser::_checkIndentationConsistency(bool containsTab, bool containsSpace)
  {
    if (containsTab) {
      if (containsSpace) {
        scanner.error("Tabs and spaces may not be mixed.",
          *context.logger, scanner.pstate());
          /* position: scanner.position - scanner.column,
          length : scanner.column*/
      }
      else if (indentWithSpaces()) {
        scanner.error("Expected spaces, was tabs.",
          *context.logger, scanner.pstate());
          /* position: scanner.position - scanner.column,
          length : scanner.column*/
      }
    }
    else if (containsSpace && indentWithTabs()) {
      scanner.error("Expected tabs, was spaces.",
        *context.logger, scanner.pstate());
        /* position: scanner.position - scanner.column, length : scanner.column*/
    }
  }

}
