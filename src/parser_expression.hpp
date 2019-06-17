#ifndef SASS_PARSER_EXPRESSION_H
#define SASS_PARSER_EXPRESSION_H

#include "ast_fwd_decl.hpp"
#include "parser_stylesheet.hpp"

namespace Sass {

  // Helper class for stylesheet parser
  class ExpressionParser {

  public:
    const char* start;

    std::vector<ExpressionObj> commaExpressions;

    ExpressionObj singleEqualsOperand;

    std::vector<ExpressionObj> spaceExpressions;

    // Operators whose right-hand operands are not fully parsed yet, in order of
    // appearance in the document. Because a low-precedence operator will cause
    // parsing to finish for all preceding higher-precedence operators, this is
    // naturally ordered from lowest to highest precedence.
    std::vector<enum Sass_OP> operators;

    // The left-hand sides of [operators]. `operands[n]` is the left-hand side
    // of `operators[n]`.
    std::vector<ExpressionObj> operands;

    /// Whether the single expression parsed so far may be interpreted as
    /// slash-separated numbers.
    bool allowSlash = false;

    /// The leftmost expression that's been fully-parsed. Never `null`.
    ExpressionObj singleExpression;

    StylesheetParser& parser;

  public:

    ExpressionParser(StylesheetParser& parser);

    void resetState();

    void resolveOneOperation();

    void resolveOperations();

    void addSingleExpression(ExpressionObj expression, bool number = false);

    void addOperator(Sass_OP op);

    void resolveSpaceExpressions();

  };

}

#endif
