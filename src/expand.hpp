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
  typedef Environment<AST_Node_Ptr> Env;
  struct Backtrace;

  class Expand : public Operation_CRTP<Statement_Ptr, Expand> {
  public:

    Env* environment();
    Context& context();
    CommaSequence_Selector_Ptr selector();
    Backtrace* backtrace();

    Context&          ctx;
    Eval              eval;

    // it's easier to work with vectors
    std::vector<Env*>           env_stack;
    std::vector<Block_Ptr>         block_stack;
    std::vector<AST_Node_Ptr>      call_stack;
    std::vector<String_Ptr>        property_stack;
    std::vector<CommaSequence_Selector_Ptr> selector_stack;
    std::vector<Media_Block_Ptr>   media_block_stack;
    std::vector<Backtrace*>     backtrace_stack;
    bool                        in_keyframes;
    bool                        at_root_without_rule;
    bool                        old_at_root_without_rule;

    Statement_Ptr fallback_impl(AST_Node_Ptr n);

  private:
    void expand_selector_list(Selector_Ptr, CommaSequence_Selector_Ptr extender);

  public:
    Expand(Context&, Env*, Backtrace*, std::vector<CommaSequence_Selector_Ptr>* stack = NULL);
    ~Expand() { }

    Block_Ptr operator()(Block_Ptr);
    Statement_Ptr operator()(Ruleset_Ptr);
    Statement_Ptr operator()(Media_Block_Ptr);
    Statement_Ptr operator()(Supports_Block_Ptr);
    Statement_Ptr operator()(At_Root_Block_Ptr);
    Statement_Ptr operator()(Directive_Ptr);
    Statement_Ptr operator()(Declaration_Ptr);
    Statement_Ptr operator()(Assignment_Ptr);
    Statement_Ptr operator()(Import_Ptr);
    Statement_Ptr operator()(Import_Stub_Ptr);
    Statement_Ptr operator()(Warning_Ptr);
    Statement_Ptr operator()(Error_Ptr);
    Statement_Ptr operator()(Debug_Ptr);
    Statement_Ptr operator()(Comment_Ptr);
    Statement_Ptr operator()(If_Ptr);
    Statement_Ptr operator()(For_Ptr);
    Statement_Ptr operator()(Each_Ptr);
    Statement_Ptr operator()(While_Ptr);
    Statement_Ptr operator()(Return_Ptr);
    Statement_Ptr operator()(Extension_Ptr);
    Statement_Ptr operator()(Definition_Ptr);
    Statement_Ptr operator()(Mixin_Call_Ptr);
    Statement_Ptr operator()(Content_Ptr);

    template <typename U>
    Statement_Ptr fallback(U x) { return fallback_impl(x); }

    void append_block(Block_Obj);
  };

}

#endif
