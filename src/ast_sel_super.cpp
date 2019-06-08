#include "sass.hpp"
#include "ast.hpp"
#include "context.hpp"
#include "node.hpp"
#include "eval.hpp"
#include "extend.hpp"
#include "emitter.hpp"
#include "color_maps.hpp"
#include "ast_fwd_decl.hpp"
#include <set>
#include <queue>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <functional>
#include <cctype>
#include <locale>

#include "ast_fwd_decl.hpp"
#include "ast_selectors.hpp"
#include "prelexer.hpp"
#include "debugger.hpp"

namespace Sass {

  bool listIsSuperslector(
    std::vector<ComplexSelector_Obj> list1,
    std::vector<ComplexSelector_Obj> list2);

    // Returns all pseudo selectors in [compound] that have
  // a selector argument, and that have the given [name].
  std::vector<Pseudo_Selector_Obj> selectorPseudoNamed(
    CompoundSelector_Obj compound, std::string name)
  {
    std::vector<Pseudo_Selector_Obj> rv;
    for (Simple_Selector_Obj sel : compound->elements()) {
      if (Pseudo_Selector_Obj pseudo = Cast<Pseudo_Selector>(sel)) {
        if (/* pseudo->isClass() && */ pseudo->selector2()) {
          if (normName(sel->name()) == normName(name)) {
            rv.push_back(sel);
          }
        }
      }
    }
    return rv;
  }

  /// For example, `.foo` is a superselector of `:matches(.foo)`.
  bool isSubselectorPseudo(const std::string& norm)
  {
    return equalsLiteral("any", norm)
      || equalsLiteral("matches", norm)
      || equalsLiteral("nth-child", norm)
      || equalsLiteral("nth-last-child", norm);
  }


  bool simpleIsSuperselectorOfSimple(
    Simple_Selector_Obj simple, Simple_Selector_Obj theirSimple)
  {

    if (*simple == *theirSimple) return true;

    if (Pseudo_Selector_Obj pseudo = Cast<Pseudo_Selector>(theirSimple)) {
      if (pseudo->selector2() && isSubselectorPseudo(pseudo->normalized())) {
        for (auto cpx : pseudo->selector2()->elements()) {
          ComplexSelector_Obj complex = toComplexSelector(cpx);
          // Make sure we have exacly one items
          if (complex->length() != 1) return false;
          // That items must be a compound selector
          if (auto compound = Cast<CompoundSelector>(complex->at(0))) {
            // It must contain the lhs simple selector
            if (!compound->contains(simple)) return false;
          }
        }
        return true;
      }
    }
    return false;
  }

  // Returns whether [simple] is a superselector of [compound].
  // That is, whether [simple] matches every element that
  // [compound] matches, as well as possibly additional elements.
  bool simpleIsSuperselectorOfCompound(
    Simple_Selector_Obj simple, CompoundSelector_Obj compound)
  {
    for (Simple_Selector_Obj simple2 : compound->elements()) {
      if (simpleIsSuperselectorOfSimple(simple, simple2)) {
        return true;
      }
    }
    return false;
  }



  bool typeIsSuperselectorOfCompound(
    Type_Selector_Obj type,
    CompoundSelector_Obj compound)
  {
    for (Simple_Selector_Obj simple : compound->elements()) {
      if (Type_Selector_Obj rhs = Cast<Type_Selector>(rhs)) {
        if (*type != *rhs) return true;
      }
    }
    return false;
  }

  bool idIsSuperselectorOfCompound(
    Id_Selector_Obj id,
    CompoundSelector_Obj compound)
  {
    for (Simple_Selector_Obj simple : compound->elements()) {
      if (Id_Selector_Obj rhs = Cast<Id_Selector>(rhs)) {
        if (*id != *rhs) return true;
      }
    }
    return false;
  }

