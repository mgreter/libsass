#include "parser_stylesheet.hpp"
#include "parser_expression.hpp"

#include "sass.hpp"
#include "ast.hpp"

#include "LUrlParser/LUrlParser.hpp"

#include "charcode.hpp"
#include "character.hpp"
#include "util_string.hpp"
#include "color_maps.hpp"
#include "debugger.hpp"

namespace Sass {

  // Import some namespaces
  using namespace Charcode;
  using namespace Character;

  std::vector<StatementObj> StylesheetParser::parse()
  {
    // wrapSpanFormatException

    // const char* start = scanner.position;
    // Allow a byte-order mark at the beginning of the document.
    // scanner.scanChar(0xFEFF);

    scanner.scan("\xEF\xBB\xBF");
    // utf_8_bom[] = { 0xEF, 0xBB, 0xBF };

    std::vector<StatementObj> statements;

    ParserState pstate(scanner.pstate());
    Block_Obj root = SASS_MEMORY_NEW(Block, pstate, 0, true);

    // check seems a bit esoteric but works
    if (context.resources.size() == 1) {
      // apply headers only on very first include
      context.apply_custom_headers(root, pstate.path, pstate);
    }

    root->concat(this->statements(&StylesheetParser::_rootStatement));

    scanner.expectDone();

    /// Ensure that all gloal variable assignments produce a variable in this
    /// stylesheet, even if they aren't evaluated. See sass/language#50.
    /*statements.addAll(_globalVariables.values.map((declaration) = >
      VariableDeclaration(declaration.name,
        NullExpression(declaration.expression.span), declaration.span,
        guarded: true)));*/

    // return Stylesheet(statements, scanner.spanFrom(start),
    //   plainCss: plainCss);

    return root->elements();

  }

  // Consumes a variable declaration.
  Statement* StylesheetParser::variableDeclaration()
  {
    // var precedingComment = lastSilentComment;
    // lastSilentComment = null;
    Position start(scanner);

    std::string ns;
    std::string name = variableName();
    if (scanner.scanChar($dot)) {
      ns = name;
      name = _publicIdentifier();
    }

    if (plainCss()) {
      error("Sass variables aren't allowed in plain CSS.",
        scanner.pstate(start));
    }

    whitespace();
    scanner.expectChar($colon);
    whitespace();

    ExpressionObj value = expression();

    bool guarded = false;
    bool global = false;

    Position flagStart(scanner);
    while (scanner.scanChar($exclamation)) {
       std::string flag = identifier();
       if (flag == "default") {
         guarded = true;
       }
       else if (flag == "global") {
         if (!ns.empty()) {
           error("!global isn't allowed for variables in other modules.",
             scanner.pstate(flagStart));
         }
         global = true;
       }
       else {
         error("Invalid flag name.",
           scanner.pstate(flagStart));
       }

       whitespace();
       flagStart = scanner.offset;
    }

    expectStatementSeparator("variable declaration");
    Assignment* declaration = SASS_MEMORY_NEW(Assignment,
      scanner.pstate(start), name, value, guarded, global);

    // if (global) _globalVariables.putIfAbsent(name, () = > declaration);

    return declaration;

  }
  // EO variableDeclaration

  // Consumes a statement that's allowed at the top level of the stylesheet or
  // within nested style and at rules. If [root] is `true`, this parses at-rules
  // that are allowed only at the root of the stylesheet.
  Statement* StylesheetParser::_statement(bool root)
  {
    // std::cerr << "StylesheetParser::_statement\n";
    Position start(scanner);
    switch (scanner.peekChar()) {
    case $at:
      // std::cerr << "Parse at rule\n";
      return atRule(&StylesheetParser::_childStatement, root);

    case $plus:
      if (!isIndented() || !lookingAtIdentifier(1)) {
        return _styleRule();
      }
      _isUseAllowed = false;
      start = scanner.offset;
      scanner.readChar();
      return _includeRule(start);

    case $equal:
      if (!isIndented()) return _styleRule();
      _isUseAllowed = false;
      start = scanner.offset;
      scanner.readChar();
      whitespace();
      return _mixinRule(start);

    default:
      _isUseAllowed = false;
      if (_inStyleRule || _inUnknownAtRule || _inMixin || _inContentBlock) {
        return _declarationOrStyleRule();
      }
      else {
        return _styleRule();
      }
    }
    return nullptr;
  }
  // EO _statement

  // Consumes a style rule.
  StyleRule* StylesheetParser::_styleRule()
  {
    LOCAL_FLAG(_inStyleRule, true);

    // The indented syntax allows a single backslash to distinguish a style rule
    // from old-style property syntax. We don't support old property syntax, but
    // we do support the backslash because it's easy to do.
    if (isIndented()) scanner.scanChar($backslash);

    InterpolationObj styleRule = styleRuleSelector();
    return _withChildren<StyleRule>(
      &StylesheetParser::_childStatement,
      styleRule.ptr());
  }

  // Consumes a [Declaration] or a [StyleRule].
  //
  // When parsing the contents of a style rule, it can be difficult to tell
  // declarations apart from nested style rules. Since we don't thoroughly
  // parse selectors until after resolving interpolation, we can share a bunch
  // of the parsing of the two, but we need to disambiguate them first. We use
  // the following criteria:
  //
  // * If the entity doesn't start with an identifier followed by a colon,
  //   it's a selector. There are some additional mostly-unimportant cases
  //   here to support various declaration hacks.
  //
  // * If the colon is followed by another colon, it's a selector.
  //
  // * Otherwise, if the colon is followed by anything other than
  //   interpolation or a character that's valid as the beginning of an
  //   identifier, it's a declaration.
  //
  // * If the colon is followed by interpolation or a valid identifier, try
  //   parsing it as a declaration value. If this fails, backtrack and parse
  //   it as a selector.
  //
  // * If the declaration value is valid but is followed by "{", backtrack and
  //   parse it as a selector anyway. This ensures that ".foo:bar {" is always
  //   parsed as a selector and never as a property with nested properties
  //   beneath it.
  Statement* StylesheetParser::_declarationOrStyleRule()
  {

    if (plainCss() && _inStyleRule && !_inUnknownAtRule) {
      return _declaration();
    }

    // The indented syntax allows a single backslash to distinguish a style rule
    // from old-style property syntax. We don't support old property syntax, but
    // we do support the backslash because it's easy to do.
    if (isIndented() && scanner.scanChar($backslash)) {
      return _styleRule();
    }

    InterpolationBuffer buffer;
    Position start(scanner);
    Declaration* declaration = _declarationOrBuffer(buffer);

    if (declaration != nullptr) {
      return declaration;
    }

    buffer.addInterpolation(styleRuleSelector());
    ParserState selectorPstate = scanner.pstate(start);

    LOCAL_FLAG(_inStyleRule, true);

    if (buffer.empty()) scanner.error("expected \"}\".");

    InterpolationObj itpl = buffer.getInterpolation(scanner.pstate(start));
    StyleRuleObj rule = _withChildren<StyleRule>(
      &StylesheetParser::_childStatement,
      itpl.ptr());

    if (isIndented() && rule->empty()) {
      warn("This selector doesn't have any properties and won't be rendered.",
        selectorPstate);
    }

    return rule.detach();

  }
  // _declarationOrStyleRule


  // Tries to parse a declaration, and returns the value parsed so
  // far if it fails. This can return either an [InterpolationBuffer],
  // indicating that it couldn't consume a declaration and that selector
  // parsing should be attempted; or it can return a [Declaration],
  // indicating that it successfully consumed a declaration.
  Declaration* StylesheetParser::_declarationOrBuffer(InterpolationBuffer& nameBuffer)
  {
    Position start(scanner);

    // Allow the "*prop: val", ":prop: val",
    // "#prop: val", and ".prop: val" hacks.
    uint8_t first = scanner.peekChar();
    if (first == $colon ||
        first == $asterisk ||
        first == $dot ||
        (first == $hash && scanner.peekChar(1) != $lbrace)) {
      nameBuffer.write(scanner.readChar());
      nameBuffer.write(rawText(&StylesheetParser::whitespace));
    }

    if (!_lookingAtInterpolatedIdentifier()) {
      return nullptr;
    }

    nameBuffer.addInterpolation(interpolatedIdentifier());
    if (scanner.matches("/*")) nameBuffer.write(rawText(&StylesheetParser::loudComment));

    StringBuffer midBuffer;
    midBuffer.write(rawText(&StylesheetParser::whitespace));
    ParserState beforeColon(scanner.pstate(start));
    if (!scanner.scanChar($colon)) {
      if (!midBuffer.empty()) {
        nameBuffer.write($space);
      }
      return nullptr;
    }
    midBuffer.write($colon);

    // Parse custom properties as declarations no matter what.
    InterpolationObj name = nameBuffer.getInterpolation(beforeColon);
    StringLiteralObj plain = name->getInitialPlain();
    if (Util::equalsLiteral("--", plain->text())) {
      StringObj value = _interpolatedDeclarationValue();
      expectStatementSeparator("custom property");
      return SASS_MEMORY_NEW(Declaration,
        scanner.pstate(start),
        name, value, true);
    }

    if (scanner.scanChar($colon)) {
      nameBuffer.write(midBuffer.toString());
      nameBuffer.write($colon);
      return nullptr;
    }
    else if (isIndented() && _lookingAtInterpolatedIdentifier()) {
      // In the indented syntax, `foo:bar` is always
      // considered a selector rather than a property.
      nameBuffer.write(midBuffer.toString());
      return nullptr;
    }

    std::string postColonWhitespace = rawText(&StylesheetParser::whitespace);
    if (lookingAtChildren()) {
      return _withChildren<Declaration>(&StylesheetParser::_declarationChild, name);
    }

    midBuffer.write(postColonWhitespace);
    bool couldBeSelector = postColonWhitespace.empty()
      && _lookingAtInterpolatedIdentifier();

    LineScannerState2 beforeDeclaration = scanner.state();
    ExpressionObj value;

    try {

      if (lookingAtChildren()) {
        ParserState pstate = scanner.pstate(scanner);
        Interpolation* itpl = SASS_MEMORY_NEW(Interpolation, pstate);
        value = SASS_MEMORY_NEW(StringExpression, pstate, itpl, true);
      }
      else {
        value = expression();
      }

      if (lookingAtChildren()) {
        // Properties that are ambiguous with selectors can't have additional
        // properties nested beneath them, so we force an error. This will be
        // caught below and cause the text to be reparsed as a selector.
        if (couldBeSelector) {
          expectStatementSeparator();
        }
      }
      else if (!atEndOfStatement()) {
        // Force an exception if there isn't a valid end-of-property character
        // but don't consume that character. This will also cause the text to be
        // reparsed.
        expectStatementSeparator();
      }

    }
    catch (Exception::InvalidSyntax&) {
      if (!couldBeSelector) throw;

      // If the value would be followed by a semicolon, it's
      // definitely supposed to be a property, not a selector.
      scanner.resetState(beforeDeclaration);
      InterpolationObj additional = almostAnyValue();
      if (!isIndented() && scanner.peekChar() == $semicolon) throw;

      nameBuffer.write(midBuffer.toString());
      nameBuffer.addInterpolation(additional);
      return nullptr;
    }

    if (lookingAtChildren()) {
      return _withChildren<Declaration>(
        &StylesheetParser::_declarationChild,
        name, value);
    }
    else {
      expectStatementSeparator();
      return SASS_MEMORY_NEW(Declaration,
        scanner.pstate(start), name, value);
    }

  }
  // EO _declarationOrBuffer

  // Consumes a property declaration. This is only used in contexts where
  // declarations are allowed but style rules are not, such as nested
  // declarations. Otherwise, [_declarationOrStyleRule] is used instead.
  Declaration* StylesheetParser::_declaration()
  {

    Position start(scanner);

    InterpolationObj name;
    // Allow the "*prop: val", ":prop: val",
    // "#prop: val", and ".prop: val" hacks.
    uint8_t first = scanner.peekChar();
    if (first == $colon ||
        first == $asterisk ||
        first == $dot ||
        (first == $hash && scanner.peekChar(1) != $lbrace)) {
      InterpolationBuffer nameBuffer;
      nameBuffer.write(scanner.readChar());
      nameBuffer.write(rawText(&StylesheetParser::whitespace));
      nameBuffer.addInterpolation(interpolatedIdentifier());
      name = nameBuffer.getInterpolation(scanner.pstate(start));
    }
    else {
      name = interpolatedIdentifier();
    }

    whitespace();
    scanner.expectChar($colon);
    whitespace();

    if (lookingAtChildren()) {
      if (plainCss()) {
        scanner.error("Nested declarations aren't allowed in plain CSS.");
      }
      return _withChildren<Declaration>(&StylesheetParser::_declarationChild, name);
    }

    ExpressionObj value = expression();
    if (lookingAtChildren()) {
      if (plainCss()) {
        scanner.error("Nested declarations aren't allowed in plain CSS.");
      }
      return _withChildren<Declaration>(
        &StylesheetParser::_declarationChild,
        name, value);
    }
    else {
      expectStatementSeparator();
      return SASS_MEMORY_NEW(Declaration,
        scanner.pstate(start), name, value);
    }

  }
  // EO _declaration

