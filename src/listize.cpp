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

  // Implements dart-sass SelectorList.asList
  Value* Listize::listize(SelectorList* selectors)
  {
    ListObj list = SASS_MEMORY_NEW(List, selectors->pstate(), sass::vector<ValueObj>(), SASS_COMMA);
    for (ComplexSelector* complex : selectors->elements()) {
      list->append(listize(complex));
    }
    if (list->length()) return list.detach();
    return SASS_MEMORY_NEW(Null, list->pstate());
  }

  Value* Listize::listize(ComplexSelector* selector)
  {
    ListObj list = SASS_MEMORY_NEW(List, selector->pstate(), sass::vector<ValueObj>(), SASS_SPACE);
    for (SelectorComponent* component : selector->elements()) {
      list->append(SASS_MEMORY_NEW(String,
        component->pstate(), component->to_css(false)));
    }
    return list.detach();
  }

}
