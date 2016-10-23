#ifndef SASS_LISTIZE_H
#define SASS_LISTIZE_H

#include <vector>
#include <iostream>

#include "ast.hpp"
#include "context.hpp"
#include "operation.hpp"
#include "environment.hpp"

namespace Sass {

  typedef Environment<AST_Node_Ptr> Env;
  struct Backtrace;

  class Listize : public Operation_CRTP<Expression_Ptr, Listize> {

    Memory_Manager& mem;

    Expression_Ptr fallback_impl(AST_Node_Ptr n);

  public:
    Listize(Memory_Manager&);
    ~Listize() { }

    Expression_Ptr operator()(CommaSequence_Selector_Ptr);
    Expression_Ptr operator()(Sequence_Selector_Ptr);
    Expression_Ptr operator()(SimpleSequence_Selector_Ptr);

    template <typename U>
    Expression_Ptr fallback(U x) { return fallback_impl(x); }
  };

}

#endif
