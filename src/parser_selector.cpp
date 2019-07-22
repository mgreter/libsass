#include "parser_selector.hpp"
#include "parser_expression.hpp"

#include "sass.hpp"
#include "ast.hpp"

#include "charcode.hpp"
#include "character.hpp"
#include "util_string.hpp"
#include "color_maps.hpp"
#include "debugger.hpp"

namespace Sass {

  // Import some namespaces
  using namespace Charcode;
  using namespace Character;

  SelectorListObj SelectorParser::parse()
  {
    // std::cerr << "parsing [" << scanner.startpos << "]\n";
    // return wrapSpanFormatException(() {
    Position start = scanner.offset;
    auto selector = _selectorList();
      if (!scanner.isDone()) {
        scanner.error("expected selector.",
          scanner.pstate(start));
      }
      // debug_ast(selector);
      return selector;
    // });
  }

  CompoundSelectorObj SelectorParser::parseCompoundSelector()
  {
    // return wrapSpanFormatException(() {
      auto compound = _compoundSelector();
      if (!scanner.isDone()) scanner.error("expected selector.");
      return compound;
    // });
  }

  SimpleSelectorObj SelectorParser::parseSimpleSelector()
  {
    // return wrapSpanFormatException(() {
    auto simple = _simpleSelector(_allowParent);
    if (!scanner.isDone()) scanner.error("unexpected token.");
    return simple;
    // });
  }

  SelectorListObj SelectorParser::_selectorList()
  {
    const char* previousLine = scanner.position;
    SelectorListObj slist = SASS_MEMORY_NEW(SelectorList, "[pstate]");
    slist->append(_complexSelector());

    whitespace();
    while (scanner.scanChar($comma)) {
      whitespace();
      uint8_t next = scanner.peekChar();
      if (next == $comma) continue;
      if (scanner.isDone()) break;

      bool lineBreak = scanner.hasLineBreak(previousLine); // ToDo
      // var lineBreak = scanner.line != previousLine;
      // if (lineBreak) previousLine = scanner.line;
      slist->append(_complexSelector(lineBreak));
    }

    return slist;
  }

  ComplexSelectorObj SelectorParser::_complexSelector(bool lineBreak)
  {

    uint8_t next;
    Position start(scanner);
    ComplexSelectorObj complex = SASS_MEMORY_NEW(ComplexSelector,
      scanner.pstate());
    complex->hasPreLineFeed(lineBreak);

    while (true) {
      whitespace();

      Position before(scanner);
      if (!scanner.peekChar(next)) {
        goto endOfLoop;
      }
      switch (next) {
      case $plus:
        scanner.readChar();
        complex->append(SASS_MEMORY_NEW(SelectorCombinator,
          scanner.pstate(before), SelectorCombinator::ADJACENT));
        break;

      case $gt:
        scanner.readChar();
        complex->append(SASS_MEMORY_NEW(SelectorCombinator,
          scanner.pstate(before), SelectorCombinator::CHILD));
        break;

      case $tilde:
        scanner.readChar();
        complex->append(SASS_MEMORY_NEW(SelectorCombinator,
          scanner.pstate(before), SelectorCombinator::GENERAL));
        break;

      case $lbracket:
      case $dot:
      case $hash:
      case $percent:
      case $colon:
      case $ampersand:
      case $asterisk:
      case $pipe:
        complex->append(_compoundSelector());
        if (scanner.peekChar() == $ampersand) {
          scanner.error(
            "\"&\" may only used at the beginning of a compound selector.");
        }
        break;

      default:
        if (!lookingAtIdentifier()) goto endOfLoop;
        complex->append(_compoundSelector());
        if (scanner.peekChar() == $ampersand) {
          scanner.error(
            "\"&\" may only used at the beginning of a compound selector.");
        }
        break;
      }
    }

  endOfLoop:

    if (complex->empty()) scanner.error("expected selector.");
    complex->pstate(scanner.pstate(start));
    return complex;

  }
  // EO _complexSelector

  // Consumes a compound selector.
  CompoundSelectorObj SelectorParser::_compoundSelector()
  {
    // Note: libsass uses a flag on the compound selector to
    // signal that it contains a real parent reference.
    // dart-sass uses ParentSelector with a suffix.
    Position start(scanner);
    CompoundSelectorObj compound = SASS_MEMORY_NEW(CompoundSelector,
      scanner.pstate());

    if (scanner.scanChar($amp)) {
      if (!_allowParent) {
        error(
          "Parent selectors aren't allowed here.",
          scanner.pstate(start)); // ToDo: this fails spec?
      }

      compound->hasRealParent(true);
      if (lookingAtIdentifierBody()) {
        Position before(scanner);
        std::string body(identifierBody());
        SimpleSelectorObj simple = SASS_MEMORY_NEW(TypeSelector,
          scanner.pstate(before), body);
        if (!simple.isNull()) compound->append(simple);
      }
    }

    if (compound->hasRealParent() == false) {
      SimpleSelectorObj simple = _simpleSelector(false);
      if (!simple.isNull()) compound->append(simple);
    }

    while (isSimpleSelectorStart(scanner.peekChar())) {
      SimpleSelectorObj simple = _simpleSelector(false);
      if (!simple.isNull()) compound->append(simple);
    }

    compound->pstate(scanner.pstate(start));
    return compound;

  }