  // Consumes a statement that's allowed within a declaration.
  Statement* StylesheetParser::_declarationChild()
  {
    if (scanner.peekChar() == $at) {
      return _declarationAtRule();
    }
    return _declaration();
  }
  // EO _declarationChild

  // Consumes an at-rule. This consumes at-rules that are allowed at all levels
  // of the document; the [child] parameter is called to consume any at-rules
  // that are specifically allowed in the caller's context. If [root] is `true`,
  // this parses at-rules that are allowed only at the root of the stylesheet.
  Statement* StylesheetParser::atRule(Statement* (StylesheetParser::* child)(), bool root)
  {
    // NOTE: this logic is largely duplicated in CssParser.atRule.
    // Most changes here should be mirrored there.

    Position start(scanner);
    scanner.expectChar($at, "@-rule");
    InterpolationObj name = interpolatedIdentifier();
    whitespace();

    // We want to set [_isUseAllowed] to `false` *unless* we're parsing
    // `@charset`, `@forward`, or `@use`. To avoid double-comparing the rule
    // name, we always set it to `false` and then set it back to its previous
    // value if we're parsing an allowed rule.
    bool wasUseAllowed = _isUseAllowed;
    LOCAL_FLAG(_isUseAllowed, false);

    std::string plain(name->getPlainString());
    if (plain == "at-root") {
      return _atRootRule(start);
    }
    else if (plain == "charset") {
      // std::cerr << "parse charset\n";
      _isUseAllowed = wasUseAllowed;
      if (!root) _disallowedAtRule(start);
      string();
      return nullptr;
    }
    else if (plain == "content") {
      return _contentRule(start);
    }
    else if (plain == "debug") {
      return _debugRule(start);
    }
    else if (plain == "each") {
      return _eachRule(start, child);
    }
    else if (plain == "else") {
      return _disallowedAtRule(start);
    }
    else if (plain == "error") {
      return _errorRule(start);
    }
    else if (plain == "extend") {
      return _extendRule(start);
    }
    else if (plain == "for") {
      return _forRule(start, child);
    }
    else if (plain == "function") {
      return _functionRule(start);
    }
    else if (plain == "if") {
      return _ifRule(start, child);
    }
    else if (plain == "import") {
      Import* imp = _importRule(start);
      return imp;
      auto block = SASS_MEMORY_NEW(Block, "[pstateF]");
      for (size_t i = 0, S = imp->incs().size(); i < S; ++i) {
        block->append(SASS_MEMORY_NEW(Import_Stub, "[pstateG]", imp->incs()[i]));
      }
      block->append(imp);
      return block;
    }
    else if (plain == "include") {
      return _includeRule(start);
    }
    else if (plain == "media") {
      return mediaRule(start);
    }
    else if (plain == "mixin") {
      return _mixinRule(start);
    }
    else if (plain == "-moz-document") {
      return mozDocumentRule(start, name);
    }
    else if (plain == "return") {
      return _disallowedAtRule(start);
    }
    else if (plain == "supports") {
      return supportsRule(start);
    }
    else if (plain == "use") {
      _isUseAllowed = wasUseAllowed;
      if (!root) _disallowedAtRule(start);
      std::cerr << "use rule\n"; exit(1);
      // return _useRule(start);
    }
    else if (plain == "warn") {
      return _warnRule(start);
    }
    else if (plain == "while") {
      return _whileRule(start, child);
    }
    else {
      return unknownAtRule(start, name);
    }

  }

  Statement* StylesheetParser::_declarationAtRule()
  {
    Position start(scanner);
    std::string name = _plainAtRuleName();
    
    if (name == "content") {
      return _contentRule(start);
    }
    else if (name == "debug") {
      return _debugRule(start);
    }
    else if (name == "each") {
      return _eachRule(start, &StylesheetParser::_declarationChild);
    }
    else if (name == "else") {
      return _disallowedAtRule(start);
    }
    else if (name == "error") {
      return _errorRule(start);
    }
    else if (name == "for") {
      return _forRule(start, &StylesheetParser::_declarationAtRule);
    }
    else if (name == "if") {
      return _ifRule(start, &StylesheetParser::_declarationChild);
    }
    else if (name == "include") {
      return _includeRule(start);
    }
    else if (name == "warn") {
      return _warnRule(start);
    }
    else if (name == "while") {
      return _whileRule(start, &StylesheetParser::_declarationChild);
    }
    else {
      return _disallowedAtRule(start);
    }
  }

  Statement* StylesheetParser::_functionAtRule()
  {
    if (scanner.peekChar() != $at) {
      // const char* position = scanner.position;
      StatementObj statement;
      // try {
      statement = _declarationOrStyleRule();
      // } on SourceSpanFormatException catch (_) {
        // If we can't parse a valid declaration or style rule, throw a more
        // generic error message.
        // scanner.error("expected @-rule", position: position);
      // }

      bool isStyleRule = Cast<StyleRule>(statement) != nullptr;
      error(
        std::string("@function rules may not contain ")
        + (isStyleRule ? "style rules." : "declarations."),
        statement->pstate());
    }

    Position start(scanner);
    std::string name(_plainAtRuleName());
    if (name == "debug") {
      return _debugRule(start);
    }
    else if (name == "each") {
      return _eachRule(start, &StylesheetParser::_functionAtRule);
    }
    else if (name == "else") {
      return _disallowedAtRule(start);
    }
    else if (name == "error") {
      return _errorRule(start);
    }
    else if (name == "for") {
      return _forRule(start, &StylesheetParser::_functionAtRule);
    }
    else if (name == "if") {
      return _ifRule(start, &StylesheetParser::_functionAtRule);
    }
    else if (name == "return") {
      return _returnRule(start);
    }
    else if (name == "warn") {
      return _warnRule(start);
    }
    else if (name == "while") {
      return _whileRule(start, &StylesheetParser::_functionAtRule);
    }
    else {
      return _disallowedAtRule(start);
    }
  }

  std::string StylesheetParser::_plainAtRuleName()
  {
    scanner.expectChar($at, "@-rule");
    std::string name = identifier();
    whitespace();
    return name;
  }

  // Consumes an `@at-root` rule.
  // [start] should point before the `@`.
  At_Root_Block* StylesheetParser::_atRootRule(Position start)
  {
    if (scanner.peekChar() == $lparen) {
      auto query = _atRootQuery();
      whitespace();
      return _withChildren<At_Root_Block>(
        &StylesheetParser::_childStatement,
        query);
    }
    else if (lookingAtChildren()) {
      return _withChildren<At_Root_Block>(
        &StylesheetParser::_childStatement);
    }
    else {
      StyleRule* child = _styleRule();
      ParserState pstate(scanner.pstate(start));
      Block* block = SASS_MEMORY_NEW(Block, pstate, { child });
      return SASS_MEMORY_NEW(At_Root_Block, pstate, {}, block);
    }

    return nullptr;
  }
  // EO _atRootRule

  At_Root_Query* StylesheetParser::_atRootQuery()
  {
    if (scanner.peekChar() == $hash) {
      Expression* interpolation = singleInterpolation();
      Interpolation* itpl = SASS_MEMORY_NEW(Interpolation, "[pstateJ]");
      itpl->append(interpolation);
      return SASS_MEMORY_NEW(At_Root_Query, "[pstateK]", itpl);
    }

//    const char* start = scanner.position;
    InterpolationBuffer buffer;
    InterpolationBuffer buffer2;
    Expression* ex1 = nullptr;
    Expression* ex2 = nullptr;
    scanner.expectChar($lparen);
    // buffer.write($lparen);
    whitespace();

    buffer.add(ex1 = expression());
    if (scanner.scanChar($colon)) {
      whitespace();
      // buffer.write($colon);
      // buffer.write($space);
      buffer2.add(ex2 = expression());
    }

    scanner.expectChar($rparen);
    whitespace();
    // buffer.write($rparen);
    // ex2 = buffer2.toSchema();

    ListExpression* lst = Cast<ListExpression>(ex2);
    if (lst == nullptr) {
      lst = SASS_MEMORY_NEW(ListExpression, "[pstate]");
      lst->append(ex2);
    }

    return SASS_MEMORY_NEW(At_Root_Query, "[pstateL]", ex1, lst); // buffer.interpolation(scanner.spanFrom(start));
  }

  // Consumes a `@content` rule.
  // [start] should point before the `@`.
  Content* StylesheetParser::_contentRule(Position start)
  {
    if (!_inMixin) {
      error("@content is only allowed within mixin declarations.",
        scanner.pstate(start));
    }

    whitespace();

    Arguments* args = nullptr;
    if (scanner.peekChar() == $lparen) {
      args = _argumentInvocation(true);
    }
    else {
      args = SASS_MEMORY_NEW(Arguments, scanner.pstate());
    }

    LOCAL_FLAG(_mixinHasContent, true);
    expectStatementSeparator("@content rule");
    // ToDo: ContentRule
    return SASS_MEMORY_NEW(Content,
      scanner.pstate(start), args);

  }
  // EO _contentRule

  bool StylesheetParser::_forRuleUntil()
  {
    if (!lookingAtIdentifier()) return false;
    if (scanIdentifier("to")) {
      exclusive = 2; // true
      return true;
    }
    else if (scanIdentifier("through")) {
      exclusive = 1; // false
      return true;
    }
    else {
      return false;
    }
  }

  Each* StylesheetParser::_eachRule(Position start, Statement* (StylesheetParser::* child)())
  {
    LOCAL_FLAG(_inControlDirective, true);

    std::vector<std::string> variables;
    variables.push_back(variableName());
    whitespace();
    while (scanner.scanChar($comma)) {
      whitespace();
      variables.push_back(variableName());
      whitespace();
    }

    expectIdentifier("in");
    whitespace();

    ExpressionObj list = expression();
    Each* loop = _withChildren<Each>(
      child, variables, list);
    loop->update_pstate(scanner.pstate(start));
    return loop;

  }

  Error* StylesheetParser::_errorRule(Position start)
  {
    ExpressionObj value = expression();
    expectStatementSeparator("@error rule");
    return SASS_MEMORY_NEW(Error,
      scanner.pstate(start), value);
  }
  // EO _errorRule

  // Consumes an `@extend` rule.
  // [start] should point before the `@`.
  ExtendRule* StylesheetParser::_extendRule(Position start)
  {
    if (!_inStyleRule && !_inMixin && !_inContentBlock) {
      error("@extend may only be used within style rules.",
        scanner.pstate(start));
    }

    InterpolationObj value = almostAnyValue();
    bool optional = scanner.scanChar($exclamation);
    if (optional) expectIdentifier("optional");
    expectStatementSeparator("@extend rule");
    return SASS_MEMORY_NEW(ExtendRule,
      scanner.pstate(start), value, optional);
  }
  // EO _extendRule

  // Consumes a function declaration.
  // [start] should point before the `@`.
  Definition* StylesheetParser::_functionRule(Position start)
  {
    // var precedingComment = lastSilentComment;
    // lastSilentComment = null;
    std::string name = identifier();
    std::string normalized(name);
    whitespace();
    // std::cerr << "_functionRule\n"; exit(1);

    Parameters_Obj params = _argumentDeclaration();

    if (_inMixin || _inContentBlock) {
      error("Mixins may not contain function declarations.",
        scanner.pstate(start));
    }
    else if (_inControlDirective) {
      error("Functions may not be declared in control directives.",
        scanner.pstate(start));
    }

    std::string fname(Util::unvendor(name));
    if (fname == "calc" || fname == "element" || fname == "expression" || fname == "url" || fname == "and" || fname == "or" || fname == "not") {
      error("Invalid function name.",
        scanner.pstate(start));
    }

    whitespace();
    Block_Obj block = SASS_MEMORY_NEW(Block, "[pstateP]");
    Definition* rule = _withChildren<Definition>(
      &StylesheetParser::_functionAtRule,
      normalized, params, block, Definition::FUNCTION);
    block->update_pstate(scanner.pstate(start));
    rule->update_pstate(scanner.pstate(start));
    return rule;
  }
  // EO _functionRule

