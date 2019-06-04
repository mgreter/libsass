#ifndef SASS_LISTIZE_H
#define SASS_LISTIZE_H

// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include <vector>
#include <iostream>

#include "ast.hpp"
#include "context.hpp"
#include "operation.hpp"
#include "environment.hpp"

namespace Sass {

  struct Backtrace;

  class Listize : public Operation_CRTP<Expression*, Listize> {

  public:

    static Expression* perform(AST_Node* node);

  public:
    Listize();
    ~Listize() { }

    Expression* operator()(SelectorList*);
    Expression* operator()(Selector_List*);
    Expression* operator()(ComplexSelector*);
    Expression* operator()(Complex_Selector*);
    Expression* operator()(CompoundSelector*);
    Expression* operator()(Compound_Selector*);

    // generic fallback
    template <typename U>
    Expression* fallback(U x)
    { return Cast<Expression>(x); }
  };

}

#endif