  bool pseudoIsSuperselectorOfPseudo(
    Pseudo_Selector_Obj pseudo1,
    Pseudo_Selector_Obj pseudo2,
    ComplexSelector_Obj parent
  )
  {
    if (pseudo1->name() == pseudo2->name() && pseudo2->selector2()) {
      SelectorList_Obj list = toSelectorList(pseudo2->selector2());
      // // std::cerr << "GO INTO OTHER LOOP " << debug_vec(list) << " [" << debug_vec(parent) << "]\n";
      return listIsSuperslector(list->elements(), { parent });
    }
    return false;
  }

  bool pseudoNotIsSuperselectorOfCompound(
    Pseudo_Selector_Obj pseudo1,
    CompoundSelector_Obj compound2,
    ComplexSelector_Obj parent)
  {
    // return compound2.components.any((simple2) ->
    for (Simple_Selector_Obj simple2 : compound2->elements()) {
      if (Type_Selector_Obj type2 = Cast<Type_Selector>(simple2)) {
        if (CompoundSelector_Obj compound1 = Cast<CompoundSelector>(parent->last())) {
          if (typeIsSuperselectorOfCompound(type2, compound1)) return true;
        }
      }
      else if (Id_Selector_Obj id2 = Cast<Id_Selector>(simple2)) {
        if (CompoundSelector_Obj compound1 = Cast<CompoundSelector>(parent->last())) {
          if (idIsSuperselectorOfCompound(id2, compound1)) return true;
        }
      }
      else if (Pseudo_Selector_Obj pseudo2 = Cast<Pseudo_Selector>(simple2)) {
        if (pseudoIsSuperselectorOfPseudo(pseudo1, pseudo2, parent)) return true;
      }
        // pseudoIsSuperselectorOfPseudo
    }
    return false;
  }

  // Returns whether [pseudo1] is a superselector of [compound2].
  // That is, whether [pseudo1] matches every element that [compound2]
  // matches, as well as possibly additional elements. This assumes that
  // [pseudo1]'s `selector` argument is not `null`. If [parents] is passed,
  // it represents the parents of [compound2]. This is relevant for pseudo
  // selectors with selector arguments, where we may need to know if the
  // parent selectors in the selector argument match [parents].
  bool selectorPseudoIsSuperselector(
    Pseudo_Selector_Obj pseudo1, CompoundSelector_Obj compound2,
    std::vector<CompoundOrCombinator_Obj>::iterator parents_from,
    std::vector<CompoundOrCombinator_Obj>::iterator parents_to)
  {

    std::string name(pseudo1->name());
    name = normName(name);

    if (name == "matches" || name == "any") {

      std::vector<Pseudo_Selector_Obj> pseudos =
        selectorPseudoNamed(compound2, name);
      SelectorList_Obj selector1 = toSelectorList(pseudo1->selector2());
      for (Pseudo_Selector_Obj pseudo2 : pseudos) {
        SelectorList_Obj selector2 = toSelectorList(pseudo2->selector2());
        if (selector1->isSuperselectorOf(selector2)) {
          return true;
        }
      }

      for (ComplexSelector_Obj complex1 : selector1->elements()) {
        std::vector<CompoundOrCombinator_Obj> parents;
        for (auto cur = parents_from; cur != parents_to; cur++) {
          parents.push_back(*cur);
        }
        parents.push_back(compound2);
        if (complexIsSuperselector(complex1->elements(), parents)) {
          return true;
        }
      }

    }
    else if (name == "has" || name == "host" || name == "host-context" || name == "slotted") {
      // // std::cerr << "IN HAS\n";
      std::vector<Pseudo_Selector_Obj> pseudos =
        selectorPseudoNamed(compound2, name);
      // // std::cerr << "SELS [" << debug_vec(pseudos) << "]\n";
      SelectorList_Obj selector1 = toSelectorList(pseudo1->selector2());
      for (Pseudo_Selector_Obj pseudo2 : pseudos) {
        SelectorList_Obj selector2 = toSelectorList(pseudo2->selector2());
        if (selector1->isSuperselectorOf(selector2)) {
          // // std::cerr << "GOT true\n";
          return true;
        }
      }

    }
    else if (name == "not") {
      // return pseudo1.selector.components.every((complex) ->
      for (ComplexSelector_Obj complex : pseudo1->selector2()->elements()) {
        if (!pseudoNotIsSuperselectorOfCompound(pseudo1, compound2, complex)) return false;
      }
      return true;
      // // std::cerr << "IN NOT\n";


    }
    else if (name == "current") {
      std::vector<Pseudo_Selector_Obj> pseudos =
        selectorPseudoNamed(compound2, ":current");
      for (Pseudo_Selector_Obj pseudo2 : pseudos) {
        if (*pseudo1 == *pseudo2) return true;
      }

    }
    else if (name == "nth-child" || name == "nth-last-child") {

    }

    return false;

  }

