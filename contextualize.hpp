#ifndef SASS_CONTEXTUALIZE_H
#define SASS_CONTEXTUALIZE_H

#include <vector>
#include <iostream>

#include "ast.hpp"
#include "context.hpp"
#include "operation.hpp"
#include "environment.hpp"

namespace Sass {
  using namespace std;

  typedef Environment<AST_Node*> Env;
  struct Backtrace;

  class Listize2 : public Operation_CRTP<Expression*, Listize2> {

    Context&            ctx;

    Expression* fallback_impl(AST_Node* n);

  public:
    Listize2(Context&);
    virtual ~Listize2() { }

    using Operation<Expression*>::operator();

    Expression* operator()(Selector_List*);
    Expression* operator()(Complex_Selector*);
    Expression* operator()(Compound_Selector*);
    Expression* operator()(Parent_Selector*);

    template <typename U>
    Expression* fallback(U x) { return fallback_impl(x); }
  };

}

#endif
