#ifndef SASS_BIND_H
#define SASS_BIND_H

#include <string>
#include "environment.hpp"

namespace Sass {
  class   AST_Node;
  class   Parameters;
  class   Arguments;
  class   Context;
  class   Eval;
  typedef Environment<AST_Node_Ptr> Env;

  void bind(std::string type, std::string name, Parameters_Ptr, Arguments_Ptr, Context*, Env*, Eval*);
}

#endif