  SimpleSelectorObj SelectorParser::_simpleSelector(bool allowParent)
  {

    Position start(scanner);
    uint8_t next = scanner.peekChar();
    if (next == $lbracket) {
      return _attributeSelector();
    }
    else if (next == $dot) {
      return _classSelector();
    }
    else if (next == $hash) {
      return _idSelector();
    }
    else if (next == $percent)
    {
      auto selector = _placeholderSelector();
      if (!_allowPlaceholder) {
        error("Placeholder selectors aren't allowed here.",
          scanner.pstate(start));
      }
      return selector;
    }
    else if (next == $colon) {
      return _pseudoSelector();
    }
    else if (next == $ampersand)
    {
      // auto selector = _parentSelector();
      if (!allowParent) {
        error(
          "Parent selectors aren't allowed here.",
          scanner.pstate(start));
      }
      return {};
    }
    else {
      return _typeOrUniversalSelector();
    }

  }

  AttributeSelectorObj SelectorParser::_attributeSelector()
  {

    scanner.expectChar($lbracket);

    whitespace();
    AttributeSelectorObj attr = SASS_MEMORY_NEW(
      AttributeSelector, "[pstate]", _attributeName(), "", {});
    whitespace();

    if (scanner.scanChar($rbracket)) {
      return attr.detach();
    }

    attr->op(_attributeOperator());

    whitespace();

    uint8_t next = scanner.peekChar();
    // Check if we are looking at an unquoted text
    if (next != $single_quote && next != $double_quote) {
      attr->isIdentifier(true);
      attr->value(identifier());
    }
    else {
      std::string unquoted(string());
      attr->isIdentifier(isIdentifier(unquoted));
      attr->value(unquoted);
    }

    whitespace();

    if (isAlphabetic(scanner.peekChar())) {
      attr->modifier(scanner.readChar());
    }

    scanner.expectChar($rbracket);
    return attr.detach();

  }

  std::string SelectorParser::_attributeName()
  {

    if (scanner.scanChar($asterisk)) {
      scanner.expectChar($pipe);
      std::string name("*|");
      name += identifier();
      return name;
      // return QualifiedName(identifier(), namespace : "*");
    }

    std::string nameOrNamespace = identifier();
    if (scanner.peekChar() != $pipe || scanner.peekChar(1) == $equal) {
      return nameOrNamespace;
    }

    scanner.readChar();
    return nameOrNamespace + "|" + identifier();

  }

  std::string SelectorParser::_attributeOperator()
  {
    Position start(scanner);
    switch (scanner.readChar()) {
    case $equal:
      return "="; // AttributeOperator.equal;

    case $tilde:
      scanner.expectChar($equal);
      return "~="; //  AttributeOperator.include;

    case $pipe:
      scanner.expectChar($equal);
      return "|="; //  AttributeOperator.dash;

    case $caret:
      scanner.expectChar($equal);
      return "^="; //  AttributeOperator.prefix;

    case $dollar:
      scanner.expectChar($equal);
      return "$="; //  AttributeOperator.suffix;

    case $asterisk:
      scanner.expectChar($equal);
      return "*="; //  AttributeOperator.substring;

    default:
      scanner.error("Expected \"]\".",
        scanner.pstate(start));
      throw "Unreachable";
    }
  }

  ClassSelectorObj SelectorParser::_classSelector()
  {
    Position start(scanner);
    scanner.expectChar($dot);
    std::string name = identifier();
    return SASS_MEMORY_NEW(ClassSelector,
      scanner.pstate(start), "." + name);
  }
  // EO _classSelector

  IDSelectorObj SelectorParser::_idSelector()
  {
    Position start(scanner);
    scanner.expectChar($hash);
    std::string name = identifier();
    return SASS_MEMORY_NEW(IDSelector,
      scanner.pstate(start), "#" + name);
  }
  // EO _idSelector

  PlaceholderSelectorObj SelectorParser::_placeholderSelector()
  {
    Position start(scanner);
    scanner.expectChar($percent);
    std::string name = identifier();
    return SASS_MEMORY_NEW(PlaceholderSelector,
      scanner.pstate(start), "%" + name);
  }
  // EO _placeholderSelector

