/*****************************************************************************/
/* Part of LibSass, released under the MIT license (See LICENSE.txt).        */
/*****************************************************************************/
#include "extension.hpp"

#include "callstack.hpp"
#include "ast_helpers.hpp"
#include "exceptions.hpp"

namespace Sass {

  /////////////////////////////////////////////////////////////////////////
  // Static function to create a copy with a new extender
  /////////////////////////////////////////////////////////////////////////
  Extension Extension::withExtender(const ComplexSelectorObj& newExtender) const
  {
    Extension extension(newExtender);
    extension.specificity = specificity;
    extension.isOptional = isOptional;
    extension.target = target;
    return extension;
  }

  /////////////////////////////////////////////////////////////////////////
  // Asserts that the [mediaContext] for a selector is
  // compatible with the query context for this extender.
  /////////////////////////////////////////////////////////////////////////
  void Extension::assertCompatibleMediaContext(CssMediaRuleObj mediaQueryContext, BackTraces& traces) const
  {

    if (this->mediaContext.isNull()) return;

    if (mediaQueryContext && mediaContext == mediaQueryContext) return;

    if (ObjEqualityFn<CssMediaRuleObj>(mediaQueryContext, mediaContext)) return;

    throw Exception::ExtendAcrossMedia(traces, *this);

  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

}
