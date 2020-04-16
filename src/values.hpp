#ifndef SASS_VALUES_H
#define SASS_VALUES_H

#include "ast_fwd_decl.hpp"

namespace Sass {

  struct SassValue* ast_node_to_sass_value (Value* val);
  Value* sass_value_to_ast_node (struct SassValue* val);

}
#endif
