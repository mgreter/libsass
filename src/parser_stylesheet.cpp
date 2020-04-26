#include "parser_stylesheet.hpp"
#include "parser_expression.hpp"

#include "sass.hpp"
#include "ast.hpp"

#include "LUrlParser/LUrlParser.hpp"

#include "charcode.hpp"
#include "character.hpp"
#include "util_string.hpp"
#include "string_utils.hpp"
#include "color_maps.hpp"
#include "debugger.hpp"
#include "strings.hpp"

namespace Sass {

  // Import some namespaces
  using namespace Charcode;
  using namespace Character;

  RootObj StylesheetParser::parse()
  {

    // skip over optional utf8 bom
    // ToDo: check influence on count
    scanner.scan(Strings::utf8bom);

    Offset start(scanner.offset);
    sass::vector<StatementObj> statements;

    SourceSpan pstate(scanner.relevantSpanFrom(start));

    // check seems a bit esoteric but works
    if (context.included_sources.size() == 1) {
      // apply headers only on very first include
      context.apply_custom_headers2(statements, pstate);
    }

    // parse all root statements and append to statements
    sass::vector<StatementObj> parsed(this->statements(&StylesheetParser::_rootStatement));
    std::move(parsed.begin(), parsed.end(), std::back_inserter(statements));

    // make sure everything is parsed
    scanner.expectDone();

    return SASS_MEMORY_NEW(Root, scanner.relevantSpanFrom(start), std::move(statements));

  }

  // Consumes a variable declaration.
  Statement* StylesheetParser::variableDeclaration()
  {
    // var precedingComment = lastSilentComment;
    // lastSilentComment = null;
    Offset start(scanner.offset);

    sass::string ns;
    sass::string name = variableName();
    if (scanner.scanChar($dot)) {
      ns = name;
      name = _publicIdentifier();
    }

    if (plainCss()) {
      error("Sass variables aren't allowed in plain CSS.",
        *context.logger123, scanner.relevantSpanFrom(start));
    }

    whitespace();
    scanner.expectChar($colon);
    whitespace();

    ExpressionObj value = expression();

    bool guarded = false;
    bool global = false;

    Offset flagStart(scanner.offset);
    while (scanner.scanChar($exclamation)) {
       sass::string flag = identifier();
       if (flag == "default") {
         guarded = true;
       }
       else if (flag == "global") {
         if (!ns.empty()) {
           error("!global isn't allowed for variables in other modules.",
             scanner.relevantSpanFrom(flagStart));
         }
         global = true;
       }
       else {
         error("Invalid flag name.",
           scanner.relevantSpanFrom(flagStart));
       }

       whitespace();
       flagStart = scanner.offset;
    }

    expectStatementSeparator("variable declaration");

    IdxRef vidx = context.varStack.back()->hoistVariable(name, global); //  || guarded
    if (FunctionExpression* fn = Cast<FunctionExpression>(value)) {
      // const sass::string& fnName(fn->name()->getPlainString());
      // if (name == "map-merge") {
        auto& args = fn->arguments();
        auto& pos = args->positional();
        if (pos.size() > 0) {
          if (Variable* var = Cast<Variable>(pos[0])) {
            if (var->name().norm() == name) {
              // SASS_MEMORY_NEW(MapMerge,
              //   scanner.relevantSpan(start), name, vidx, value, guarded, global);
              // debug_ast(var);
              // Can bring 15%
              fn->selfAssign(true);
            }
          }
        }
        // debug_ast(fn->arguments());
      }
    // }
    Assignment* declaration = SASS_MEMORY_NEW(Assignment,
      scanner.relevantSpanFrom(start), name, vidx, value, guarded, global);

    // if (global) _globalVariables.putIfAbsent(name, () = > declaration);

    return declaration;

  }
  // EO variableDeclaration

