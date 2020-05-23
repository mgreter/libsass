#ifndef SASS_EVAL_H
#define SASS_EVAL_H

// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"
#include "ast.hpp"

#include "context.hpp"
#include "listize.hpp"
#include "compiler.hpp"
#include "operation.hpp"
#include "environment.hpp"

namespace Sass {

  class Context;



  /// The result of evaluating arguments to a function or mixin.
  class ArgumentResults {

    // Arguments passed by position.
    ADD_REF(sass::vector<ValueObj>, positional);

    // The [AstNode]s that hold the spans for each [positional]
    // argument, or `null` if source span tracking is disabled. This
    // stores [AstNode]s rather than [FileSpan]s so it can avoid
    // calling [AstNode.span] if the span isn't required, since
    // some nodes need to do real work to manufacture a source span.
    // sass::vector<Ast_NodeObj> positionalNodes;

    // Arguments passed by name.
    // A list implementation might be more efficient
    // I dont expect any function to have many arguments
    // Normally the tradeoff is around 8 items in the list
    ADD_REF(EnvKeyFlatMap<ValueObj>, named);

    // The [AstNode]s that hold the spans for each [named] argument,
    // or `null` if source span tracking is disabled. This stores
    // [AstNode]s rather than [FileSpan]s so it can avoid calling
    // [AstNode.span] if the span isn't required, since some nodes
    // need to do real work to manufacture a source span.
    // EnvKeyFlatMap<Ast_NodeObj> namedNodes;

    // The separator used for the rest argument list, if any.
    ADD_REF(Sass_Separator, separator);

    ADD_PROPERTY(size_t, bidx);

  public:

    ArgumentResults() :
      separator_(SASS_UNDEF),
      bidx_(sass::string::npos)
    {};

    ArgumentResults(
      const ArgumentResults& other) noexcept;


    ArgumentResults(
      ArgumentResults&& other) noexcept;

    ArgumentResults& operator=(
      ArgumentResults&& other) noexcept;

    ArgumentResults(
      sass::vector<ValueObj>&& positional,
      EnvKeyFlatMap<ValueObj>&& named,
      Sass_Separator separator);

    void clear() {
      named_.clear();
      positional_.clear();
    }

    void clear2() {
      named_.clear();
      positional_.clear();
    }

  };

  class ResultsBuffer {

  public:

    Eval& eval;
    ArgumentResults& buffer;

    ResultsBuffer(Eval& eval);
    ~ResultsBuffer();

  };

  class Eval : public Operation_CRTP<Value*, Eval> {

  private:

    void _addChild(CssParentNode* parent, CssParentNode* node);

   public:

     Logger& logger456;

     sass::vector<size_t> freeResultBuffers; // (arguments->evaluated);
     sass::vector<ArgumentResults*> resultBufferes; // (arguments->evaluated);


     ArgumentResults& getResultBuffer() {

       if (freeResultBuffers.empty()) {
         size_t bidx = resultBufferes.size();
         resultBufferes.push_back(new ArgumentResults());
         resultBufferes.back()->bidx(bidx);
         return *resultBufferes.back();
       }

       size_t bidx = freeResultBuffers.back();
       freeResultBuffers.pop_back();
       return *resultBufferes[bidx];

     }

     void returnResultBuffer(ArgumentResults& results)
     {
       results.named().clear();
       results.positional().clear();
       freeResultBuffers.push_back(results.bidx());
     }

     size_t counter = 0;

     bool inMixin;
     CssParentNode* current;
     MediaStack mediaStack;
     SelectorStack originalStack;
     SelectorStack selectorStack;

     bool _hasMediaQueries;
     sass::vector<CssMediaQueryObj> _mediaQueries;

     // Current content block
     UserDefinedCallable* content88;

     // The style rule that defines the current parent selector, if any.
     CssStyleRule* _styleRule;

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


    Compiler& compiler;
    Extender extender;

    BackTraces& traces;
    Eval(Compiler& compiler);
    ~Eval();

    bool isRoot() const;

    Value* _runUserDefinedCallable2(
      sass::vector<ValueObj>& positional,
      EnvKeyFlatMap<ValueObj>& named,
      enum Sass_Separator separator,
      UserDefinedCallable* callable,
      UserDefinedCallable* content,
      bool isMixinCall,
      Value* (Eval::* run)(UserDefinedCallable*, CssImportTrace*),
      CssImportTrace* trace,
      const SourceSpan& pstate);


    Value* _runBuiltInCallable(
      ArgumentInvocation* arguments,
      BuiltInCallable* callable,
      const SourceSpan& pstate,
      bool selfAssign);

    Value* _runBuiltInCallables(
      ArgumentInvocation* arguments,
      BuiltInCallables* callable,
      const SourceSpan& pstate,
      bool selfAssign);

    BooleanObj bool_true;
    BooleanObj bool_false;

    SelectorListObj& selector();
    SelectorListObj& original();

    bool isInContentBlock() const;

    // Whether we're currently building the output of a style rule.
    bool isInStyleRule() const;

    Value* _acceptNodeChildren(ParentStatement* parent);

    std::pair<sass::vector<ExpressionObj>, EnvKeyFlatMap<ExpressionObj>> _evaluateMacroArguments(CallableInvocation& invocation);

