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

  private:

   public:

     bool inMixin;
     BlockStack blockStack;
     MediaStack mediaStack;
     SelectorStack originalStack;
     SelectorStack selectorStack;

     // The style rule that defines the current parent selector, if any.
     StyleRuleObj _styleRule;

     // The name of the current declaration parent. Used for BEM-
     // declaration blocks as in `div { prefix: { suffix: val; } }`;
     sass::string _declarationName;

     // Whether we're currently executing a function.
     bool _inFunction = false;

     // Whether we're currently building the output of an unknown at rule.
     bool _inUnknownAtRule = false;

     // Whether we're directly within an `@at-root`
     // rule that excludes style rules.
     bool _atRootExcludingStyleRule = false;

     // Whether we're currently building the output of a `@keyframes` rule.
     bool _inKeyframes = false;

     bool plainCss = false;

     // Old flags
     bool              at_root_without_rule;


    Context& ctx;
    Backtraces& traces;
    Eval(Context& ctx);
    ~Eval();

    Value* _runUserDefinedCallable(
      ArgumentInvocation* arguments,
      UserDefinedCallable* callable,
      UserDefinedCallable* content,
      bool isMixinCall,
      Value* (Eval::* run)(UserDefinedCallable*, Trace*),
      Trace* trace,
      const SourceSpan& pstate);

    Value* _runBuiltInCallable(
      ArgumentInvocation* arguments,
      BuiltInCallable* callable,
      const SourceSpan& pstate);

    Value* _runBuiltInCallables(
      ArgumentInvocation* arguments,
      BuiltInCallables* callable,
      const SourceSpan& pstate);

    Boolean_Obj bool_true;
    Boolean_Obj bool_false;

    SelectorListObj& selector();
    SelectorListObj& original();

    bool isInContentBlock() const;

    // Whether we're currently building the output of a style rule.
    bool isInStyleRule();


    std::pair<sass::vector<ExpressionObj>, KeywordMap<ExpressionObj>> _evaluateMacroArguments(CallableInvocation& invocation);

    const sass::string cwd();
    CalleeStack& callee_stack();
    struct Sass_Inspect_Options& options();
    struct Sass_Compiler* compiler();

    Value* _runExternalCallable(ArgumentInvocation* arguments, ExternalCallable* callable, const SourceSpan& pstate);

    sass::string serialize(AST_Node* node);

    Interpolation* evalInterpolation(InterpolationObj interpolation, bool warnForColor);
    sass::string performInterpolation(InterpolationObj interpolation, bool warnForColor);
    sass::string interpolationToValue(InterpolationObj interpolation, bool trim, bool warnForColor);

    ArgumentInvocation* visitArgumentInvocation(ArgumentInvocation* args);

    Value* operator()(ContentRule* node) { return visitContentRule(node); }
    Value* operator()(FunctionRule* node) { return visitFunctionRule(node); }
    Value* operator()(MixinRule* node) { return visitMixinRule(node); }
    Value* operator()(IncludeRule* node) { return visitIncludeRule(node); }
    Value* operator()(ExtendRule* node) { return visitExtendRule(node); }
    Value* operator()(StyleRule* node) { return visitStyleRule(node); }
    Value* operator()(SupportsRule* node) { return visitSupportsRule(node); }
    Value* operator()(MediaRule* node) { return visitMediaRule(node); }
    Value* operator()(AtRootRule* node) { return visitAtRootRule(node); }
    Value* operator()(AtRule* node) { return visitAtRule(node); }
    Value* operator()(Declaration* node) { return visitDeclaration(node); }
    Value* operator()(Assignment* node) { return visitVariableDeclaration(node); }
    Value* operator()(Import* node) { return visitImport99(node); }
    Value* operator()(ImportRule* node) { return visitImportRule99(node); }
    Value* operator()(StaticImport* node) { return visitStaticImport99(node); }
    Value* operator()(DynamicImport* node) { return visitDynamicImport99(node); }
    Value* operator()(Import_Stub* node) { return visitImportStub99(node); }
    Value* operator()(SilentComment* node) { return visitSilentComment(node); }
    Value* operator()(LoudComment* node) { return visitLoudComment(node); }
    Value* operator()(If* node) { return visitIfRule(node); }
    Value* operator()(For* f) { return visitForRule(f); }
    Value* operator()(Each* node) { return visitEachRule(node); }
    Value* operator()(WhileRule* node) { return visitWhileRule(node); }

    Value* visitBlock(Block* node);

    // for evaluating function bodies
    Value* operator()(Block*);
    Value* operator()(Return*);
    Value* operator()(WarnRule*);
    void visitWarnRule(WarnRule* node);

    Value* operator()(ErrorRule*);
    void visitErrorRule(ErrorRule* node);

    Value* operator()(DebugRule*);
    void visitDebugRule(DebugRule* node);

    SassMap* operator()(SassMap*);
    SassList* operator()(SassList*);
    Map* operator()(MapExpression*);
    SassList* operator()(ListExpression*);
    Value* operator()(ValueExpression*);
    Value* operator()(ParenthesizedExpression*);
    Value* operator()(Binary_Expression*);
    Value* evalBinOp(Binary_Expression* b_in);
    Value* operator()(Unary_Expression*);
    Callable* _getFunction(const IdxRef& fidx, const sass::string& name, const sass::string& ns);
    UserDefinedCallable* _getMixin(const IdxRef& fidx, const EnvString& name, const sass::string& ns);
    Value* _runFunctionCallable(ArgumentInvocation* arguments, Callable* callable, const SourceSpan& pstate);
    Value* operator()(FunctionExpression*);
    Value* operator()(IfExpression*);
    Value* operator()(Variable*);
    Value* operator()(Number*);
    Value* operator()(Color_RGBA*);
    Value* operator()(Color_HSLA*);
    Value* operator()(Boolean*);

    String_Constant* operator()(Interpolation*);
    Value* operator()(StringExpression*);

    Value* operator()(String_Constant*);
    Value* operator()(StringLiteral*);
    Value* operator()(Null*);
    Value* operator()(Argument*);

    inline void keywordMapMap2(
      KeywordMap<ValueObj>& result,
      const KeywordMap<ExpressionObj>& map)
    {
      for (auto kv : map) {
        result[kv.first] =
          kv.second->perform(this);
      }
    }

    void _addRestMap(KeywordMap<ValueObj>& values, SassMap* map, const SourceSpan& nodeForSpan);
    void _addRestMap2(KeywordMap<ExpressionObj>& values, SassMap* map, const SourceSpan& nodeForSpan);
	ArgumentResults2& _evaluateArguments(ArgumentInvocation* arguments);

    sass::string _evaluateToCss(Expression* expression, bool quote = true);

    sass::string _serialize(Expression* expression, bool quote = true);

    sass::string _parenthesize(SupportsCondition* condition);
    sass::string _parenthesize(SupportsCondition* condition, SupportsOperation::Operand operand);
    sass::string _visitSupportsCondition(SupportsCondition* condition);

    String_Constant* operator()(SupportsCondition*);

    // actual evaluated selectors
    Value* operator()(Parent_Reference*);

    Value* _runAndCheck(UserDefinedCallable*, Trace*);

    Value* visitSupportsRule(SupportsRule* node);
    sass::vector<CssMediaQueryObj> mergeMediaQueries(const sass::vector<CssMediaQueryObj>& lhs, const sass::vector<CssMediaQueryObj>& rhs);
    Value* visitMediaRule(MediaRule* node);

    Value* visitAtRootRule(AtRootRule* a);

    CssString* interpolationToCssString(InterpolationObj interpolation, bool trim, bool warnForColor);

    Value* visitAtRule(AtRule* node);

    Value* visitDeclaration(Declaration* node);

    Value* visitVariableDeclaration(Assignment* a);

    Value* visitLoudComment(LoudComment* c);

    Value* visitIfRule(If* i);

    Value* visitForRule(For* f);

    Value* visitExtendRule(ExtendRule* e);

    Value* visitStyleRule(StyleRule* r);

    SelectorListObj itplToSelector(Interpolation* itpl, bool plainCss, bool allowParent = true);

    Value* visitEachRule(Each* e);

    Value* visitWhileRule(WhileRule* node);

    Value* visitContentRule(ContentRule* c);

    Value* visitMixinRule(MixinRule* rule);

    Value* visitFunctionRule(FunctionRule* rule);

    Value* _runWithBlock(UserDefinedCallable* callable, Trace* trace);

    Value* visitIncludeRule(IncludeRule* node);

    Value* visitSilentComment(SilentComment* c);

    Value* visitImportStub99(Import_Stub* i);

    Value* visitDynamicImport99(DynamicImport* rule);

    Value* visitStaticImport99(StaticImport* rule);

    Value* visitImportRule99(ImportRule* rule);

    Value* visitImport99(Import* imp);

    void append_block(Block* block);

    bool isInMixin();

    Block* visitRootBlock99(Block* b);

    // generic fallback
    template <typename U>
    Value* fallback(U x)
    { return Cast<Value>(x); }

  };

}

#endif
