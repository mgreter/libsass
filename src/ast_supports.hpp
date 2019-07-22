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
  class SupportsRule : public Has_Block {
    ADD_PROPERTY(SupportsCondition_Obj, condition)
  public:
    SupportsRule(ParserState pstate, SupportsCondition_Obj condition, Block_Obj block = {});
    bool is_invisible() const override;
    bool bubbles() override;
    ATTACH_COPY_OPERATIONS(SupportsRule)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  //////////////////////////////////////////////////////
  // The abstract superclass of all Supports conditions.
  //////////////////////////////////////////////////////
  class SupportsCondition : public Expression {
  public:
    SupportsCondition(ParserState pstate);
    virtual bool needs_parens(SupportsCondition_Obj cond) const { return false; }
    // ATTACH_COPY_OPERATIONS(SupportsCondition)
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
    SupportsOperation(ParserState pstate, SupportsCondition_Obj l, SupportsCondition_Obj r, Operand o);
    virtual bool needs_parens(SupportsCondition_Obj cond) const override;
    // ATTACH_COPY_OPERATIONS(SupportsOperation)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  //////////////////////////////////////////
  // A negation condition (`not CONDITION`).
  //////////////////////////////////////////
  class SupportsNegation : public SupportsCondition {
  private:
    ADD_PROPERTY(SupportsCondition_Obj, condition);
  public:
    SupportsNegation(ParserState pstate, SupportsCondition_Obj c);
    virtual bool needs_parens(SupportsCondition_Obj cond) const override;
    // ATTACH_COPY_OPERATIONS(SupportsNegation)
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
    SupportsDeclaration(ParserState pstate, Expression_Obj f, Expression_Obj v);
    virtual bool needs_parens(SupportsCondition_Obj cond) const override;
    // ATTACH_COPY_OPERATIONS(SupportsDeclaration)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ///////////////////////////////////////////////
  // An interpolation condition (e.g. `#{$var}`).
  ///////////////////////////////////////////////
  class SupportsInterpolation : public SupportsCondition {
  private:
    ADD_PROPERTY(Expression_Obj, value);
  public:
    SupportsInterpolation(ParserState pstate, Expression_Obj v);
    virtual bool needs_parens(SupportsCondition_Obj cond) const override;
    // ATTACH_COPY_OPERATIONS(SupportsInterpolation)
    ATTACH_CRTP_PERFORM_METHODS()
  };

}

#endif