  // Returns whether [compound1] is a superselector of [compound2].
  // That is, whether [compound1] matches every element that [compound2]
  // matches, as well as possibly additional elements. If [parents] is
  // passed, it represents the parents of [compound2]. This is relevant
  // for pseudo selectors with selector arguments, where we may need to
  // know if the parent selectors in the selector argument match [parents].
  bool compoundIsSuperselector(
    CompoundSelector_Obj compound1, CompoundSelector_Obj compound2,
    std::vector<CompoundOrCombinator_Obj>::iterator parents_from,
    std::vector<CompoundOrCombinator_Obj>::iterator parents_to) {

    // // std::cerr << "CMP SUP [" << std::string(compound1) << "/" << std::string(compound2) << "]\n";

    // Every selector in [compound1.components] must have
    // a matching selector in [compound2.components].
    for (Simple_Selector_Obj simple1 : compound1->elements()) {
      Pseudo_Selector_Obj pseudo1 = Cast<Pseudo_Selector>(simple1);
      if (pseudo1 && pseudo1->selector2()) {
        // // std::cerr << "NOW WE HAVE A PSEUO WITH SEL\n";
        if (!selectorPseudoIsSuperselector(pseudo1, compound2, parents_from, parents_to)) {
          // // std::cerr << "selectorPseudoIsSuperselector FALSE\n";
          return false;
        }
      }
      else if (!simpleIsSuperselectorOfCompound(simple1, compound2)) {
        // // std::cerr << "is not simple 0\n";
        return false;
      }
    }

    // [compound1] can't be a superselector of a selector
    // with pseudo-elements that [compound2] doesn't share.
    for (Simple_Selector_Obj simple2 : compound2->elements()) {
      Pseudo_Selector_Obj pseudo2 = Cast<Pseudo_Selector>(simple2);
      // if (pseudo2) // // std::cerr << "IS A PSEUDO\n";
      if (pseudo2 && pseudo2->isElement()) {
        // if (pseudo2) // // std::cerr << "AND IS ELEMENT\n";
        if (!simpleIsSuperselectorOfCompound(pseudo2, compound1)) {
          // // std::cerr << "is not simple 1\n";
          return false;
        }
      }
      else {
        // // std::cerr << "IS NOT PSEUDO NOR ELEMENT\n";
      }
    }

    // // std::cerr << "CMP OK - RET TRUE\n";
    return true;
  }

  bool compoundIsSuperselector(
    CompoundSelector_Obj compound1,
    CompoundSelector_Obj compound2,
    std::vector<CompoundOrCombinator_Obj> parents)
  {
    bool rv = compoundIsSuperselector(
      compound1, compound2,
      parents.begin(), parents.end()
    );
    // if (compound1->empty()) return true;
    // if (compound2->empty()) return false;
    // // std::cerr << "SUPI " << debug_vec(compound1) << " vs " << debug_vec(compound2) << " => " << (rv ? "true" : "false") << "\n";
    return rv;
  }

