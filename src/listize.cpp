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

  // Implements dart-sass SelectorList.asSassList
  Value* Listize::listize(SelectorList* selectors)
  {
    // ListObj list = SASS_MEMORY_NEW(List, selectors->pstate(), 0, SASS_COMMA);
    SassListObj list = SASS_MEMORY_NEW(SassList, selectors->pstate(), {}, SASS_COMMA);
    for (ComplexSelector* complex : selectors->elements()) {
      list->append(listize(complex));
    }
    // if (list->length() < 3) list->separator(SASS_UNDEF);
    if (list->length()) return list.detach();
    return SASS_MEMORY_NEW(Null, list->pstate());
  }

  Value* Listize::listize(ComplexSelector* selector)
  {
    SassListObj list = SASS_MEMORY_NEW(SassList, selector->pstate(), {}, SASS_SPACE);
    // ListObj list = SASS_MEMORY_NEW(List, selector->pstate(), 0, SASS_SPACE);
    for (SelectorComponent* component : selector->elements()) {
      // std::cerr << "err [" << component->to_css() << "]\n";
      list->append(SASS_MEMORY_NEW(StringLiteral,
        component->pstate(), component->to_css()));
    }
    // if (list->length() < 3) list->separator(SASS_UNDEF);
    return list.detach();
  }

}
