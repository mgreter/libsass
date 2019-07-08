#include "parser_css.hpp"

#include "character.hpp"
#include "utf8/checked.h"

namespace Sass {

  // Import some namespaces
  using namespace Charcode;
  using namespace Character;

  void CssParser::silentComment() {
    Offset start(scanner.offset);
    ScssParser::silentComment();
    error("Silent comments aren't allowed in plain CSS.",
      *context.logger, scanner.relevantSpanFrom(start));
  }

  bool isForbiddenAtRule(const sass::string& name)
  {
    return name == "at-root"
      || name == "content"
      || name == "debug"
      || name == "each"
      || name == "error"
      || name == "extend"
      || name == "for"
      || name == "function"
      || name == "if"
      || name == "include"
      || name == "mixin"
      || name == "return"
      || name == "warn"
      || name == "while";
  }

  Statement* CssParser::atRule(Statement* (StylesheetParser::* child)(), bool root)
  {
    // NOTE: logic is largely duplicated in CssParser.atRule.
    // Most changes here should be mirrored there.

    Offset start(scanner.offset);
    // const char* pos = scanner.position;
    scanner.expectChar($at);
    InterpolationObj name = interpolatedIdentifier();
    whitespace();

    sass::string plain(name->getPlainString());

    if (isForbiddenAtRule(plain)) {
      InterpolationObj value = almostAnyValue();
      error("This at-rule isn't allowed in plain CSS.",
        *context.logger, scanner.relevantSpanFrom(start));
    }
    else if (plain == "charset") {
      sass::string charset(string());
      if (!root) {
        error("This at-rule is not allowed here.",
          *context.logger, scanner.relevantSpanFrom(start));
      }
    }
    else if (plain == "import") {
      return _cssImportRule(start);
    }
    else if (plain == "media") {
      return mediaRule(start);
    }
    else if (plain == "-moz-document") {
      return mozDocumentRule(start, name);
    }
    else if (plain == "supports") {
      return supportsRule(start);
    }
    else {
      return unknownAtRule(start, name);
    }
    return nullptr;
  }

  // Consumes a plain-CSS `@import` rule that disallows
  // interpolation. [start] should point before the `@`.
  ImportRule* CssParser::_cssImportRule(Offset start) {

    // Position urlStart(scanner);
    uint8_t next = scanner.peekChar();
    ExpressionObj url;
    if (next == $u || next == $U) {
      url = dynamicUrl();
    }
    else {
      // StringExpression(itpl.asInterpolation(static: true));
      StringExpressionObj ex(interpolatedString());
      InterpolationObj itpl(ex->getAsInterpolation());
      url = SASS_MEMORY_NEW(StringExpression, ex->pstate(), itpl);
    }

    whitespace();
    Offset beforeQuery(scanner.offset);
    auto queries(tryImportQueries());
    expectStatementSeparator("@import rule");
    SourceSpan span(scanner.relevantSpanFrom(beforeQuery));
    ImportRuleObj imp(SASS_MEMORY_NEW(ImportRule, span));
    InterpolationObj urlItpl(SASS_MEMORY_NEW(Interpolation, span, url));
    StaticImportObj entry(SASS_MEMORY_NEW(StaticImport, span,
      urlItpl, queries.first, queries.second));
    entry->outOfOrder(false);
    imp->append(entry);
    return imp.detach();

  }

  bool isDisallowedFunction(sass::string name)
  {
    return name == "red"
      || name == "green"
      || name == "blue"
      || name == "mix"
      || name == "hue"
      || name == "saturation"
      || name == "lightness"
      || name == "adjust-hue"
      || name == "lighten"
      || name == "darken"
      || name == "saturate"
      || name == "desaturate"
      || name == "complement"
      || name == "opacify"
      || name == "fade-in"
      || name == "transparentize"
      || name == "fade-out"
      || name == "adjust-color"
      || name == "scale-color"
      || name == "change-color"
      || name == "ie-hex-str"
      || name == "unquote"
      || name == "quote"
      || name == "str-length"
      || name == "str-insert"
      || name == "str-index"
      || name == "str-slice"
      || name == "to-upper-case"
      || name == "to-lower-case"
      || name == "percentage"
      || name == "round"
      || name == "ceil"
      || name == "floor"
      || name == "abs"
      || name == "max"
      || name == "min"
      || name == "random"
      || name == "length"
      || name == "nth"
      || name == "set-nth"
      || name == "join"
      || name == "append"
      || name == "zip"
      || name == "index"
      || name == "list-separator"
      || name == "is-bracketed"
      || name == "map-get"
      || name == "map-merge"
      || name == "map-remove"
      || name == "map-keys"
      || name == "map-values"
      || name == "map-has-key"
      || name == "keywords"
      || name == "selector-nest"
      || name == "selector-append"
      || name == "selector-extend"
      || name == "selector-replace"
      || name == "selector-unify"
      || name == "is-superselector"
      || name == "simple-selectors"
      || name == "selector-parse"
      || name == "feature-exists"
      || name == "inspect"
      || name == "type-of"
      || name == "unit"
      || name == "unitless"
      || name == "comparable"
      || name == "if"
      || name == "unique-id";
  }

  Expression* CssParser::identifierLike()
  {
    Offset start(scanner.offset);
    // StringScannerState pos = scanner.state();
    InterpolationObj identifier = interpolatedIdentifier();
    sass::string plain(identifier->getPlainString());

    StringExpressionObj specialFunction = trySpecialFunction(
      Util::ascii_str_tolower(plain), start);
    if (specialFunction != nullptr) {
      return specialFunction;
    }

    Offset beforeArguments(scanner.offset);
    if (!scanner.scanChar($lparen)) {
      return SASS_MEMORY_NEW(StringExpression,
        scanner.rawSpanFrom(start), identifier);
    }

    sass::vector<ExpressionObj> arguments;

    if (!scanner.scanChar($rparen)) {
      do {
        whitespace();
        Expression* argument = expression(false, true);
        arguments.emplace_back(SASS_MEMORY_NEW(Argument,
          argument->pstate(), argument, EnvKey()));
        whitespace();
      } while (scanner.scanChar($comma));
      scanner.expectChar($rparen);
    }

    // arguments->pstate(scanner.relevantSpan(start));

    if (isDisallowedFunction(plain)) {
      error(
        "This function isn't allowed in plain CSS.",
        *context.logger, scanner.relevantSpanFrom(start));
    }

    Interpolation* name = SASS_MEMORY_NEW(Interpolation, identifier->pstate());
    name->append(SASS_MEMORY_NEW(StringExpression, identifier->pstate(), identifier));

    ArgumentInvocation* args = SASS_MEMORY_NEW(ArgumentInvocation,
      scanner.rawSpanFrom(start), arguments, EnvKeyFlatMap2{ "null", {} });
    
    return SASS_MEMORY_NEW(FunctionExpression,
      scanner.rawSpanFrom(start), name, args);

  }


}
