// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include "extend.hpp"
#include "extension.hpp"
#include "context.hpp"
#include "backtrace.hpp"
#include "paths.hpp"
#include "parser.hpp"
#include "expand.hpp"
#include "node.hpp"
#include "sass_util.hpp"
#include "remove_placeholders.hpp"
#include "debugger.hpp"
#include "debug.hpp"
#include <iostream>
#include <deque>
#include <set>

namespace Sass {

  // Static function to create a copy with a new extender
  Extension Extension::withExtender(ComplexSelector_Obj newExtender)
  {
    Extension extension(newExtender);
    extension.specificity = specificity;
    extension.isOptional = isOptional;
    extension.target = target;
    return extension;
  }

  size_t Extension::hash() const {
    size_t hash = 0;
    if (target) hash_combine(hash, target->hash());
    if (extender) hash_combine(hash, extender->hash());
    return hash;
  }

  bool HashExtensionFn(const Extension& extension) {
    size_t hash = extension.extender->hash();
    hash_combine(hash, extension.target->hash());
    return hash;
  }

  bool CompareExtensionFn(const Extension& lhs, const Extension& rhs) {
    return CompareNodesFn(lhs.extender, rhs.extender)
      && CompareNodesFn(lhs.target, rhs.target);
  }

  bool Extension::operator== (const Extension& rhs) const {
    return CompareNodesFn(extender, rhs.extender);
  }

  // Asserts that the [mediaContext] for a selector is
  // compatible with the query context for this extender.
  void Extension::assertCompatibleMediaContext(Media_Block_Obj mediaContext) {
    /*
    if (this.mediaContext == null) return;
    if (mediaContext != null && listEquals(this.mediaContext, mediaContext))
      return;

    throw SassException(
      "You may not @extend selectors across media queries.", span);
      */
  }

}