  // Consumes a statement that's allowed at the top level of the stylesheet or
  // within nested style and at rules. If [root] is `true`, this parses at-rules
  // that are allowed only at the root of the stylesheet.
  Statement* StylesheetParser::_statement(bool root)
  {
    Offset start(scanner.offset);
    switch (scanner.peekChar()) {
    case $at:
      return atRule(&StylesheetParser::_childStatement, root);

    case $plus:
      if (!isIndented() || !lookingAtIdentifier(1)) {
        return _styleRule();
      }
      _isUseAllowed = false;
      start = scanner.offset;
      scanner.readChar();
      return _includeRule2(start);

    case $equal:
      if (!isIndented()) return _styleRule();
      _isUseAllowed = false;
      start = scanner.offset;
      scanner.readChar();
      whitespace();
      return _mixinRule2(start);

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

    EnvFrame local(context.varStack.back(), true);
    ScopedStackFrame<EnvFrame>
      scoped(context.varStack, &local);

    auto qwe = _withChildren<StyleRule>(
      &StylesheetParser::_childStatement,
      styleRule.ptr());
    qwe->idxs(local.getIdxs());
    return qwe;
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

    Offset start(scanner.offset);
    InterpolationBuffer buffer(scanner);
    Declaration* declaration = _declarationOrBuffer(buffer);

    if (declaration != nullptr) {
      return declaration;
    }

    buffer.addInterpolation(styleRuleSelector());
    SourceSpan selectorPstate = scanner.relevantSpanFrom(start);

    LOCAL_FLAG(_inStyleRule, true);

    if (buffer.empty()) {
      scanner.error("expected \"}\".",
        *context.logger123, scanner.relevantSpan());
    }

    EnvFrame local(context.varStack.back());
    ScopedStackFrame<EnvFrame>
      scoped(context.varStack, &local);

    InterpolationObj itpl = buffer.getInterpolation(scanner.relevantSpanFrom(start));
    StyleRuleObj rule = _withChildren<StyleRule>(
      &StylesheetParser::_childStatement,
      itpl.ptr());
    rule->idxs(local.getIdxs());
    if (isIndented() && rule->empty()) {
      context.logger123->addWarn33(
        "This selector doesn't have any properties and won't be rendered.",
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
    Offset start(scanner.offset);

    // Allow the "*prop: val", ":prop: val",
    // "#prop: val", and ".prop: val" hacks.
    uint8_t first = scanner.peekChar();
    if (first == $colon || first == $asterisk || first == $dot ||
        (first == $hash && scanner.peekChar(1) != $lbrace)) {
      sass::sstream strm;
      strm << scanner.readChar();
      strm << rawText(&StylesheetParser::whitespace);
      nameBuffer.write(strm.str(), scanner.relevantSpanFrom(start));
    }

    if (!_lookingAtInterpolatedIdentifier()) {
      return nullptr;
    }

    nameBuffer.addInterpolation(interpolatedIdentifier());
    if (scanner.matches("/*")) nameBuffer.write(rawText(&StylesheetParser::loudComment));

    StringBuffer midBuffer;
    midBuffer.write(rawText(&StylesheetParser::whitespace));
    SourceSpan beforeColon(scanner.relevantSpanFrom(start));
    if (!scanner.scanChar($colon)) {
      if (!midBuffer.empty()) {
        nameBuffer.write($space);
      }
      return nullptr;
    }
    midBuffer.write($colon);

    // Parse custom properties as declarations no matter what.
    InterpolationObj name = nameBuffer.getInterpolation(beforeColon);
    if (StringUtils::startsWith(name->getInitialPlain(), "--")) {
      StringExpressionObj value = _interpolatedDeclarationValue();
      expectStatementSeparator("custom property");
      return SASS_MEMORY_NEW(Declaration,
        scanner.relevantSpanFrom(start),
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

    sass::string postColonWhitespace = rawText(&StylesheetParser::whitespace);
    if (lookingAtChildren()) {
      return _withChildren<Declaration>(&StylesheetParser::_declarationChild, name);
    }

    midBuffer.write(postColonWhitespace);
    bool couldBeSelector = postColonWhitespace.empty()
      && _lookingAtInterpolatedIdentifier();

    StringScannerState beforeDeclaration = scanner.state();
    ExpressionObj value;

    try {

      if (lookingAtChildren()) {
        SourceSpan pstate = scanner.relevantSpanFrom(scanner.offset);
        Interpolation* itpl = SASS_MEMORY_NEW(Interpolation, pstate);
        value = SASS_MEMORY_NEW(StringExpression, pstate, itpl, true);
      }
      else {
        value = expression();
      }

      if (lookingAtChildren()) {
        // Properties that are ambiguous with selectors can't have additional
        // properties nested beneath them, so we force an error. This will be
        // caught below and cause the text to be re-parsed as a selector.
        if (couldBeSelector) {
          expectStatementSeparator();
        }
      }
      else if (!atEndOfStatement()) {
        // Force an exception if there isn't a valid end-of-property character but
        // don't consume that character. This will also cause text to be re-parsed.
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
        scanner.relevantSpanFrom(start), name, value);
    }

  }
  // EO _declarationOrBuffer

  // Consumes a property declaration. This is only used in contexts where
  // declarations are allowed but style rules are not, such as nested
  // declarations. Otherwise, [_declarationOrStyleRule] is used instead.
  Declaration* StylesheetParser::_declaration()
  {

    Offset start(scanner.offset);

    InterpolationObj name;
    // Allow the "*prop: val", ":prop: val",
    // "#prop: val", and ".prop: val" hacks.
    uint8_t first = scanner.peekChar();
    if (first == $colon ||
        first == $asterisk ||
        first == $dot ||
        (first == $hash && scanner.peekChar(1) != $lbrace)) {
      InterpolationBuffer nameBuffer(scanner);
      nameBuffer.write(scanner.readChar());
      nameBuffer.write(rawText(&StylesheetParser::whitespace));
      nameBuffer.addInterpolation(interpolatedIdentifier());
      name = nameBuffer.getInterpolation(scanner.relevantSpanFrom(start));
    }
    else {
      name = interpolatedIdentifier();
    }

    whitespace();
    scanner.expectChar($colon);
    whitespace();

    if (lookingAtChildren()) {
      if (plainCss()) {
        scanner.error("Nested declarations aren't allowed in plain CSS.",
          *context.logger123, scanner.rawSpan());
      }
      return _withChildren<Declaration>(&StylesheetParser::_declarationChild, name);
    }

    ExpressionObj value = expression();
    if (lookingAtChildren()) {
      if (plainCss()) {
        scanner.error("Nested declarations aren't allowed in plain CSS.",
          *context.logger123, scanner.rawSpan());
      }
      // only without children;
      return _withChildren<Declaration>(
        &StylesheetParser::_declarationChild,
        name, value);
    }
    else {
      expectStatementSeparator();
      return SASS_MEMORY_NEW(Declaration,
        scanner.relevantSpanFrom(start), name, value);
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

    Offset start(scanner.offset);
    scanner.expectChar($at, "@-rule");
    InterpolationObj name = interpolatedIdentifier();
    whitespace();

    // We want to set [_isUseAllowed] to `false` *unless* we're parsing
    // `@charset`, `@forward`, or `@use`. To avoid double-comparing the rule
    // name, we always set it to `false` and then set it back to its previous
    // value if we're parsing an allowed rule.
    bool wasUseAllowed = _isUseAllowed;
    LOCAL_FLAG(_isUseAllowed, false);

    sass::string plain(name->getPlainString());
    if (plain == "at-root") {
      return _atRootRule(start);
    }
    else if (plain == "charset") {
      _isUseAllowed = wasUseAllowed;
      if (!root) _disallowedAtRule(start);
      sass::string throwaway(string());
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
      // return _importRule(start);
      return _importRule2(start);
    }
    else if (plain == "include") {
      return _includeRule2(start);
    }
    else if (plain == "media") {
      return mediaRule(start);
    }
    else if (plain == "mixin") {
      return _mixinRule2(start);
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
    Offset start(scanner.offset);
    sass::string name = _plainAtRuleName();
    
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
      return _includeRule2(start);
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
        sass::string("@function rules may not contain ")
        + (isStyleRule ? "style rules." : "declarations."),
        statement->pstate());
    }

    Offset start(scanner.offset);
    sass::string name(_plainAtRuleName());
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

  sass::string StylesheetParser::_plainAtRuleName()
  {
    scanner.expectChar($at, "@-rule");
    sass::string name = identifier();
    whitespace();
    return name;
  }

  // Consumes an `@at-root` rule.
  // [start] should point before the `@`.
  AtRootRule* StylesheetParser::_atRootRule(Offset start)
  {

    EnvFrame local(context.varStack.back(), true);
    ScopedStackFrame<EnvFrame>
      scoped(context.varStack, &local);

    if (scanner.peekChar() == $lparen) {
      InterpolationObj query = _atRootQuery();
      whitespace();
      auto atRoot = _withChildren<AtRootRule>(
        &StylesheetParser::_childStatement,
        query);
      atRoot->idxs(local.getIdxs());
      return atRoot;
    }
    else if (lookingAtChildren()) {
      auto qwe = _withChildren<AtRootRule>(
        &StylesheetParser::_childStatement);
      qwe->idxs(local.getIdxs());
      return qwe;
    }
    StyleRule* child = _styleRule();
    SourceSpan pstate(scanner.relevantSpanFrom(start));
    auto atRoot = SASS_MEMORY_NEW(AtRootRule, pstate, {}, { child });
    atRoot->idxs(local.getIdxs());
    return atRoot;
  }
  // EO _atRootRule

  Interpolation* StylesheetParser::_atRootQuery()
  {
    if (scanner.peekChar() == $hash) {
      Expression* interpolation(singleInterpolation());
      return SASS_MEMORY_NEW(Interpolation,
        interpolation->pstate(), interpolation);
    }

    Offset start(scanner.offset);
    InterpolationBuffer buffer(scanner);
    scanner.expectChar($lparen);
    buffer.writeCharCode($lparen);
    whitespace();

    buffer.add(expression());
    if (scanner.scanChar($colon)) {
      whitespace();
      buffer.writeCharCode($colon);
      buffer.writeCharCode($space);
      buffer.add(expression());
    }

    scanner.expectChar($rparen);
    whitespace();
    buffer.writeCharCode($rparen);

    return buffer.getInterpolation(
      scanner.relevantSpanFrom(start));

  }

  // Consumes a `@content` rule.
  // [start] should point before the `@`.
  ContentRule* StylesheetParser::_contentRule(Offset start)
  {
    if (!_inMixin) {
      error("@content is only allowed within mixin declarations.",
        scanner.relevantSpanFrom(start));
    }

    whitespace();

    ArgumentInvocationObj args;
    if (scanner.peekChar() == $lparen) {
      args = _argumentInvocation(true);
    }
    else {
      args = SASS_MEMORY_NEW(ArgumentInvocation,
        scanner.relevantSpan(), sass::vector<ExpressionObj>(), EnvKeyFlatMap2{ {} });
    }

    LOCAL_FLAG(_mixinHasContent, true);
    expectStatementSeparator("@content rule");
    // ToDo: ContentRule
    return SASS_MEMORY_NEW(ContentRule,
      scanner.relevantSpanFrom(start), args);

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

  Each* StylesheetParser::_eachRule(Offset start, Statement* (StylesheetParser::* child)())
  {
    LOCAL_FLAG(_inControlDirective, true);
    sass::vector<EnvKey> variables;
    EnvFrame local(context.varStack.back());
    ScopedStackFrame<EnvFrame>
      scoped2(context.varStack, &local);
    variables.emplace_back(variableName());
    local.createVariable(variables.back());
    whitespace();
    while (scanner.scanChar($comma)) {
      whitespace();
      variables.emplace_back(variableName());
      local.createVariable(variables.back());
      whitespace();
    }

    expectIdentifier("in");
    whitespace();

    ExpressionObj list = expression();
    Each* loop = _withChildren<Each>(
      child, variables, list);
    loop->pstate(scanner.relevantSpanFrom(start));
    loop->idxs(local.getIdxs());
    return loop;

  }

  ErrorRule* StylesheetParser::_errorRule(Offset start)
  {
    ExpressionObj value = expression();
    expectStatementSeparator("@error rule");
    return SASS_MEMORY_NEW(ErrorRule,
      scanner.relevantSpanFrom(start), value);
  }
  // EO _errorRule

  // Consumes an `@extend` rule.
  // [start] should point before the `@`.
  ExtendRule* StylesheetParser::_extendRule(Offset start)
  {
    if (!_inStyleRule && !_inMixin && !_inContentBlock) {
      error("@extend may only be used within style rules.",
        scanner.relevantSpanFrom(start));
    }

    InterpolationObj value = almostAnyValue();
    bool optional = scanner.scanChar($exclamation);
    if (optional) expectIdentifier("optional");
    expectStatementSeparator("@extend rule");
    return SASS_MEMORY_NEW(ExtendRule,
      scanner.relevantSpanFrom(start), value, optional);
  }
  // EO _extendRule

  // Consumes a function declaration.
  // [start] should point before the `@`.
  FunctionRule* StylesheetParser::_functionRule(Offset start)
  {
    // Variables should not be hoisted through
    EnvFrame* parent = context.varStack.back();
    EnvFrame local(context.varStack.back(), true);
    ScopedStackFrame<EnvFrame>
      scoped(context.varStack, &local);

    // var precedingComment = lastSilentComment;
    // lastSilentComment = null;
    sass::string name = identifier();
    sass::string normalized(name);
    whitespace();

    ArgumentDeclarationObj arguments = _argumentDeclaration2();

    if (_inMixin || _inContentBlock) {
      error("Mixins may not contain function declarations.",
        scanner.relevantSpanFrom(start));
    }
    else if (_inControlDirective) {
      error("Functions may not be declared in control directives.",
        scanner.relevantSpanFrom(start));
    }

    sass::string fname(Util::unvendor(name));
    if (fname == "calc" || fname == "element" || fname == "expression" ||
      fname == "url" || fname == "and" || fname == "or" || fname == "not") {
      error("Invalid function name.",
        scanner.relevantSpanFrom(start));
    }

    whitespace();
    FunctionRule* rule = _withChildren<FunctionRule>(
      &StylesheetParser::_functionAtRule,
      name, arguments, nullptr);
    IdxRef fidx = parent->createFunction(name);
    rule->pstate(scanner.relevantSpanFrom(start));
    rule->idxs(local.getIdxs());
    rule->fidx(fidx);
    return rule;
  }
  // EO _functionRule

  For* StylesheetParser::_forRule(Offset start, Statement* (StylesheetParser::* child)())
  {
    LOCAL_FLAG(_inControlDirective, true);

    // Create new variable frame from parent
    // EnvFrame* parent = context.varStack.back();
    EnvFrame local(context.varStack.back());
    ScopedStackFrame<EnvFrame>
      scoped(context.varStack, &local);

    sass::string variable = variableName();
    local.createVariable(variable);
    whitespace();

    expectIdentifier("from");
    whitespace();

    exclusive = 0; // reset flag on parser (hackish)
    ExpressionObj from = expression(false, false,
      &StylesheetParser::_forRuleUntil);

    if (exclusive == 0) {
      scanner.error("Expected \"to\" or \"through\".",
        *context.logger123, scanner.relevantSpan());
    }

    whitespace();
    ExpressionObj to = expression();
    For* loop = _withChildren<For>(
      child, variable, from, to, exclusive == 1);
    loop->pstate(scanner.relevantSpanFrom(start));
    loop->idxs(local.getIdxs());
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
  If* StylesheetParser::_ifRule(Offset start, Statement* (StylesheetParser::* child)())
  {
    // var ifIndentation = currentIndentation;
    size_t ifIndentation = 0;
    LOCAL_FLAG(_inControlDirective, true);
    ExpressionObj condition = expression();

    IfObj root;
    IfObj cur;

    EnvFrame local(context.varStack.back());
    ScopedStackFrame<EnvFrame>
      scoped2(context.varStack, &local);

    sass::vector<StatementObj> children
      = this->children(child);
    whitespaceWithoutComments();

    sass::vector<StatementObj> clauses;
    SourceSpan pstate(scanner.relevantSpanFrom(start));
    cur = root = SASS_MEMORY_NEW(If, pstate, std::move(condition), std::move(children));

    sass::vector<If*> ifs;
    ifs.push_back(root);

    while (scanElse(ifIndentation)) {
      whitespace();

      // EnvFrame local(context.varStack.back());
      // ScopedStackFrame<EnvFrame>
      //   scoped2(context.varStack, &local);

      if (scanIdentifier("if")) {
        whitespace();

        ExpressionObj condition = expression();


        start = scanner.offset;
        children = this->children(child);
        SourceSpan pstate(scanner.relevantSpanFrom(start));
        If* alternative = SASS_MEMORY_NEW(If, pstate, condition, std::move(children));
        cur->alternatives().push_back(alternative);
        cur = alternative;
        ifs.push_back(cur);
      }
      else {
        start = scanner.offset;
        children = this->children(child);
        SourceSpan pstate(scanner.relevantSpanFrom(start));
        cur->alternatives(std::move(children));
        break;
      }
    }

    for (auto iffy : ifs) {
      iffy->idxs(local.getIdxs());
    }

    whitespaceWithoutComments();

    return root.detach();

  }

  ImportRule* StylesheetParser::_importRule2(Offset start)
  {

    ImportRuleObj rule = SASS_MEMORY_NEW(
      ImportRule, scanner.relevantSpanFrom(start));

    do {
      whitespace();
      importArgument(rule);
      whitespace();
    } while (scanner.scanChar($comma));
    // Check for expected finalization token
    expectStatementSeparator("@import rule");
    return rule.detach();

  }

  void StylesheetParser::importArgument(ImportRule* rule)
  {
    const char* startpos = scanner.position;
    Offset start(scanner.offset);
    uint8_t next = scanner.peekChar();
    if (next == $u || next == $U) {
      Expression* url = dynamicUrl();
      whitespace();
      auto queries = tryImportQueries();
      rule->append(SASS_MEMORY_NEW(StaticImport,
        scanner.relevantSpanFrom(start),
        SASS_MEMORY_NEW(Interpolation,
           url->pstate(), url),
        queries.first, queries.second));
      return;
    }

    sass::string url = string();
    const char* rawUrlPos = scanner.position;
    SourceSpan pstate = scanner.relevantSpanFrom(start);
    whitespace();
    auto queries = tryImportQueries();
    if (_isPlainImportUrl(url) || queries.first != nullptr || queries.second != nullptr) {
      // Create static import that is never
      // resolved by libsass (output as is)
      rule->append(SASS_MEMORY_NEW(StaticImport,
        scanner.relevantSpanFrom(start),
        SASS_MEMORY_NEW(Interpolation, pstate,
          SASS_MEMORY_NEW(String, pstate,
            sass::string(startpos, rawUrlPos))),
         queries.first, queries.second));
    }
    // Otherwise return a dynamic import
    // Will resolve during the eval stage
    else {
      // Check for valid dynamic import
      if (_inControlDirective || _inMixin) {
        _disallowedAtRule(rule->pstate().position);
      }
      // Call custom importers and check if any of them handled the import
      if (!context.callCustomImporters(unquote(url), pstate, rule)) {
        // Try to load url into context.sheets
        resolveDynamicImport(rule, start, url);
      }
    }
  
  }

  void StylesheetParser::resolveDynamicImport(ImportRule* rule, Offset start, const sass::string& url)
  {

    sass::string path(unquote(url));

    SourceSpan pstate = scanner.relevantSpanFrom(start);

    // Parse the sass source (stored on context.sheets)
    const Importer importer(path, scanner.sourceUrl);
    Include include(context.load_import(importer, pstate));

    // Error out in case nothing was found
    if (include.abs_path.empty()) {
      BackTraces& traces = *context.logger123;
      traces.push_back(BackTrace(pstate));
      throw Exception::InvalidSyntax(traces,
        "Can't find stylesheet to import.");
    }

    // 
    rule->append(SASS_MEMORY_NEW(IncludeImport,
      SASS_MEMORY_NEW(DynamicImport,
        scanner.relevantSpanFrom(start), url),
      include));

  }


  /*
  sass::string StylesheetParser::parseImportUrl(sass::string url)
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

  bool StylesheetParser::_isPlainImportUrl(const sass::string& url) const
  {
    if (url.length() < 5) return false;

    if (StringUtils::endsWithIgnoreCase(url, ".css", 4)) return true;

    uint8_t first = url[0];
    if (first == $slash) return url[1] == $slash;
    if (first != $h) return false;
    return StringUtils::startsWithIgnoreCase(url, "http://", 7)
      || StringUtils::startsWithIgnoreCase(url, "https://", 6);
  }

  // Consumes a supports condition and/or a media query after an `@import`.
  // Returns `null` if neither type of query can be found.
  std::pair<SupportsCondition_Obj, InterpolationObj> StylesheetParser::tryImportQueries()
  {
    SupportsCondition_Obj supports;
    if (scanIdentifier("supports")) {
      scanner.expectChar($lparen);
      Offset start(scanner.offset);
      if (scanIdentifier("not")) {
        whitespace();
        SupportsCondition* condition = _supportsConditionInParens();
        supports = SASS_MEMORY_NEW(SupportsNegation,
          scanner.relevantSpanFrom(start), condition);
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
          scanner.relevantSpanFrom(start), name, value);
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
  IncludeRule* StylesheetParser::_includeRule2(Offset start)
  {

    sass::string ns;
    sass::string name = identifier();
    if (scanner.scanChar($dot)) {
      ns = name;
      name = _publicIdentifier();
    }

    whitespace();
    ArgumentInvocationObj arguments;
    if (scanner.peekChar() == $lparen) {
      arguments = _argumentInvocation(true);
    }
    whitespace();

    EnvFrame local(context.varStack.back());
    ScopedStackFrame<EnvFrame>
      scoped2(context.varStack, &local);

    ArgumentDeclarationObj contentArguments;
    if (scanIdentifier("using")) {
      whitespace();
      contentArguments = _argumentDeclaration2();
      whitespace();
    }

    // ToDo: Add checks to allow to omit arguments fully
    if (!arguments) {
      SourceSpan pstate(scanner.relevantSpanFrom(start));
      arguments = SASS_MEMORY_NEW(ArgumentInvocation,
        pstate, sass::vector<ExpressionObj>(),
        EnvKeyFlatMap2{ {} }
      );
    }

    IncludeRuleObj rule = SASS_MEMORY_NEW(IncludeRule,
      scanner.relevantSpanFrom(start), name, arguments);

    if (!name.empty()) {
      // Get the function through the whole stack
      auto midx = context.varStack.back()->getMixinIdx(name);
      rule->midx(midx);
    }


    ContentBlockObj content;
    if (contentArguments || lookingAtChildren()) {
      LOCAL_FLAG(_inContentBlock, true);
      // auto cidx = local.createMixin(Keys::kwdContentRule, false);
      // EnvFrame inner(context.varStack.back());
      // ScopedStackFrame<EnvFrame>
      //   scope(context.varStack, &inner);
      if (contentArguments.isNull()) {
        // Dart-sass creates this one too
        contentArguments = SASS_MEMORY_NEW(
          ArgumentDeclaration, scanner.relevantSpan(), sass::vector<ArgumentObj>());
      }
      content = _withChildren<ContentBlock>(
        &StylesheetParser::_childStatement,
        contentArguments);
      content->pstate(scanner.relevantSpanFrom(start));
      content->idxs(local.getIdxs());
      rule->content(content);
    }
    else {
      expectStatementSeparator();
    }

    /*
    var span =
      scanner.rawSpanFrom(start, start).expand((content ? ? arguments).span);
    return IncludeRule(name, arguments, span,
      namespace : ns, content : content);
      */

    return rule.detach(); // mixin.detach();
  }
  // EO _includeRule

  // Consumes a `@media` rule.
  // [start] should point before the `@`.
  MediaRule* StylesheetParser::mediaRule(Offset start)
  {
    InterpolationObj query = _mediaQueryList();
    MediaRule* rule = _withChildren<MediaRule>(
      &StylesheetParser::_childStatement, query, false);
    rule->pstate(scanner.relevantSpanFrom(start));
    return rule;
  }

  // Consumes a mixin declaration.
  // [start] should point before the `@`.
  MixinRule* StylesheetParser::_mixinRule2(Offset start)
  {

    EnvFrame* parent = context.varStack.back();
    EnvFrame local(context.varStack.back(), true);
    ScopedStackFrame<EnvFrame>
      scoped(context.varStack, &local);
    // Create space for optional content callables
    // ToDo: check if this can be conditionaly done?
    auto cidx = local.createMixin(Keys::contentRule);
    // var precedingComment = lastSilentComment;
    // lastSilentComment = null;
    sass::string name = identifier();
    whitespace();

    ArgumentDeclarationObj arguments;
    if (scanner.peekChar() == $lparen) {
      arguments = _argumentDeclaration2();
    }
    else {
      // Dart-sass creates this one too
      arguments = SASS_MEMORY_NEW(ArgumentDeclaration,
        scanner.relevantSpan(), sass::vector<ArgumentObj>()); // empty declaration
    }

    if (_inMixin || _inContentBlock) {
      error("Mixins may not contain mixin declarations.",
        scanner.relevantSpanFrom(start));
    }
    else if (_inControlDirective) {
      error("Mixins may not be declared in control directives.",
        scanner.relevantSpanFrom(start));
    }

    whitespace();
    LOCAL_FLAG(_inMixin, true);
    LOCAL_FLAG(_mixinHasContent, false);

    IdxRef fidx = parent->createMixin(name);
    MixinRule* rule = _withChildren<MixinRule>(
      &StylesheetParser::_childStatement,
      name, arguments, nullptr);
    rule->pstate(scanner.relevantSpanFrom(start));
    rule->idxs(local.getIdxs());
    rule->fidx(fidx); // to parent
    rule->cidx(cidx);
    return rule;
  }
  // EO _mixinRule

  // Consumes a `@moz-document` rule. Gecko's `@-moz-document` diverges
  // from [the specificiation][] allows the `url-prefix` and `domain`
  // functions to omit quotation marks, contrary to the standard.
  // [the specificiation]: http://www.w3.org/TR/css3-conditional/
  AtRule* StylesheetParser::mozDocumentRule(Offset start, Interpolation* name)
  {

    Offset valueStart(scanner.offset);
    InterpolationBuffer buffer(scanner);
    bool needsDeprecationWarning = false;

    while (true) {

      if (scanner.peekChar() == $hash) {
        buffer.add(singleInterpolation());
        needsDeprecationWarning = true;
      }
      else {


        Offset identifierStart(scanner.offset);
        sass::string identifier = this->identifier();
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
          sass::string trailing = buffer.trailingString();
          if (!StringUtils::endsWithIgnoreCase(trailing, "url-prefix()", 12) &&
            !StringUtils::endsWithIgnoreCase(trailing, "url-prefix('')", 14) &&
            !StringUtils::endsWithIgnoreCase(trailing, "url-prefix(\"\")", 14)) {
            needsDeprecationWarning = true;
          }
        }
        else if (identifier == "regexp") {
          buffer.write("regexp(");
          scanner.expectChar($lparen);
          StringExpressionObj str = interpolatedString();
          buffer.addInterpolation(str->getAsInterpolation());
          scanner.expectChar($rparen);
          buffer.write($rparen);
          needsDeprecationWarning = true;
        }
        else {
          error("Invalid function name.",
            scanner.relevantSpanFrom(identifierStart));
        }
      }

      whitespace();
      if (!scanner.scanChar($comma)) break;

      buffer.write($comma);
      buffer.write(rawText(&StylesheetParser::whitespace));

    }

    InterpolationObj value = buffer.getInterpolation(scanner.rawSpanFrom(valueStart));


    AtRule* atRule = _withChildren<AtRule>(
      &StylesheetParser::_childStatement, name, value, false);
    atRule->pstate(scanner.relevantSpanFrom(start));

    if (needsDeprecationWarning) {

      context.logger123->addWarn33(
        "@-moz-document is deprecated and support will be removed from Sass "
        "in a future release. For details, see http://bit.ly/moz-document.",
        atRule->pstate(), true);
    }

    return atRule;

  }

  // Consumes a `@return` rule.
  // [start] should point before the `@`.
  Return* StylesheetParser::_returnRule(Offset start)
  {
    ExpressionObj value = expression();
    expectStatementSeparator("@return rule");
    return SASS_MEMORY_NEW(Return,
      scanner.relevantSpanFrom(start), value);
  }
  // EO _returnRule

  // Consumes a `@supports` rule.
  // [start] should point before the `@`.
  SupportsRule* StylesheetParser::supportsRule(Offset start)
  {
    auto condition = _supportsCondition();
    whitespace();
    EnvFrame local(context.varStack.back());
    ScopedStackFrame<EnvFrame>
      scoped(context.varStack, &local);
    auto asd = _withChildren<SupportsRule>(
      &StylesheetParser::_childStatement,
      condition);
    asd->idxs(local.getIdxs());
    return asd;
  }
  // EO supportsRule


  // Consumes a `@debug` rule.
  // [start] should point before the `@`.
  DebugRule* StylesheetParser::_debugRule(Offset start)
  {
    ExpressionObj value = expression();
    expectStatementSeparator("@debug rule");
    return SASS_MEMORY_NEW(DebugRule,
      scanner.relevantSpanFrom(start), value);
  }
  // EO _debugRule

  // Consumes a `@warn` rule.
  // [start] should point before the `@`.
  WarnRule* StylesheetParser::_warnRule(Offset start)
  {
    ExpressionObj value = expression();
    expectStatementSeparator("@warn rule");
    return SASS_MEMORY_NEW(WarnRule,
      scanner.relevantSpanFrom(start), value);
  }
  // EO _warnRule

  // Consumes a `@while` rule. [start] should  point before the `@`. [child] is called 
  // to consume any children that are specifically allowed in the caller's context.
  WhileRule* StylesheetParser::_whileRule(Offset start, Statement* (StylesheetParser::* child)())
  {
    LOCAL_FLAG(_inControlDirective, true);
    EnvFrame local(context.varStack.back());
    ScopedStackFrame<EnvFrame>
      scoped(context.varStack, &local);
    // Ideally copy over vars used inside
    ExpressionObj condition = expression();
    auto qwe = _withChildren<WhileRule>(child,
      condition.ptr());
    qwe->idxs(local.getIdxs());
    return qwe;
  }
  // EO _whileRule

  // Consumes an at-rule that's not explicitly supported by Sass.
  // [start] should point before the `@`. [name] is the name of the at-rule.
  AtRule* StylesheetParser::unknownAtRule(Offset start, Interpolation* name)
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
        name, value, false);
    }
    else {
      expectStatementSeparator();
      rule = SASS_MEMORY_NEW(AtRule,
        scanner.relevantSpanFrom(start),
        name, value, true);
    }

    return rule.detach();
  }
  // EO unknownAtRule

  Statement* StylesheetParser::_disallowedAtRule(Offset start)
  {
    InterpolationObj value = almostAnyValue();
    error("This at-rule is not allowed here.",
      *context.logger123, scanner.relevantSpanFrom(start));
    return nullptr;
  }

  // Argument declaration is tricky in terms of scoping.
  // The variable before the colon is defined on the new frame.
  // But the right side is evaluated in the parent scope.
  ArgumentDeclaration* StylesheetParser::_argumentDeclaration2()
  {

    Offset start(scanner.offset);
    scanner.expectChar($lparen);
    whitespace();
    sass::vector<ArgumentObj> arguments;
    EnvKeySet named;
    sass::string restArgument;
    while (scanner.peekChar() == $dollar) {
      Offset variableStart(scanner.offset);
      sass::string name(variableName());
      EnvKey norm(name);
      context.varStack.back()->createVariable(norm);
      whitespace();

      ExpressionObj defaultValue;
      if (scanner.scanChar($colon)) {
        whitespace();
        auto old = context.varStack.back();
        context.varStack.pop_back();
        defaultValue = _expressionUntilComma();
        context.varStack.push_back(old);
      }
      else if (scanner.scanChar($dot)) {
        scanner.expectChar($dot);
        scanner.expectChar($dot);
        whitespace();
        // context.varStack.back()->createVariable(name);
        restArgument = name;
        break;
      }

      arguments.emplace_back(SASS_MEMORY_NEW(Argument,
        scanner.relevantSpanFrom(variableStart), defaultValue, name));

      if (named.count(norm) == 1) {
        error("Duplicate argument.",
          *context.logger123,
          arguments.back()->pstate());
      }
      named.insert(std::move(norm));

      if (!scanner.scanChar($comma)) break;
      whitespace();
    }
    scanner.expectChar($rparen);

    return SASS_MEMORY_NEW(
      ArgumentDeclaration,
      scanner.relevantSpanFrom(start),
      arguments,
      restArgument);

  }
  // EO _argumentDeclaration

  ArgumentInvocation* StylesheetParser::_argumentInvocation(bool mixin)
  {

    Offset start(scanner.offset);
    scanner.expectChar($lparen);
    whitespace();

    sass::vector<ExpressionObj> positional;
    // Maybe make also optional?
    EnvKeyFlatMap<ExpressionObj> named;
    ExpressionObj restArg;
    ExpressionObj kwdRest;
    while (_lookingAtExpression()) {
      ExpressionObj expression = _expressionUntilComma(!mixin);
      whitespace();

      Variable* var = Cast<Variable>(expression);
      if (var && scanner.scanChar($colon)) {
        whitespace();
        if (named.count(var->name()) == 1) {
          error("Duplicate argument.",
            *context.logger123,
            expression->pstate());
        }
        auto ex = _expressionUntilComma(!mixin);
        named[var->name()] = ex;
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
        positional.emplace_back(expression);
      }

      whitespace();
      if (!scanner.scanChar($comma)) break;
      whitespace();
    }
    scanner.expectChar($rparen);

    return SASS_MEMORY_NEW(
      ArgumentInvocation,
      scanner.relevantSpanFrom(start),
      std::move(positional),
      EnvKeyFlatMap2(
        std::move(named)
      ),
      restArg, kwdRest);

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
      SourceSpan span(scanner.rawSpan());
      // callStackFrame frame(*context.logger123,
      //   BackTrace(span));
      scanner.error("Expected expression.",
        *context.logger123, span);
    }

    // StringScannerState beforeBracket;
    Offset start(scanner.offset);
    if (bracketList) {
      // beforeBracket = scanner.position;
      scanner.expectChar($lbracket);
      whitespace();

      if (scanner.scanChar($rbracket)) {
        ListExpression* list = SASS_MEMORY_NEW(ListExpression,
          scanner.relevantSpanFrom(start), SASS_UNDEF);
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
      Offset beforeToken(scanner.offset);

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
          ep.addOperator(Sass_OP::EQ, beforeToken);
        }
        break;

      case $exclamation:
        next = scanner.peekChar(1);
        if (next == $equal) {
          scanner.readChar();
          scanner.readChar();
          ep.addOperator(Sass_OP::NEQ, beforeToken);
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
          ? Sass_OP::LTE : Sass_OP::LT, beforeToken);
        break;

      case $rangle:
        scanner.readChar();
        ep.addOperator(scanner.scanChar($equal)
          ? Sass_OP::GTE : Sass_OP::GT, beforeToken);
        break;

      case $asterisk:
        scanner.readChar();
        ep.addOperator(Sass_OP::MUL, beforeToken);
        break;

      case $plus:
        if (ep.singleExpression == nullptr) {
          ep.addSingleExpression(_unaryOperation());
        }
        else {
          scanner.readChar();
          ep.addOperator(Sass_OP::ADD, beforeToken);
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
          ep.addOperator(Sass_OP::SUB, beforeToken);
        }
        break;

      case $slash:
        if (ep.singleExpression == nullptr) {
          ep.addSingleExpression(_unaryOperation());
        }
        else {
          scanner.readChar();
          ep.addOperator(Sass_OP::DIV, beforeToken);
        }
        break;

      case $percent:
        scanner.readChar();
        ep.addOperator(Sass_OP::MOD, beforeToken);
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
          ep.addOperator(Sass_OP::AND, beforeToken);
        }
        else {
          ep.addSingleExpression(identifierLike());
        }
        break;

      case $o:
        if (!plainCss() && scanIdentifier("or")) {
          ep.addOperator(Sass_OP::OR, beforeToken);
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
          SourceSpan span(scanner.rawSpan());
          // callStackFrame frame(*context.logger123,
          //   BackTrace(span));
          scanner.error("Expected expression.",
            *context.logger123, span);
        }

        ep.resolveSpaceExpressions();
        ep.commaExpressions.emplace_back(ep.singleExpression);
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
        ep.commaExpressions.emplace_back(ep.singleExpression);
      }
      ListExpression* list = SASS_MEMORY_NEW(ListExpression,
        scanner.relevantSpanFrom(start), SASS_COMMA);
      list->concat(ep.commaExpressions);
      list->hasBrackets(bracketList);
      return list;
    }
    else if (bracketList &&
        !ep.spaceExpressions.empty() &&
        ep.singleEqualsOperand == nullptr) {
      ep.resolveOperations();
      ListExpression* list = SASS_MEMORY_NEW(ListExpression,
        scanner.relevantSpanFrom(start), SASS_SPACE);
      ep.spaceExpressions.emplace_back(ep.singleExpression);
      list->concat(ep.spaceExpressions);
      list->hasBrackets(true);
      return list;
    }
    else {
      ep.resolveSpaceExpressions();
      if (bracketList) {
        ListExpression* list = SASS_MEMORY_NEW(ListExpression,
          scanner.relevantSpanFrom(start), SASS_UNDEF);
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
    Offset start(scanner.offset);
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
      // callStackFrame frame(*context.logger123,
      //   BackTrace(scanner.relevantSpan()));
      // SourceSpan span(scanner.relevantSpan());
      // callStackFrame frame(*context.logger123,
      //   BackTrace(span));
      scanner.error("Expected expression.",
        *context.logger123, scanner.relevantSpan());
      return nullptr;
    }
  }
  // EO _singleExpression

  // Consumes a parenthesized expression.
  Expression* StylesheetParser::_parentheses()
  {
    if (plainCss()) {
      // This one is needed ...
      scanner.error("Parentheses aren't allowed in plain CSS.",
        *context.logger123, scanner.rawSpan());
    }

    LOCAL_FLAG(_inParentheses, true);

    Offset start(scanner.offset);
    scanner.expectChar($lparen);
    whitespace();
    if (!_lookingAtExpression()) {
      scanner.expectChar($rparen);
      // ToDo: ListExpression
      return SASS_MEMORY_NEW(ListExpression,
        scanner.relevantSpanFrom(start),
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
        scanner.relevantSpanFrom(start), first);
    }
    whitespace();

    sass::vector<ExpressionObj>
      expressions = { first };

    // We did parse a comma above
// This one fails
    ListExpressionObj list = SASS_MEMORY_NEW(
      ListExpression,
      scanner.relevantSpanFrom(start),
      SASS_COMMA);

    while (true) {
      if (!_lookingAtExpression()) {
        break;
      }
      expressions.emplace_back(_expressionUntilComma());
      if (!scanner.scanChar($comma)) {
        break;
      }
      list->separator(SASS_COMMA);
      whitespace();
    }

    scanner.expectChar($rparen);
    // ToDo: ListExpression
    list->concat(expressions);
    list->pstate(scanner.relevantSpanFrom(start));
    return list.detach();
  }
  // EO _parentheses

  // Consumes a map expression. This expects to be called after the
  // first colon in the map, with [first] as the expression before
  // the colon and [start] the point before the opening parenthesis.
  Expression* StylesheetParser::_map(Expression* first, Offset start)
  {

    MapExpressionObj map = SASS_MEMORY_NEW(
      MapExpression, scanner.relevantSpanFrom(start));

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
    map->pstate(scanner.relevantSpanFrom(start));
    return map.detach();
  }
  // EO _map

  Expression* StylesheetParser::_hashExpression()
  {
    // assert(scanner.peekChar() == $hash);
    if (scanner.peekChar(1) == $lbrace) {
      return identifierLike();
    }

    Offset start(scanner.offset);
    StringScannerState state(scanner.state());
    scanner.expectChar($hash);

    uint8_t first = scanner.peekChar();
    if (first != $nul && isDigit(first)) {
      // ColorExpression
      return _hexColorContents(state);
    }

    StringScannerState afterHash = scanner.state();
    InterpolationObj identifier = interpolatedIdentifier();
    if (_isHexColor(identifier)) {
      scanner.resetState(afterHash);
      return _hexColorContents(state);
    }


    InterpolationBuffer buffer(scanner);
    buffer.write($hash);
    buffer.addInterpolation(identifier);
    SourceSpan pstate(scanner.relevantSpanFrom(start));
    return SASS_MEMORY_NEW(StringExpression,
      pstate, buffer.getInterpolation(pstate));
  }

  Color* StylesheetParser::_hexColorContents(StringScannerState state)
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

    SourceSpan pstate(scanner.relevantSpanFrom(state.offset));
    // pstate.position = scanner.position;

    sass::string original(state.position, scanner.position);

    return SASS_MEMORY_NEW(Color_RGBA,
      pstate, red, green, blue, alpha, original);
  }

