#ifndef SASS_EXTENSION_H
#define SASS_EXTENSION_H

#include "ast_fwd_decl.hpp"
#include "tsl/ordered_map.h"
#include "tsl/ordered_set.h"

namespace Sass {

  class Extension;

  bool HashExtensionFn(const Extension& extension);

  bool CompareExtensionFn(const Extension& lhs, const Extension& rhs);

  class Extension {

  public:

    // The selector in which the `@extend` appeared.
    ComplexSelector_Obj extender;

    // The selector that's being extended.
    // `null` for one-off extensions.
    Simple_Selector_Obj target;

    // The minimum specificity required for any
    // selector generated from this extender.
    size_t specificity;

    // Whether this extension is optional.
    bool isOptional;

    // Whether this is a one-off extender representing a selector that was
    // originally in the document, rather than one defined with `@extend`.
    bool isOriginal;

    bool isSatisfied;

    // The media query context to which this extend is restricted,
    // or `null` if it can apply within any context.
    Media_Block_Obj mediaContext;

    // ExtendRule(ComplexSelector_Obj extender, Simple_Selector_Obj )


    // Creates a one-off extension that's not intended to be modified over time.
    // If [specificity] isn't passed, it defaults to `extender.maxSpecificity`.
    Extension(ComplexSelector_Obj extender) :
      extender(extender),
      target({}),
      specificity(0),
      isOptional(true),
      isOriginal(false),
      isSatisfied(false),
      mediaContext({}) {

    }

    // Copy constructor
    Extension(const Extension& extension) :
      extender(extension.extender),
      target(extension.target),
      specificity(extension.specificity),
      isOptional(extension.isOptional),
      isOriginal(extension.isOriginal),
      isSatisfied(extension.isSatisfied),
      mediaContext(extension.mediaContext) {

    }

    Extension() :
      extender({}),
      target({}),
      specificity(0),
      isOptional(false),
      isOriginal(false),
      isSatisfied(false),
      mediaContext({}) {
    }


    /// Asserts that the [mediaContext] for a selector is compatible with the
    /// query context for this extender.
    void assertCompatibleMediaContext(Media_Block_Obj mediaContext);

    Extension withExtender(ComplexSelector_Obj newExtender);

    size_t operator() (const Extension& extension) const {
      return HashExtensionFn(extension);
    }

    bool operator() (const Extension& lhs, const Extension& rhs) const {
      return CompareExtensionFn(lhs, rhs);
    }

    bool operator== (const Extension& rhs) const;

    size_t hash() const;

  };

  typedef std::unordered_set<Extension> ExtensionSet;

}

// Add global hash function
namespace std {
  template<> class
    hash<Sass::Extension> {
  public:
    size_t operator()(const Sass::Extension& extension) const {
      return extension.hash();
    }
  };
}

#endif
