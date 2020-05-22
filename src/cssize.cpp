
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

  void Cssize::operator()(Function* f)
  {
    bool inspect = output_style() == SASS_STYLE_INSPECT;
    if (!inspect) {
      throw Exception::InvalidCssValue({}, *f);
    }
    Inspect::operator()(f);
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

  void Cssize::operator()(List* value)
  {
    Inspect::operator()(value);
  }


}

