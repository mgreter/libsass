#include "parser_expression.hpp"

#include "character.hpp"
#include "utf8/checked.h"

namespace Sass {

  // Import some namespaces
  using namespace Charcode;
  using namespace Character;

  void ExpressionParser::addOperator(Sass_OP op) {
	  if (parser.plainCss() && op != Sass_OP::DIV) {
		  parser.scanner.error("Operators aren't allowed in plain CSS." /* ,
																		position: scanner.position - operator.operator.length,
																		length : operator.operator.length */);
	  }

	  allowSlash = allowSlash && op == Sass_OP::DIV;
	  while ((!operators.empty()) &&
		  (sass_op_to_precedence(operators.back())
		  >= sass_op_to_precedence(op))) {

		  resolveOneOperation();
	  }
	  operators.push_back(op);

	  // assert(singleExpression != null);
	  operands.push_back(singleExpression);
	  parser.whitespace();
	  allowSlash = allowSlash && parser.lookingAtNumber();
	  singleExpression = parser._singleExpression();
    // debug_ast(singleExpression);
	  allowSlash = allowSlash && Cast<Number>(singleExpression);
  }

  ExpressionParser::ExpressionParser(StylesheetParser& parser) :
    start(parser.scanner.position),
    commaExpressions(),
    singleEqualsOperand(),
    spaceExpressions(),
    operators(),
    operands(),
    allowSlash(false),
    singleExpression(),
    parser(parser)
  {
    allowSlash = parser.lookingAtNumber();
    singleExpression = parser._singleExpression();
  }

  void ExpressionParser::resetState()
  {
    commaExpressions.clear();
    spaceExpressions.clear();
    operators.clear();
    operands.clear();
    parser.scanner.position = start;
    allowSlash = parser.lookingAtNumber();
    singleExpression = parser._singleExpression();
  }

  void ExpressionParser::resolveOneOperation()
  {
    enum Sass_OP op = operators.back();
    operators.pop_back();
    if (op != Sass_OP::DIV) allowSlash = false;
    if (allowSlash && !parser._inParentheses) {
      // in dart sass this sets allowSlash to true!
      Binary_Expression* binex = SASS_MEMORY_NEW(Binary_Expression,
        "[pstate]", Sass_OP::DIV, operands.back(), singleExpression);
      singleExpression = binex; // down cast
      binex->allowsSlash(true);
      operands.pop_back();
    }
    else {
      // in dart sass this sets allowSlash to false!
      Binary_Expression* binex = SASS_MEMORY_NEW(Binary_Expression,
        "[pstate]", op, operands.back(), singleExpression);
      singleExpression = binex; // down cast
      binex->allowsSlash(false);
      operands.pop_back();
    }
  }

  void ExpressionParser::resolveOperations()
  {
    if (operators.empty()) return;
    while (!operators.empty()) {
      resolveOneOperation();
    }
  }

  void ExpressionParser::addSingleExpression(ExpressionObj expression, bool number)
  {
    if (singleExpression != nullptr) {
		  // If we discover we're parsing a list whose first element is a division
		  // operation, and we're in parentheses, reparse outside of a paren
		  // context. This ensures that `(1/2 1)` doesn't perform division on its
		  // first element.
		  if (parser._inParentheses) {
			  parser._inParentheses = false;
			  if (allowSlash) {
				  resetState();
				  return;
			  }
		  }

		  resolveOperations();
		  spaceExpressions.push_back(singleExpression);
		  allowSlash = number;
	  }
	  else if (!number) {
		  allowSlash = false;
	  }
    // debug_ast(expression);
	  singleExpression = expression;

  }

  void ExpressionParser::resolveSpaceExpressions() {
    resolveOperations();

	  if (!spaceExpressions.empty()) {
		  spaceExpressions.push_back(singleExpression);
		  List* list = SASS_MEMORY_NEW(List,
			  "[pstate]", 0, SASS_SPACE);
		  list->concat(spaceExpressions);
		  singleExpression = list;
		  spaceExpressions.clear();
	  }

    if (singleEqualsOperand) {
      singleExpression = SASS_MEMORY_NEW(Binary_Expression, "[pstate]",
        Sass_OP::IESEQ, singleEqualsOperand, singleExpression);
      singleEqualsOperand = {};

    }
      /*
	  // Seem to be for ms stuff
	  singleExpression = SASS_MEMORY_NEW(Binary_Expression,
	  "[pstate]", )

	  BinaryOperationExpression(
	  BinaryOperator.singleEquals, singleEqualsOperand, singleExpression);
	  singleEqualsOperand = null;
	  }
	  */

  }

}
