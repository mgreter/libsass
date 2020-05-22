
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

    if (std::isnan(n->value())) {
      append_string("NaN");
      return;
    }

    if (std::isinf(n->value())) {
      append_string("Infinity");
      return;
    }

    sass::sstream ss;
    ss.precision(opt.precision);
    ss << std::fixed << n->value();

    sass::string res = ss.str();
    size_t s = res.length();

    // delete trailing zeros
    for (s = s - 1; s > 0; --s)
    {
      if (res[s] == '0') {
        res.erase(s, 1);
      }
      else break;
    }

    // delete trailing decimal separator
    if (res[s] == '.') res.erase(s, 1);

    // some final cosmetics
    if (res == "0.0") res = "0";
    else if (res == "") res = "0";
    else if (res == "-0") res = "0";
    else if (res == "-0.0") res = "0";

    // add unit now
    res += n->unit();


    // output the final token
    append_token(res, n);

    // Inspect::operator()(number);
  }

}

