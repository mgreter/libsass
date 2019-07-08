#ifndef SASS_C2AST_H
#define SASS_C2AST_H

#include "position.hpp"
#include "backtrace.hpp"
#include "ast_fwd_decl.hpp"

namespace Sass {

  Value* c2ast(union Sass_Value* v, BackTraces traces, SourceSpan pstate);

}

#endif