  For* StylesheetParser::_forRule(Position start, Statement* (StylesheetParser::* child)())
  {
    LOCAL_FLAG(_inControlDirective, true);
    std::string variable = variableName();
    whitespace();

    expectIdentifier("from");
    whitespace();

    exclusive = 0; // reset flag on parser (hackish)
    ExpressionObj from = expression(false, false,
      &StylesheetParser::_forRuleUntil);

    if (exclusive == 0) {
      scanner.error("Expected \"to\" or \"through\".");
    }

    whitespace();
    ExpressionObj to = expression();
    For* loop = _withChildren<For>(
      child, variable, from, to, exclusive == 1);
    loop->update_pstate(scanner.pstate(start));
    return loop;

      /*
    var from = expressionUntil(until: () {
    });


    return _withChildren(child, start, (children, span) {
      _inControlDirective = wasInControlDirective;

      return ForRule(variable, from, to, children, span, exclusive: exclusive);
    });


    return ForRule();
    */
  }

  // ToDo: dart-sass stores all else ifs in the same object, smart ...
  If* StylesheetParser::_ifRule(Position start, Statement* (StylesheetParser::* child)())
  {
    // var ifIndentation = currentIndentation;
    size_t ifIndentation = 0;
    LOCAL_FLAG(_inControlDirective, true);
    ExpressionObj condition = expression();
    std::vector<StatementObj> children
      = this->children(child);
    whitespaceWithoutComments();

    std::vector<StatementObj> clauses;
    Block* block = SASS_MEMORY_NEW(Block, "[pstateQ]");
    block->concat(children);
    IfObj root = SASS_MEMORY_NEW(If, "[pstateR]", condition, block);
    IfObj cur = root;

    while (scanElse(ifIndentation)) {
      whitespace();
      if (scanIdentifier("if")) {
        whitespace();
        ExpressionObj condition = expression();
        std::vector<StatementObj> children = this->children(child);
        Block* block = SASS_MEMORY_NEW(Block, "[pstateS]", children);
        If* alternative = SASS_MEMORY_NEW(If, "[pstateT]", condition, block);
        cur->alternative(SASS_MEMORY_NEW(Block, "[pstateU]", { alternative }));
        cur = alternative;
      }
      else {
        std::vector<StatementObj> children = this->children(child);
        cur->alternative(SASS_MEMORY_NEW(Block, "[pstate]", children));


        break;
      }
    }

    whitespaceWithoutComments();

    return root.detach();

  }

  Import* StylesheetParser::_importRule(Position start)
  {

    ImportObj imp = SASS_MEMORY_NEW(Import, "[pstate]");
    // ImportRuleObj rule = SASS_MEMORY_NEW(ImportRule, "[pstate]");

    do {
      whitespace();
      ImportBaseObj argument = importArgument();
      // redebug_ast(argument);
      if (auto dyn = Cast<DynamicImport>(argument)) {
        if (_inControlDirective || _inMixin) {
          _disallowedAtRule(start);
        }
        context.import_url(imp, dyn->url(), scanner.sourceUrl);
      }
      else if (auto stat = Cast<StaticImport>(argument)) {
        imp->urls().push_back(stat->url());
        // if (!imp->import_queries()) {
          // imp->import_queries(SASS_MEMORY_NEW(List, "[pstate]"));
        // }
        if (stat->media()) {
          imp->queries2().push_back(stat->media());
        }
      }
      else if (auto imp2 = Cast<Import>(argument)) {
        for (auto inc : imp2->incs()) {
          imp->incs().push_back(inc);
        }
      }
      // rule->append(argument);
      whitespace();
    }
    while (scanner.scanChar($comma));
    expectStatementSeparator("@import rule");
    return imp.detach();

  }

  ImportBase* StylesheetParser::importArgument()
  {
    const char* startpos = scanner.position;
    Position start(scanner);
    uint8_t next = scanner.peekChar();
    if (next == $u || next == $U) {
      Expression* url = dynamicUrl();
      whitespace();
      auto queries = tryImportQueries();
      // std::cerr << "STATIC IMPORT 1\n";
      // debug_ast(queries.first);
      // debug_ast(queries.second);
      if (Interpolation * sc = Cast<Interpolation>(url)) {
        // std::cerr << "STATIC IMPORT 2\n";
        auto asd = SASS_MEMORY_NEW(StaticImport, "[pstate]",
          sc, queries.first, queries.second);
        // std::cerr << "got " << sc->length() << "\n";
        // debug_ast(queries.second);
//         debug_ast(asd);
        return asd;
      }
      else {
        // std::cerr << "STATIC IMPORT 3\n";
        Interpolation* sc2 = SASS_MEMORY_NEW(Interpolation, "[pstate]");
        // std::cerr << "STATIC IMPORT 4\n";
        sc2->append(url);
        // std::cerr << "static schema wrap\n";
        return SASS_MEMORY_NEW(StaticImport, "[pstate]",
          sc2, queries.first, queries.second);
      }
      debug_ast(url);
      std::cerr << "no schema?"; exit(1);
      // std::cerr << "static " << url->length() << "\n";
      // return asd;
    }

    std::string url = string();
    std::string url2(startpos, scanner.position);
    ParserState pstate = scanner.pstate(start);
    // var urlSpan = scanner.spanFrom(start);
    whitespace();
    auto queries = tryImportQueries();
    if (_isPlainImportUrl(url) || queries.first != nullptr || queries.second != nullptr) {
      InterpolationObj itpl = SASS_MEMORY_NEW(Interpolation, "[pstate]");
      itpl->append(SASS_MEMORY_NEW(StringLiteral, "[pstate]", url2));
      // std::cerr << "TO ITPL [" << itpl->to_string() << "]\n";
      // std::cerr << "STATIC IMPORT 2\n";
      return SASS_MEMORY_NEW(StaticImport, "[pstate]",
        itpl, queries.first, queries.second);
    }
    else {
      try {
        Import_Obj imp = SASS_MEMORY_NEW(Import, pstate);
        if (!context.call_importers(unquote(url), pstate.path, pstate, imp)) {
          // std::cerr << "RET Dynamic Import\n";
          return SASS_MEMORY_NEW(DynamicImport, "[pstate]", url);
          // context.import_url(imp, url, pstate.path);
        }
        else {
          // std::cerr << "REGULAR IMPORT\n";
          return imp.detach();
        }
      }
      catch (Exception::InvalidSyntax& err) {
        // ToDo: refactor to return and throw here
        error("Invalid URL: ${innerError.message}",
          err.pstate);
      }
    }
    // Never gets here!
    return nullptr;

  }

  /*
  std::string StylesheetParser::parseImportUrl(std::string url)
  {

    // Backwards-compatibility for implementations that
    // allow absolute Windows paths in imports.
    if (File::is_absolute_path(url)) {
    //   return p.windows.toUri(url).toString();
    }
    

    using LUrlParser::clParseURL;
    clParseURL clURL = clParseURL::ParseURL(url);

    if (clURL.IsValid()) {

    }


    // Throw a [FormatException] if [url] is invalid.
    // Uri.parse(url);
    return url;
  }
  */

  bool StylesheetParser::_isPlainImportUrl(const std::string& url) const
  {
    if (url.length() < 5) return false;

    if (Util::ascii_str_ends_with_insensitive(url, ".css")) return true;

    uint8_t first = url[0];
    if (first == $slash) return url[1] == $slash;
    if (first != $h) return false;
    return Util::ascii_str_starts_with_insensitive(url, "http://")
      || Util::ascii_str_starts_with_insensitive(url, "https://");
  }

  // Consumes a supports condition and/or a media query after an `@import`.
  // Returns `null` if neither type of query can be found.
  std::pair<SupportsCondition_Obj, InterpolationObj> StylesheetParser::tryImportQueries()
  {
    SupportsCondition_Obj supports;
    if (scanIdentifier("supports")) {
      scanner.expectChar($lparen);
      Position start(scanner);
      if (scanIdentifier("not")) {
        whitespace();
        SupportsCondition* condition = _supportsConditionInParens();
        supports = SASS_MEMORY_NEW(SupportsNegation,
          scanner.pstate(start), condition);
      }
      else if (scanner.peekChar() == $lparen) {
        supports = _supportsCondition();
      }
      else {
        Expression* name = expression();
        scanner.expectChar($colon);
        whitespace();
        Expression* value = expression();
        supports = SASS_MEMORY_NEW(SupportsDeclaration,
          scanner.pstate(start), name, value);
      }
      scanner.expectChar($rparen);
      whitespace();
    }

    InterpolationObj media;
    if (scanner.peekChar() == $lparen) {
      media = _mediaQueryList();
    }
    else if (_lookingAtInterpolatedIdentifier()) {
      media = _mediaQueryList();
    }
    return std::make_pair(supports, media);
  }
  // EO tryImportQueries

  // Consumes an `@include` rule.
  // [start] should point before the `@`.
  Mixin_Call* StylesheetParser::_includeRule(Position start)
  {

    std::string ns;
    std::string name = identifier();
    if (scanner.scanChar($dot)) {
      ns = name;
      name = _publicIdentifier();
    }

    whitespace();
    ArgumentsObj arguments;
    if (scanner.peekChar() == $lparen) {
      arguments = _argumentInvocation(true);
    }
    whitespace();

    ParametersObj contentArguments;
    if (scanIdentifier("using")) {
      whitespace();
      contentArguments = _argumentDeclaration();
      whitespace();
    }

    // ToDo: Add checks to allow to ommit arguments fully
    if (!arguments) arguments = SASS_MEMORY_NEW(Arguments, "[pstate]");
    Mixin_CallObj mixin = SASS_MEMORY_NEW(Mixin_Call,
      scanner.pstate(start), name, arguments);

    if (contentArguments || lookingAtChildren()) {
      LOCAL_FLAG(_inContentBlock, true);
      auto block = _withChildren<Block>(
        &StylesheetParser::_childStatement);

      mixin->block_parameters(contentArguments);
      block->update_pstate(scanner.pstate(start));
      mixin->block(block);
    }
    else {
      expectStatementSeparator();
    }

    /*
    var span =
      scanner.spanFrom(start, start).expand((content ? ? arguments).span);
    return IncludeRule(name, arguments, span,
      namespace : ns, content : content);
      */

    // std::cerr << "_includeRule\n"; exit(1);
    return mixin.detach();
  }
  // EO _includeRule

  // Consumes a `@media` rule.
  // [start] should point before the `@`.
  MediaRule* StylesheetParser::mediaRule(Position start)
  {
    InterpolationObj query = _mediaQueryList();
    MediaRule* rule = _withChildren<MediaRule>(
      &StylesheetParser::_childStatement, query);
    rule->update_pstate(scanner.pstate(start));
    return rule;
  }

  // Consumes a mixin declaration.
  // [start] should point before the `@`.
  Definition* StylesheetParser::_mixinRule(Position start)
  {

    // var precedingComment = lastSilentComment;
    // lastSilentComment = null;
    std::string name = identifier();
    whitespace();
    ParametersObj params = {};
    if (scanner.peekChar() == $lparen) {
      params = _argumentDeclaration();
    }

    if (_inMixin || _inContentBlock) {
      error("Mixins may not contain mixin declarations.",
        scanner.pstate(start));
    }
    else if (_inControlDirective) {
      error("Mixins may not be declared in control directives.",
        scanner.pstate(start));
    }

    whitespace();
    LOCAL_FLAG(_inMixin, true);
    LOCAL_FLAG(_mixinHasContent, false);
    // bool hadContent = _mixinHasContent;
    Block_Obj block = SASS_MEMORY_NEW(Block, "[pstate]");
    Definition* def = _withChildren<Definition>(
      &StylesheetParser::_childStatement,
      name, params, block, Definition::MIXIN);
    def->update_pstate(scanner.pstate(start));
    return def;
  }
  // EO _mixinRule