    Value* _runExternalCallable(ArgumentInvocation* arguments, ExternalCallable* callable, const SourceSpan& pstate);

    sass::string serialize(AST_Node* node);

    Interpolation* evalInterpolation(InterpolationObj interpolation, bool warnForColor);
    sass::string performInterpolation(InterpolationObj interpolation, bool warnForColor);
    sass::string interpolationToValue(InterpolationObj interpolation, bool trim, bool warnForColor);
    SourceData* performInterpolationToSource(InterpolationObj interpolation, bool warnForColor);

    // ArgumentInvocation* visitArgumentInvocation(ArgumentInvocation* args);

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
    Value* operator()(ImportRule* node) { return visitImportRule99(node); }
    Value* operator()(StaticImport* node) { return visitStaticImport99(node); }
    Value* operator()(DynamicImport* node) { return visitDynamicImport99(node); }
    Value* operator()(IncludeImport* node) { return visitIncludeImport99(node); }
    Value* operator()(SilentComment* node) { return visitSilentComment(node); }
    Value* operator()(LoudComment* node) { return visitLoudComment(node); }
    Value* operator()(If* node) { return visitIfRule(node); }
    Value* operator()(For* f) { return visitForRule(f); }
    Value* operator()(Each* node) { return visitEachRule(node); }
    Value* operator()(WhileRule* node) { return visitWhileRule(node); }

    // for evaluating function bodies
    Value* visitChildren(const sass::vector<StatementObj>& children);
    Value* visitChildren2(CssParentNode* parent, const sass::vector<StatementObj>& children);

    Value* operator()(Return*);
    Value* operator()(WarnRule*);
    void visitWarnRule(WarnRule* node);

    Value* operator()(ErrorRule*);
    void visitErrorRule(ErrorRule* node);

    Value* operator()(DebugRule*);
    void visitDebugRule(DebugRule* node);

    Map* operator()(Map*);
    List* operator()(List*);
    Map* operator()(MapExpression*);
    List* operator()(ListExpression*);
    Value* operator()(ValueExpression*);
    Value* operator()(ParenthesizedExpression*);
    Value* operator()(Binary_Expression*);
    Value* operator()(UnaryExpression*);
    Callable* _getFunction(const IdxRef& fidx, const sass::string& name, const sass::string& ns);
    Callable* _getMixin(const IdxRef& fidx, const EnvKey& name, const sass::string& ns);
    Value* _runFunctionCallable(ArgumentInvocation* arguments, Callable* callable, const SourceSpan& pstate, bool selfAssign);
    Value* operator()(FunctionExpression*);
    Value* operator()(IfExpression*);
    Value* operator()(Variable*);
    Value* operator()(Number*);
    Value* operator()(Color_RGBA*);
    Value* operator()(Color_HSLA*);
    Value* operator()(Boolean*);

    String* operator()(Interpolation*);
    Value* operator()(StringExpression*);

    Value* operator()(String*);
    // Value* operator()(ItplString*);
    Value* operator()(Null*);
    Value* operator()(Argument*);

    inline void keywordMapMap2(
      EnvKeyFlatMap<ValueObj>& result,
      const EnvKeyFlatMap<ExpressionObj>& map)
    {
      for (const auto& kv : map) {
        result[kv.first] =
          kv.second->perform(this);
      }
    }

    CssParentNode* getRoot();

    CssParentNode* _trimIncluded(sass::vector<CssParentNodeObj>& nodes);

    void _addRestMap(EnvKeyFlatMap<ValueObj>& values, Map* map, const SourceSpan& nodeForSpan);
    void _addRestMap2(EnvKeyFlatMap<ExpressionObj>& values, Map* map, const SourceSpan& nodeForSpan);
	  void _evaluateArguments(ArgumentInvocation* arguments, ArgumentResults& evaluated);

    sass::string _evaluateToCss(Expression* expression, bool quote = true);

    sass::string _parenthesize(SupportsCondition* condition);
    sass::string _parenthesize(SupportsCondition* condition, SupportsOperation::Operand operand);
    sass::string _visitSupportsCondition(SupportsCondition* condition);

    String* operator()(SupportsCondition*);

    // actual evaluated selectors
    Value* operator()(Parent_Reference*);

    Value* _runAndCheck(UserDefinedCallable*, CssImportTrace*);

    CssParentNode* hoistStyleRule(CssParentNode* node);

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

    Value* _runWithBlock(UserDefinedCallable* callable, CssImportTrace* trace);

    Value* visitIncludeRule(IncludeRule* node);

    Value* visitSilentComment(SilentComment* c);

    Value* visitDynamicImport99(DynamicImport* rule);
    Value* visitIncludeImport99(IncludeImport* rule);

    sass::vector<CssMediaQueryObj> evalMediaQueries(Interpolation* itpl);

    Value* visitStaticImport99(StaticImport* rule);

    Value* visitImportRule99(ImportRule* rule);

    bool isInMixin();

    CssRoot* visitRoot32(Root* b);

    // generic fallback
    template <typename U>
    Value* fallback(U x)
    { return Cast<Value>(x); }

  };

}

#endif
