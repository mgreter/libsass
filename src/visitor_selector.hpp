#ifndef SASS_VISITOR_SELECTOR_H
#define SASS_VISITOR_SELECTOR_H

#include "ast_fwd_decl.hpp"

namespace Sass {

  // An interface for [visitors][] that traverse selectors.
  // [visitors]: https://en.wikipedia.org/wiki/Visitor_pattern

  template <typename T>
  class SelectorVisitor {

  public:

    virtual T visitAttributeSelector(AttributeSelector* attribute) = 0;
    virtual T visitClassSelector(ClassSelector* klass) = 0;
    virtual T visitComplexSelector(ComplexSelector* complex) = 0;
    virtual T visitCompoundSelector(CompoundSelector* compound) = 0;
    virtual T visitSelectorCombinator(SelectorCombinator* combinator) = 0;
    virtual T visitIDSelector(IDSelector* id) = 0;
    virtual T visitPlaceholderSelector(PlaceholderSelector* placeholder) = 0;
    virtual T visitPseudoSelector(PseudoSelector* pseudo) = 0;
    virtual T visitSelectorList(SelectorList* list) = 0;
    virtual T visitTypeSelector(TypeSelector* type) = 0;
    // virtual T visitParentSelector(Parent_Selector* placeholder) = 0;
    // virtual T visitUniversalSelector(UniversalSelector* universal) = 0;

  };

}

#endif
