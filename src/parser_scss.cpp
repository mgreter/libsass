#include "parser_scss.hpp"

#include "character.hpp"
#include "utf8/checked.h"

namespace Sass {

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

  void ScssParser::expectStatementSeparator(sass::string name)
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

  bool ScssParser::scanElse(size_t)
  {
    StringScannerState start = scanner.state();
    whitespace();
    // StringScannerState beforeAt = scanner.state();
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

  sass::vector<StatementObj> ScssParser::children(
    Statement* (StylesheetParser::*child)())
  {
    SilentCommentObj comment;
    scanner.expectChar($lbrace);
    whitespaceWithoutComments();
    sass::vector<StatementObj> children;
    while (true) {
      switch (scanner.peekChar()) {
      case $dollar:
        children.emplace_back(variableDeclaration());
        break;

      case $slash:
        switch (scanner.peekChar(1)) {
        case $slash:
          // children.emplace_back(_silentComment());
          comment = _silentComment(); // parse and forget
          whitespaceWithoutComments();
          break;
        case $asterisk:
          children.emplace_back(_loudComment());
          whitespaceWithoutComments();
          break;
        default:
          children.emplace_back((this->*child)());
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
        children.emplace_back((this->*child)());
        break;
      }
    }
  }

  sass::vector<StatementObj> ScssParser::statements(
    Statement* (StylesheetParser::* statement)())
  {
    whitespaceWithoutComments();
    sass::vector<StatementObj> statements;
    SilentCommentObj lastComment;
    while (!scanner.isDone()) {
      switch (scanner.peekChar()) {
      case $dollar:
        statements.emplace_back(variableDeclaration());
        break;

      case $slash:
        switch (scanner.peekChar(1)) {
        case $slash:
          // statements.emplace_back(_silentComment());
          lastComment = _silentComment();
          whitespaceWithoutComments();
          break;
        case $asterisk:
          statements.emplace_back(_loudComment());
          whitespaceWithoutComments();
          break;
        default:
          StatementObj child = (this->*statement)();
          if (child != nullptr) statements.emplace_back(child);
          break;
        }
        break;

      case $semicolon:
        scanner.readChar();
        whitespaceWithoutComments();
        break;

      default:
        StatementObj child = (this->*statement)();
        if (child != nullptr) statements.emplace_back(child);
        break;
      }
    }
    return statements;
  }

  SilentComment* ScssParser::_silentComment()
  {
    StringScannerState start = scanner.state();
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
        *context.logger, scanner.relevantSpanFrom(start.offset));
    }

    return SASS_MEMORY_NEW(SilentComment, scanner.rawSpanFrom(start.offset),
        scanner.substring(start.position, scanner.position));
  }

  LoudComment* ScssParser::_loudComment()
  {
    InterpolationBuffer buffer(scanner);
    Offset start(scanner.offset);
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
          scanner.rawSpanFrom(start),
          buffer.getInterpolation(scanner.rawSpanFrom(start)));

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