  // Consumes a `@moz-document` rule. Gecko's `@-moz-document` diverges
  // from [the specificiation][] allows the `url-prefix` and `domain`
  // functions to omit quotation marks, contrary to the standard.
  // [the specificiation]: http://www.w3.org/TR/css3-conditional/
  AtRule* StylesheetParser::mozDocumentRule(Position start, Interpolation* name)
  {

    Position valueStart(scanner);
    InterpolationBuffer buffer;
    // bool needsDeprecationWarning = false;

    while (true) {

      if (scanner.peekChar() == $hash) {
        buffer.add(singleInterpolation());
        // needsDeprecationWarning = true;
      }
      else {


        LineScannerState2 identifierStart = scanner.state();
        std::string identifier = this->identifier();
        if (identifier == "url" || identifier == "url-prefix" || identifier == "domain") {
          Interpolation* contents = _tryUrlContents(identifierStart, /* name: */ identifier);
          if (contents != nullptr) {
            buffer.addInterpolation(contents);
          }
          else {
            scanner.expectChar($lparen);
            whitespace();
            StringExpressionObj argument = interpolatedString();
            scanner.expectChar($rparen);

            buffer.write(identifier);
            buffer.write($lparen);
            buffer.addInterpolation(argument->getAsInterpolation());
            buffer.write($rparen);
          }

          // A url-prefix with no argument, or with an empty string as an
          // argument, is not (yet) deprecated.
          std::string trailing = buffer.trailingString();
          if (!Util::ascii_str_ends_with_insensitive(trailing, "url-prefix()") &&
            !Util::ascii_str_ends_with_insensitive(trailing, "url-prefix('')") &&
            !Util::ascii_str_ends_with_insensitive(trailing, "url-prefix(\"\")")) {
            // needsDeprecationWarning = true;
          }
        }
        else if (identifier == "regexp") {
          buffer.write("regexp(");
          scanner.expectChar($lparen);
          StringExpressionObj str = interpolatedString();
          buffer.addInterpolation(str->getAsInterpolation());
          scanner.expectChar($rparen);
          buffer.write($rparen);
          // needsDeprecationWarning = true;
        }
        else {
          error("Invalid function name.",
            scanner.pstate(identifierStart));
        }
      }

      whitespace();
      if (!scanner.scanChar($comma)) break;

      buffer.write($comma);
      buffer.write(rawText(&StylesheetParser::whitespace));

    }

    InterpolationObj value = buffer.getInterpolation(scanner.spanFrom(valueStart));

    AtRule* directive = _withChildren<AtRule>(
      &StylesheetParser::_childStatement, name);
    directive->update_pstate(scanner.pstate(start));
    directive->value(value);
    return directive;

/*
    }

    return _withChildren(_statement, start, (children, span) {
      if (needsDeprecationWarning) {
        logger.warn("""
@-moz-document is deprecated and support will be removed from Sass in a future
relase. For details, see http://bit.ly/moz-document.
""", span: span, deprecation: true);
      }

      return AtRule(name, span, value: value, children: children);
    });
*/

  }

  // Consumes a `@return` rule.
  // [start] should point before the `@`.
  Return* StylesheetParser::_returnRule(Position start)
  {
    ExpressionObj value = expression();
    expectStatementSeparator("@return rule");
    return SASS_MEMORY_NEW(Return,
      scanner.pstate(start), value);
  }
  // EO _returnRule

  // Consumes a `@supports` rule.
  // [start] should point before the `@`.
  SupportsRule* StylesheetParser::supportsRule(Position start)
  {
    auto condition = _supportsCondition();
    whitespace();
    return _withChildren<SupportsRule>(
      &StylesheetParser::_childStatement,
      condition);
  }
  // EO supportsRule


  // Consumes a `@debug` rule.
  // [start] should point before the `@`.
  Debug* StylesheetParser::_debugRule(Position start)
  {
    ExpressionObj value = expression();
    expectStatementSeparator("@debug rule");
    return SASS_MEMORY_NEW(Debug,
      scanner.pstate(start), value);
  }
  // EO _debugRule

  // Consumes a `@warn` rule.
  // [start] should point before the `@`.
  Warning* StylesheetParser::_warnRule(Position start)
  {
    ExpressionObj value = expression();
    expectStatementSeparator("@warn rule");
    return SASS_MEMORY_NEW(Warning,
      scanner.pstate(start), value);
  }
  // EO _warnRule

  // Consumes a `@while` rule. [start] should  point before the `@`. [child] is called 
  // to consume any children that are specifically allowed in the caller's context.
  While* StylesheetParser::_whileRule(Position start, Statement* (StylesheetParser::* child)())
  {
    LOCAL_FLAG(_inControlDirective, true);
    ExpressionObj condition = expression();
    return _withChildren<While>(child, condition.ptr());
  }
  // EO _whileRule

  // Consumes an at-rule that's not explicitly supported by Sass.
  // [start] should point before the `@`. [name] is the name of the at-rule.
  AtRule* StylesheetParser::unknownAtRule(Position start, Interpolation* name)
  {
    LOCAL_FLAG(_inUnknownAtRule, true);

    InterpolationObj value;
    uint8_t next = scanner.peekChar();
    if (next != $exclamation && !atEndOfStatement()) {
      value = almostAnyValue();
    }

    AtRuleObj rule;
    if (lookingAtChildren()) {
      rule = _withChildren<AtRule>(
        &StylesheetParser::_childStatement,
        name);
      rule->name2(name);
      rule->value2(value);
    }
    else {
      expectStatementSeparator();
      rule = SASS_MEMORY_NEW(AtRule, "[pstate]", name);
      rule->name2(name);
      rule->value2(value);
      // rule = AtRule(name, scanner.spanFrom(start), value: value);
    }

    return rule.detach();
  }
  // EO unknownAtRule

  Statement* StylesheetParser::_disallowedAtRule(Position start)
  {
    InterpolationObj value = almostAnyValue();
    error("This at-rule is not allowed here.",
      scanner.spanFrom(start));
    return nullptr;
  }

  Parameters* StylesheetParser::_argumentDeclaration()
  {
    const char* start = scanner.position;
    scanner.expectChar($lparen);
    whitespace();
    std::vector<ParameterObj> parameters;
    NormalizeSet named;
    std::string restArgument;
    while (scanner.peekChar() == $dollar) {
      Position variableStart(scanner);
      std::string name(variableName());
      whitespace();

      ExpressionObj defaultValue;
      if (scanner.scanChar($colon)) {
        whitespace();
        defaultValue = _expressionUntilComma();
      }
      else if (scanner.scanChar($dot)) {
        scanner.expectChar($dot);
        scanner.expectChar($dot);
        whitespace();
        restArgument = name;
        break;
      }

     
      parameters.push_back(SASS_MEMORY_NEW(Parameter,
        scanner.pstate(variableStart), name, defaultValue));

      if (named.count(name) == 1) {
        error("Duplicate argument.",
          parameters.back()->pstate());
      }
      named.insert(name);

      if (!scanner.scanChar($comma)) break;
      whitespace();
    }
    scanner.expectChar($rparen);
    Parameters* params = SASS_MEMORY_NEW(Parameters,
      scanner.pstate());
    if (!restArgument.empty()) {
      params->has_rest_parameter(true);
      parameters.push_back(SASS_MEMORY_NEW(Parameter,
        "[pstate]", restArgument, {}, true));
      // ToDo: pstate rest argument
    }
    params->pstate(scanner.pstate(start));
    params->concat(parameters);
    return params;
  }
  // EO _argumentDeclaration

  ArgumentDeclaration* StylesheetParser::_argumentDeclaration2()
  {

    Position start(scanner);
    scanner.expectChar($lparen);
    whitespace();
    std::vector<ArgumentObj> arguments;
    NormalizeSet named;
    std::string restArgument;
    while (scanner.peekChar() == $dollar) {
      Position variableStart(scanner);
      std::string name(variableName());
      whitespace();

      ExpressionObj defaultValue;
      if (scanner.scanChar($colon)) {
        whitespace();
        defaultValue = _expressionUntilComma();
      }
      else if (scanner.scanChar($dot)) {
        scanner.expectChar($dot);
        scanner.expectChar($dot);
        whitespace();
        restArgument = name;
        break;
      }

      arguments.push_back(SASS_MEMORY_NEW(Argument,
        scanner.pstate(variableStart), defaultValue, name));

      if (named.count(name) == 1) {
        error("Duplicate argument.",
          arguments.back()->pstate());
      }
      named.insert(name);

      if (!scanner.scanChar($comma)) break;
      whitespace();
    }
    scanner.expectChar($rparen);

    return SASS_MEMORY_NEW(
      ArgumentDeclaration,
      scanner.pstate(),
      arguments,
      restArgument);

  }

  Arguments* StylesheetParser::_argumentInvocation(bool mixin)
  {

    Position start(scanner);
    scanner.expectChar($lparen);
    whitespace();

    std::vector<ExpressionObj> positional;

    NormalizedMap<ExpressionObj> named;

    // Convert to old libsass arguments (ToDo: refactor)
    ArgumentsObj args = SASS_MEMORY_NEW(Arguments,
      scanner.pstate());

    ExpressionObj restArg;
    ExpressionObj kwdRest;
    while (_lookingAtExpression()) {
      ExpressionObj expression = _expressionUntilComma(!mixin);
      whitespace();

      Variable* var = Cast<Variable>(expression);
      if (var && scanner.scanChar($colon)) {
        std::string name(var->name());
        whitespace();
        if (named.count(name) == 1) {
          error("Duplicate argument.",
            expression->pstate());
        }
        auto ex = _expressionUntilComma(!mixin);
        named[name] = ex;
        args->append(SASS_MEMORY_NEW(Argument,
          "[pstate]", ex, name));
      }
      else if (scanner.scanChar($dot)) {
        scanner.expectChar($dot);
        scanner.expectChar($dot);
        if (restArg == nullptr) {
          restArg = expression;
        }
        else {
          kwdRest = expression;
          whitespace();
          break;
        }
      }
      else if (!named.empty()) {
        scanner.expect("...");
      }
      else {
        args->append(SASS_MEMORY_NEW(Argument,
          "[pstate]", expression));
        positional.push_back(expression);
      }

      whitespace();
      if (!scanner.scanChar($comma)) break;
      whitespace();
    }
    scanner.expectChar($rparen);

    if (restArg != nullptr) {
      args->append(SASS_MEMORY_NEW(Argument,
        "[pstate]", restArg, "", true));
    }

    if (kwdRest != nullptr) {
      args->append(SASS_MEMORY_NEW(Argument,
        "[pstate]", kwdRest, "", false, true));
    }

    return args.detach();
  }
  // EO _argumentInvocation