  // Returns whether [interpolation] is a plain
  // string that can be parsed as a hex color.
  bool StylesheetParser::_isHexColor(Interpolation* interpolation) const
  {
    sass::string plain = interpolation->getPlainString();
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
      // SourceSpan span(scanner.relevantSpan());
      // callStackFrame frame(*context.logger123,
      //   BackTrace(span));
      scanner.error("Expected hex digit.",
        *context.logger123, scanner.relevantSpan());
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
    Offset start(scanner.offset);
    scanner.readChar();
    whitespace();
    expectIdentifier("important");
    return StringExpression::plain(
      scanner.relevantSpanFrom(start), "!important");
  }
  // EO _importantExpression

  // Consumes a unary operation expression.
  UnaryExpression* StylesheetParser::_unaryOperation()
  {
    Offset start(scanner.offset);
    UnaryExpression::Type op =
      UnaryExpression::Type::PLUS;
    switch (scanner.readChar()) {
    case $plus:
      op = UnaryExpression::Type::PLUS;
      break;
    case $minus:
      op = UnaryExpression::Type::MINUS;
      break;
    case $slash:
      op = UnaryExpression::Type::SLASH;
      break;
    default:
      scanner.error("Expected unary operator.",
        *context.logger123, scanner.relevantSpan());
    }

    if (plainCss() && op != UnaryExpression::Type::SLASH) {
      scanner.error("Operators aren't allowed in plain CSS.",
        *context.logger123, scanner.relevantSpan());
    }

    whitespace();
    Expression* operand = _singleExpression();
    return SASS_MEMORY_NEW(UnaryExpression,
      scanner.relevantSpanFrom(start),
      op, operand);
  }
  // EO _unaryOperation

