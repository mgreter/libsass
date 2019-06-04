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
#include "debugger.hpp"

// #define DEBUG_UNIFY

namespace Sass {

  std::vector<std::vector<CompoundOrCombinator_Obj>> weave(
    std::vector<std::vector<CompoundOrCombinator_Obj>> complexes);














  /*
  Compound_Selector* Compound_Selector::unify_with(Compound_Selector* rhs)
  {
    CompoundSelector_Obj asd = toCompoundSelector(this);
    CompoundSelector_Obj qwe = toCompoundSelector(rhs);
    CompoundSelector_Obj sel = asd->unify_with(qwe);
    return toCompound_Selector(sel).detach();
  }
  */

  CompoundSelector* CompoundSelector::unify_with(CompoundSelector* rhs)
  {
#ifdef DEBUG_UNIFY
    const std::string debug_call = "unify(Compound[" + this->to_string() + "], Compound[" + rhs->to_string() + "])";
    std::cerr << debug_call << std::endl;
#endif

    if (empty()) return rhs;
    CompoundSelector_Obj unified = SASS_MEMORY_COPY(rhs);
    for (const Simple_Selector_Obj& sel : elements()) {
      CompoundSelector_Obj cp = unified;
      unified = sel->unifyWith(unified);
      if (unified.isNull()) break;
    }

#ifdef DEBUG_UNIFY
    std::cerr << "> " << debug_call << " = Compound[" << unified->to_string() << "]" << std::endl;
#endif
    return unified.detach();
  }

  Compound_Selector* Simple_Selector::unify_with(Compound_Selector* rhs)
  {
    return unifyWith(rhs->toCompSelector())->toCompoundSelector().detach();
  }

  CompoundSelector* Simple_Selector::unifyWith(CompoundSelector* rhs)
  {
#ifdef DEBUG_UNIFY
    const std::string debug_call = "unify(Simple[" + this->to_string() + "], Compound[" + rhs->to_string() + "])";
    std::cerr << debug_call << std::endl;
#endif

    if (rhs->length() == 1) {
      if (rhs->at(0)->is_universal()) {
        CompoundSelector* this_compound = SASS_MEMORY_NEW(CompoundSelector, pstate());
        this_compound->append(SASS_MEMORY_COPY(this));
        CompoundSelector* unified = rhs->at(0)->unifyWith(this_compound);
        if (unified == nullptr || unified != this_compound) delete this_compound;

#ifdef DEBUG_UNIFY
        std::cerr << "> " << debug_call << " = " << "Compound[" << unified->to_string() << "]" << std::endl;
#endif
        return unified;
      }
    }
    for (const Simple_Selector_Obj& sel : rhs->elements()) {
      if (*this == *sel) {
#ifdef DEBUG_UNIFY
        std::cerr << "> " << debug_call << " = " << "Compound[" << rhs->to_string() << "]" << std::endl;
#endif
        return rhs;
      }
    }
    const int lhs_order = this->unification_order();
    size_t i = rhs->length();
    while (i > 0 && lhs_order < rhs->at(i - 1)->unification_order()) --i;
    rhs->insert(rhs->begin() + i, this);
#ifdef DEBUG_UNIFY
    std::cerr << "> " << debug_call << " = " << "Compound[" << rhs->to_string() << "]" << std::endl;
#endif
    return rhs;
  }

  Simple_Selector* Type_Selector::unify_with(Simple_Selector* rhs)
  {
    #ifdef DEBUG_UNIFY
    const std::string debug_call = "unify(Type[" + this->to_string() + "], Simple[" + rhs->to_string() + "])";
    std::cerr << debug_call << std::endl;
    #endif

    bool rhs_ns = false;
    if (!(is_ns_eq(*rhs) || rhs->is_universal_ns())) {
      if (!is_universal_ns()) {
        #ifdef DEBUG_UNIFY
        std::cerr << "> " << debug_call << " = nullptr" << std::endl;
        #endif
        return nullptr;
      }
      rhs_ns = true;
    }
    bool rhs_name = false;
    if (!(name_ == rhs->name() || rhs->is_universal())) {
      if (!(is_universal())) {
        #ifdef DEBUG_UNIFY
        std::cerr << "> " << debug_call << " = nullptr" << std::endl;
        #endif
        return nullptr;
      }
      rhs_name = true;
    }
    if (rhs_ns) {
      ns(rhs->ns());
      has_ns(rhs->has_ns());
    }
    if (rhs_name) name(rhs->name());
    #ifdef DEBUG_UNIFY
    std::cerr << "> " << debug_call << " = Simple[" << this->to_string() << "]" << std::endl;
    #endif
    return this;
  }

  Compound_Selector* Type_Selector::unify_with(Compound_Selector* rhs)
  {
    return toCompound_Selector(unifyWith(toCompoundSelector(rhs))).detach();
  }

