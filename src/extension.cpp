// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include "ast_helpers.hpp"
#include "extension.hpp"
#include "ast.hpp"

namespace Sass {

  // ##########################################################################
  // Static function to create a copy with a new extender
  // ##########################################################################
  Extension Extension::withExtender(const ComplexSelectorObj& newExtender) const
  {
    Extension extension(newExtender);
    extension.specificity = specificity;
    extension.isOptional = isOptional;
    extension.target = target;
    return extension;
  }

  // ##########################################################################
  // Asserts that the [mediaContext] for a selector is
  // compatible with the query context for this extender.
  // ##########################################################################
  void Extension::assertCompatibleMediaContext(CssMediaRuleObj mediaQueryContext, Backtraces& traces) const
  {
    // callStackFrame inner2(traces, Backtrace(target->pstate()));

    if (this->mediaContext.isNull()) return;

    if (mediaQueryContext && mediaContext == mediaQueryContext) return;

    if (ObjEqualityFn<CssMediaRuleObj>(mediaQueryContext, mediaContext)) return;

    // callStackFrame frame(traces, Backtrace(mediaContext->pstate()));
    // callStackFrame outer(traces, Backtrace(extender->pstate()));
    // callStackFrame inner(traces, Backtrace(target->pstate()));
    throw Exception::ExtendAcrossMedia(traces, *this);

  }

  // ##########################################################################
  // ##########################################################################

}
