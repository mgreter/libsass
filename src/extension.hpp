#ifndef SASS_EXTENSION_H
#define SASS_EXTENSION_H

#include "ast_fwd_decl.hpp"

namespace Sass {

  class Extension2 {

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

    // The media query context to which this extend is restricted,
    // or `null` if it can apply within any context.
    // std::vector<Media_Query_Obj> mediaContext;

    // ExtendRule(ComplexSelector_Obj extender, Simple_Selector_Obj )


    // Creates a one-off extension that's not intended to be modified over time.
    // If [specificity] isn't passed, it defaults to `extender.maxSpecificity`.
    Extension2(ComplexSelector_Obj extender) :
      extender(extender),
      target({}),
      specificity(0),
      isOptional(true),
      isOriginal(false) {

    }

    // Copy constructor
    Extension2(const Extension2& extension) :
      extender(extension.extender),
      target(extension.target),
      specificity(extension.specificity),
      isOptional(extension.isOptional),
      isOriginal(extension.isOriginal) {

    }

    Extension2() :
      extender({}),
      target({}),
      specificity(0),
      isOptional(false),
      isOriginal(false) {

    }


    Extension2 withExtender(ComplexSelector_Obj newExtender);


  };

}

#endif