  // Consumes an expression. If [bracketList] is true, parses this expression as
  // the contents of a bracketed list. If [singleEquals] is true, allows the
  // Microsoft-style `=` operator at the top level. If [until] is passed, it's
  // called each time the expression could end and still be a valid expression.
  // When it returns `true`, this returns the expression.
  Expression* StylesheetParser::expression(bool bracketList, bool singleEquals, bool(StylesheetParser::* until)())
  {

    NESTING_GUARD(_recursion);

    if (until != nullptr && (this->*until)()) {
      scanner.error("Expected expression.");
    }

    // LineScannerState beforeBracket;
    LineScannerState2 start(scanner.state());
    if (bracketList) {
      // beforeBracket = scanner.position;
      scanner.expectChar($lbracket);
      whitespace();

      if (scanner.scanChar($rbracket)) {
        ListExpression* list = SASS_MEMORY_NEW(ListExpression,
          scanner.pstate(start), SASS_UNDEF);
        list->hasBrackets(true);
        return list;
      }
    }

    ExpressionParser ep(*this);

    // const char* start = scanner.position;
    bool wasInParentheses = _inParentheses;

    while (true) {
      whitespace();
      if (until != nullptr && (this->*until)()) break;

      uint8_t next, first = scanner.peekChar();

      switch (first) {
      case $lparen:
        // Parenthesized numbers can't be slash-separated.
        ep.addSingleExpression(_parentheses());
        break;

      case $lbracket:
        ep.addSingleExpression(expression(true));
        break;

      case $dollar:
        ep.addSingleExpression(_variable());
        break;

      case $ampersand:
        ep.addSingleExpression(_selector());
        break;

      case $single_quote:
      case $double_quote:
        ep.addSingleExpression(interpolatedString());
        break;

      case $hash:
        ep.addSingleExpression(_hashExpression());
        break;

      case $equal:
        scanner.readChar();
        if (singleEquals && scanner.peekChar() != $equal) {
          ep.resolveSpaceExpressions();
          ep.singleEqualsOperand = ep.singleExpression;
          ep.singleExpression = {};
        }
        else {
          scanner.expectChar($equal);
          ep.addOperator(Sass_OP::EQ);
        }
        break;

      case $exclamation:
        next = scanner.peekChar(1);
        if (next == $equal) {
          scanner.readChar();
          scanner.readChar();
          ep.addOperator(Sass_OP::NEQ);
        }
        else if (next == $nul ||
            equalsLetterIgnoreCase($i, next) ||
          isWhitespace(next))
        {
          ep.addSingleExpression(_importantExpression());
        }
        else {
          goto endOfLoop;
        }
        break;

      case $langle:
        scanner.readChar();
        ep.addOperator(scanner.scanChar($equal)
          ? Sass_OP::LTE : Sass_OP::LT);
        break;

      case $rangle:
        scanner.readChar();
        ep.addOperator(scanner.scanChar($equal)
          ? Sass_OP::GTE : Sass_OP::GT);
        break;

      case $asterisk:
        scanner.readChar();
        ep.addOperator(Sass_OP::MUL);
        break;

      case $plus:
        if (ep.singleExpression == nullptr) {
          ep.addSingleExpression(_unaryOperation());
        }
        else {
          scanner.readChar();
          ep.addOperator(Sass_OP::ADD);
        }
        break;

      case $minus:
        next = scanner.peekChar(1);
        if ((isDigit(next) || next == $dot) &&
          // Make sure `1-2` parses as `1 - 2`, not `1 (-2)`.
          (ep.singleExpression == nullptr ||
            isWhitespace(scanner.peekChar(-1)))) {
          ep.addSingleExpression(_number(), true);
        }
        else if (_lookingAtInterpolatedIdentifier()) {
          ep.addSingleExpression(identifierLike());
        }
        else if (ep.singleExpression == nullptr) {
          ep.addSingleExpression(_unaryOperation());
        }
        else {
          scanner.readChar();
          ep.addOperator(Sass_OP::SUB);
        }
        break;

      case $slash:
        if (ep.singleExpression == nullptr) {
          ep.addSingleExpression(_unaryOperation());
        }
        else {
          scanner.readChar();
          ep.addOperator(Sass_OP::DIV);
        }
        break;

      case $percent:
        scanner.readChar();
        ep.addOperator(Sass_OP::MOD);
        break;

      case $0:
      case $1:
      case $2:
      case $3:
      case $4:
      case $5:
      case $6:
      case $7:
      case $8:
      case $9:
        ep.addSingleExpression(_number(), true);
        break;

      case $dot:
        if (scanner.peekChar(1) == $dot) goto endOfLoop;
        ep.addSingleExpression(_number(), true);
        break;

      case $a:
        if (!plainCss() && scanIdentifier("and")) {
          ep.addOperator(Sass_OP::AND);
        }
        else {
          ep.addSingleExpression(identifierLike());
        }
        break;

      case $o:
        if (!plainCss() && scanIdentifier("or")) {
          ep.addOperator(Sass_OP::OR);
        }
        else {
          ep.addSingleExpression(identifierLike());
        }
        break;

      case $u:
      case $U:
        if (scanner.peekChar(1) == $plus) {
          ep.addSingleExpression(_unicodeRange());
        }
        else {
          ep.addSingleExpression(identifierLike());
        }
        break;

      case $b:
      case $c:
      case $d:
      case $e:
      case $f:
      case $g:
      case $h:
      case $i:
      case $j:
      case $k:
      case $l:
      case $m:
      case $n:
      case $p:
      case $q:
      case $r:
      case $s:
      case $t:
      case $v:
      case $w:
      case $x:
      case $y:
      case $z:
      case $A:
      case $B:
      case $C:
      case $D:
      case $E:
      case $F:
      case $G:
      case $H:
      case $I:
      case $J:
      case $K:
      case $L:
      case $M:
      case $N:
      case $O:
      case $P:
      case $Q:
      case $R:
      case $S:
      case $T:
      case $V:
      case $W:
      case $X:
      case $Y:
      case $Z:
      case $_:
      case $backslash:
        ep.addSingleExpression(identifierLike());
        break;

      case $comma:
        // If we discover we're parsing a list whose first element is a
        // division operation, and we're in parentheses, reparse outside of a
        // paren context. This ensures that `(1/2, 1)` doesn't perform division
        // on its first element.
        if (_inParentheses) {
          _inParentheses = false;
          if (ep.allowSlash) {
            ep.resetState();
            break;
          }
        }

        if (ep.singleExpression == nullptr) {
          scanner.error("Expected expression.");
        }

        ep.resolveSpaceExpressions();
        ep.commaExpressions.push_back(ep.singleExpression);
        scanner.readChar();
        ep.allowSlash = true;
        ep.singleExpression = {};
        break;

      default:
        if (first != $nul && first >= 0x80) {
          ep.addSingleExpression(identifierLike());
          break;
        }
        else {
          goto endOfLoop;
        }
      }
    }

  endOfLoop:

    if (bracketList) scanner.expectChar($rbracket);
    if (!ep.commaExpressions.empty()) {
      ep.resolveSpaceExpressions();
      _inParentheses = wasInParentheses;
      if (ep.singleExpression != nullptr) {
        ep.commaExpressions.push_back(ep.singleExpression);
      }
      ListExpression* list = SASS_MEMORY_NEW(ListExpression,
        scanner.pstate(start), SASS_COMMA);
      list->concat(ep.commaExpressions);
      list->hasBrackets(bracketList);
      return list;
    }
    else if (bracketList &&
        !ep.spaceExpressions.empty() &&
        ep.singleEqualsOperand == nullptr) {
      ep.resolveOperations();
      ListExpression* list = SASS_MEMORY_NEW(ListExpression,
        scanner.pstate(start), SASS_SPACE);
      ep.spaceExpressions.push_back(ep.singleExpression);
      list->concat(ep.spaceExpressions);
      list->hasBrackets(true);
      return list;
    }
    else {
      ep.resolveSpaceExpressions();
      if (bracketList) {
        ListExpression* list = SASS_MEMORY_NEW(ListExpression,
          scanner.pstate(start), SASS_UNDEF);
        list->append(ep.singleExpression);
        list->hasBrackets(true);
        return list;
      }
      return ep.singleExpression.detach();
    }

  }
  // EO expression

  // Helper function for until condition
  bool StylesheetParser::nextCharIsComma()
  {
    return scanner.peekChar() == $comma;
  }

  Expression* StylesheetParser::_expressionUntilComma(bool singleEquals)
  {
    return expression(false, singleEquals,
      &StylesheetParser::nextCharIsComma);
  }

  // Consumes an expression that doesn't contain any top-level whitespace.
  Expression* StylesheetParser::_singleExpression()
  {
    NESTING_GUARD(_recursion);
    uint8_t first = scanner.peekChar();
    switch (first) {
      // Note: when adding a new case, make sure it's reflected in
      // [_lookingAtExpression] and [_expression].
    case $lparen:
      return _parentheses();
    case $slash:
      return _unaryOperation();
    case $dot:
      return _number().detach();
    case $lbracket:
      return expression(true);
    case $dollar:
      return _variable();
    case $ampersand:
      return _selector();

    case $single_quote:
    case $double_quote:
      return interpolatedString();

    case $hash:
      return _hashExpression();

    case $plus:
      return _plusExpression();

    case $minus:
      return _minusExpression();

    case $exclamation:
      return _importantExpression();

    case $u:
    case $U:
      if (scanner.peekChar(1) == $plus) {
        return _unicodeRange();
      }
      else {
        return identifierLike();
      }
      break;

    case $0:
    case $1:
    case $2:
    case $3:
    case $4:
    case $5:
    case $6:
    case $7:
    case $8:
    case $9:
      return _number().detach();
      break;

    case $a:
    case $b:
    case $c:
    case $d:
    case $e:
    case $f:
    case $g:
    case $h:
    case $i:
    case $j:
    case $k:
    case $l:
    case $m:
    case $n:
    case $o:
    case $p:
    case $q:
    case $r:
    case $s:
    case $t:
    case $v:
    case $w:
    case $x:
    case $y:
    case $z:
    case $A:
    case $B:
    case $C:
    case $D:
    case $E:
    case $F:
    case $G:
    case $H:
    case $I:
    case $J:
    case $K:
    case $L:
    case $M:
    case $N:
    case $O:
    case $P:
    case $Q:
    case $R:
    case $S:
    case $T:
    case $V:
    case $W:
    case $X:
    case $Y:
    case $Z:
    case $_:
    case $backslash:
      return identifierLike();
      break;

    default:
      if (first != $nul && first >= 0x80) {
        return identifierLike();
      }
      scanner.error("Expected expression.");
      return nullptr;
    }
  }
  // EO _singleExpression

  // Consumes a parenthesized expression.
  Expression* StylesheetParser::_parentheses()
  {
    if (plainCss()) {
      scanner.error("Parentheses aren't allowed in plain CSS."
      /* , length: 1 */);
    }

    LOCAL_FLAG(_inParentheses, true);

    Position start(scanner);
    scanner.expectChar($lparen);
    whitespace();
    if (!_lookingAtExpression()) {
      scanner.expectChar($rparen);
      // ToDo: ListExpression
      return SASS_MEMORY_NEW(ListExpression,
        scanner.pstate(start),
        SASS_UNDEF);
    }

    ExpressionObj first = _expressionUntilComma();
    if (scanner.scanChar($colon)) {
      whitespace();
      return _map(first, start);
    }

    if (!scanner.scanChar($comma)) {
      scanner.expectChar($rparen);
      return SASS_MEMORY_NEW(ParenthesizedExpression,
        scanner.pstate(start), first);
    }
    whitespace();

    std::vector<ExpressionObj>
      expressions = { first };

    // We did parse a comma above
// This one fails
    ListExpressionObj list = SASS_MEMORY_NEW(
      ListExpression,
      scanner.pstate(start),
      SASS_COMMA);

    while (true) {
      if (!_lookingAtExpression()) {
        break;
      }
      expressions.push_back(_expressionUntilComma());
      if (!scanner.scanChar($comma)) {
        break;
      }
      list->separator(SASS_COMMA);
      whitespace();
    }

    scanner.expectChar($rparen);
    // ToDo: ListExpression
    list->concat(expressions);
    list->update_pstate(scanner.pstate(start));
    return list.detach();
  }
  // EO _parentheses

  // Consumes a map expression. This expects to be called after the
  // first colon in the map, with [first] as the expression before
  // the colon and [start] the point before the opening parenthesis.
  Expression* StylesheetParser::_map(Expression* first, Position start)
  {
    // ListObj map = SASS_MEMORY_NEW(List,
    //   scanner.pstate(start),
    //   0, SASS_HASH);
    MapExpressionObj map = SASS_MEMORY_NEW(
      MapExpression, scanner.pstate(start));

    map->append(first);
    map->append(_expressionUntilComma());

    while (scanner.scanChar($comma)) {
      whitespace();
      if (!_lookingAtExpression()) break;

      map->append(_expressionUntilComma());
      scanner.expectChar($colon);
      whitespace();
      map->append(_expressionUntilComma());
    }

    scanner.expectChar($rparen);
    map->pstate(scanner.pstate(start));
    return map.detach();
  }
  // EO _map

  Expression* StylesheetParser::_hashExpression()
  {
    // assert(scanner.peekChar() == $hash);
    if (scanner.peekChar(1) == $lbrace) {
      return identifierLike();
    }

    Position start(scanner);
    scanner.expectChar($hash);

    uint8_t first = scanner.peekChar();
    if (first != $nul && isDigit(first)) {
      // ColorExpression
      return _hexColorContents(start);
    }

    const char* afterHash = scanner.position;
    InterpolationObj identifier = interpolatedIdentifier();
    if (_isHexColor(identifier)) {
      scanner.position = afterHash;
      // ColorExpression
      return _hexColorContents(start);
    }


    InterpolationBuffer buffer;
    buffer.write($hash);
    buffer.addInterpolation(identifier);
    ParserState pstate(scanner.pstate(start));
    return SASS_MEMORY_NEW(StringExpression,
      pstate, buffer.getInterpolation(pstate));
  }

  Color* StylesheetParser::_hexColorContents(Position start)
  {

    uint8_t digit1 = _hexDigit();
    uint8_t digit2 = _hexDigit();
    uint8_t digit3 = _hexDigit();

    uint8_t red;
    uint8_t green;
    uint8_t blue;
    double alpha = 1.0;
    if (!isHex(scanner.peekChar())) {
      // #abc
      red = (digit1 << 4) + digit1;
      green = (digit2 << 4) + digit2;
      blue = (digit3 << 4) + digit3;
    }
    else {
      uint8_t digit4 = _hexDigit();
      if (!isHex(scanner.peekChar())) {
        // #abcd
        red = (digit1 << 4) + digit1;
        green = (digit2 << 4) + digit2;
        blue = (digit3 << 4) + digit3;
        alpha = ((digit4 << 4) + digit4) / 255.0;
      }
      else {
        red = (digit1 << 4) + digit2;
        green = (digit3 << 4) + digit4;
        blue = (_hexDigit() << 4) + _hexDigit();

        if (isHex(scanner.peekChar())) {
          alpha = ((_hexDigit() << 4) + _hexDigit()) / 255.0;
        }
      }
    }

    ParserState pstate(scanner.pstate(start));
    pstate.position = scanner.position;

    std::string original(start.position, pstate.position);

    return SASS_MEMORY_NEW(Color_RGBA,
      pstate, red, green, blue, alpha, original);
  }

  // Returns whether [interpolation] is a plain
  // string that can be parsed as a hex color.
  bool StylesheetParser::_isHexColor(Interpolation* interpolation) const
  {
    std::string plain = interpolation->getPlainString();
    if (plain.empty()) return false;
    if (plain.length() != 3 &&
      plain.length() != 4 &&
      plain.length() != 6 &&
      plain.length() != 8)
    {
      return false;
    }
    // return plain.codeUnits.every(isHex);
    for (size_t i = 0; i < plain.length(); i++) {
      if (!isHex(plain[i])) return false;
    }
    return true;
  }
  // EO _isHexColor

