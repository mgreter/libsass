#include "parser_expression.hpp"

#include "character.hpp"
#include "utf8/checked.h"

namespace Sass {

  // Import some namespaces
  using namespace Charcode;
  using namespace Character;

  void ExpressionParser::addOperator(Sass_OP op, Position& start) {
	  if (parser.plainCss() && op != Sass_OP::DIV) {
      callStackFrame frame(*parser.context.logger,
        Backtrace(parser.scanner.pstate9(start)));
      parser.scanner.error("Operators aren't allowed in plain CSS.",
        *parser.context.logger, parser.scanner.pstate9(start));
        /* ,
																		position: scanner.position - operator.operator.length,
																		length : operator.operator.length */
	  }

	  allowSlash = allowSlash && op == Sass_OP::DIV;
	  while ((!operators.empty()) &&
		  (sass_op_to_precedence(operators.back())
		  >= sass_op_to_precedence(op))) {

      // std::cerr << "Resolve ops\n";
      resolveOneOperation();
      // std::cerr << "Resolved ops\n";
    }
	  operators.emplace_back(op);

	  // assert(singleExpression != null);

    operands.emplace_back(singleExpression);
	  parser.whitespace();
	  allowSlash = allowSlash && parser.lookingAtNumber();
	  singleExpression = parser._singleExpression();
    // debug_ast(singleExpression);
	  allowSlash = allowSlash && Cast<Number>(singleExpression);
  }

  ExpressionParser::ExpressionParser(StylesheetParser& parser) :
    start(parser.scanner.state()),
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
    parser.scanner.resetState(start);
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
        SourceSpan::fake("[pstateS3]"), Sass_OP::DIV, operands.back(), singleExpression);
      singleExpression = binex; // down cast
      binex->allowsSlash(true);
      operands.pop_back();
    }
    else {
      // in dart sass this sets allowSlash to false!
      Expression* lhs = operands.back();
      SourceSpan pstate = SourceSpan::delta(
        lhs->pstate(), singleExpression->pstate());

        parser.scanner.pstate9(start.offset);
      Binary_Expression* binex = SASS_MEMORY_NEW(Binary_Expression,
        pstate, op, lhs, singleExpression);
      singleExpression = binex; // down cast
      binex->allowsSlash(false);
      operands.pop_back();
      // debug_ast(binex);
      // exit(1);
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
		  spaceExpressions.emplace_back(singleExpression);
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
		  spaceExpressions.emplace_back(singleExpression);
      SourceSpan span = SourceSpan::delta(
        spaceExpressions.front()->pstate(),
        spaceExpressions.back()->pstate());
      ListExpression* list = SASS_MEMORY_NEW(
        ListExpression, span, SASS_SPACE);
		  list->concat(spaceExpressions);
		  singleExpression = list;
		  spaceExpressions.clear();
	  }

    if (singleEqualsOperand) {
      singleExpression = SASS_MEMORY_NEW(Binary_Expression,
        SourceSpan::delta(singleEqualsOperand->pstate(), singleExpression->pstate()),
        Sass_OP::IESEQ, singleEqualsOperand, singleExpression);
      singleEqualsOperand = {};

    }
      /*
	  // Seem to be for ms stuff
	  singleExpression = SASS_MEMORY_NEW(Binary_Expression,
	  "[pstateS2]", )

	  BinaryOperationExpression(
	  BinaryOperator.singleEquals, singleEqualsOperand, singleExpression);
	  singleEqualsOperand = null;
	  }
	  */

  }

}
