#ifndef SASS_C2AST_H
#define SASS_C2AST_H

#include "position.hpp"
#include "backtrace.hpp"
#include "ast_fwd_decl.hpp"

namespace Sass {

  Value* c2ast(struct SassValue* v, BackTraces traces, SourceSpan pstate);

}

#endif
