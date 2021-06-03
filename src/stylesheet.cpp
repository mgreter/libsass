/*****************************************************************************/
/* Part of LibSass, released under the MIT license (See LICENSE.txt).        */
/*****************************************************************************/
#include "stylesheet.hpp"

namespace Sass {

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Root::Root(const SourceSpan& pstate, size_t reserve)
    : AstNode(pstate), Vectorized<Statement>(reserve), Module(nullptr)
  {}

  Root::Root(const SourceSpan& pstate, StatementVector&& vec)
    : AstNode(pstate), Vectorized<Statement>(std::move(vec)), Module(nullptr)
  {}

  void Root::addExtension(
    const SelectorListObj& extend,
    const SimpleSelectorObj& target,
    const CssMediaRuleObj& mediaQueryContext,
    bool is_optional)
  {
    for (Root* mod : upstream) {
      mod->addExtension(extend, target, mediaQueryContext, is_optional);
    }
    if (extender) extender->addExtension(extend, target, mediaQueryContext, is_optional);
  }


  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

}