  // Consumes a single hexadecimal digit.
  uint8_t StylesheetParser::_hexDigit()
  {
    uint8_t chr = scanner.peekChar();
    if (chr == $nul || !isHex(chr)) {
      scanner.error("Expected hex digit.");
    }
    return asHex(scanner.readChar());
  }
  // EO _hexDigit

  // Consumes an expression that starts with a `+`.
  Expression* StylesheetParser::_plusExpression()
  {
    SASS_ASSERT(scanner.peekChar() == $plus,
      "plusExpression expects a plus sign");
    uint8_t next = scanner.peekChar(1);
    if (isDigit(next) || next == $dot) {
      return _number().detach();
    }
    else {
      return _unaryOperation();
    }
  }
  // EO _plusExpression

  // Consumes an expression that starts with a `-`.
  Expression* StylesheetParser::_minusExpression()
  {
    SASS_ASSERT(scanner.peekChar() == $minus,
      "minusExpression expects a minus sign");
    uint8_t next = scanner.peekChar(1);
    if (isDigit(next) || next == $dot) return _number().detach();
    if (_lookingAtInterpolatedIdentifier()) return identifierLike();
    return _unaryOperation();
  }
  // EO _minusExpression

  // Consumes an `!important` expression.
  StringExpression* StylesheetParser::_importantExpression()
  {
    SASS_ASSERT(scanner.peekChar() == $exclamation,
      "importantExpression expects an exclamation");
    Position start(scanner);
    scanner.readChar();
    whitespace();
    expectIdentifier("important");
    return StringExpression::plain(
      scanner.pstate(start), "!important");
  }
  // EO _importantExpression

  // Consumes a unary operation expression.
  Unary_Expression* StylesheetParser::_unaryOperation()
  {
    // const char* start = scanner.position;
    Unary_Expression::Type op =
      _unaryOperatorFor(scanner.readChar());
    if (op == Unary_Expression::Type::END) {
      scanner.error("Expected unary operator." /* , position: scanner.position - 1 */);
    }
    else if (plainCss() && op != Unary_Expression::Type::SLASH) {
      scanner.error("Operators aren't allowed in plain CSS." /* ,
        position: scanner.position - 1, length : 1 */);
    }

    whitespace();
    Expression* operand = _singleExpression();
    return SASS_MEMORY_NEW(Unary_Expression,
      "[pstate]", op, operand);
  }
  // EO _unaryOperation

  // Returns the unsary operator corresponding to [character],
  // or `null` if the character is not a unary operator.
  Unary_Expression::Type StylesheetParser::_unaryOperatorFor(uint8_t character)
  {
    switch (character) {
    case $plus:
      return Unary_Expression::Type::PLUS;
    case $minus:
      return Unary_Expression::Type::MINUS;
    case $slash:
      return Unary_Expression::Type::SLASH;
    default:
      return Unary_Expression::Type::END;
    }
  }
  // EO _unaryOperatorFor

  // Consumes a number expression.
  NumberObj StylesheetParser::_number()
  {
    LineScannerState2 start = scanner.state();
    uint8_t first = scanner.peekChar();

    double sign = first == $minus ? -1 : 1;
    if (first == $plus || first == $minus) scanner.readChar();

    double number = scanner.peekChar() == $dot ? 0.0 : naturalNumber();

    // Don't complain about a dot after a number unless the number
    // starts with a dot. We don't allow a plain ".", but we need
    // to allow "1." so that "1..." will work as a rest argument.
    number += _tryDecimal(scanner.position != start.position);
    number *= _tryExponent();

    std::string unit;
    if (scanner.scanChar($percent)) {
      unit = "%";
    }
    else if (lookingAtIdentifier() &&
      // Disallow units beginning with `--`.
      (scanner.peekChar() != $dash || scanner.peekChar(1) != $dash)) {
      unit = identifier(true);
    }

    return SASS_MEMORY_NEW(Number,
      scanner.pstate(start.offset),
      sign * number, unit);
  }

  // Consumes the decimal component of a number and returns its value, or 0
  // if there is no decimal component. If [allowTrailingDot] is `false`, this
  // will throw an error if there's a dot without any numbers following it.
  // Otherwise, it will ignore the dot without consuming it.
  double StylesheetParser::_tryDecimal(bool allowTrailingDot)
  {
    Position start(scanner);
    if (scanner.peekChar() != $dot) return 0.0;

    if (!isDigit(scanner.peekChar(1))) {
      if (allowTrailingDot) return 0.0;
      scanner.error("Expected digit.",
        scanner.pstate(start));
    }

    scanner.readChar();
    while (isDigit(scanner.peekChar())) {
      scanner.readChar();
    }

    // Use built-in double parsing so that we don't accumulate
    // floating-point errors for numbers with lots of digits.
    std::string nr(scanner.substring(start));
    return sass_strtod(nr.c_str());
  }
  // EO _tryDecimal

  // Consumes the exponent component of a number and returns
  // its value, or 1 if there is no exponent component.
  double StylesheetParser::_tryExponent()
  {
    uint8_t first = scanner.peekChar();
    if (first != $e && first != $E) return 1.0;

    uint8_t next = scanner.peekChar(1);
    if (!isDigit(next) && next != $minus && next != $plus) return 1.0;

    scanner.readChar();
    double exponentSign = next == $minus ? -1.0 : 1.0;
    if (next == $plus || next == $minus) scanner.readChar();
    if (!isDigit(scanner.peekChar())) scanner.error("Expected digit.");

    double exponent = 0.0;
    while (isDigit(scanner.peekChar())) {
      exponent *= 10.0;
      exponent += scanner.readChar() - $0;
    }

    return pow(10.0, exponentSign * exponent);
  }
  // EO _tryExponent

  // Consumes a unicode range expression.
  StringExpression* StylesheetParser::_unicodeRange()
  {
    LineScannerState2 state = scanner.state();
    expectCharIgnoreCase($u);
    scanner.expectChar($plus);

    size_t i = 0;
    for (; i < 6; i++) {
      if (!scanCharIf(isHex)) break;
    }

    if (scanner.scanChar($question)) {
      i++;
      for (; i < 6; i++) {
        if (!scanner.scanChar($question)) break;
      }
      return StringExpression::plain(
        scanner.spanFrom(state.offset),
        scanner.substring(state.position)
      );
    }
    if (i == 0) scanner.error("Expected hex digit or \"?\".");

    if (scanner.scanChar($minus)) {
      size_t j = 0;
      for (; j < 6; j++) {
        if (!scanCharIf(isHex)) break;
      }
      if (j == 0) scanner.error("Expected hex digit.");
    }

    if (_lookingAtInterpolatedIdentifierBody()) {
      scanner.error("Expected end of identifier.");
    }

    return StringExpression::plain(
      scanner.pstate(state.offset),
      scanner.substring(state.position)
    );
  }
  // EO _unicodeRange

  // Consumes a variable expression.
  Variable* StylesheetParser::_variable()
  {
    Position start(scanner);

    std::string ns, name = variableName();
    if (scanner.peekChar() == $dot && scanner.peekChar(1) != $dot) {
      scanner.readChar();
      ns = name;
      name = _publicIdentifier();
    }

    if (plainCss()) {
      error("Sass variables aren't allowed in plain CSS.",
        scanner.pstate(start));
    }

    return SASS_MEMORY_NEW(Variable,
      scanner.pstate(start),
      name /*, ns */);

  }
  // _variable

  // Consumes a selector expression.
  Parent_Reference* StylesheetParser::_selector()
  {
    if (plainCss()) {
      scanner.error("The parent selector isn't allowed in plain CSS." /* ,
        length: 1 */);
    }

    LineScannerState2 start(scanner.state());
    scanner.expectChar($ampersand);

    if (scanner.scanChar($ampersand)) {
      warn(
        "In Sass, \"&&\" means two copies of the parent selector. You "
        "probably want to use \"and\" instead.",
        scanner.pstate(start.offset));
      scanner.offset.column -= 1;
      scanner.position -= 1;
    }

    return SASS_MEMORY_NEW(Parent_Reference,
      scanner.pstate(start.offset));
  }
  // _selector

  // Consumes a quoted string expression.
  StringExpression* StylesheetParser::interpolatedString()
  {
    // NOTE: this logic is largely duplicated in ScssParser.interpolatedString.
    // Most changes here should be mirrored there.

    Position start(scanner);
    uint8_t quote = scanner.readChar();
    uint8_t next = 0, second = 0;

    if (quote != $single_quote && quote != $double_quote) {
      // ToDo: dart-sass passes the start position!?
      scanner.error("Expected string.",
        /*position:*/ scanner.pstate(start));
    }

    InterpolationBuffer buffer;
    while (true) {
      if (!scanner.peekChar(next)) {
        break;
      }
      if (next == quote) {
        scanner.readChar();
        break;
      }
      else if (next == $nul || isNewline(next)) {
        std::stringstream strm;
        strm << "Expected " << quote << ".";
        scanner.error(strm.str());
      }
      else if (next == $backslash) {
        if (!scanner.peekChar(second, 1)) {
          break;
        }
        if (isNewline(second)) {
          scanner.readChar();
          scanner.readChar();
          if (second == $cr) scanner.scanChar($lf);
        }
        else {
          buffer.writeCharCode(escapeCharacter());
        }
      }
      else if (next == $hash) {
        if (scanner.peekChar(1) == $lbrace) {
          buffer.add(singleInterpolation());
        }
        else {
          buffer.write(scanner.readChar());
        }
      }
      else {
        buffer.write(scanner.readChar());
      }
    }

    InterpolationObj itpl = buffer.getInterpolation();
    return SASS_MEMORY_NEW(StringExpression,
      itpl->pstate(), itpl, true);

  }
  // EO interpolatedString

  // Consumes an expression that starts like an identifier.
  Expression* StylesheetParser::identifierLike()
  {
    LineScannerState2 start = scanner.state();
    InterpolationObj identifier = interpolatedIdentifier();
    std::string plain(identifier->getPlainString());

    if (!plain.empty()) {
      if (plain == "if") {
        Arguments* args = _argumentInvocation();
        return SASS_MEMORY_NEW(FunctionExpression,
          "[pstate]", plain, args, "");

        // ToDo: dart-sass has an if expression class for this
        // return SASS_MEMORY_NEW(If, "[pstate]", invocation, {});
        // return IfExpression(invocation, spanForList([identifier, invocation]));
      }
      else if (plain == "not") {
        whitespace();
        Expression* expression = _singleExpression();
        return SASS_MEMORY_NEW(Unary_Expression,
          scanner.pstate(start),
          Unary_Expression::NOT,
          expression);
      }

      std::string lower(plain);
      Util::ascii_str_tolower(&lower);
      if (scanner.peekChar() != $lparen) {
        if (plain == "false") {
          return SASS_MEMORY_NEW(Boolean,
            scanner.pstate(start), false);
        }
        else if (plain == "true") {
          return SASS_MEMORY_NEW(Boolean,
            scanner.pstate(start), true);
        }
        else if (plain == "null") {
          return SASS_MEMORY_NEW(Null,
            scanner.pstate(start));
        }

        // ToDo: dart-sass has ColorExpression(color);
        if (const Color_RGBA* color = name_to_color(lower)) {
          Color_RGBA* copy = SASS_MEMORY_COPY(color);
          copy->pstate(identifier->pstate());
          copy->disp(plain);
          return copy;
        }
      }

      auto specialFunction = trySpecialFunction(lower, start);
      if (specialFunction != nullptr) {
        return specialFunction;
      }
    }

    std::string ns;
    Position beforeName(scanner);
    uint8_t next = scanner.peekChar();
    if (next == $dot) {

      if (scanner.peekChar(1) == $dot) {
        return SASS_MEMORY_NEW(StringExpression,
          scanner.pstate(beforeName), identifier);
      }

      ns = identifier->getPlainString();
      scanner.readChar();
      beforeName = scanner.offset;

      StringLiteralObj ident = _publicIdentifier2();
      InterpolationObj itpl = SASS_MEMORY_NEW(Interpolation,
        ident->pstate(), ident);

      if (ns.empty()) {
        error("Interpolation isn't allowed in namespaces.",
          scanner.pstate(start));
      }

      Arguments* args = _argumentInvocation();
      return SASS_MEMORY_NEW(FunctionExpression,
        scanner.pstate(start), itpl, args, ns);

    }
    else if (next == $lparen) {
      Arguments* args = _argumentInvocation();
      return SASS_MEMORY_NEW(FunctionExpression,
        scanner.pstate(start), identifier, args, ns);
    }
    else {
      return SASS_MEMORY_NEW(StringExpression,
        identifier->pstate(), identifier);
    }

  }
  // identifierLike

