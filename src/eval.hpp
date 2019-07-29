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

  class Eval : public Operation_CRTP<Value*, Eval> {

   public:
    Expand& exp;
    Context& ctx;
    Backtraces& traces;
    Eval(Expand& exp);
    ~Eval();

    Value* _runUserDefinedCallable(
      ArgumentInvocation* arguments,
      UserDefinedCallable* callable,
      Value* (Eval::* run)(UserDefinedCallable*),
      ParserState pstate);

    Value* _runBuiltInCallable(
      ArgumentInvocation* arguments,
      BuiltInCallable* callable,
      ParserState pstate);

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
    std::string performInterpolation(InterpolationObj interpolation, bool warnForColor = false);
    std::string interpolationToValue(InterpolationObj interpolation, bool trim, bool warnForColor);

    // for evaluating function bodies
    Value* operator()(Block*);
    Value* operator()(Assignment*);
    Value* operator()(If*);
    Value* operator()(For*);
    Value* operator()(Each*);
    Value* operator()(While*);
    Value* operator()(Return*);
    Value* operator()(Warning*);
    Value* operator()(Error*);
    Value* operator()(Debug*);

    // Expression* operator()(List*);
    SassMap* operator()(SassMap*);
    SassList* operator()(SassList*);
    Map* operator()(MapExpression*);
    SassList* operator()(ListExpression*);
    Value* operator()(ValueExpression*);
    Value* operator()(ParenthesizedExpression*);
    Value* operator()(Binary_Expression*);
    Value* evalBinOp(Binary_Expression* b_in);
    Value* operator()(Unary_Expression*);
    Value* operator()(FunctionExpression*);
    Callable* _getFunction(std::string name, std::string ns);
    Value* _runFunctionCallable(ArgumentInvocation* arguments, Callable* callable, ParserState pstate);
    Value* operator()(FunctionExpression2*);
    Value* operator()(MixinExpression*);
    Value* operator()(Variable*);
    Value* operator()(Number*);
    Value* operator()(Color_RGBA*);
    Value* operator()(Color_HSLA*);
    Value* operator()(Boolean*);

    Value* operator()(StringLiteral*);
    String_Constant* operator()(Interpolation*);
    Value* operator()(StringExpression*);

    Value* operator()(String_Quoted*);
    Value* operator()(String_Constant*);
    Value* operator()(Null*);
    Value* operator()(Argument*);
    Value* operator()(Arguments*);
    Value* operator()(LoudComment*);
    Value* operator()(SilentComment*);

    Argument* visitArgument(Argument* arg);
    Arguments* visitArguments(Arguments* args);
    NormalizedMap<ValueObj> normalizedMapMap(const NormalizedMap<ExpressionObj>& map);
    ArgumentResults* _evaluateArguments(ArgumentInvocation* arguments);

    std::string _evaluateToCss(Expression* expression, bool quote = true);

    std::string _serialize(Expression* expression, bool quote = true);

    std::string _parenthesize(SupportsCondition* condition);
    std::string _parenthesize(SupportsCondition* condition, SupportsOperation::Operand operand);
    std::string _visitSupportsCondition(SupportsCondition* condition);

    String* operator()(SupportsCondition*);

    // Expression* operator()(SupportsOperation*);
    // Expression* operator()(SupportsNegation*);
    // Expression* operator()(SupportsDeclaration*);
    // Expression* operator()(SupportsInterpolation*);

    // actual evaluated selectors
    Value* operator()(Parent_Reference*);

    Value* _runAndCheck(UserDefinedCallable*);
    Value* _runWithBlock(UserDefinedCallable*);

    // generic fallback
    template <typename U>
    Value* fallback(U x)
    { return Cast<Value>(x); }

  };

}

#endif
