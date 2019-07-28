#ifndef SASS_PARSER_STYLESHEET_H
#define SASS_PARSER_STYLESHEET_H

#include "sass.hpp"
#include "ast.hpp"

#include "parser_base.hpp"
#include "character.hpp"
#include "stylesheet.hpp"
#include "interpolation.hpp"
#include "context.hpp"
#include "debugger.hpp"

namespace Sass {

  // Whether to parse `@use` rules.
  //
  // This is set to `false` on Dart Sass's master branch and `true` on the
  // `feature.use` branch. It allows us to avoid having separate development
  // tracks as much as possible without shipping `@use` support until we're
  // ready.
  // static bool _parseUse = false;

  typedef const char* LineScannerState;

  class StylesheetParser : public BaseParser {

  public:

    StylesheetParser(Context& context, const char* content, const char* path, size_t srcid) :
      BaseParser(context, content, path, srcid),
      _recursion(0),
      _isUseAllowed(true),
      _inMixin(false),
      _mixinHasContent(false),
      _inContentBlock(false),
      _inControlDirective(false),
      _inUnknownAtRule(false),
      _inStyleRule(false),
      _inParentheses(false),
      lastSilentComment()
    {}

  public:

    size_t _recursion = 0;

    // Backtraces traces;

    // Whether we've consumed a rule other than `@charset`, `@forward`, or
    // `@use`.
    bool _isUseAllowed = true;

    // Whether the parser is currently parsing the contents of a mixin
    // declaration.
    bool _inMixin = false;

    // Whether the current mixin contains at least one `@content` rule.
    //
    // This is `null` unless [_inMixin] is `true`.
    bool _mixinHasContent;

    // Whether the parser is currently parsing a content block passed to a mixin.
    bool _inContentBlock = false;

    // Whether the parser is currently parsing a control directive such as `@if`
    // or `@each`.
    bool _inControlDirective = false;

    // Whether the parser is currently parsing an unknown rule.
    bool _inUnknownAtRule = false;

    // Whether the parser is currently parsing a style rule.
    bool _inStyleRule = false;

    // Whether the parser is currently within a parenthesized expression.
    bool _inParentheses = false;

    // The silent comment this parser encountered previously.
    SilentCommentObj lastSilentComment;

  public:

    // Whether this is a plain CSS stylesheet.
    virtual bool plainCss() const { return false; }

    // Whether this is parsing the indented syntax.
    virtual bool isIndented() const { return false; };

    virtual Interpolation* styleRuleSelector() = 0;

    // Asserts that the scanner is positioned before a statement separator, or at
    // the end of a list of statements.
    // If the [name] of the parent rule is passed, it's used for error reporting.
    // This consumes whitespace, but nothing else, including comments.
    virtual void expectStatementSeparator(std::string name = "") = 0;

    // Whether the scanner is positioned at the end of a statement.
    virtual bool atEndOfStatement() = 0;

    // Whether the scanner is positioned before a block of
    // children that can be parsed with [children].
    virtual bool lookingAtChildren() = 0;

    // Tries to scan an `@else` rule after an `@if` block. Returns whether
    // that succeeded. This should just scan the rule name, not anything 
    // afterwards. [ifIndentation] is the result of [currentIndentation]
    // from before the corresponding `@if` was parsed.
    virtual bool scanElse(size_t ifIndentation) = 0;

    virtual std::vector<StatementObj> children(Statement* (StylesheetParser::* child)()) = 0;

    virtual std::vector<StatementObj> statements(Statement* (StylesheetParser::* statement)()) = 0;


    /// A map from all variable names that are assigned with `!global` in the
    /// current stylesheet to the nodes where they're defined.
    ///
    /// These are collected at parse time because they affect the variables
    /// exposed by the module generated for this stylesheet, *even if they aren't
    /// evaluated*. This allows us to ensure that the stylesheet always exposes
    /// the same set of variable names no matter how it's evaluated.
    // final _globalVariables = normalizedMap<VariableDeclaration>();

    std::vector<StatementObj> parse();

    /*

    ArgumentDeclaration* parseArgumentDeclaration();

    Expression* parseExpression();

*/

    // VariableDeclaration
    virtual Statement* variableDeclaration();

    // std::pair<std::string, ArgumentDeclaration*> parseSignature();

    Statement* _statement(bool root = false);

    Statement* _rootStatement() {
      return _statement(true);
    }

    Statement* _childStatement() {
      return _statement(false);
    }

    StyleRule* _styleRule();

    Statement* _declarationOrStyleRule();

    Declaration* _declarationOrBuffer(InterpolationBuffer& buffer);

    Declaration* _declaration();

    Statement* _declarationChild();

    virtual Statement* atRule(Statement* (StylesheetParser::* child)(), bool root = false);

    Statement* _declarationAtRule();

    Statement* _functionAtRule();

    std::string _plainAtRuleName();

    At_Root_Block* _atRootRule(Position start); // AtRootRule

    // Consumes a query expression of the form `(foo: bar)`.
    At_Root_Query* _atRootQuery();

    Content* _contentRule(Position start); // ContentRule

    Debug* _debugRule(Position start); // DebugRule

    uint8_t exclusive;

    bool _forRuleUntil();

