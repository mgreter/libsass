// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include "sass.h"
#include "values.hpp"
#include "ast.hpp"

#include <stdint.h>

namespace Sass {

  // convert value from C++ side to C-API
  struct SassValue* ast_node_to_sass_value (Value* val)
  {
    return val->toSassValue();
  }

  // convert value from C-API to C++ side
  Value* sass_value_to_ast_node (struct SassValue* val)
  {
    return reinterpret_cast<Value*>(val);
  }

}
