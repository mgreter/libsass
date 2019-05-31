// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include <iostream>
#include <typeinfo>
#include <string>

#include "listize.hpp"
#include "debugger.hpp"
#include "context.hpp"
#include "backtrace.hpp"
#include "error_handling.hpp"

namespace Sass {

  Listize::Listize()
  {  }

  Expression* Listize::operator()(SelectorList* sel)
  {
    List_Obj l = SASS_MEMORY_NEW(List, sel->pstate(), sel->length(), SASS_COMMA);
    l->from_selector(true);
    for (size_t i = 0, L = sel->length(); i < L; ++i) {
      if (!sel->at(i)) continue;
      // Complex_Selector_Obj cplx = toComplex_Selector(sel->at(i));
      l->append(sel->at(i)->perform(this));
    }
    if (l->length()) return l.detach();
    return SASS_MEMORY_NEW(Null, l->pstate());
  }

  Expression* Listize::operator()(CompoundSelector* sel)
  {
    std::string str;
    for (size_t i = 0, L = sel->length(); i < L; ++i) {
      Expression* e = (*sel)[i]->perform(this);
      if (e) str += e->to_string();
    }
    return SASS_MEMORY_NEW(String_Quoted, sel->pstate(), str);
  }

  Expression* Listize::operator()(ComplexSelector* sel)
  {
    List_Obj l = SASS_MEMORY_NEW(List, sel->pstate());
    // ToDo: investigate what this does
    l->from_selector(true);

    for (auto component : sel->elements()) {
      if (CompoundSelector_Obj compound = Cast<CompoundSelector>(component)) {
        if (!compound->empty()) {
          Expression_Obj hh = compound->perform(this);
          if (hh) l->append(hh);
        }
      }
      else if (component) {
        l->append(SASS_MEMORY_NEW(String_Quoted, component->pstate(), component->to_string()));
      }
    }

    if (l->length() == 0) return 0;
    return l.detach();
  }


  Expression* Listize::operator()(Selector_List* sel)
  {
    SelectorList_Obj rhs = toSelectorList(sel);
    return operator()(rhs);
  }

  Expression* Listize::operator()(Compound_Selector* sel)
  {
    CompoundSelector_Obj s = toCompoundSelector(sel);
    return operator()(s);
  }

  Expression* Listize::operator()(Complex_Selector* sel)
  {
    ComplexSelector_Obj s = toComplexSelector(sel);
    return operator()(s);
  }

}
