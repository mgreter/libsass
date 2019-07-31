#include "serialize.hpp"

#include "ast.hpp"

namespace Sass {

  SerializeVisitor::SerializeVisitor(bool inspect, bool quote, bool useSpaces) :
    _inspect(inspect), // _quote(quote) //, _useSpaces(useSpaces)
  {
  }

  void SerializeVisitor::visitBoolean(Boolean* value)
  {
  }

  // Returns whether [color]'s hex pair
  // representation is symmetrical (e.g. `FF`).
  bool SerializeVisitor::_isSymmetricalHex(uint8_t color)
  {
    return false;
    // return (color & 0xF) == (color >> 4);
  }

  // Returns whether [color] can be represented
  // as a short hexadecimal color (e.g. `#fff`).
  bool SerializeVisitor::_canUseShortHex(Color* color)
  {
    return false;
    /*
    return _isSymmetricalHex(color->red())
      && _isSymmetricalHex(color->green())
      && _isSymmetricalHex(color->blue());
      */
  }

  void SerializeVisitor::visitColor(Color* value)
  {
    // In compressed mode, emit colors in the shortest representation possible.
    /*
    if (_isCompressed && fuzzyEquals(value->alpha(), 1.0)) {
      var name = namesByColor[value];
      var hexLength = _canUseShortHex(value) ? 4 : 7;
      if (name != null && name.length <= hexLength) {
        _buffer.write(name);
      }
      else if (_canUseShortHex(value)) {
        _buffer.writeCharCode($hash);
        _buffer.writeCharCode(hexCharFor(value.red & 0xF));
        _buffer.writeCharCode(hexCharFor(value.green & 0xF));
        _buffer.writeCharCode(hexCharFor(value.blue & 0xF));
      }
      else {
        _buffer.writeCharCode($hash);
        _writeHexComponent(value.red);
        _writeHexComponent(value.green);
        _writeHexComponent(value.blue);
      }
      return;
    }

    if (value.original != null) {
      _buffer.write(value.original);
    }
    else if (namesByColor.containsKey(value) &&
      // Always emit generated transparent colors in rgba format. This works
      // around an IE bug. See sass/sass#1782.
      !fuzzyEquals(value.alpha, 0)) {
      _buffer.write(namesByColor[value]);
    }
    else if (fuzzyEquals(value.alpha, 1)) {
      _buffer.writeCharCode($hash);
      _writeHexComponent(value.red);
      _writeHexComponent(value.green);
      _writeHexComponent(value.blue);
    }
    else {
      _buffer
        ..write("rgba(${value.red}")
        ..write(_commaSeparator)
        ..write(value.green)
        ..write(_commaSeparator)
        ..write(value.blue)
        ..write(_commaSeparator);
      _writeNumber(value.alpha);
      _buffer.writeCharCode($rparen);
    }*/

  }

  void SerializeVisitor::visitList(List* value)
  {

    /*
    if (value->is_bracketed()) {
      _buffer.writeCharCode($lbracket);
    }
    else if (value->empty()) {
      if (!_inspect) {
        // throw SassScriptException("() isn't a valid CSS value");
      }
      _buffer.write("()");
      return;
    }

    bool singleton = _inspect &&
      value->length() == 1 &&
      value->separator() == SASS_COMMA;

    if (singleton && !value->is_bracketed()) {
      _buffer.writeCharCode($lparen);
    }

    // addSrcMapOpener(list);

    std::string joiner =
      value->separator() == SASS_SPACE
        ? " " : ", ";  // _commaSeparator;

    std::vector<ValueObj> values = value->elements();
    */
    /*
    _writeBetween<Value>(
      _inspect
      ? value.asList
      : value.asList.where((element) = > !element.isBlank),
      value.separator == ListSeparator.space ? " " : _commaSeparator,
      _inspect
      ? (element) {
      var needsParens = _elementNeedsParens(value.separator, element);
      if (needsParens) _buffer.writeCharCode($lparen);
      element.accept(this);
      if (needsParens) _buffer.writeCharCode($rparen);
    }
    : (element) {
      element.accept(this);
    });

    // addSrcMapCloser(list);

    if (singleton) {
      _buffer.writeCharCode($comma);
      if (!value.hasBrackets) _buffer.writeCharCode($rparen);
    }

    */

    // if (value->is_bracketed()) {
    //   _buffer.writeCharCode($rbracket);
    // }

  }

  void SerializeVisitor::visitMap(Map* value)
  {
  }

  void SerializeVisitor::visitNull(Null* value)
  {
    if (_inspect) {
      _buffer.write("null");
    }
  }

  void SerializeVisitor::visitNumber(Number* value)
  {
    /*
    if (value.asSlash != null) {
      visitNumber(value.asSlash.item1);
      _buffer.writeCharCode($slash);
      visitNumber(value.asSlash.item2);
      return;
    }

    _writeNumber(value.value);

    if (!_inspect) {
      if (value.numeratorUnits.length > 1 ||
        value.denominatorUnits.isNotEmpty) {
        throw SassScriptException("$value isn't a valid CSS value.");
      }

      if (value.numeratorUnits.isNotEmpty) {
        _buffer.write(value.numeratorUnits.first);
      }
    }
    else {
      _buffer.write(value.unitString);
    }
    */
  }

