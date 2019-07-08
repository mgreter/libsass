#ifndef SASS_VALUES_H
#define SASS_VALUES_H

#include "ast_fwd_decl.hpp"

namespace Sass {

  union Sass_Value* ast_node_to_sass_value (const Value* val);
  Value* sass_value_to_ast_node (const union Sass_Value* val);

}
#endif
