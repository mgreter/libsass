// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include "ast_selectors.hpp"

namespace Sass {

  /*#########################################################################*/
  /*#########################################################################*/

  bool SelectorList::operator== (const SelectorList& rhs) const
  {
    if (&rhs == this) return true;
    if (rhs.length() != length()) return false;
    std::unordered_set<const ComplexSelector*, PtrObjHash, PtrObjEquality> lhs_set;
    lhs_set.reserve(length());
    for (const ComplexSelectorObj& element : elements()) {
      lhs_set.insert(element.ptr());
    }
    for (const ComplexSelectorObj& element : rhs.elements()) {
      if (lhs_set.find(element.ptr()) == lhs_set.end()) return false;
    }
    return true;
  }

  bool ExtendRule::operator== (const ExtendRule& rhs) const
  {
    return false;
  }

  /*#########################################################################*/
  // Compare SelectorList against all other selector types
  /*#########################################################################*/

  /*#########################################################################*/
  // Compare ComplexSelector against itself
  /*#########################################################################*/

  bool ComplexSelector::operator== (const ComplexSelector& rhs) const
  {
    size_t len = length();
    size_t rlen = rhs.length();
    if (len != rlen) return false;
    for (size_t i = 0; i < len; i += 1) {
      if (*get(i) != *rhs.get(i)) return false;
    }
    return true;
  }

  /*#########################################################################*/
  // Compare SelectorCombinator against itself
  /*#########################################################################*/

  bool SelectorCombinator::operator==(const SelectorCombinator& rhs) const
  {
    return combinator() == rhs.combinator();
  }

  /*#########################################################################*/
  // Compare SelectorCombinator against SelectorComponent
  /*#########################################################################*/

  bool SelectorCombinator::operator==(const SelectorComponent& rhs) const
  {
    if (const SelectorCombinator * sel = rhs.getCombinator()) {
      return *this == *sel;
    }
    return false;
  }

  bool CompoundSelector::operator==(const SelectorComponent& rhs) const
  {
    if (const CompoundSelector * sel = rhs.getCompound()) {
      return *this == *sel;
    }
    return false;
  }

  /*#########################################################################*/
  // Compare CompoundSelector against itself
  /*#########################################################################*/
  // ToDo: Verifiy implementation
  /*#########################################################################*/

  bool CompoundSelector::operator== (const CompoundSelector& rhs) const
  {
    // std::cerr << "comp vs comp\n";
    if (&rhs == this) return true;
    if (rhs.length() != length()) return false;
    std::unordered_set<const SimpleSelector*, PtrObjHash, PtrObjEquality> lhs_set;
    lhs_set.reserve(length());
    for (const SimpleSelectorObj& element : elements()) {
      lhs_set.insert(element.ptr());
    }
    // there is no break?!
    for (const SimpleSelectorObj& element : rhs.elements()) {
      if (lhs_set.find(element.ptr()) == lhs_set.end()) return false;
    }
    return true;
  }

  /*#########################################################################*/
  // Compare SimpleSelector against itself (upcast from abstract base)
  /*#########################################################################*/

  // DOES NOT EXIST FOR ABSTRACT BASE CLASS

  /*#########################################################################*/
  /*#########################################################################*/

  bool IDSelector::operator== (const SimpleSelector& rhs) const
  {
    auto sel = Cast<IDSelector>(&rhs);
    return sel ? *this == *sel : false;
  }

  bool TypeSelector::operator== (const SimpleSelector& rhs) const
  {
    auto sel = Cast<TypeSelector>(&rhs);
    return sel ? *this == *sel : false;
  }

  bool ClassSelector::operator== (const SimpleSelector& rhs) const
  {
    auto sel = Cast<ClassSelector>(&rhs);
    return sel ? *this == *sel : false;
  }

  bool PseudoSelector::operator== (const SimpleSelector& rhs) const
  {
    auto sel = Cast<PseudoSelector>(&rhs);
    return sel ? *this == *sel : false;
  }

  bool AttributeSelector::operator== (const SimpleSelector& rhs) const
  {
    auto sel = Cast<AttributeSelector>(&rhs);
    return sel ? *this == *sel : false;
  }

  bool PlaceholderSelector::operator== (const SimpleSelector& rhs) const
  {
    auto sel = Cast<PlaceholderSelector>(&rhs);
    return sel ? *this == *sel : false;
  }

  /*#########################################################################*/
  /*#########################################################################*/

  bool IDSelector::operator== (const IDSelector& rhs) const
  {
    // ID has no namespacing
    return name() == rhs.name();
  }

  bool TypeSelector::operator== (const TypeSelector& rhs) const
  {
    return is_ns_eq(rhs) && name() == rhs.name();
  }

  bool ClassSelector::operator== (const ClassSelector& rhs) const
  {
    // Class has no namespacing
    return name() == rhs.name();
  }

  bool PlaceholderSelector::operator== (const PlaceholderSelector& rhs) const
  {
    // Placeholder has no namespacing
    return name() == rhs.name();
  }

  bool AttributeSelector::operator== (const AttributeSelector& rhs) const
  {
    // smaller return, equal go on, bigger abort
    if (is_ns_eq(rhs)) {
      return op() == rhs.op()
        && name() == rhs.name()
        && value() == rhs.value();
    }
    else { return false; }
  }

  bool PseudoSelector::operator== (const PseudoSelector& rhs) const
  {
    if (is_ns_eq(rhs)) {
      if (name() != rhs.name()) return false;
      if (isElement() != rhs.isElement()) return false;
      if (argument() != rhs.argument()) return false;
      const SelectorList* lhs_sel = selector();
      const SelectorList* rhs_sel = rhs.selector();
      return PtrObjEquality()(lhs_sel, rhs_sel);
    }
    else { return false; }
  }

  /*#########################################################################*/
  /*#########################################################################*/

}
