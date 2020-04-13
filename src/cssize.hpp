#ifndef SASS_CSSIZE_H
#define SASS_CSSIZE_H

#include "ast.hpp"
#include "context.hpp"
#include "operation.hpp"
#include "environment.hpp"

namespace Sass {

  struct BackTrace;

  class Cssize : public Operation_CRTP<Statement*, Cssize> {

    // Share callStack with outside
    BackTraces& callStack;

    sass::vector<CssParentNodeObj>  cssStack;

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

    CssParentNode* parent();
    void visitBlockStatements(const sass::vector<StatementObj>& children, sass::vector<StatementObj>& results);

    Statement* bubble(CssAtRule*);
    Statement* bubble(CssAtRootRule*);
    Statement* bubble(CssMediaRule*);
    Statement* bubble(CssSupportsRule*);

    sass::vector<StatementObj> debubble(
      const sass::vector<StatementObj>&,
      Statement* parent = nullptr);

    void slice_by_bubble(const sass::vector<StatementObj>& children, std::vector<std::pair<bool, sass::vector<StatementObj>>>&);

    // generic fall-back
    template <typename U>
    Statement* fallback(U x)
    { return Cast<Statement>(x); }

  };

}

#endif
