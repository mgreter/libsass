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
    // return wrapSpanFormatException(() {
    // Offset start = scanner.offset;
    auto selector = _selectorList();
      if (!scanner.isDone()) {
        scanner.error("expected selector.",
          *context.logger123, scanner.rawSpan());
      }
      return selector;
    // });
  }

  CompoundSelectorObj SelectorParser::parseCompoundSelector()
  {
    // return wrapSpanFormatException(() {
      auto compound = _compoundSelector();
      if (!scanner.isDone()) {
        scanner.error("expected selector.",
          *context.logger123, scanner.rawSpan());
      }
      return compound;
    // });
  }

  SimpleSelectorObj SelectorParser::parseSimpleSelector()
  {
    // return wrapSpanFormatException(() {
    auto simple = _simpleSelector(_allowParent);
    if (!scanner.isDone()) scanner.error("unexpected token.",
      *context.logger123, scanner.relevantSpan());
    return simple;
    // });
  }

  SelectorListObj SelectorParser::_selectorList()
  {
    Offset start(scanner.offset);
    const char* previousLine = scanner.position;
    sass::vector<ComplexSelectorObj> items;
    items.emplace_back(_complexSelector());

    whitespace();
    while (scanner.scanChar($comma)) {
      whitespace();
      uint8_t next = scanner.peekChar();
      if (next == $comma) continue;
      if (scanner.isDone()) break;

      bool lineBreak = scanner.hasLineBreak(previousLine); // ToDo
      // var lineBreak = scanner.line != previousLine;
      // if (lineBreak) previousLine = scanner.line;
      items.emplace_back(_complexSelector(lineBreak));
    }

    return SASS_MEMORY_NEW(SelectorList,
      scanner.relevantSpanFrom(start), std::move(items));
  }

  ComplexSelectorObj SelectorParser::_complexSelector(bool lineBreak)
  {

    uint8_t next;
    Offset start(scanner.offset);
    sass::vector<SelectorComponentObj> complex;

    while (true) {
      whitespace();

      Offset before(scanner.offset);
      if (!scanner.peekChar(next)) {
        goto endOfLoop;
      }
      switch (next) {
      case $plus:
        scanner.readChar();
        complex.emplace_back(SASS_MEMORY_NEW(SelectorCombinator,
          scanner.rawSpanFrom(before), SelectorCombinator::ADJACENT));
        break;

      case $gt:
        scanner.readChar();
        complex.emplace_back(SASS_MEMORY_NEW(SelectorCombinator,
          scanner.rawSpanFrom(before), SelectorCombinator::CHILD));
        break;

      case $tilde:
        scanner.readChar();
        complex.emplace_back(SASS_MEMORY_NEW(SelectorCombinator,
          scanner.rawSpanFrom(before), SelectorCombinator::GENERAL));
        break;

      case $lbracket:
      case $dot:
      case $hash:
      case $percent:
      case $colon:
      case $ampersand:
      case $asterisk:
      case $pipe:
        complex.emplace_back(_compoundSelector());
        if (scanner.peekChar() == $ampersand) {
          scanner.error(
            "\"&\" may only used at the beginning of a compound selector.",
            *context.logger123, scanner.rawSpan());
        }
        break;

      default:
        if (!lookingAtIdentifier()) goto endOfLoop;
        complex.emplace_back(_compoundSelector());
        if (scanner.peekChar() == $ampersand) {
          scanner.error(
            "\"&\" may only used at the beginning of a compound selector.",
            *context.logger123, scanner.rawSpan());
        }
        break;
      }
    }

  endOfLoop:

    if (complex.empty()) {
      scanner.error("expected selector.",
        *context.logger123, scanner.rawSpan());
    }

    ComplexSelector* selector = SASS_MEMORY_NEW(ComplexSelector,
      scanner.rawSpanFrom(start), std::move(complex));
    selector->hasPreLineFeed(lineBreak);
    return selector;

  }
  // EO _complexSelector

  // Consumes a compound selector.
  CompoundSelectorObj SelectorParser::_compoundSelector()
  {
    // Note: libsass uses a flag on the compound selector to
    // signal that it contains a real parent reference.
    // dart-sass uses ParentSelector with a suffix.
    Offset start(scanner.offset);
    CompoundSelectorObj compound = SASS_MEMORY_NEW(CompoundSelector,
      scanner.relevantSpan());

    if (scanner.scanChar($amp)) {
      if (!_allowParent) {
        error(
          "Parent selectors aren't allowed here.",
          *context.logger123, scanner.rawSpanFrom(start)); // ToDo: this fails spec?
      }

      compound->hasRealParent(true);
      if (lookingAtIdentifierBody()) {
        Offset before(scanner.offset);
        sass::string body(identifierBody());
        SimpleSelectorObj simple = SASS_MEMORY_NEW(TypeSelector,
          scanner.rawSpanFrom(before), body);
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

    compound->pstate(scanner.rawSpanFrom(start));
    return compound;

  }

  SimpleSelectorObj SelectorParser::_simpleSelector(bool allowParent)
  {

    Offset start(scanner.offset);
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
          *context.logger123, scanner.rawSpanFrom(start));
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
          *context.logger123, scanner.rawSpanFrom(start));
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
    Offset start(scanner.offset);
    sass::string name(_attributeName());
    SourceSpan span(scanner.relevantSpanFrom(start));
    whitespace();

    if (scanner.scanChar($rbracket)) {
      return SASS_MEMORY_NEW(AttributeSelector,
        span, std::move(name));
    }

    sass::string op(_attributeOperator());

    whitespace();

    bool isIdent = true;
    sass::string value;
    uint8_t next = scanner.peekChar();
    // Check if we are looking at an unquoted text
    if (next != $single_quote && next != $double_quote) {
      value = identifier();
    }
    else {
      value = string();
      isIdent = isIdentifier(value);
    }

    whitespace();
    uint8_t modifier = 0;
    if (isAlphabetic(scanner.peekChar())) {
      modifier = scanner.readChar();
      whitespace();
    }

    span = scanner.relevantSpanFrom(start);

    scanner.expectChar($rbracket);

    return SASS_MEMORY_NEW(AttributeSelector, span,
      std::move(name), std::move(op),
      std::move(value), isIdent,
      modifier);

  }

  sass::string SelectorParser::_attributeName()
  {

    if (scanner.scanChar($asterisk)) {
      scanner.expectChar($pipe);
      sass::string name("*|");
      name += identifier();
      return name;
      // return QualifiedName(identifier(), namespace : "*");
    }

    sass::string nameOrNamespace = identifier();
    if (scanner.peekChar() != $pipe || scanner.peekChar(1) == $equal) {
      return nameOrNamespace;
    }

    scanner.readChar();
    return nameOrNamespace + "|" + identifier();

  }

  sass::string SelectorParser::_attributeOperator()
  {
    Offset start(scanner.offset);
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
        *context.logger123,
        scanner.rawSpanFrom(start));
      throw "Unreachable";
    }
  }

  ClassSelectorObj SelectorParser::_classSelector()
  {
    Offset start(scanner.offset);
    scanner.expectChar($dot);
    sass::string name = identifier();
    return SASS_MEMORY_NEW(ClassSelector,
      scanner.rawSpanFrom(start), "." + name);
  }
  // EO _classSelector

  IDSelectorObj SelectorParser::_idSelector()
  {
    Offset start(scanner.offset);
    scanner.expectChar($hash);
    sass::string name = identifier();
    return SASS_MEMORY_NEW(IDSelector,
      scanner.rawSpanFrom(start), "#" + name);
  }
  // EO _idSelector

  PlaceholderSelectorObj SelectorParser::_placeholderSelector()
  {
    Offset start(scanner.offset);
    scanner.expectChar($percent);
    sass::string name = identifier();
    return SASS_MEMORY_NEW(PlaceholderSelector,
      scanner.rawSpanFrom(start), "%" + name);
  }
  // EO _placeholderSelector

  PseudoSelectorObj SelectorParser::_pseudoSelector()
  {
    Offset start(scanner.offset);
    scanner.expectChar($colon);
    bool element = scanner.scanChar($colon);
    sass::string name = identifier();

    if (!scanner.scanChar($lparen)) {
      return SASS_MEMORY_NEW(PseudoSelector,
        scanner.rawSpanFrom(start), name, element);
    }
    whitespace();

    sass::string unvendored(name);
    unvendored = Util::unvendor(unvendored);

    sass::string argument;
    // Offset beforeArgument(scanner.offset);
    SelectorListObj selector = SASS_MEMORY_NEW(SelectorList, scanner.relevantSpan());
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
      StringUtils::makeRightTrimmed(argument);
    }
    scanner.expectChar($rparen);

    auto pseudo = SASS_MEMORY_NEW(PseudoSelector,
      scanner.rawSpanFrom(start), name, element != 0);
    if (!selector->empty()) pseudo->selector(selector);
    pseudo->argument(argument);
    return pseudo;

  }
  // EO _placeholderSelector

  sass::string SelectorParser::_aNPlusB()
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
      scanner.error("Expected a number.",
        *context.logger123, scanner.rawSpan());
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
    Offset start(scanner.offset);
    uint8_t first = scanner.peekChar();
    if (first == $asterisk) {
      scanner.readChar();
      if (!scanner.scanChar($pipe)) {
        return SASS_MEMORY_NEW(TypeSelector,
          scanner.rawSpanFrom(start), "*");
        // return UniversalSelector();
      }
      if (scanner.scanChar($asterisk)) {
        return SASS_MEMORY_NEW(TypeSelector,
          scanner.rawSpanFrom(start), "*|*");
        // return UniversalSelector(namespace : "*");
      }
      else {
        return SASS_MEMORY_NEW(TypeSelector,
          scanner.rawSpanFrom(start), "*|" + identifier());
        // return TypeSelector(QualifiedName(identifier(), namespace : "*"));
      }
    }
    else if (first == $pipe) {
      scanner.readChar();
      if (scanner.scanChar($asterisk)) {
        return SASS_MEMORY_NEW(TypeSelector,
          scanner.rawSpanFrom(start), "|*");
        // return UniversalSelector(namespace : "");
      }
      else {
        return SASS_MEMORY_NEW(TypeSelector,
          scanner.rawSpanFrom(start), "|" + identifier());
        // return TypeSelector(QualifiedName(identifier(), namespace : ""));
      }
    }

    sass::string nameOrNamespace = identifier();
    if (!scanner.scanChar($pipe)) {
      return SASS_MEMORY_NEW(TypeSelector,
        scanner.rawSpanFrom(start), nameOrNamespace);
      // return TypeSelector(QualifiedName(nameOrNamespace));
    }
    else if (scanner.scanChar($asterisk)) {
      return SASS_MEMORY_NEW(TypeSelector,
        scanner.rawSpanFrom(start), nameOrNamespace + "|*");
      // return UniversalSelector(namespace : nameOrNamespace);
    }
    else {
      return SASS_MEMORY_NEW(TypeSelector,
        scanner.rawSpanFrom(start), nameOrNamespace + "|" + identifier());
      // return TypeSelector(QualifiedName(identifier(), namespace : nameOrNamespace));
    }

  }
  // EO _typeOrUniversalSelector

}
