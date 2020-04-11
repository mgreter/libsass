#ifndef SASS_OPERATORS_H
#define SASS_OPERATORS_H

#include "ast_fwd_decl.hpp"
#include "source_span.hpp"
#include "sass/values.h"
#include "values.hpp"

namespace Sass {

  namespace Operators {

    // equality operator using AST Node operator==
    bool eq(ValueObj, ValueObj);
    bool neq(ValueObj, ValueObj);
    // specific operators based on cmp and eq
    bool lt(ValueObj, ValueObj);
    bool gt(ValueObj, ValueObj);
    bool lte(ValueObj, ValueObj);
    bool gte(ValueObj, ValueObj);

    // arithmetic for all the combinations that matter
    Value* op_strings(Sass::Operand, Value&, Value&, struct Sass_Inspect_Options opt, const SourceSpan& pstate, bool delayed = false);
    Value* op_colors(enum Sass_OP, const Color_RGBA&, const Color_RGBA&, const SourceSpan& pstate);
    Value* op_numbers(enum Sass_OP, const Number&, const Number&, const SourceSpan& pstate);
    Value* op_number_color(enum Sass_OP, const Number&, const Color_RGBA&, struct Sass_Inspect_Options opt, const SourceSpan& pstate);
    Value* op_color_number(enum Sass_OP, const Color_RGBA&, const Number&, struct Sass_Inspect_Options opt, const SourceSpan& pstate);

  };

}

#endif