  bool complexIsSuperselector(std::vector<CompoundOrCombinator_Obj> complex1,
    std::vector<CompoundOrCombinator_Obj> complex2) {
    
    // // std::cerr << "====================================================\n";
    // // std::cerr << "complexIsSuperselector " << debug_vec(complex1) << " vs " << debug_vec(complex2) << "\n";

    // Selectors with trailing operators are neither superselectors nor subselectors.
    if (!complex1.empty() && Cast<SelectorCombinator>(complex1.back())) return false;
    if (!complex2.empty() && Cast<SelectorCombinator>(complex2.back())) return false;

    size_t i1 = 0, i2 = 0;
    while (true) {

      // // std::cerr << "Run " << i1 << "/" << i2 << "\n";
      size_t remaining1 = complex1.size() - i1;
      size_t remaining2 = complex2.size() - i2;
      // // std::cerr << "Rem " << remaining1 << "/" << remaining2 << "\n";

      if (remaining1 == 0 || remaining2 == 0) {
        // // std::cerr << "Check complex 0\n";
        return false;
      }
      // More complex selectors are never
      // superselectors of less complex ones.
      if (remaining1 > remaining2) {
        // // std::cerr << "Check complex 1\n";
        return false;
      }

      // Selectors with leading operators are
      // neither superselectors nor subselectors.
      if (Cast<SelectorCombinator>(complex1[i1])) {
        // // std::cerr << "Check complex 2 [" << std::string(complex1[i1]) << "]\n";
        return false;
      }
      if (Cast<SelectorCombinator>(complex2[i2])) {
        // // std::cerr << "Check complex 3 [" << std::string(complex2[i2]) << "]\n";
        return false;
      }

      CompoundSelector_Obj compound1 = Cast<CompoundSelector>(complex1[i1]);
      CompoundSelector_Obj compound2 = Cast<CompoundSelector>(complex2.back());

      // // std::cerr << "DO [" << std::string(compound1) << "/" << std::string(compound2) << "]\n";

      if (remaining1 == 1) {
        std::vector<CompoundOrCombinator_Obj>::iterator parents_to = complex2.end();
        std::vector<CompoundOrCombinator_Obj>::iterator parents_from = complex2.begin();
        std::advance(parents_from, i2 + 1); // equivalent to dart `.skip(i2 + 1)`
        bool rv = compoundIsSuperselector(compound1, compound2, parents_from, parents_to);
        std::vector<CompoundOrCombinator_Obj> pp;

        std::vector<CompoundOrCombinator_Obj>::iterator end = parents_to;
        std::vector<CompoundOrCombinator_Obj>::iterator beg = parents_from;
        while (beg != end) {
          pp.push_back(*beg);
          beg++;
        }

        // // std::cerr << "compoundIsSuperselector" << debug_vec(compound1) << " vs " << debug_vec(compound2) << " with [" << debug_vec(pp) << "]\n";
        // // std::cerr << "RETURN REM1 " << (rv ? "true" : "false") << "\n";
        return rv;
      }

      // Find the first index where `complex2.sublist(i2, afterSuperselector)`
      // is a subselector of [compound1]. We stop before the superselector
      // would encompass all of [complex2] because we know [complex1] has 
      // more than one element, and consuming all of [complex2] wouldn't 
      // leave anything for the rest of [complex1] to match.
      size_t afterSuperselector = i2 + 1;
      for (; afterSuperselector < complex2.size(); afterSuperselector++) {
        CompoundOrCombinator_Obj component2 = complex2[afterSuperselector - 1];
        if (CompoundSelector_Obj compound2 = Cast<CompoundSelector>(component2)) {
          std::vector<CompoundOrCombinator_Obj>::iterator parents_to = complex2.begin();
          std::vector<CompoundOrCombinator_Obj>::iterator parents_from = complex2.begin();
          // complex2.take(afterSuperselector - 1).skip(i2 + 1)
          std::advance(parents_from, i2 + 1); // equivalent to dart `.skip`
          std::advance(parents_to, afterSuperselector); // equivalent to dart `.take`
          if (compoundIsSuperselector(compound1, compound2, parents_from, parents_to)) {
            // // std::cerr << "CMP BREAK\n";
            break;
          }
        }
      }
      if (afterSuperselector == complex2.size()) {
        // // std::cerr << "RETURN AFTER SUP\n";
        return false;
      }

      CompoundOrCombinator_Obj component1 = complex1[i1 + 1],
        component2 = complex2[afterSuperselector];

      SelectorCombinator_Obj combinator1 = Cast<SelectorCombinator>(component1);
      SelectorCombinator_Obj combinator2 = Cast<SelectorCombinator>(component2);

      // // std::cerr << "COMBIN [" << std::string(combinator1) << "/" << std::string(combinator2) << "]\n";

      if (!combinator1.isNull()) {

        if (combinator2.isNull()) {
          // // std::cerr << "COMB2 NULL\n";
          return false;
        }
        // `.a ~ .b` is a superselector of `.a + .b`,
        // but otherwise the combinators must match.
        if (combinator1->isFollowingSibling()) {
          if (combinator2->isChildCombinator()) {
            // // std::cerr << "COMB2 IS CHILD\n";
            return false;
          }
        }
        else if (*combinator1 != *combinator2) {
          // // std::cerr << "COMB NO EQUAL\n";
          return false;
        }

        // `.foo > .baz` is not a superselector of `.foo > .bar > .baz` or
        // `.foo > .bar .baz`, despite the fact that `.baz` is a superselector of
        // `.bar > .baz` and `.bar .baz`. Same goes for `+` and `~`.
        if (remaining1 == 3 && remaining2 > 3) {
          // // std::cerr << "COMB STRANGE\n";
          return false;
        }

        // // std::cerr << "INCREMENT 1\n";
        i1 += 2; i2 = afterSuperselector + 1;

      }
      else if (!combinator2.isNull()) {
        if (!combinator2->isChildCombinator()) {
          // // std::cerr << "RET INC 2\n";
          return false;
        }
        // // std::cerr << "INCREMENT 2\n";
        i1 += 1; i2 = afterSuperselector + 1;
      }
      else {
        // // std::cerr << "INCREMENT 3\n";
        i1 += 1; i2 = afterSuperselector;
      }
    }


    return false;

  }

