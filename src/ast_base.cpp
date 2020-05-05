#include "ast_base.hpp"

namespace Sass {

  // Needs to be in sync with SassOp enum
  uint8_t SassOpPresedence[15] = {
    1, 2, 3, 3, 4, 4, 4, 4,
    5, 5, 6, 6, 6, 9, 255
  };

  // Needs to be in sync with SassOp enum
  const char* SassOpNames[15] = {
    "or", "and", "eq", "neq", "gt", "gte", "lt", "lte",
    "plus", "minus", "times", "div", "mod", "seq", "invalid"
  };

  // Needs to be in sync with SassOp enum
  const char* SassOpName[15] = {
    "or", "and", "eq", "neq", "gt", "gte", "lt", "lte",
    "plus", "minus", "times", "div", "mod", "seq", "invalid"
  };

  // Needs to be in sync with SassOp enum
  const char* SassOpOperator[15] = {
    "||", "&&", "==", "!=", ">", ">=", "<", "<=",
    "+", "-", "*", "/", "%", "=", "invalid"
  };

  // Precedence is used to decide order
  // in ExpressionParser::addOperator.
  uint8_t sass_op_to_precedence(enum Sass_OP op)
  {
    return SassOpPresedence[op];
  }

  // Get readable name for error messages
  const char* sass_op_to_name(enum Sass_OP op)
  {
    return SassOpName[op];
  }

  // Get readable name for operator (e.g. `==`)
  const char* sass_op_separator(enum Sass_OP op)
  {
    return SassOpOperator[op];
  }

};