  // Consumes a number expression.
  NumberObj StylesheetParser::_number()
  {
    StringScannerState start = scanner.state();
    uint8_t first = scanner.peekChar();

    double sign = first == $minus ? -1 : 1;
    if (first == $plus || first == $minus) scanner.readChar();

    double number = scanner.peekChar() == $dot ? 0.0 : naturalNumber();

    // Don't complain about a dot after a number unless the number
    // starts with a dot. We don't allow a plain ".", but we need
    // to allow "1." so that "1..." will work as a rest argument.
    number += _tryDecimal(scanner.position != start.position);
    number *= _tryExponent();

    sass::string unit;
    if (scanner.scanChar($percent)) {
      unit = "%";
    }
    else if (lookingAtIdentifier() &&
      // Disallow units beginning with `--`.
      (scanner.peekChar() != $dash || scanner.peekChar(1) != $dash)) {
      unit = identifier(true);
    }

    return SASS_MEMORY_NEW(Number,
      scanner.relevantSpanFrom(start.offset),
      sign * number, unit);
  }

  // Consumes the decimal component of a number and returns its value, or 0
  // if there is no decimal component. If [allowTrailingDot] is `false`, this
  // will throw an error if there's a dot without any numbers following it.
  // Otherwise, it will ignore the dot without consuming it.
  double StylesheetParser::_tryDecimal(bool allowTrailingDot)
  {
    Offset start(scanner.offset);
    StringScannerState state(scanner.state());
    if (scanner.peekChar() != $dot) return 0.0;

    if (!isDigit(scanner.peekChar(1))) {
      if (allowTrailingDot) return 0.0;
      // SourceSpan span(scanner.relevantSpan());
      // callStackFrame frame(*context.logger123,
      //   BackTrace(span));
      scanner.error("Expected digit.",
        *context.logger123,
        scanner.relevantSpanFrom(start));
    }

    scanner.readChar();
    while (isDigit(scanner.peekChar())) {
      scanner.readChar();
    }

    // Use built-in double parsing so that we don't accumulate
    // floating-point errors for numbers with lots of digits.
    sass::string nr(scanner.substring(state.position));
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
    if (!isDigit(scanner.peekChar())) {
      SourceSpan span(scanner.relevantSpan());
      callStackFrame frame(*context.logger123,
        BackTrace(span));
      scanner.error(
        "Expected digit.",
        *context.logger123,
        scanner.relevantSpan());
    }

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
    StringScannerState state = scanner.state();
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
        scanner.rawSpanFrom(state.offset),
        scanner.substring(state.position)
      );
    }
    if (i == 0) {
      // SourceSpan span(scanner.relevantSpan());
      // callStackFrame frame(*context.logger123,
      //   BackTrace(span));
      scanner.error("Expected hex digit or \"?\".",
        *context.logger123, scanner.relevantSpan());
    }

    if (scanner.scanChar($minus)) {
      size_t j = 0;
      for (; j < 6; j++) {
        if (!scanCharIf(isHex)) break;
      }
      if (j == 0) {
        // SourceSpan span(scanner.relevantSpan());
        // callStackFrame frame(*context.logger123,
        //   BackTrace(span));
        scanner.error("Expected hex digit.",
          *context.logger123, scanner.relevantSpan());
      }
    }

    if (_lookingAtInterpolatedIdentifierBody()) {
      // SourceSpan span(scanner.relevantSpan());
      // callStackFrame frame(*context.logger123,
      //   BackTrace(span));
      scanner.error("Expected end of identifier.",
        *context.logger123, scanner.relevantSpan());
    }

    return StringExpression::plain(
      scanner.relevantSpanFrom(state.offset),
      scanner.substring(state.position)
    );
  }
  // EO _unicodeRange

  // Consumes a variable expression.
  Variable* StylesheetParser::_variable(bool hoist)
  {
    Offset start(scanner.offset);

    sass::string ns, name = variableName();
    // Check if variable already exists, otherwise error!
    // context.varStack.back()->hoistVariable(name);
    if (scanner.peekChar() == $dot && scanner.peekChar(1) != $dot) {
      scanner.readChar();
      ns = name;
      name = _publicIdentifier();
    }

    if (plainCss()) {
      error("Sass variables aren't allowed in plain CSS.",
        *context.logger123, scanner.relevantSpanFrom(start));
    }

    IdxRef vidx;

    // Completely static code, no loops
    // Therefore variables are fully static
    // Access before intialization points to parent
    // Access after point to the local scoped variable
    // If the variable does not exist yet, it means it will
    // access parent scope until the variable is also created
    // in the local scope, then it will access this variable.
    // auto vidx3 = context.varStack.back()->getVariableIdx(name);
    vidx = context.varStack.back()->getVariableIdx(name);

    /*
    if (!vidx.isValid()) {
      // Postpone this check into runtime
      error("Accessing uninitialized variable.",
        *context.logger123, scanner.relevantSpanFrom(start));
    }
    */

    return SASS_MEMORY_NEW(Variable,
      scanner.relevantSpanFrom(start),
      name, vidx);

  }
  // _variable

  // Consumes a selector expression.
  Parent_Reference* StylesheetParser::_selector()
  {
    if (plainCss()) {
      scanner.error("The parent selector isn't allowed in plain CSS.",
        *context.logger123, scanner.rawSpan());
      /* ,length: 1 */
    }

    Offset start(scanner.offset);
    scanner.expectChar($ampersand);

    if (scanner.scanChar($ampersand)) {
      context.logger123->addWarn33(
        "In Sass, \"&&\" means two copies of the parent selector. You "
        "probably want to use \"and\" instead.",
        scanner.relevantSpanFrom(start));
      scanner.offset.column -= 1;
      scanner.position -= 1;
    }

    return SASS_MEMORY_NEW(Parent_Reference,
      scanner.relevantSpanFrom(start));
  }
  // _selector

  // Consumes a quoted string expression.
  StringExpression* StylesheetParser::interpolatedString()
  {
    // NOTE: this logic is largely duplicated in ScssParser.interpolatedString.
    // Most changes here should be mirrored there.

    Offset start(scanner.offset);
    uint8_t quote = scanner.readChar();
    uint8_t next = 0, second = 0;

    if (quote != $single_quote && quote != $double_quote) {
      // ToDo: dart-sass passes the start position!?
      // SourceSpan span(scanner.relevantSpan());
      // callStackFrame frame(*context.logger123,
      //   BackTrace(span));
      scanner.error("Expected string.",
        *context.logger123,
        /*position:*/ scanner.relevantSpanFrom(start));
    }

    InterpolationBuffer buffer(scanner);
    while (true) {
      if (!scanner.peekChar(next)) {
        break;
      }
      if (next == quote) {
        scanner.readChar();
        break;
      }
      else if (next == $nul || isNewline(next)) {
        sass::sstream strm;
        strm << "Expected " << quote << ".";
        // SourceSpan span(scanner.relevantSpan());
        // callStackFrame frame(*context.logger123,
        //   BackTrace(span));
        scanner.error(strm.str(),
          *context.logger123,
          scanner.relevantSpan());
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

    InterpolationObj itpl = buffer.getInterpolation(
      scanner.relevantSpanFrom(start));
    return SASS_MEMORY_NEW(StringExpression,
      itpl->pstate(), itpl, true);

  }
  // EO interpolatedString

  // Consumes an expression that starts like an identifier.
  Expression* StylesheetParser::identifierLike()
  {
    Offset start(scanner.offset);
    InterpolationObj identifier = interpolatedIdentifier();
    sass::string plain(identifier->getPlainString());

    if (!plain.empty()) {
      if (plain == "if") {
        ArgumentInvocation* invocation = _argumentInvocation();
        return SASS_MEMORY_NEW(IfExpression,
          invocation->pstate(), invocation);
      }
      else if (plain == "not") {
        whitespace();
        Expression* expression = _singleExpression();
        return SASS_MEMORY_NEW(UnaryExpression,
          scanner.relevantSpanFrom(start),
          UnaryExpression::NOT,
          expression);
      }

      if (scanner.peekChar() != $lparen) {
        if (plain == "false") {
          return SASS_MEMORY_NEW(Boolean,
            scanner.relevantSpanFrom(start), false);
        }
        else if (plain == "true") {
          return SASS_MEMORY_NEW(Boolean,
            scanner.relevantSpanFrom(start), true);
        }
        else if (plain == "null") {
          return SASS_MEMORY_NEW(Null,
            scanner.relevantSpanFrom(start));
        }

        // ToDo: dart-sass has ColorExpression(color);
        if (const Color_RGBA* color = name_to_color(plain)) {
          Color_RGBA* copy = SASS_MEMORY_COPY(color);
          copy->pstate(identifier->pstate());
          copy->disp(plain);
          return copy;
        }
      }

      auto specialFunction = trySpecialFunction(plain, start);
      if (specialFunction != nullptr) {
        return specialFunction;
      }
    }

    sass::string ns;
    Offset beforeName(scanner.offset);
    uint8_t next = scanner.peekChar();
    if (next == $dot) {

      if (scanner.peekChar(1) == $dot) {
        return SASS_MEMORY_NEW(StringExpression,
          scanner.relevantSpanFrom(beforeName), identifier);
      }

      ns = identifier->getPlainString();
      scanner.readChar();
      beforeName = scanner.offset;

      StringObj ident = _publicIdentifier2();
      InterpolationObj itpl = SASS_MEMORY_NEW(Interpolation,
        ident->pstate());

      if (ns.empty()) {
        error("Interpolation isn't allowed in namespaces.",
          scanner.relevantSpanFrom(start));
      }

      ArgumentInvocation* args = _argumentInvocation();
      return SASS_MEMORY_NEW(FunctionExpression,
        scanner.relevantSpanFrom(start), itpl, args, ns);

    }
    else if (next == $lparen) {
      ArgumentInvocation* args = _argumentInvocation();
      FunctionExpression* fn = SASS_MEMORY_NEW(FunctionExpression,
        scanner.relevantSpanFrom(start), identifier, args, ns);
      sass::string name(identifier->getPlainString());
      if (!name.empty()) {
        // Get the function through the whole stack
        auto fidx = context.varStack.back()->getFunctionIdx(name);
        fn->fidx(fidx);
      }
      return fn;
    }
    else {
      return SASS_MEMORY_NEW(StringExpression,
        identifier->pstate(), identifier);
    }

  }
  // identifierLike

  // If [name] is the name of a function with special syntax, consumes it.
  // Otherwise, returns `null`. [start] is the location before the beginning of [name].
  StringExpression* StylesheetParser::trySpecialFunction(sass::string name, const Offset& start)
  {
    uint8_t next = 0;
    StringUtils::makeLowerCase(name);
    InterpolationBuffer buffer(scanner);
    sass::string normalized(Util::unvendor(name));

    if (normalized == "calc" || normalized == "element" || normalized == "expression") {
      if (!scanner.scanChar($lparen)) return nullptr;
      buffer.write(name);
      buffer.write($lparen);
    }
    else if (normalized == "min" || normalized == "max") {
      // min() and max() are parsed as the plain CSS mathematical functions if
      // possible, and otherwise are parsed as normal Sass functions.
      StringScannerState beginningOfContents = scanner.state();
      if (!scanner.scanChar($lparen)) return nullptr;
      whitespace();

      buffer.write(name);
      buffer.write($lparen);

      if (!_tryMinMaxContents(buffer)) {
        scanner.resetState(beginningOfContents);
        return nullptr;
      }

      SourceSpan pstate(scanner.relevantSpanFrom(start));
      return SASS_MEMORY_NEW(StringExpression,
        pstate, buffer.getInterpolation(pstate));
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
        scanner.relevantSpanFrom(start), contents);
    }
    else {
      return nullptr;
    }

    StringExpressionObj qwe = _interpolatedDeclarationValue(true);
    buffer.addInterpolation(qwe->text());
    scanner.expectChar($rparen);
    buffer.write($rparen);

    SourceSpan pstate(scanner.relevantSpanFrom(start));
    return SASS_MEMORY_NEW(StringExpression,
      pstate, buffer.getInterpolation(pstate));
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
  bool StylesheetParser::_tryMinMaxFunction(InterpolationBuffer& buffer, sass::string name)
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
  Interpolation* StylesheetParser::_tryUrlContents(const Offset& start, sass::string name)
  {
    // NOTE: this logic is largely duplicated in Parser.tryUrl.
    // Most changes here should be mirrored there.
    StringScannerState beginningOfContents = scanner.state();
    if (!scanner.scanChar($lparen)) return nullptr;
    whitespaceWithoutComments();

    // Match Ruby Sass's behavior: parse a raw URL() if possible, and if not
    // backtrack and re-parse as a function expression.
    InterpolationBuffer buffer(scanner);
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
        return buffer.getInterpolation(scanner.relevantSpanFrom(start));
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
  // Returns either a  `StringExpression` or a `FunctionExpression`
  Expression* StylesheetParser::dynamicUrl()
  {
    Offset start(scanner.offset);
    expectIdentifier("url");
    String* fnName = SASS_MEMORY_NEW(String,
      scanner.relevantSpanFrom(start), "url");
    InterpolationObj itpl = SASS_MEMORY_NEW(Interpolation,
      scanner.relevantSpanFrom(start), fnName);
    InterpolationObj contents = _tryUrlContents(start);
    if (contents != nullptr) {
      return SASS_MEMORY_NEW(StringExpression,
        scanner.relevantSpanFrom(start), contents);
    }

    SourceSpan pstate(scanner.relevantSpanFrom(start));
    ArgumentInvocation* args = _argumentInvocation();
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
    InterpolationBuffer buffer(scanner);
    const char* commentStart;
    StringExpressionObj strex;
    StringScannerState start = scanner.state();
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
        contents = _tryUrlContents(start.offset);
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
          buffer.write(identifierLiteral());
        }
        else {
          buffer.write(scanner.readChar());
        }
        break;
      }
    }

  endOfLoop:
    // scanner.relevant
    // scanner.resetState(scanner.relevant);
    // whitespaceWithoutComments(); // consume trailing white-space
    return buffer.getInterpolation(scanner.relevantSpanFrom(start.offset), true);

  }
  // almostAnyValue

  // Consumes tokens until it reaches a top-level `";"`, `")"`, `"]"`, or `"}"` and 
  // returns their contents as a string. If [allowEmpty] is `false` (the default), this
  // requires at least one token. Unlike [declarationValue], this allows interpolation.
  StringExpression* StylesheetParser::_interpolatedDeclarationValue(bool allowEmpty)
  {
    // NOTE: this logic is largely duplicated in Parser.declarationValue and
    // isIdentifier in utils.dart. Most changes here should be mirrored there.
    StringScannerState beforeUrl = scanner.state();
    Interpolation* contents;

    InterpolationBuffer buffer(scanner);
    Offset start(scanner.offset);
    sass::vector<uint8_t> brackets;
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

        contents = _tryUrlContents(beforeUrl.offset);
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
      // SourceSpan span(scanner.relevantSpan());
      // callStackFrame frame(*context.logger123,
      //   BackTrace(span));
      scanner.error("Expected token.",
        *context.logger123,
        scanner.relevantSpan());
    }
    SourceSpan pstate(scanner.rawSpanFrom(start));
    return SASS_MEMORY_NEW(StringExpression,
      pstate, buffer.getInterpolation(pstate));

  }
  // _interpolatedDeclarationValue

  // Consumes an identifier that may contain interpolation.
  Interpolation* StylesheetParser::interpolatedIdentifier()
  {
    InterpolationBuffer buffer(scanner);
    Offset start(scanner.offset);

    if (scanner.scanChar($dash)) {
      buffer.writeCharCode($dash);
      if (scanner.scanChar($dash)) {
        buffer.writeCharCode($dash);
        interpolatedIdentifierBody(buffer);
        return buffer.getInterpolation(scanner.relevantSpanFrom(start));
      }
    }

    uint8_t first = 0; // , next = 0;
    if (!scanner.peekChar(first)) {
      // SourceSpan span(scanner.relevantSpan());
      // callStackFrame frame(*context.logger123,
      //   BackTrace(span));
      scanner.error("Expected identifier.",
        *context.logger123,
        scanner.relevantSpanFrom(start));
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
      // SourceSpan span(scanner.relevantSpan());
      // callStackFrame frame(*context.logger123,
      //   BackTrace(span));
      scanner.error("Expected identifier.",
        *context.logger123, scanner.relevantSpan());
    }

    interpolatedIdentifierBody(buffer);
    return buffer.getInterpolation(scanner.relevantSpanFrom(start));

  }

  void StylesheetParser::interpolatedIdentifierBody(InterpolationBuffer& buffer)
  {

    uint8_t /* first = 0, */ next = 0;
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

  }
  // interpolatedIdentifier

  // Consumes interpolation.
  Expression* StylesheetParser::singleInterpolation()
  {
    Offset start(scanner.offset);
    scanner.expect("#{");
    whitespace();
    ExpressionObj contents = expression();
    scanner.expectChar($rbrace);

    if (plainCss()) {
      error(
        "Interpolation isn't allowed in plain CSS.",
        *context.logger123, scanner.rawSpanFrom(start));
    }

    return contents.detach();
  }
  // singleInterpolation

  // Consumes a list of media queries.
  Interpolation* StylesheetParser::_mediaQueryList()
  {
    Offset start(scanner.offset);
    InterpolationBuffer buffer(scanner);
    while (true) {
      whitespace();
      _mediaQuery(buffer);
      if (!scanner.scanChar($comma)) break;
      buffer.write($comma);
      buffer.write($space);
    }
    return buffer.getInterpolation(scanner.relevantSpanFrom(start));
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

    Offset start(scanner.offset);
    InterpolationBuffer buffer(scanner);
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

    return buffer.getInterpolation(scanner.relevantSpanFrom(start));

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
    Offset start(scanner.offset);
    uint8_t first = scanner.peekChar();
    if (first != $lparen && first != $hash) {
      Offset start(scanner.offset);
      expectIdentifier("not");
      whitespace();
      return SASS_MEMORY_NEW(SupportsNegation,
        scanner.relevantSpanFrom(start), _supportsConditionInParens());
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
        scanner.relevantSpanFrom(start), condition, right, op);
      whitespace();
    }

    return condition.detach();
  }
  // EO _supportsCondition

  // Consumes a parenthesized supports condition, or an interpolation.
  SupportsCondition* StylesheetParser::_supportsConditionInParens()
  {
    Offset start(scanner.offset);
    if (scanner.peekChar() == $hash) {
      return SASS_MEMORY_NEW(SupportsInterpolation,
        scanner.relevantSpanFrom(start), singleInterpolation());
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
      scanner.relevantSpanFrom(start), name, value);
  }
  // EO _supportsConditionInParens

  // Tries to consume a negated supports condition. Returns `null` if it fails.
  SupportsNegation* StylesheetParser::_trySupportsNegation()
  {
    StringScannerState start = scanner.state();
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
      scanner.relevantSpanFrom(start.offset),
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
    // if (isNameStart(second)) return true;
    // if (second == $backslash) return true;

    if (second == $hash) return scanner.peekChar(2) == $lbrace;
    // if (second != $dash) return false;

    // uint8_t third = scanner.peekChar(2);
    // if (third == $nul) return false;
    // if (third != $hash) return isNameStart(third);
    //  else return scanner.peekChar(3) == $lbrace;

    return isNameStart(second)
      || second == $backslash
      || second == $dash;
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
  sass::string StylesheetParser::_publicIdentifier()
  {
    Offset start(scanner.offset);
    auto result = identifier();

    uint8_t first = result[0];
    if (first == $dash || first == $underscore) {
      error("Private members can't be accessed from outside their modules.",
        scanner.rawSpanFrom(start));
    }

    return result;
  }
  // EO _publicIdentifier

  String* StylesheetParser::_publicIdentifier2()
  {
    Offset start(scanner.offset);
    sass::string ident(_publicIdentifier());
    return SASS_MEMORY_NEW(String, scanner.relevantSpanFrom(start), ident);
  }

}