  // Writes [number] without exponent notation and with at most
  // [SassNumber.precision] digits after the decimal point.
  // void SerializeVisitor::_writeNumber(double number) {
  // }

  void visitString(String* string)
  {
    /*
    if (_quote && string.hasQuotes) {
      _visitQuotedString(string.text);
    }
    else {
      _visitUnquotedString(string.text);
    }
    */
  }

 // void SerializeVisitor::visitCssMediaRule(CssMediaRule* node)
 // {
   // writeIndentation();
    /*
        _

    _for(node, () {
    */

    //    _buffer.write("@media");

    // if (!_isCompressed || !node.queries.first()->isCondition()) {
    //   _buffer.writeCharCode($space);
    // }

    // _writeBetween(node.queries, _commaSeparator, _visitMediaQuery);

//    _writeOptionalSpace();

    // _visitChildren(node.children);

//  }

  void SerializeVisitor::visitAttributeSelector(AttributeSelector* attribute)
  {
    _buffer.write($lbracket);
    _buffer.write(attribute->name());
    if (!attribute->op().empty()) {
      _buffer.write(attribute->op());
      /*
      if (Parser.isIdentifier(attribute->value()) &&
        // Emit identifiers that start with `--` with quotes, because IE11
        // doesn't consider them to be valid identifiers.
        !attribute.value.startsWith('--'))
      {
        _buffer.write(attribute.value);
        if (attribute.modifier != null) {
          _buffer.write($space);
        }
      }
      else {
        _visitQuotedString(attribute.value);
        if (attribute.modifier != null) {
          _writeOptionalSpace();
        }
      }
      */
      if (attribute->modifier() != 0) {
        _buffer.write(attribute->modifier());
      }
    }
    _buffer.write($rbracket);
  }

  void SerializeVisitor::visitClassSelector(ClassSelector* klass)
  {
    _buffer.write($dot);
    _buffer.write(klass->name());
  }

  void SerializeVisitor::visitComplexSelector(ComplexSelector* complex)
  {
    SelectorComponent* lastComponent = nullptr;
    for (SelectorComponent* component : complex->elements()) {
      if (lastComponent != nullptr &&
        !_omitSpacesAround(lastComponent) &&
        !_omitSpacesAround(component)) {
        _buffer.write(" ");
      }
      if (CompoundSelector* compound = Cast<CompoundSelector>(component)) {
        visitCompoundSelector(compound);
      }
      else if (SelectorCombinator * combinator = Cast<SelectorCombinator>(component)) {
        visitSelectorCombinator(combinator);
        // _buffer.write(component);
      }
      else {
        // add an assertion here?
      }
      lastComponent = component;
    }
  }

  // When [_style] is [OutputStyle.compressed], omit spaces around combinators.
  bool SerializeVisitor::_omitSpacesAround(SelectorComponent* component) {
    return _isCompressed && component->isCombinator();
  }

  void SerializeVisitor::visitCompoundSelector(CompoundSelector* compound)
  {
    for (SimpleSelector* simple : compound->elements()) {
      simple->accept(*this);
    }
  }

  void SerializeVisitor::visitSelectorCombinator(SelectorCombinator* combinator)
  {
  }

  void SerializeVisitor::visitIDSelector(IDSelector* id)
  {
    _buffer.write($hash);
    _buffer.write(id->name());
  }

  void SerializeVisitor::visitPlaceholderSelector(PlaceholderSelector* placeholder)
  {
    _buffer.write($percent);
    _buffer.write(placeholder->name());
  }

  void SerializeVisitor::visitPseudoSelector(PseudoSelector* pseudo)
  {
/*
    // `:not(%a)` is semantically identical to `*`.
    if (pseudo.selector != null &&
        pseudo.name == 'not' &&
        pseudo.selector.isInvisible) {
      return;
    }

    _buffer.write($colon);
    if (pseudo.isSyntacticElement) _buffer.write($colon);
    _buffer.write(pseudo.name);
    if (pseudo.argument == null && pseudo.selector == null) return;

    _buffer.write($lparen);
    if (pseudo.argument != null) {
      _buffer.write(pseudo.argument);
      if (pseudo.selector != null) _buffer.write($space);
    }
    if (pseudo.selector != null) visitSelectorList(pseudo.selector);
    _buffer.write($rparen);
*/
  }

  void SerializeVisitor::visitSelectorList(SelectorList* list)
  {
    bool first = true;
    for (auto complex : list->elements()) {
      if (!_inspect && complex->isInvisible()) {
        continue;
      }
      if (first) {
        first = false;
      }
      else {
        _buffer.write($comma);
        // complex.lineBreak
        if (complex->hasPreLineFeed()) {
          _writeLineFeed();
        }
        else {
          _writeOptionalSpace();
        }
      }
      visitComplexSelector(complex);
    }
  }

  void SerializeVisitor::visitTypeSelector(TypeSelector* type)
  {
    _buffer.write(type->name());
  }


  // Converts [selector] to a CSS string. If [inspect] is `true`, this will emit an
  // unambiguous representation of the source structure. Note however that, although
  // this will be valid SCSS, it may not be valid CSS. If [inspect] is `false` and
  // [selector] can't be represented in plain CSS, throws a [SassScriptException].
  std::string serializeSelector(Selector* selector, bool inspect) {
	  SerializeVisitor visitor(inspect, true, true);
	  selector->accept(visitor);
	  return visitor.getString();
  }

}
