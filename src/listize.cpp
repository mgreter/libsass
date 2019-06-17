// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include <iostream>
#include <typeinfo>
#include <string>

#include "debugger.hpp"
#include "listize.hpp"
#include "context.hpp"
#include "backtrace.hpp"
#include "error_handling.hpp"

namespace Sass {

  Value* Listize::listize(SelectorList* selectors)
  {
    List_Obj list = SASS_MEMORY_NEW(List,
      selectors->pstate(), selectors->length(), SASS_COMMA);
    for (size_t i = 0, L = selectors->length(); i < L; ++i) {
      list->append(listize(selectors->get(i)));
    }
    if (list->length()) return list.detach();
    return SASS_MEMORY_NEW(Null, list->pstate());
  }

  Value* Listize::listize(ComplexSelector* selector)
  {
    List_Obj list = SASS_MEMORY_NEW(List,
      selector->pstate(), selector->length(), SASS_SPACE);
    for (auto component : selector->elements()) {
      list->append(SASS_MEMORY_NEW(StringLiteral,
        component->pstate(), component->to_css()));
    }
    return list.detach();
  }

}