  // If [name] is the name of a function with special syntax, consumes it.
  // Otherwise, returns `null`. [start] is the location before the beginning of [name].
  StringExpression* StylesheetParser::trySpecialFunction(std::string name, LineScannerState2 start)
  {
    uint8_t next = 0;
    InterpolationBuffer buffer;
    std::string normalized(Util::unvendor(name));

    if (normalized == "calc" || normalized == "element" || normalized == "expression") {
      if (!scanner.scanChar($lparen)) return nullptr;
      buffer.write(name);
      buffer.write($lparen);
    }
    else if (normalized == "min" || normalized == "max") {
      // min() and max() are parsed as the plain CSS mathematical functions if
      // possible, and otherwise are parsed as normal Sass functions.
      LineScannerState2 beginningOfContents = scanner.state();
      if (!scanner.scanChar($lparen)) return nullptr;
      whitespace();

      buffer.write(name);
      buffer.write($lparen);

      if (!_tryMinMaxContents(buffer)) {
        scanner.resetState(beginningOfContents);
        return nullptr;
      }

      return SASS_MEMORY_NEW(StringExpression,
        scanner.pstate(start), buffer.getInterpolation());
    }
    else if (normalized == "progid") {
      if (!scanner.scanChar($colon)) return nullptr;
      buffer.write(name);
      buffer.write($colon);
      while (scanner.peekChar(next) &&
          (isAlphabetic(next) || next == $dot)) {
        buffer.write(scanner.readChar());
      }
      scanner.expectChar($lparen);
      buffer.write($lparen);
    }
    else if (normalized == "url") {
      InterpolationObj contents = _tryUrlContents(start);
      if (contents == nullptr) return nullptr;
      return SASS_MEMORY_NEW(StringExpression,
        scanner.pstate(start), contents);
    }
    else {
      return nullptr;
    }

    StringExpressionObj qwe = _interpolatedDeclarationValue(true);
    buffer.addInterpolation(qwe->text());
    scanner.expectChar($rparen);
    buffer.write($rparen);

    return SASS_MEMORY_NEW(StringExpression,
      scanner.pstate(start), buffer.getInterpolation());
  }
  // trySpecialFunction

  // Consumes the contents of a plain-CSS `min()` or `max()` function into
  // [buffer] if one is available. Returns whether this succeeded. If [allowComma]
  // is `true` (the default), this allows `CalcValue` productions separated by commas.
  bool StylesheetParser::_tryMinMaxContents(InterpolationBuffer& buffer, bool allowComma)
  {
    uint8_t next = 0;
    // The number of open parentheses that need to be closed.
    while (true) {
      if (!scanner.peekChar(next)) {
        return false;
      }
      switch (next) {
      case $minus:
      case $plus:
      case $0:
      case $1:
      case $2:
      case $3:
      case $4:
      case $5:
      case $6:
      case $7:
      case $8:
      case $9:
        try {
          buffer.write(rawText(&StylesheetParser::_number));
        }
        catch (Exception::InvalidSyntax&) {
           return false;
        }
        break;

      case $hash:
        if (scanner.peekChar(1) != $lbrace) return false;
        buffer.add(singleInterpolation());
        break;

      case $c:
      case $C:
        if (!_tryMinMaxFunction(buffer, "calc")) return false;
        break;

      case $e:
      case $E:
        if (!_tryMinMaxFunction(buffer, "env")) return false;
        break;

      case $v:
      case $V:
        if (!_tryMinMaxFunction(buffer, "var")) return false;
        break;

      case $lparen:
        buffer.write(scanner.readChar());
        if (!_tryMinMaxContents(buffer, false)) return false;
        break;

      case $m:
      case $M:
        scanner.readChar();
        if (scanCharIgnoreCase($i)) {
          if (!scanCharIgnoreCase($n)) return false;
          buffer.write("min(");
        }
        else if (scanCharIgnoreCase($a)) {
          if (!scanCharIgnoreCase($x)) return false;
          buffer.write("max(");
        }
        else {
          return false;
        }
        if (!scanner.scanChar($lparen)) return false;

        if (!_tryMinMaxContents(buffer)) return false;
        break;

      default:
        return false;
      }

      whitespace();

      next = scanner.peekChar();
      switch (next) {
      case $rparen:
        buffer.write(scanner.readChar());
        return true;

      case $plus:
      case $minus:
      case $asterisk:
      case $slash:
        buffer.write($space);
        buffer.write(scanner.readChar());
        buffer.write($space);
        break;

      case $comma:
        if (!allowComma) return false;
        buffer.write(scanner.readChar());
        buffer.write($space);
        break;

      default:
        return false;
      }

      whitespace();
    }
  }
  // EO _tryMinMaxContents

  // Consumes a function named [name] containing an
  // `InterpolatedDeclarationValue` if possible, and adds its text to [buffer].
  // Returns whether such a function could be consumed.
  bool StylesheetParser::_tryMinMaxFunction(InterpolationBuffer& buffer, std::string name)
  {
    if (!scanIdentifier(name)) return false;
    if (!scanner.scanChar($lparen)) return false;
    buffer.write(name);
    buffer.write($lparen);
    StringExpressionObj decl = _interpolatedDeclarationValue(true);
    buffer.addInterpolation(decl->getAsInterpolation());
    buffer.write($rparen);
    if (!scanner.scanChar($rparen)) return false;
    return true;
  }
  // _tryMinMaxFunction

  // Like [_urlContents], but returns `null` if the URL fails to parse.
  // [start] is the position before the beginning of the name.
  // [name] is the function's name; it defaults to `"url"`.
  Interpolation* StylesheetParser::_tryUrlContents(LineScannerState2 start, std::string name)
  {
    // NOTE: this logic is largely duplicated in Parser.tryUrl.
    // Most changes here should be mirrored there.
    LineScannerState2 beginningOfContents = scanner.state();
    if (!scanner.scanChar($lparen)) return nullptr;
    whitespaceWithoutComments();

    // Match Ruby Sass's behavior: parse a raw URL() if possible, and if not
    // backtrack and re-parse as a function expression.
    InterpolationBuffer buffer;
    buffer.write(name.empty() ? "url" : name);
    buffer.write($lparen);
    while (true) {
      uint8_t next = scanner.peekChar();
      if (next == $nul) {
        break;
      }
      else if (next == $exclamation ||
          next == $percent ||
          next == $ampersand ||
          (next >= $asterisk && next <= $tilde) ||
          next >= 0x0080) {
        buffer.write(scanner.readChar());
      }
      else if (next == $backslash) {
        buffer.write(escape());
      }
      else if (next == $hash) {
        if (scanner.peekChar(1) == $lbrace) {
          buffer.add(singleInterpolation());
        }
        else {
          buffer.write(scanner.readChar());
        }
      }
      else if (isWhitespace(next)) {
        whitespaceWithoutComments();
        if (scanner.peekChar() != $rparen) break;
      }
      else if (next == $rparen) {
        buffer.write(scanner.readChar());
        return buffer.getInterpolation(scanner.pstate(start));
      }
      else {
        break;
      }
    }

    scanner.resetState(beginningOfContents);
    return nullptr;

  }
  // _tryUrlContents

  // Consumes a [url] token that's allowed to contain SassScript.
  Expression* StylesheetParser::dynamicUrl()
  {
    LineScannerState2 start = scanner.state();
    expectIdentifier("url");
    StringLiteral* fnName = SASS_MEMORY_NEW(StringLiteral,
      scanner.pstate(start), "url");
    InterpolationObj itpl = SASS_MEMORY_NEW(Interpolation,
      scanner.pstate(start), fnName);
    InterpolationObj contents = _tryUrlContents(start);
    if (contents != nullptr) {
      return SASS_MEMORY_NEW(StringExpression,
        scanner.pstate(start), contents);
    }

    ParserState pstate(scanner.pstate(start));
    Arguments* args = _argumentInvocation();
    return SASS_MEMORY_NEW(FunctionExpression,
      pstate, itpl, args, "");
  }
  // dynamicUrl

  // Consumes tokens up to "{", "}", ";", or "!".
  // This respects string and comment boundaries and supports interpolation.
  // Once this interpolation is evaluated, it's expected to be re-parsed.
  // Differences from [_interpolatedDeclarationValue] include:
  // * This does not balance brackets.
  // * This does not interpret backslashes, since
  //   the text is expected to be re-parsed.
  // * This supports Sass-style single-line comments.
  // * This does not compress adjacent whitespace characters.
  Interpolation* StylesheetParser::almostAnyValue()
  {
    // const char* start = scanner.position;
    InterpolationBuffer buffer;
    const char* commentStart;
    StringExpressionObj strex;
    LineScannerState2 start = scanner.state();
    Interpolation* contents;
    uint8_t next = 0;

    while (true) {
      if (!scanner.peekChar(next)) {
        goto endOfLoop;
      }
      switch (next) {
      case $backslash:
        // Write a literal backslash because this text will be re-parsed.
        buffer.write(scanner.readChar());
        buffer.write(scanner.readChar());
        break;

      case $double_quote:
      case $single_quote:
        strex = interpolatedString();
        buffer.addInterpolation(strex->getAsInterpolation());
        break;

      case $slash:
        commentStart = scanner.position;
        if (scanComment()) {
          buffer.write(scanner.substring(commentStart));
        }
        else {
          buffer.write(scanner.readChar());
        }
        break;

      case $hash:
        if (scanner.peekChar(1) == $lbrace) {
          // Add a full interpolated identifier to handle cases like
          // "#{...}--1", since "--1" isn't a valid identifier on its own.
          buffer.addInterpolation(interpolatedIdentifier());
        }
        else {
          buffer.write(scanner.readChar());
        }
        break;

      case $cr:
      case $lf:
      case $ff:
        if (isIndented()) goto endOfLoop;
        buffer.write(scanner.readChar());
        break;

      case $exclamation:
      case $semicolon:
      case $lbrace:
      case $rbrace:
        goto endOfLoop;

      case $u:
      case $U:
        start = scanner.state();
        if (!scanIdentifier("url")) {
          buffer.write(scanner.readChar());
          break;
        }
        contents = _tryUrlContents(start);
        if (contents == nullptr) {
          scanner.resetState(start);
          buffer.write(scanner.readChar());
        }
        else {
          buffer.addInterpolation(contents);
        }
        break;

      default:
        if (lookingAtIdentifier()) {
          buffer.write(identifier());
        }
        else {
          buffer.write(scanner.readChar());
        }
        break;
      }
    }

  endOfLoop:

    return buffer.getInterpolation(scanner.pstate(start));

  }
  // almostAnyValue

