#include "parser_scss.hpp"

#include "character.hpp"
#include "utf8/checked.h"

namespace Sass {

  int count = 0;

  // Import some namespaces
  using namespace Charcode;
  using namespace Character;

  bool ScssParser::plainCss() const {
    return false;
  }

  bool ScssParser::isIndented() const {
    return false;
  }

  Interpolation* ScssParser::styleRuleSelector()
  {
    return almostAnyValue();
  }

  void ScssParser::expectStatementSeparator(std::string name)
  {
    whitespaceWithoutComments();
    if (scanner.isDone()) return;
    uint8_t next = scanner.peekChar();
    if (next == $semicolon || next == $rbrace) return;
    scanner.expectChar($semicolon);
  }

  bool ScssParser::atEndOfStatement()
  {
    if (scanner.isDone()) return true;
    uint8_t next = scanner.peekChar();
    return next == $semicolon
      || next == $rbrace
      || next == $lbrace;
  }

  bool ScssParser::lookingAtChildren()
  {
    return scanner.peekChar() == $lbrace;
  }

  bool ScssParser::scanElse(size_t ifIndentation)
  {
    LineScannerState2 start = scanner.state();
    whitespace();
    // LineScannerState2 beforeAt = scanner.state();
    if (scanner.scanChar($at)) {
      if (scanIdentifier("else")) return true;
      if (scanIdentifier("elseif")) {
        /*
        logger.warn(
          "@elseif is deprecated and will not be supported in future Sass versions.\n"
          "Use "@else if" instead.",
          span: scanner.spanFrom(beforeAt),
          deprecation : true);
        */
        scanner.offset.column -= 2;
        scanner.position -= 2;
        return true;
      }
    }
    scanner.resetState(start);
    return false;
  }

  std::vector<StatementObj> ScssParser::children(
    Statement* (StylesheetParser::*child)())
  {
    SilentCommentObj comment;
    scanner.expectChar($lbrace);
    whitespaceWithoutComments();
    std::vector<StatementObj> children;
    while (true) {
      switch (scanner.peekChar()) {
      case $dollar:
        children.push_back(variableDeclaration());
        break;

      case $slash:
        switch (scanner.peekChar(1)) {
        case $slash:
          // children.push_back(_silentComment());
          comment = _silentComment(); // parse and forget
          whitespaceWithoutComments();
          break;
        case $asterisk:
          children.push_back(_loudComment());
          whitespaceWithoutComments();
          break;
        default:
          children.push_back((this->*child)());
          break;
        }
        break;

      case $semicolon:
        scanner.readChar();
        whitespaceWithoutComments();
        break;

      case $rbrace:
        scanner.expectChar($rbrace);
        return children;

      default:
        children.push_back((this->*child)());
        break;
      }
    }
  }

  std::vector<StatementObj> ScssParser::statements(
    Statement* (StylesheetParser::* statement)())
  {
    whitespaceWithoutComments();
    std::vector<StatementObj> statements;
    SilentCommentObj lastComment;
    while (!scanner.isDone()) {
      switch (scanner.peekChar()) {
      case $dollar:
        statements.push_back(variableDeclaration());
        break;

      case $slash:
        switch (scanner.peekChar(1)) {
        case $slash:
          // statements.push_back(_silentComment());
          lastComment = _silentComment();
          whitespaceWithoutComments();
          break;
        case $asterisk:
          statements.push_back(_loudComment());
          whitespaceWithoutComments();
          break;
        default:
          StatementObj child = (this->*statement)();
          if (child != nullptr) statements.push_back(child);
          break;
        }
        break;

      case $semicolon:
        scanner.readChar();
        whitespaceWithoutComments();
        break;

      default:
        StatementObj child = (this->*statement)();
        if (child != nullptr) statements.push_back(child);
        break;
      }
    }
    return statements;
  }

  SilentComment* ScssParser::_silentComment()
  {
    LineScannerState2 start = scanner.state();
    scanner.expect("//");

    do {
      while (!scanner.isDone() &&
        !isNewline(scanner.readChar())) {}
      if (scanner.isDone()) break;
      whitespaceWithoutComments();
    }
    while (scanner.scan("//"));

    if (plainCss()) {
      error("Silent comments aren't allowed in plain CSS.",
        scanner.pstate(start.offset));
    }

    return SASS_MEMORY_NEW(SilentComment, scanner.pstate(start.offset),
        scanner.substring(start.position, scanner.position));
  }

  LoudComment* ScssParser::_loudComment()
  {
    InterpolationBuffer buffer;
    Position start(scanner);
    scanner.expect("/*");
    buffer.write("/*");
    while (true) {
      switch (scanner.peekChar()) {
      case $hash:
        if (scanner.peekChar(1) == $lbrace) {
          buffer.add(singleInterpolation());
        }
        else {
          buffer.write(scanner.readChar());
        }
        break;

      case $asterisk:
        buffer.write(scanner.readChar());
        if (scanner.peekChar() != $slash) break;
        buffer.write(scanner.readChar());
        // ToDo: return LoudComment
        return SASS_MEMORY_NEW(LoudComment,
          scanner.spanFrom(start),
          buffer.getInterpolation(scanner.pstate(start)));

      case $cr:
        scanner.readChar();
        if (scanner.peekChar() != $lf) {
          buffer.write($lf);
        }
        break;

      case $ff:
        scanner.readChar();
        buffer.write($lf);
        break;

      default:
        buffer.write(scanner.readChar());
        break;
      }
    }

    return nullptr;
  }

}
