#ifndef SASS_EVALUATE_H
#define SASS_EVALUATE_H

#include "ast_fwd_decl.hpp"

#include "visitor_css.hpp"
#include "visitor_statement.hpp"
#include "visitor_expression.hpp"

namespace Sass {

  class EvaluateVisitor :
    public StatementVisitor<Value*>,
    public ExpressionVisitor<Value*>,
    public CssVisitor<void>
  {

    Value* visitBinaryOperationExpression(Binary_Expression* node);
    Value* visitBooleanExpression(Boolean* node);
    Value* visitColorExpression(Color* node);
    Value* visitIfExpression(If* node);
    // Value* visitListExpression(List* node);
    Value* visitMapExpression(Map* node);
    Value* visitNullExpression(Null* node);
    Value* visitNumberExpression(Number* node);
    Value* visitParenthesizedExpression(ParenthesizedExpression* node);
    // T* visitSelectorExpression(SelectorExpression* node);
    Value* visitStringExpression(String_Constant* node);
    Value* visitUnaryOperationExpression(Unary_Expression* node);
    Value* visitValueExpression(Value* node);
    Value* visitVariableExpression(Variable* node);

  };

}



#endif
