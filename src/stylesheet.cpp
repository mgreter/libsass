#include "stylesheet.hpp"

namespace Sass {

  Root::Root(const SourceSpan& pstate, size_t reserve)
    : AstNode(pstate), Vectorized<Statement>(reserve)
  {}
  Root::Root(const SourceSpan& pstate, StatementVector&& vec)
    : AstNode(pstate), Vectorized<Statement>(std::move(vec))
  {}
}