  // Consumes tokens until it reaches a top-level `";"`, `")"`, `"]"`, or `"}"` and 
  // returns their contents as a string. If [allowEmpty] is `false` (the default), this
  // requires at least one token. Unlike [declarationValue], this allows interpolation.
  StringExpression* StylesheetParser::_interpolatedDeclarationValue(bool allowEmpty)
  {
    // NOTE: this logic is largely duplicated in Parser.declarationValue and
    // isIdentifier in utils.dart. Most changes here should be mirrored there.
    LineScannerState2 beforeUrl = scanner.state();
    Interpolation* contents;

    InterpolationBuffer buffer;
    Position start(scanner);
    std::vector<uint8_t> brackets;
    bool wroteNewline = false;
    uint8_t next = 0;

    InterpolationObj itpl;
    StringExpressionObj strex;

    while (true) {
      if (!scanner.peekChar(next)) {
        goto endOfLoop;
      }
      switch (next) {
      case $backslash:
        buffer.write(escape(true));
        wroteNewline = false;
        break;

      case $double_quote:
      case $single_quote:
        strex = interpolatedString();
        itpl = strex->getAsInterpolation();
        buffer.addInterpolation(itpl);
        wroteNewline = false;
        break;

      case $slash:
        if (scanner.peekChar(1) == $asterisk) {
          buffer.write(rawText(&StylesheetParser::loudComment));
        }
        else {
          buffer.write(scanner.readChar());
        }
        wroteNewline = false;
        break;

      case $hash:
        if (scanner.peekChar(1) == $lbrace) {
          // Add a full interpolated identifier to handle cases like
          // "#{...}--1", since "--1" isn't a valid identifier on its own.
          itpl = interpolatedIdentifier();
          buffer.addInterpolation(itpl);
        }
        else {
          buffer.write(scanner.readChar());
        }
        wroteNewline = false;
        break;

      case $space:
      case $tab:
        if (wroteNewline || !isWhitespace(scanner.peekChar(1))) {
          buffer.write(scanner.readChar());
        }
        else {
          scanner.readChar();
        }
        break;

      case $lf:
      case $cr:
      case $ff:
        if (isIndented()) goto endOfLoop;
        if (!isNewline(scanner.peekChar(-1))) buffer.writeln();
        scanner.readChar();
        wroteNewline = true;
        break;

      case $lparen:
      case $lbrace:
      case $lbracket:
        buffer.write(next);
        brackets.push_back(opposite(scanner.readChar()));
        wroteNewline = false;
        break;

      case $rparen:
      case $rbrace:
      case $rbracket:
        if (brackets.empty()) goto endOfLoop;
        buffer.write(next);
        scanner.expectChar(brackets.back());
        brackets.pop_back();
        wroteNewline = false;
        break;

      case $semicolon:
        if (brackets.empty()) goto endOfLoop;
        buffer.write(scanner.readChar());
        break;

      case $u:
      case $U:
        beforeUrl = scanner.state();
        if (!scanIdentifier("url")) {
          buffer.write(scanner.readChar());
          wroteNewline = false;
          break;
        }

        contents = _tryUrlContents(beforeUrl);
        if (contents == nullptr) {
          scanner.resetState(beforeUrl);
          buffer.write(scanner.readChar());
        }
        else {
          buffer.addInterpolation(contents);
        }
        wroteNewline = false;
        break;

      default:
        if (next == $nul) goto endOfLoop;

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

  endOfLoop:

    if (!brackets.empty()) scanner.expectChar(brackets.back());
    if (!allowEmpty && buffer.empty()) {
      scanner.error("Expected token.");
    }
    itpl = buffer.getInterpolation();
    return SASS_MEMORY_NEW(StringExpression,
      scanner.spanFrom(start), itpl);

  }
  // _interpolatedDeclarationValue

  // Consumes an identifier that may contain interpolation.
  Interpolation* StylesheetParser::interpolatedIdentifier()
  {
    InterpolationBuffer buffer;
    Position start(scanner);

    while (scanner.scanChar($dash)) {
      buffer.write($dash);
    }

    uint8_t first = 0, next = 0;
    if (!scanner.peekChar(first)) {
      scanner.error("Expected identifier.",
        scanner.pstate(start));
    }
    else if (isNameStart(first)) {
      buffer.write(scanner.readChar());
    }
    else if (first == $backslash) {
      buffer.write(escape(true));
    }
    else if (first == $hash && scanner.peekChar(1) == $lbrace) {
      ExpressionObj ex = singleInterpolation();
      buffer.add(ex);
    }
    else {
      scanner.error("Expected identifier.");
    }

    while (true) {
      if (!scanner.peekChar(next)) {
        break;
      }
      else if (next == $underscore ||
          next == $dash ||
          isAlphanumeric(next) ||
          next >= 0x0080) {
        buffer.write(scanner.readChar());
      }
      else if (next == $backslash) {
        buffer.write(escape());
      }
      else if (next == $hash && scanner.peekChar(1) == $lbrace) {
        buffer.add(singleInterpolation());
      }
      else {
        break;
      }
    }

    return buffer.getInterpolation(scanner.pstate(start));
  }
  // interpolatedIdentifier

  // Consumes interpolation.
  Expression* StylesheetParser::singleInterpolation()
  {
    Position start(scanner);
    scanner.expect("#{");
    whitespace();
    ExpressionObj contents = expression();
    scanner.expectChar($rbrace);

    if (plainCss()) {
      error(
        "Interpolation isn't allowed in plain CSS.",
        scanner.spanFrom(start));
    }

    return contents.detach();
  }
  // singleInterpolation

  // Consumes a list of media queries.
  Interpolation* StylesheetParser::_mediaQueryList()
  {
    Position start(scanner);
    InterpolationBuffer buffer;
    while (true) {
      whitespace();
      _mediaQuery(buffer);
      if (!scanner.scanChar($comma)) break;
      buffer.write($comma);
      buffer.write($space);
    }
    return buffer.getInterpolation(scanner.pstate(start));
  }
  // _mediaQueryList

  // Consumes a single media query.
  void StylesheetParser::_mediaQuery(InterpolationBuffer& buffer)
  {
    // This is somewhat duplicated in MediaQueryParser._mediaQuery.
    if (scanner.peekChar() != $lparen) {
      buffer.addInterpolation(interpolatedIdentifier());
      whitespace();

      if (!_lookingAtInterpolatedIdentifier()) {
        // For example, "@media screen {".
        return;
      }

      buffer.write($space);
      InterpolationObj identifier =
        interpolatedIdentifier();
      whitespace();

      if (equalsIgnoreCase(identifier->getPlainString(), "and")) {
        // For example, "@media screen and ..."
        buffer.write(" and ");
      }
      else {
        buffer.addInterpolation(identifier);
        if (scanIdentifier("and")) {
          // For example, "@media only screen and ..."
          whitespace();
          buffer.write(" and ");
        }
        else {
          // For example, "@media only screen {"
          return;
        }
      }
    }

    // We've consumed either `IDENTIFIER "and"` or
    // `IDENTIFIER IDENTIFIER "and"`.

    while (true) {
      whitespace();
      buffer.addInterpolation(_mediaFeature());
      whitespace();
      if (!scanIdentifier("and")) break;
      buffer.write(" and ");
    }
  }
  // _mediaQuery

  // Consumes a media query feature.
  Interpolation* StylesheetParser::_mediaFeature()
  {
    if (scanner.peekChar() == $hash) {
      Expression* interpolation = singleInterpolation();
      Interpolation* itpl = SASS_MEMORY_NEW(Interpolation,
        interpolation->pstate());
      itpl->append(interpolation);
      return itpl;
    }

    Position start(scanner);
    InterpolationBuffer buffer;
    scanner.expectChar($lparen);
    buffer.write($lparen);
    whitespace();

    buffer.add(_expressionUntilComparison());
    if (scanner.scanChar($colon)) {
      whitespace();
      buffer.write($colon);
      buffer.write($space);
      buffer.add(expression());
    }
    else {
      uint8_t next = scanner.peekChar();
      bool isAngle = next == $langle || next == $rangle;
      if (isAngle || next == $equal) {
        buffer.write($space);
        buffer.write(scanner.readChar());
        if (isAngle && scanner.scanChar($equal)) {
          buffer.write($equal);
        }
        buffer.write($space);

        whitespace();
        buffer.add(_expressionUntilComparison());

        if (isAngle && scanner.scanChar(next)) {
          buffer.write($space);
          buffer.write(next);
          if (scanner.scanChar($equal)) {
            buffer.write($equal);
          }
          buffer.write($space);

          whitespace();
          buffer.add(_expressionUntilComparison());
        }
      }
    }

    scanner.expectChar($rparen);
    whitespace();
    buffer.write($rparen);

    return buffer.getInterpolation(scanner.pstate(start));

  }
  // _mediaFeature

  // Helper function for until condition
  bool StylesheetParser::nextIsComparison()
  {
    uint8_t next = scanner.peekChar();
    if (next == $equal) return scanner.peekChar(1) != $equal;
    return next == $langle || next == $rangle;
  }
  // nextIsComparison

  // Consumes an expression until it reaches a
  // top-level `<`, `>`, or a `=` that's not `==`.
  Expression* StylesheetParser::_expressionUntilComparison()
  {
    return expression(false, false, &StylesheetParser::nextIsComparison);
  }

  // Consumes a `@supports` condition.
  SupportsCondition* StylesheetParser::_supportsCondition()
  {
    Position start(scanner);
    uint8_t first = scanner.peekChar();
    if (first != $lparen && first != $hash) {
      Position start(scanner);
      expectIdentifier("not");
      whitespace();
      return SASS_MEMORY_NEW(SupportsNegation,
        scanner.pstate(start), _supportsConditionInParens());
    }

    SupportsCondition_Obj condition =
      _supportsConditionInParens();
    whitespace();
    while (lookingAtIdentifier()) {
      SupportsOperation::Operand op;
      if (scanIdentifier("or")) {
        op = SupportsOperation::OR;
      }
      else {
        expectIdentifier("and");
        op = SupportsOperation::AND;
      }

      whitespace();
      SupportsCondition_Obj right =
        _supportsConditionInParens();
      
      condition = SASS_MEMORY_NEW(SupportsOperation,
        scanner.pstate(start), condition, right, op);
      whitespace();
    }

    return condition.detach();
  }
  // EO _supportsCondition

  // Consumes a parenthesized supports condition, or an interpolation.
  SupportsCondition* StylesheetParser::_supportsConditionInParens()
  {
    Position start(scanner);
    if (scanner.peekChar() == $hash) {
      return SASS_MEMORY_NEW(SupportsInterpolation,
        scanner.pstate(start), singleInterpolation());
    }

    scanner.expectChar($lparen);
    whitespace();
    uint8_t next = scanner.peekChar();
    if (next == $lparen || next == $hash) {
      SupportsCondition_Obj condition
        = _supportsCondition();
      whitespace();
      scanner.expectChar($rparen);
      return condition.detach();
    }

    if (next == $n || next == $N) {
      SupportsNegation_Obj negation
        = _trySupportsNegation();
      if (negation != nullptr) {
        scanner.expectChar($rparen);
        return negation.detach();
      }
    }

    ExpressionObj name = expression();
    scanner.expectChar($colon);
    whitespace();
    ExpressionObj value = expression();
    scanner.expectChar($rparen);

    return SASS_MEMORY_NEW(SupportsDeclaration,
      scanner.pstate(start), name, value);
  }
  // EO _supportsConditionInParens

  // Tries to consume a negated supports condition. Returns `null` if it fails.
  SupportsNegation* StylesheetParser::_trySupportsNegation()
  {
    LineScannerState2 start = scanner.state();
    if (!scanIdentifier("not") || scanner.isDone()) {
      scanner.resetState(start);
      return nullptr;
    }

    uint8_t next = scanner.peekChar();
    if (!isWhitespace(next) && next != $lparen) {
      scanner.resetState(start);
      return nullptr;
    }

    whitespace();

    return SASS_MEMORY_NEW(SupportsNegation,
      scanner.pstate(start),
      _supportsConditionInParens());

  }
  // _trySupportsNegation

  // Returns whether the scanner is immediately before an identifier that may contain
  // interpolation. This is based on the CSS algorithm, but it assumes all backslashes
  // start escapes and it considers interpolation to be valid in an identifier.
  // https://drafts.csswg.org/css-syntax-3/#would-start-an-identifier
  bool StylesheetParser::_lookingAtInterpolatedIdentifier() const
  {
    // See also [ScssParser._lookingAtIdentifier].

    uint8_t first = scanner.peekChar();
    if (first == $nul) return false;
    if (isNameStart(first) || first == $backslash) return true;
    if (first == $hash) return scanner.peekChar(1) == $lbrace;

    if (first != $dash) return false;
    uint8_t second = scanner.peekChar(1);
    if (second == $nul) return false;
    if (isNameStart(second)) return true;
    if (second == $backslash) return true;

    if (second == $hash) return scanner.peekChar(2) == $lbrace;
    if (second != $dash) return false;

    uint8_t third = scanner.peekChar(2);
    if (third == $nul) return false;
    if (third != $hash) return isNameStart(third);
    else return scanner.peekChar(3) == $lbrace;
  }
  // EO _lookingAtInterpolatedIdentifier

  // Returns whether the scanner is immediately before a sequence
  // of characters that could be part of an CSS identifier body.
  // The identifier body may include interpolation.
  bool StylesheetParser::_lookingAtInterpolatedIdentifierBody() const
  {
    uint8_t first = scanner.peekChar();
    if (first == $nul) return false;
    if (isName(first) || first == $backslash) return true;
    return first == $hash && scanner.peekChar(1) == $lbrace;
  }
  // EO _lookingAtInterpolatedIdentifierBody

  // Returns whether the scanner is immediately before a SassScript expression.
  bool StylesheetParser::_lookingAtExpression() const
  {
    uint8_t character, next;
    if (!scanner.peekChar(character)) {
      return false;
    }
    if (character == $dot) {
      return scanner.peekChar(1) != $dot;
    }
    if (character == $exclamation) {
      if (!scanner.peekChar(next, 1)) {
      }
      return isWhitespace(next)
        || equalsLetterIgnoreCase($i, next);
    }

    return character == $lparen
      || character == $slash
      || character == $lbracket
      || character == $single_quote
      || character == $double_quote
      || character == $hash
      || character == $plus
      || character == $minus
      || character == $backslash
      || character == $dollar
      || character == $ampersand
      || isNameStart(character)
      || isDigit(character);
  }
  // EO _lookingAtExpression

  // Like [identifier], but rejects identifiers that begin with `_` or `-`.
  std::string StylesheetParser::_publicIdentifier()
  {
    Position start(scanner);
    auto result = identifier();

    uint8_t first = result[0];
    if (first == $dash || first == $underscore) {
      error("Private members can't be accessed from outside their modules.",
        scanner.spanFrom(start));
    }

    return result;
  }
  // EO _publicIdentifier

  StringLiteral* StylesheetParser::_publicIdentifier2()
  {
    Position start = scanner.offset;
    std::string ident(_publicIdentifier());
    return SASS_MEMORY_NEW(StringLiteral, scanner.pstate(start), ident);
  }

}
