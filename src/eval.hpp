#ifndef SASS_EVAL_H
#define SASS_EVAL_H

// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"
#include "ast.hpp"

#include "context.hpp"
#include "listize.hpp"
#include "operation.hpp"
#include "environment.hpp"

namespace Sass {

  class Expand;
  class Context;

  class Eval : public Operation_CRTP<Expression*, Eval> {

   public:
    Expand& exp;
    Context& ctx;
    Backtraces& traces;
    Eval(Expand& exp);
    ~Eval();

    Boolean_Obj bool_true;
    Boolean_Obj bool_false;

    Env* environment();
    EnvStack& env_stack();
    const std::string cwd();
    CalleeStack& callee_stack();
    struct Sass_Inspect_Options& options();
    struct Sass_Compiler* compiler();

    std::string serialize(AST_Node* node);

    std::string joinStrings(std::vector<std::string>& strings, const char* const separator);
    std::string performInterpolation(InterpolationObj interpolation, bool warnForColor);
    std::string interpolationToValue(InterpolationObj interpolation, bool trim, bool warnForColor);

    // for evaluating function bodies
    Expression* operator()(Block*);
    Expression* operator()(Assignment*);
    Expression* operator()(If*);
    Expression* operator()(For*);
    Expression* operator()(Each*);
    Expression* operator()(While*);
    Expression* operator()(Return*);
    Expression* operator()(Warning*);
    Expression* operator()(Error*);
    Expression* operator()(Debug*);

    Expression* operator()(List*);
    Expression* operator()(Map*);
    Expression* operator()(ParenthesizedExpression*);
    Expression* operator()(Binary_Expression*);
    Expression* evalBinOp(Binary_Expression* b_in);
    Expression* operator()(Unary_Expression*);
    Expression* operator()(FunctionExpression*);
    Expression* operator()(Variable*);
    Expression* operator()(Number*);
    Expression* operator()(Color_RGBA*);
    Expression* operator()(Color_HSLA*);
    Expression* operator()(Boolean*);

    Expression* operator()(StringLiteral*);
    String_Constant* operator()(Interpolation*);
    Expression* operator()(StringExpression2*);

    Expression* operator()(String_Quoted*);
    Expression* operator()(String_Constant*);
    Expression* operator()(At_Root_Query*);
    Expression* operator()(Supports_Operator*);
    Expression* operator()(Supports_Negation*);
    Expression* operator()(Supports_Declaration*);
    Expression* operator()(Supports_Interpolation*);
    Expression* operator()(Null*);
    Expression* operator()(Argument*);
    Expression* operator()(Arguments*);
    Expression* operator()(LoudComment*);
    Expression* operator()(SilentComment*);

    // actual evaluated selectors
    Expression* operator()(Parent_Reference*);

    // generic fallback
    template <typename U>
    Expression* fallback(U x)
    { return Cast<Expression>(x); }

  };

}

#endif
