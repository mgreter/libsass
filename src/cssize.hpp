#ifndef SASS_CSSIZE_H
#define SASS_CSSIZE_H

#include "ast.hpp"
#include "context.hpp"
#include "operation.hpp"
#include "environment.hpp"

namespace Sass {

  struct Backtrace;

  class Cssize : public Operation_CRTP<Statement*, Cssize> {

    Backtraces&                 traces;
    BlockStack      block_stack;
    std::vector<Statement*>  p_stack;

  public:
    Cssize(Context&);
    ~Cssize() { }

    Block* operator()(Block*);
    Statement* operator()(CssStyleRule*);
    // Statement* operator()(Bubble*);
    Statement* operator()(CssMediaRule*);
    Statement* operator()(SupportsRule*);
    Statement* operator()(CssSupportsRule*);
    Statement* operator()(At_Root_Block*);
    Statement* operator()(AtRule*);
    Statement* operator()(Keyframe_Rule*);
    Statement* operator()(Trace*);
    Statement* operator()(Declaration*);
    // Statement* operator()(Assignment*);
    // Statement* operator()(Import*);
    // Statement* operator()(Import_Stub*);
    // Statement* operator()(Warning*);
    // Statement* operator()(Error*);
    // Statement* operator()(If*);
    // Statement* operator()(For*);
    // Statement* operator()(Each*);
    // Statement* operator()(While*);
    // Statement* operator()(Return*);
    // Statement* operator()(ExtendRule*);
    // Statement* operator()(Definition*);
    // Statement* operator()(Mixin_Call*);
    // Statement* operator()(Content*);
    // Not used anymore? Why do we not get nulls here?
    // they seem to be already catched earlier
    // Statement* operator()(Null*);

    Statement* parent();
    std::vector<std::pair<bool, Block_Obj>> slice_by_bubble(Block*);
    Statement* bubble(AtRule*);
    Statement* bubble(At_Root_Block*);
    Statement* bubble(CssMediaRule*);
    Statement* bubble(SupportsRule*);
    Statement* bubble(CssSupportsRule*);

    Block* debubble(Block* children, Statement* parent = 0);
    Block* flatten(const Block*);
    bool bubblable(Statement*);

    // generic fallback
    template <typename U>
    Statement* fallback(U x)
    { return Cast<Statement>(x); }

    void append_block(Block*, Block*);
  };

}

#endif
