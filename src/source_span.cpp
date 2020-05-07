#include "source_span.hpp"
#include "source.hpp"
#include "ast.hpp"

namespace Sass {

  SourceSpan::SourceSpan(
    SourceDataObj source,
    const Offset& position,
    const Offset& span) :
    SourceState(source, position),
    span(span)
  {}

  SourceSpan SourceSpan::tmp(const char* label)
  {
    return SourceSpan(SASS_MEMORY_NEW(
      SourceString, "sass://phony", label),
    {},
    {}
    );

  }

  const char* SourceSpan::end() const {
    return Offset::move(getContent(), position + span);
  }

  SourceSpan SourceSpan::delta(const SourceSpan& lhs, const SourceSpan& rhs)
  {
    return SourceSpan(
      lhs.getSource(), lhs.position,
      Offset::distance(lhs.position,
        rhs.position + rhs.span));
  }

  SourceSpan SourceSpan::delta(AST_Node* lhs, AST_Node* rhs)
  {
    return SourceSpan::delta(
      lhs->pstate(), rhs->pstate());
  }

}
