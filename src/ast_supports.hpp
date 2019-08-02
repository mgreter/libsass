#ifndef SASS_AST_SUPPORTS_H
#define SASS_AST_SUPPORTS_H

// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include <set>
#include <deque>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <typeinfo>
#include <algorithm>
#include "sass/base.h"
#include "ast_fwd_decl.hpp"

#include "util.hpp"
#include "units.hpp"
#include "context.hpp"
#include "position.hpp"
#include "constants.hpp"
#include "operation.hpp"
#include "position.hpp"
#include "inspect.hpp"
#include "source_map.hpp"
#include "environment.hpp"
#include "error_handling.hpp"
#include "ast_def_macros.hpp"
#include "ast_fwd_decl.hpp"
#include "source_map.hpp"
#include "fn_utils.hpp"

#include "sass.h"

namespace Sass {

  ////////////////////
  // `@supports` rule.
  ////////////////////
  class SupportsRule : public ParentStatement {
    ADD_PROPERTY(SupportsCondition_Obj, condition);
    ADD_POINTER(IDXS*, idxs);
  public:
    SupportsRule(SourceSpan pstate, SupportsCondition_Obj condition, Block_Obj block = {});
    ATTACH_CLONE_OPERATIONS(SupportsRule)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  //////////////////////////////////////////////////////
  // The abstract superclass of all Supports conditions.
  //////////////////////////////////////////////////////
  class SupportsCondition : public Expression {
  public:
    SupportsCondition(SourceSpan pstate);
    // ATTACH_CLONE_OPERATIONS(SupportsCondition)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ////////////////////////////////////////////////////////////
  // An operator condition (e.g. `CONDITION1 and CONDITION2`).
  ////////////////////////////////////////////////////////////
  class SupportsOperation : public SupportsCondition {
  public:
    enum Operand { AND, OR };
  private:
    ADD_PROPERTY(SupportsCondition_Obj, left);
    ADD_PROPERTY(SupportsCondition_Obj, right);
    ADD_PROPERTY(Operand, operand);
  public:
    SupportsOperation(SourceSpan pstate, SupportsCondition_Obj l, SupportsCondition_Obj r, Operand o);
    // ATTACH_CLONE_OPERATIONS(SupportsOperation)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  //////////////////////////////////////////
  // A negation condition (`not CONDITION`).
  //////////////////////////////////////////
  class SupportsNegation : public SupportsCondition {
  private:
    ADD_PROPERTY(SupportsCondition_Obj, condition);
  public:
    SupportsNegation(SourceSpan pstate, SupportsCondition_Obj c);
    // ATTACH_CLONE_OPERATIONS(SupportsNegation)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  /////////////////////////////////////////////////////
  // A declaration condition (e.g. `(feature: value)`).
  /////////////////////////////////////////////////////
  class SupportsDeclaration : public SupportsCondition {
  private:
    ADD_PROPERTY(Expression_Obj, feature);
    ADD_PROPERTY(Expression_Obj, value);
  public:
    SupportsDeclaration(SourceSpan pstate, Expression_Obj f, Expression_Obj v);
    // ATTACH_CLONE_OPERATIONS(SupportsDeclaration)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ///////////////////////////////////////////////
  // An interpolation condition (e.g. `#{$var}`).
  ///////////////////////////////////////////////
  class SupportsInterpolation : public SupportsCondition {
  private:
    ADD_PROPERTY(Expression_Obj, value);
  public:
    SupportsInterpolation(SourceSpan pstate, Expression_Obj v);
    // ATTACH_CLONE_OPERATIONS(SupportsInterpolation)
    ATTACH_CRTP_PERFORM_METHODS()
  };

}

#endif
