
#include "charcode.hpp"
#include "character.hpp"

#include "ast.hpp"
#include "util.hpp"
#include "inspect.hpp"
#include "cssize.hpp"
#include "context.hpp"
#include "debugger.hpp"
#include "listize.hpp"
#include "color_maps.hpp"
#include "string_utils.hpp"
#include "utf8/checked.h"
#include "debugger.hpp"

namespace Sass {

  // Import some namespaces
  using namespace Charcode;
  using namespace Character;

  void Cssize::operator()(Function* f)
  {
    throw Exception::InvalidCssValue({}, *f);
  }

  void Cssize::operator()(Map* value)
  {
    if (output_style() == SASS_STYLE_TO_CSS) {
      // should be handle in check_expression
      throw Exception::InvalidCssValue2({}, *value);
    }
    if (value->empty()) return;

    Inspect::operator()(value);
  }

  void Cssize::operator()(List* list)
  {
    if (list->empty() && !list->hasBrackets()) {
      throw Exception::InvalidCssValue({}, *list);
    }
    Inspect::operator()(list);
  }

  void Cssize::operator()(Number* n)
  {

    if (n->lhsAsSlash() && n->rhsAsSlash()) {
      n->lhsAsSlash()->perform(this);
      append_string("/");
      n->rhsAsSlash()->perform(this);
      return;
    }

    // reduce units
    n->reduce();

    if (opt.output_style == SASS_STYLE_TO_CSS && !n->is_valid_css_unit()) {
      // traces.push_back(BackTrace(nr->pstate()));
      // issue_1804
      throw Exception::InvalidCssValue({}, *n);
    }

    Inspect::operator()(n);

  }

}

