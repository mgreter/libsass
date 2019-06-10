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
    SelectorList_Obj selector2();
    Selector_List_Obj selector();
    Selector_List_Obj popFromSelectorStack();
    SelectorStack2 getSelectorStack2();
    void pushToSelectorStack(SelectorList_Obj selector);

    Context&          ctx;
    Backtraces&       traces;
    Eval              eval;
    size_t            recursions;
    bool              in_keyframes;
    bool              at_root_without_rule;
    bool              old_at_root_without_rule;

    // it's easier to work with vectors
    EnvStack      env_stack;
    BlockStack    block_stack;
    CallStack     call_stack;
  private:
    SelectorStack2 selector_stack2;
  public:
    MediaStack    media_stack;
    MediaStack2    mediaStack;

    Boolean_Obj bool_true;

  private:
    void expand_selector_list(Selector_Obj, Selector_List_Obj extender, ExtendRule* e);
    std::vector<CssMediaQuery_Obj> mergeMediaQueries(const std::vector<CssMediaQuery_Obj>& lhs, const std::vector<CssMediaQuery_Obj>& rhs);

  public:
    Expand(Context&, Env*, SelectorStack2* stack = NULL);
    ~Expand() { }

    Block* operator()(Block*);
    Statement* operator()(Ruleset*);

    Statement* operator()(MediaRule*);

    Statement* operator()(Media_Block*);
    Statement* operator()(Supports_Block*);
    Statement* operator()(At_Root_Block*);
    Statement* operator()(Directive*);
    Statement* operator()(Declaration*);
    Statement* operator()(Assignment*);
    Statement* operator()(Import*);
    Statement* operator()(Import_Stub*);
    Statement* operator()(Warning*);
    Statement* operator()(Error*);
    Statement* operator()(Debug*);
    Statement* operator()(Comment*);
    Statement* operator()(If*);
    Statement* operator()(For*);
    Statement* operator()(Each*);
    Statement* operator()(While*);
    Statement* operator()(Return*);
    Statement* operator()(ExtendRule*);
    Statement* operator()(Definition*);
    Statement* operator()(Mixin_Call*);
    Statement* operator()(Content*);

    void append_block(Block*);

  };

}

#endif