  CompoundSelector* Type_Selector::unifyWith(CompoundSelector* rhs)
  {
#ifdef DEBUG_UNIFY
    const std::string debug_call = "unify(Type[" + this->to_string() + "], Compound[" + rhs->to_string() + "])";
    std::cerr << debug_call << std::endl;
#endif

    if (rhs->empty()) {
      rhs->append(this);
#ifdef DEBUG_UNIFY
      std::cerr << "> " << debug_call << " = Compound[" << rhs->to_string() << "]" << std::endl;
#endif
      return rhs;
    }
    Type_Selector* rhs_0 = Cast<Type_Selector>(rhs->at(0));
    if (rhs_0 != nullptr) {
      Simple_Selector* unified = unify_with(rhs_0);
      if (unified == nullptr) {
#ifdef DEBUG_UNIFY
        std::cerr << "> " << debug_call << " = nullptr" << std::endl;
#endif
        return nullptr;
      }
      rhs->elements()[0] = unified;
    }
    else if (!is_universal() || (has_ns_ && ns_ != "*")) {
      rhs->insert(rhs->begin(), this);
    }
#ifdef DEBUG_UNIFY
    std::cerr << "> " << debug_call << " = Compound[" << rhs->to_string() << "]" << std::endl;
#endif
    return rhs;
  }

  Compound_Selector* Class_Selector::unify_with(Compound_Selector* rhs)
  {
    return toCompound_Selector(unifyWith(toCompoundSelector(rhs))).detach();
  }

  CompoundSelector* Class_Selector::unifyWith(CompoundSelector* rhs)
  {
    // rhs->hasPostLineBreak(hasPostLineBreak());
    return Simple_Selector::unifyWith(rhs);
  }

  Compound_Selector* Id_Selector::unify_with(Compound_Selector* rhs)
  {
    return toCompound_Selector(unifyWith(toCompoundSelector(rhs))).detach();
  }

  CompoundSelector* Id_Selector::unifyWith(CompoundSelector* rhs)
  {
    for (const Simple_Selector_Obj& sel : rhs->elements()) {
      if (Id_Selector * id_sel = Cast<Id_Selector>(sel)) {
        if (id_sel->name() != name()) return nullptr;
      }
    }
    // rhs->hasPostLineBreak(hasPostLineBreak());
    return Simple_Selector::unifyWith(rhs);
  }

  Compound_Selector* Pseudo_Selector::unify_with(Compound_Selector* rhs)
  {
    return toCompound_Selector(unifyWith(toCompoundSelector(rhs))).detach();
  }

  CompoundSelector* Pseudo_Selector::unifyWith(CompoundSelector* rhs)
  {
    if (is_pseudo_element()) {
      for (const Simple_Selector_Obj& sel : rhs->elements()) {
        if (Pseudo_Selector * pseudo_sel = Cast<Pseudo_Selector>(sel)) {
          if (pseudo_sel->is_pseudo_element() && pseudo_sel->name() != name()) return nullptr;
        }
      }
    }
    return Simple_Selector::unifyWith(rhs);
  }

  SelectorList* ComplexSelector::unify_with(ComplexSelector* rhs)
  {

    SelectorList_Obj list = SASS_MEMORY_NEW(SelectorList, pstate());

    std::vector<std::vector<CompoundOrCombinator_Obj>> rv = unifyComplex({ elements(), rhs->elements() });

    // std::cerr << "here 91\n";

    for (std::vector<CompoundOrCombinator_Obj> items : rv) {
      ComplexSelector_Obj sel = SASS_MEMORY_NEW(ComplexSelector, pstate());
      // std::cerr << "foobar \n";
      sel->concat(items);
      // debug_ast(sel);
      list->append(sel);
    }

    // std::cerr << "here 99\n";
    // debug_ast(list);
    return list.detach();

  }

  SelectorList* Complex_Selector::unify_with(ComplexSelector* rhs)
  {
    ComplexSelector_Obj self = toComplexSelector(this);
    SelectorList_Obj rv = self->unify_with(rhs);
    return rv.detach();
  }

  SelectorList* Selector_List::unify_with(SelectorList* rhs) {

    SelectorList_Obj self = toSelectorList(this);
    return self->unify_with(rhs);

  }

  Selector_List* Selector_List::unify_with(Selector_List* rhs) {

    SelectorList_Obj r = toSelectorList(rhs);
    SelectorList_Obj rv = unify_with(r);
    Selector_List_Obj rvv = toSelector_List(rv);
    return rvv.detach();

  }

  SelectorList* SelectorList::unify_with(SelectorList* rhs) {

    std::vector<ComplexSelector_Obj> result;
    // Unify all of children with RHS's children, storing the results in `unified_complex_selectors`
    for (ComplexSelector_Obj& seq1 : elements()) {
      for (ComplexSelector_Obj& seq2 : rhs->elements()) {
        ComplexSelector_Obj seq1_copy = seq1->clone();
        ComplexSelector_Obj seq2_copy = seq2->clone();
        SelectorList_Obj unified = seq1_copy->unify_with(seq2_copy);
        if (unified) {
          result.reserve(result.size() + unified->length());
          std::copy(unified->begin(), unified->end(), std::back_inserter(result));
        }
      }
    }

    // Creates the final Selector_List by combining all the complex selectors
    SelectorList* final_result = SASS_MEMORY_NEW(SelectorList, pstate(), result.size());
    for (ComplexSelector_Obj& sel : result) {
      final_result->append(sel);
    }

    return final_result;

  }

}