  // Like [complexIsSuperselector], but compares [complex1] and [complex2] as
  // though they shared an implicit base [SimpleSelector].
  //
  // For example, `B` is not normally a superselector of `B A`, since it doesn't
  // match elements that match `A`. However, it *is* a parent superselector,
  // since `B X` is a superselector of `B A X`.
  bool complexIsParentSuperselector(std::vector<CompoundOrCombinator_Obj> complex1,
    std::vector<CompoundOrCombinator_Obj> complex2) {
    // Try some simple heuristics to see if we can avoid allocations.
    if (complex1.empty() && complex2.empty()) return false;
    if (Cast<SelectorCombinator>(complex1.front())) return false;
    if (Cast<SelectorCombinator>(complex2.front())) return false;
    if (complex1.size() > complex2.size()) return false;
    // // std::cerr << "HERE 1\n";
    CompoundSelector_Obj base = SASS_MEMORY_NEW(CompoundSelector, ParserState("[tmp]"));
    ComplexSelector_Obj cplx1 = SASS_MEMORY_NEW(ComplexSelector, ParserState("[tmp]"));
    ComplexSelector_Obj cplx2 = SASS_MEMORY_NEW(ComplexSelector, ParserState("[tmp]"));
    // // std::cerr << "HERE 2\n";
    cplx1->concat(complex1); cplx1->append(base);
    // // std::cerr << "HERE 3\n";
    cplx2->concat(complex2); cplx2->append(base);
    // // std::cerr << "HERE 4\n";
    return cplx1->isSuperselectorOf(cplx2);
  }

