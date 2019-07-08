#include "source_span.hpp"
#include "util_string.hpp"
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

  SourceSpan::SourceSpan(const char* label) :
    SourceState(SASS_MEMORY_NEW(
      SourceFile, label, "", sass::string::npos))
  {
  }

  const char* SourceSpan::end() const {
    return Offset::move(source->begin(), position + span);
  }

  SourceSpan SourceSpan::delta(const SourceSpan& lhs, const SourceSpan& rhs)
  {
    return SourceSpan(
      lhs.source, lhs.position,
      Offset::distance(lhs.position,
        rhs.position + rhs.span));
  }

  SourceSpan SourceSpan::delta(AST_Node* lhs, AST_Node* rhs)
  {
    return SourceSpan::delta(
      lhs->pstate(), rhs->pstate());
  }

}