  PseudoSelectorObj SelectorParser::_pseudoSelector()
  {
    Position start(scanner);
    scanner.expectChar($colon);
    bool element = scanner.scanChar($colon);
    std::string name = identifier();

    if (!scanner.scanChar($lparen)) {
      return SASS_MEMORY_NEW(PseudoSelector,
        scanner.pstate(start), name, element);
    }
    whitespace();

    std::string unvendored(name);
    unvendored = Util::unvendor(unvendored);

    std::string argument;
    // Position beforeArgument(scanner);
    SelectorListObj selector = SASS_MEMORY_NEW(SelectorList, scanner.pstate());
    if (element) {
      if (isSelectorPseudoElement(unvendored)) {
        selector = _selectorList();
        for (auto complex : selector->elements()) {
          complex->chroots(true);
        }
      }
      else {
        argument = declarationValue(true);
      }
    }
    else if (isSelectorPseudoClass(unvendored)) {
      selector = _selectorList();
      for (auto complex : selector->elements()) {
        complex->chroots(true);
      }
    }
    else if (unvendored == "nth-child" || unvendored == "nth-last-child") {
      argument = _aNPlusB();
      whitespace();
      if (isWhitespace(scanner.peekChar(-1)) && scanner.peekChar() != $rparen) {
        expectIdentifier("of");
        argument += " of";
        whitespace();

        selector = _selectorList();
      }
    }
    else {
      argument = declarationValue(true); //  .trimRight();
      Util::ascii_str_rtrim(argument);
    }
    scanner.expectChar($rparen);

    auto pseudo = SASS_MEMORY_NEW(PseudoSelector,
      scanner.pstate(start), name, element != 0);
    if (!selector->empty()) pseudo->selector(selector);
    pseudo->argument(argument);
    // if (!argument.empty()) pseudo->argument(SASS_MEMORY_NEW(StringLiteral,
    //   scanner.pstate(beforeArgument), argument));
    return pseudo;

  }
  // EO _placeholderSelector

  std::string SelectorParser::_aNPlusB()
  {

    StringBuffer buffer;
    uint8_t first, next, last;
    switch (scanner.peekChar()) {
    case $e:
    case $E:
      expectIdentifier("even");
      return "even";

    case $o:
    case $O:
      expectIdentifier("odd");
      return "odd";

    case $plus:
    case $minus:
      buffer.write(scanner.readChar());
      break;
    }

    if (scanner.peekChar(first) && isDigit(first)) {
      while (isDigit(scanner.peekChar())) {
        buffer.write(scanner.readChar());
      }
      whitespace();
      if (!scanCharIgnoreCase($n)) return buffer.toString();
    }
    else {
      expectCharIgnoreCase($n);
    }
    buffer.write($n);
    whitespace();

    scanner.peekChar(next);
    if (next != $plus && next != $minus) return buffer.toString();
    buffer.write(scanner.readChar());
    whitespace();

    if (!scanner.peekChar(last) || !isDigit(last)) {
      scanner.error("Expected a number.");
    }
    while (isDigit(scanner.peekChar())) {
      buffer.write(scanner.readChar());
    }
    return buffer.toString();
  }
  // _aNPlusB

  SimpleSelectorObj SelectorParser::_typeOrUniversalSelector()
  {
    // Note: libsass has no explicit UniversalSelector,
    // we use a regular type selector with name == "*".
    Position start(scanner);
    uint8_t first = scanner.peekChar();
    if (first == $asterisk) {
      scanner.readChar();
      if (!scanner.scanChar($pipe)) {
        return SASS_MEMORY_NEW(TypeSelector,
          scanner.pstate(start), "*");
        // return UniversalSelector();
      }
      if (scanner.scanChar($asterisk)) {
        return SASS_MEMORY_NEW(TypeSelector,
          scanner.pstate(start), "*|*");
        // return UniversalSelector(namespace : "*");
      }
      else {
        return SASS_MEMORY_NEW(TypeSelector,
          scanner.pstate(start), "*|" + identifier());
        // return TypeSelector(QualifiedName(identifier(), namespace : "*"));
      }
    }
    else if (first == $pipe) {
      scanner.readChar();
      if (scanner.scanChar($asterisk)) {
        return SASS_MEMORY_NEW(TypeSelector,
          scanner.pstate(start), "|*");
        // return UniversalSelector(namespace : "");
      }
      else {
        return SASS_MEMORY_NEW(TypeSelector,
          scanner.pstate(start), "|" + identifier());
        // return TypeSelector(QualifiedName(identifier(), namespace : ""));
      }
    }

    std::string nameOrNamespace = identifier();
    if (!scanner.scanChar($pipe)) {
      return SASS_MEMORY_NEW(TypeSelector,
        scanner.pstate(start), nameOrNamespace);
      // return TypeSelector(QualifiedName(nameOrNamespace));
    }
    else if (scanner.scanChar($asterisk)) {
      return SASS_MEMORY_NEW(TypeSelector,
        scanner.pstate(start), nameOrNamespace + "|*");
      // return UniversalSelector(namespace : nameOrNamespace);
    }
    else {
      return SASS_MEMORY_NEW(TypeSelector,
        scanner.pstate(start), nameOrNamespace + "|" + identifier());
      // return TypeSelector(QualifiedName(identifier(), namespace : nameOrNamespace));
    }

  }
  // EO _typeOrUniversalSelector

}
