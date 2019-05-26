// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include <cstdlib>
#include <cmath>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <typeinfo>

#include "file.hpp"
#include "eval.hpp"
#include "ast.hpp"
#include "bind.hpp"
#include "util.hpp"
#include "inspect.hpp"
#include "operators.hpp"
#include "environment.hpp"
#include "position.hpp"
#include "debugger.hpp"
#include "sass/values.h"
#include "to_value.hpp"
#include "ast2c.hpp"
#include "c2ast.hpp"
#include "context.hpp"
#include "backtrace.hpp"
#include "lexer.hpp"
#include "prelexer.hpp"
#include "parser.hpp"
#include "expand.hpp"
#include "color_maps.hpp"
#include "sass_functions.hpp"
#include "util_string.hpp"

namespace Sass {

  SelectorList* Eval::operator()(SelectorList* s)
  {
    // return operator()(s->toSelectorList())->toSelList();

    std::vector<SelectorList_Obj> rv;
    SelectorList_Obj sl = SASS_MEMORY_NEW(SelectorList, s->pstate());
    sl->is_optional(s->is_optional());
    sl->media_block(s->media_block());
    sl->is_optional(s->is_optional());
    for (size_t i = 0, iL = s->length(); i < iL; ++i) {
      // std::cerr << "Eval item\n";
      ComplexSelector* sel = (*s)[i];
      rv.push_back(operator()(sel));
    }

    // we should actually permutate parent first
    // but here we have permutated the selector first
    size_t round = 0;
    while (round != std::string::npos) {
      bool abort = true;
      for (size_t i = 0, iL = rv.size(); i < iL; ++i) {
        if (rv[i]->length() > round) {
          sl->append((*rv[i])[round]);
          abort = false;
        }
      }
      if (abort) {
        round = std::string::npos;
      }
      else {
        ++round;
      }

    }
    return sl.detach();
  }

  CompoundOrCombinator* Eval::operator()(CompoundOrCombinator* s)
  {
    return {};
  }

  SelectorList* Eval::operator()(ComplexSelector* s)
  {
    bool implicit_parent = !exp.old_at_root_without_rule;
    if (is_in_selector_schema) exp.pushToSelectorStack({});
    SelectorList_Obj other;
    if (true) {
      other = s->resolve_parent_refs(exp.getSelectorStack2(), traces, implicit_parent);
      // toComplexSelector()
        // ->toSelList();
    }
    else {
      // other = s->resolve_parent_refs(exp.getSelectorStack(), traces, implicit_parent);
    }


    if (is_in_selector_schema) exp.popFromSelectorStack();

    for (size_t i = 0; i < other->length(); i++) {
      ComplexSelector_Obj sel = other->at(i);
      for (size_t n = 0; n < sel->length(); n++) {
        if (CompoundSelector_Obj comp = Cast<CompoundSelector>(sel->at(n))) {
          sel->at(n) = operator()(comp);
        }
      }
    }

    return other.detach();
  }

  CompoundSelector* Eval::operator()(CompoundSelector* s)
  {
    for (size_t i = 0; i < s->length(); i++) {
      Simple_Selector* ss = s->at(i);
      // skip parents here (called via resolve_parent_refs)
      if (ss == NULL || Cast<Parent_Selector>(ss)) continue;
      s->at(i) = Cast<Simple_Selector>(ss->perform(this));
    }
    return s;
  }
}
