#ifndef SASS_CSSIZE_H
#define SASS_CSSIZE_H

#include "ast.hpp"
#include "context.hpp"
#include "operation.hpp"
#include "environment.hpp"

namespace Sass {

  struct Backtrace;

  class Cssize : public Operation_CRTP<Statement*, Cssize> {

    Backtraces& callStack;
    BlockStack block_stack;
    sass::vector<Statement*>  p_stack;

  public:
    Cssize(Context&);
    ~Cssize() { }

    Block* operator()(Block*);
    Statement* operator()(CssStyleRule*);
    // Statement* operator()(Bubble*);
    Statement* operator()(CssMediaRule*);
    Statement* operator()(CssSupportsRule*);
    Statement* operator()(CssAtRootRule*);
    Statement* operator()(CssAtRule*);
    Statement* operator()(Keyframe_Rule*);
    Statement* operator()(Trace*);
    // Statement* operator()(Assignment*);
    // Statement* operator()(Import*);
    // Statement* operator()(Import_Stub*);
    // Statement* operator()(WarnRule*);
    // Statement* operator()(ErrorRule*);
    // Statement* operator()(If*);
    // Statement* operator()(For*);
    // Statement* operator()(Each*);
    // Statement* operator()(WhileRule*);
    // Statement* operator()(Return*);
    // Statement* operator()(ExtendRule*);
    // Statement* operator()(ContentRule*);
    // Not used anymore? Why do we not get nulls here?
    // they seem to be already catched earlier
    // Statement* operator()(Null*);

    Statement* parent();
    void visitBlockStatements(const sass::vector<StatementObj>& children, sass::vector<StatementObj>& results);
    void slice_by_bubble(Block*, std::vector<std::pair<bool, Block_Obj>>&);
    Statement* bubble(CssAtRule*);
    Statement* bubble(CssAtRootRule*);
    Statement* bubble(CssMediaRule*);
    Statement* bubble(CssSupportsRule*);

    Block* debubble(Block* children, Statement* parent = 0);
    Block* flatten(const Block*);
    bool bubblable(Statement*) const;

    sass::vector<StatementObj> flatten2(Statement* s);

    // generic fallback
    template <typename U>
    Statement* fallback(U x)
    { return Cast<Statement>(x); }

    void append_block(Block*, Block*);
  };

}

#endif
