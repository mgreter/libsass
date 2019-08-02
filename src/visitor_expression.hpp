#ifndef SASS_VISITOR_EXPRESSION_H
#define SASS_VISITOR_EXPRESSION_H

#include "ast_fwd_decl.hpp"

namespace Sass {

  // An interface for [visitors][] that traverse SassScript expressions.
  // [visitors]: https://en.wikipedia.org/wiki/Visitor_pattern

  template <typename T>
  class ExpressionVisitor {

    T* visitBinaryOperationExpression(Binary_Expression* node);
    T* visitBooleanExpression(Boolean* node);
    T* visitColorExpression(Color* node);
    // T* visitFunctionExpression(FunctionExpression* node);
    T* visitIfExpression(If* node);
    // T* visitListExpression(List* node);
    T* visitMapExpression(Map* node);
    T* visitNullExpression(Null* node);
    T* visitNumberExpression(Number* node);
    T* visitParenthesizedExpression(ParenthesizedExpression* node);
    // T* visitSelectorExpression(SelectorExpression* node);
    T* visitStringExpression(String_Constant* node);
    T* visitUnaryOperationExpression(Unary_Expression* node);
    T* visitValueExpression(Value* node);
    T* visitVariableExpression(Variable* node);

  };

}

#endif
