#ifndef SASS_EXPAND_H
#define SASS_EXPAND_H

#include <vector>

#include "ast.hpp"
#include "eval.hpp"
#include "operation.hpp"
#include "environment.hpp"

namespace Sass {

  class Listize;
  class Context;
  class Eval;
  struct Backtrace;

  class Expand : public Operation_CRTP<Statement*, Expand> {
  public:

    Env* environment();
    SelectorListObj& selector();
    SelectorListObj& original();
    SelectorListObj popFromSelectorStack();
    SelectorStack getSelectorStack();
    void pushToSelectorStack(SelectorListObj selector);

    SelectorListObj popFromOriginalStack();

    void pushToOriginalStack(SelectorListObj selector);

    Context&          ctx;
    Backtraces&       traces;
    Eval              eval;
    size_t            recursions;
    bool              in_keyframes;
    bool              at_root_without_rule;
    bool              old_at_root_without_rule;


    // The style rule that defines the current parent selector, if any.
    StyleRuleObj _styleRule;

    // Whether we're currently executing a function.
    bool _inFunction = false;

    // Whether we're currently building the output of an unknown at rule.
    bool _inUnknownAtRule = false;

    // Whether we're currently building the output of a style rule.
    bool isInStyleRule() {
      return !at_root_without_rule &&
        selector_stack.size() > 1;
      // return !_styleRule.isNull() &&
      //  !_atRootExcludingStyleRule;
    }

    bool isInMixin();
    bool isInContentBlock();

    // Whether we're directly within an `@at-root`
    // rule that excludes style rules.
    bool _atRootExcludingStyleRule = false;

    // Whether we're currently building the output of a `@keyframes` rule.
    bool _inKeyframes = false;

    bool plainCss = false;

    // The stylesheet that's currently being evaluated.
    // StyleSheet _stylesheet;

    // it's easier to work with vectors
    EnvStack      env_stack;
    BlockStack    block_stack;
    CallStack     call_stack;
  public:
    SelectorStack selector_stack;
  public:
    SelectorStack originalStack;
    MediaStack    mediaStack;

    Boolean_Obj bool_true;

  private:

    std::vector<CssMediaQueryObj> mergeMediaQueries(const std::vector<CssMediaQueryObj>& lhs, const std::vector<CssMediaQueryObj>& rhs);

  public:
    Expand(Context&, Env*, SelectorStack* stack = nullptr, SelectorStack* original = nullptr);
    ~Expand() { }

    Block* operator()(Block*);
    Statement* operator()(StyleRule*);

    Statement* operator()(FunctionRule*);
    Statement* operator()(IncludeRule*);
    Statement* operator()(MixinRule*);
    Statement* operator()(MediaRule*);

	At_Root_Query* visitAtRootQuery(At_Root_Query* e);

    // Css Ruleset is already static
    // Statement* operator()(CssMediaRule*);

    std::string joinStrings(
      std::vector<std::string>& strings,
      const char* const separator = "");

    std::string performInterpolation(
      InterpolationObj interpolation,
      bool warnForColor = false);

    std::string interpolationToValue(
      InterpolationObj interpolation,
      bool trim = false,
      bool warnForColor = false);

    SelectorListObj itplToSelector(Interpolation*, bool plainCss = false);

    Statement* operator()(SupportsRule*);
    Statement* operator()(At_Root_Block*);
    Statement* operator()(AtRule*);
    Statement* operator()(Declaration*);
    Statement* operator()(Assignment*);
    Statement* operator()(Import*);
    Statement* operator()(ImportRule*);
    Statement* operator()(StaticImport*);
    Statement* operator()(DynamicImport*);
    Statement* operator()(Import_Stub*);
    Statement* operator()(Warning*);
    Statement* operator()(Error*);
    Statement* operator()(Debug*);
    Statement* operator()(LoudComment*);
    Statement* operator()(SilentComment*);
    Statement* operator()(If*);
    Statement* operator()(For*);
    Statement* operator()(Each*);
    Statement* operator()(While*);
    Statement* operator()(ExtendRule*);
    Statement* operator()(Definition*);
    Statement* operator()(Mixin_Call*);
    Statement* operator()(Content*);
    
    void append_block(Block*);

  };

}

#endif