  std::vector<std::vector<CompoundOrCombinator_Obj>> unifyComplex(
    std::vector<std::vector<CompoundOrCombinator_Obj>> complexes) {

    SASS_ASSERT(!complexes.empty(), "Can't unify empty list");
    if (complexes.size() == 1) return complexes;

    // std::cerr << "UNIFY COMPLEX " << debug_vec(complexes) << "\n";

    CompoundSelector_Obj unifiedBase = SASS_MEMORY_NEW(CompoundSelector, ParserState("[tmp]"));
    for (auto complex : complexes) {
      CompoundOrCombinator_Obj base = complex.back();
      if (CompoundSelector * comp = base->getCompound()) {
        if (unifiedBase->empty()) {
          unifiedBase->concat(comp);
          // std::cerr << "base is empty " << debug_vec(unifiedBase) << "\n";
        }
        else {
          // std::cerr << "base has some\n";
          for (Simple_Selector_Obj simple : comp->elements()) {
            // std::cerr << "UNIFY " << debug_vec(simple) << "\n";
            unifiedBase = simple->unifyWith(unifiedBase);
            // std::cerr << "REC UNIFIED " << debug_vec(unifiedBase) << "\n";
            if (unifiedBase.isNull()) return {};
          }
        }
      }
      else {
        return {};
      }
    }
    // std::cerr << "unifiedBase " << debug_vec(unifiedBase) << "\n";

    // std::cerr << "complexes " << debug_vec(complexes) << "\n";

    std::vector<std::vector<CompoundOrCombinator_Obj>> complexesWithoutBases;
    for (size_t i = 0; i < complexes.size(); i += 1) {
      std::vector<CompoundOrCombinator_Obj> sel = complexes.at(i);
      sel.pop_back(); // remove last item (base) from the list
      complexesWithoutBases.push_back(sel);
    }

    // std::cerr << "complexesWithoutBases " << debug_vec(complexesWithoutBases) << "\n";

    // [[], [*]]
    // // std::cerr << "here 2\n";
    complexesWithoutBases.back().push_back(unifiedBase);

    // std::cerr << "BEFORE WEAVE " << debug_vec(complexesWithoutBases) << "\n";

    auto rv = weave(complexesWithoutBases);

    // std::cerr << "AFTER WEAVE " << debug_vec(rv) << "\n";

    // debug_ast(rv, "WEAVE: ");

    return rv;

  }

  bool listIsSuperslectorOfComplex(
    std::vector<ComplexSelector_Obj> list,
    ComplexSelector_Obj complex)
  {
    // Return true if one matches
    for (ComplexSelector_Obj lhs : list) {
      if (lhs->isSuperselectorOf(complex)) {
        return true;
      }
    }
    return false;
  }

  bool listIsSuperslector(
    std::vector<ComplexSelector_Obj> list1,
    std::vector<ComplexSelector_Obj> list2)
  {
    // Return true if every one matches
    for (ComplexSelector_Obj complex1 : list2) {
      if (!listIsSuperslectorOfComplex(list1, complex1)) {
        return false;
      }
    }
    return true;
  }


  bool ComplexSelector::isSuperselectorOf(const ComplexSelector* sub) const
  {
    // // std::cerr << "Check complexIsSuperselector\n";
    return complexIsSuperselector(elements(), sub->elements());
  }

  bool SelectorList::isSuperselectorOf(const SelectorList* sub) const
  {
    // // std::cerr << "Check listIsSuperslector\n";
    return listIsSuperslector(elements(), sub->elements());
  }

  size_t SelectorList::maxSpecificity() const
  {
    size_t specificity = 0;
    for (auto complex : elements()) {
      specificity = std::max(specificity, complex->maxSpecificity());
    }
    return specificity;
  }

  size_t SelectorList::minSpecificity() const
  {
    size_t specificity = 0;
    for (auto complex : elements()) {
      specificity = std::min(specificity, complex->minSpecificity());
    }
    return specificity;
  }



  size_t CompoundSelector::maxSpecificity() const
  {
    size_t specificity = 0;
    for (auto simple : elements()) {
      specificity += simple->maxSpecificity();
    }
    return specificity;
  }

  size_t CompoundSelector::minSpecificity() const
  {
    size_t specificity = 0;
    for (auto simple : elements()) {
      specificity += simple->minSpecificity();
    }
    return specificity;
  }

  size_t ComplexSelector::maxSpecificity() const
  {
    size_t specificity = 0;
    for (auto component : elements()) {
      specificity += component->maxSpecificity();
    }
    return specificity;
  }

  size_t ComplexSelector::minSpecificity() const
  {
    size_t specificity = 0;
    for (auto component : elements()) {
      specificity += component->minSpecificity();
    }
    return specificity;
  }

}