    Each* _eachRule(Position start, Statement* (StylesheetParser::* child)()); // EachRule
    
    Error* _errorRule(Position start); // ErrorRule

    ExtendRule* _extendRule(Position start);

    FunctionRule* _functionRule(Position start); // FunctionRule

    For* _forRule(Position start, Statement* (StylesheetParser::* child)()); // ForRule

    If* _ifRule(Position start, Statement* (StylesheetParser::* child)()); // IfRule

    Import* _importRule(Position start); // ImportRule

    virtual ImportBase* importArgument();

    // std::string parseImportUrl(std::string url);

    bool _isPlainImportUrl(const std::string& url) const;

    std::pair<SupportsCondition_Obj, InterpolationObj> tryImportQueries();

    Mixin_Call* _includeRule(Position start); // IncludeRule

    MediaRule* mediaRule(Position start);

    Definition* _mixinRule(Position start); // MixinRule

    AtRule* mozDocumentRule(Position start, Interpolation* name);

    Return* _returnRule(Position start); // ReturnRule

    // SupportsRule
    SupportsRule* supportsRule(Position start);

    // UseRule _useRule(LineScannerState start);

    Warning* _warnRule(Position start); // WarnRule

    While* _whileRule(Position start, Statement* (StylesheetParser::* child)()); // WhileRule

    AtRule* unknownAtRule(Position start, Interpolation* name);

    Statement* _disallowedAtRule(Position start);

    Parameters* _argumentDeclaration(); // ArgumentDeclaration

    ArgumentDeclaration* _argumentDeclaration2(); // ArgumentDeclaration

    Parameters* parseArgumentDeclaration() {
      return _argumentDeclaration();
    }

    ArgumentDeclaration* parseArgumentDeclaration2() {
      return _argumentDeclaration2();
    }

    ArgumentInvocation* _argumentInvocation(bool mixin = false);

    Expression* expression(bool bracketList = false, bool singleEquals = false, bool(StylesheetParser::* until)() = nullptr);

    bool nextCharIsComma();

    Expression* _expressionUntilComma(bool singleEquals = false);

    Expression* _singleExpression();

    Expression* _parentheses();

    // Not yet evaluated, therefore not Map yet
    Expression* _map(Expression* first, Position start); // MapExpression

    Expression* _hashExpression();

    Color* _hexColorContents(Position start); // ColorExpression

    bool _isHexColor(Interpolation* interpolation) const;

    uint8_t _hexDigit();

    Expression* _plusExpression();

    Expression* _minusExpression();

    StringExpression* _importantExpression();

    Unary_Expression* _unaryOperation();

    Unary_Expression::Type _unaryOperatorFor(uint8_t character);

    NumberObj _number();

    double _tryDecimal(bool allowTrailingDot = false);

    double _tryExponent();

    StringExpression* _unicodeRange();

    Variable* _variable();

    Parent_Reference* _selector(); // SelectorExpression

    StringExpression* interpolatedString();

    virtual Expression* identifierLike();

    StringExpression* trySpecialFunction(std::string name, LineScannerState2 start);

    bool _tryMinMaxContents(InterpolationBuffer& buffer, bool allowComma = true);

    bool _tryMinMaxFunction(InterpolationBuffer& buffer, std::string name = "");

    Interpolation* _tryUrlContents(LineScannerState2 start, std::string name = "");

    Expression* dynamicUrl();

    Interpolation* almostAnyValue();

    StringExpression* _interpolatedDeclarationValue(bool allowEmpty = false);

    Interpolation* interpolatedIdentifier();

    Expression* singleInterpolation();

    Interpolation* _mediaQueryList();

    void _mediaQuery(InterpolationBuffer& buffer);

    Interpolation* _mediaFeature();

    bool nextIsComparison();

    Expression* _expressionUntilComparison();

    SupportsCondition* _supportsCondition();

    SupportsCondition* _supportsConditionInParens();

    SupportsNegation* _trySupportsNegation();

    // Returns whether the scanner is immediately before an identifier that may contain
    // interpolation. This is based on the CSS algorithm, but it assumes all backslashes
    // start escapes and it considers interpolation to be valid in an identifier.
    // https://drafts.csswg.org/css-syntax-3/#would-start-an-identifier
    bool _lookingAtInterpolatedIdentifier() const;

    // Returns whether the scanner is immediately before a sequence
    // of characters that could be part of an CSS identifier body.
    // The identifier body may include interpolation.
    bool _lookingAtInterpolatedIdentifierBody() const;

    // Returns whether the scanner is immediately before a SassScript expression.
    bool _lookingAtExpression() const;

    // Like [identifier], but rejects identifiers that begin with `_` or `-`.
    std::string _publicIdentifier();
    StringLiteral* _publicIdentifier2();


    // Consumes a block of [child] statements and passes them, as well as the
      // span from [start] to the end of the child block, to [create].
    template <typename T, typename ...Args>
    T* _withChildren(Statement* (StylesheetParser::*child)(), Args... args)
    {
      std::vector<StatementObj> elements = children(child);
      T* result = SASS_MEMORY_NEW(T, "[pstate]", args...);
      whitespaceWithoutComments();
      result->concat(elements);
      return result;
    }

  };

}

#endif
