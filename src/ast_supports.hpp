#ifndef SASS_AST_SUPPORTS_H
#define SASS_AST_SUPPORTS_H

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
  class Supports_Block : public Has_Block {
    ADD_PROPERTY(Supports_Condition_Obj, condition)
  public:
    Supports_Block(ParserState pstate, Supports_Condition_Obj condition, Block_Obj block = 0);
    Supports_Block(const Supports_Block* ptr);
    bool bubbles();
    ATTACH_AST_OPERATIONS(Supports_Block)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  //////////////////////////////////////////////////////
  // The abstract superclass of all Supports conditions.
  //////////////////////////////////////////////////////
  class Supports_Condition : public Expression {
  public:
    Supports_Condition(ParserState pstate)
    : Expression(pstate)
    { }
    Supports_Condition(const Supports_Condition* ptr)
    : Expression(ptr)
    { }
    virtual bool needs_parens(Supports_Condition_Obj cond) const { return false; }
    ATTACH_AST_OPERATIONS(Supports_Condition)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ////////////////////////////////////////////////////////////
  // An operator condition (e.g. `CONDITION1 and CONDITION2`).
  ////////////////////////////////////////////////////////////
  class Supports_Operator : public Supports_Condition {
  public:
    enum Operand { AND, OR };
  private:
    ADD_PROPERTY(Supports_Condition_Obj, left);
    ADD_PROPERTY(Supports_Condition_Obj, right);
    ADD_PROPERTY(Operand, operand);
  public:
    Supports_Operator(ParserState pstate, Supports_Condition_Obj l, Supports_Condition_Obj r, Operand o);
    Supports_Operator(const Supports_Operator* ptr);
    virtual bool needs_parens(Supports_Condition_Obj cond) const;
    ATTACH_AST_OPERATIONS(Supports_Operator)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  //////////////////////////////////////////
  // A negation condition (`not CONDITION`).
  //////////////////////////////////////////
  class Supports_Negation : public Supports_Condition {
  private:
    ADD_PROPERTY(Supports_Condition_Obj, condition);
  public:
    Supports_Negation(ParserState pstate, Supports_Condition_Obj c);
    Supports_Negation(const Supports_Negation* ptr);
    virtual bool needs_parens(Supports_Condition_Obj cond) const;
    ATTACH_AST_OPERATIONS(Supports_Negation)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  /////////////////////////////////////////////////////
  // A declaration condition (e.g. `(feature: value)`).
  /////////////////////////////////////////////////////
  class Supports_Declaration : public Supports_Condition {
  private:
    ADD_PROPERTY(Expression_Obj, feature);
    ADD_PROPERTY(Expression_Obj, value);
  public:
    Supports_Declaration(ParserState pstate, Expression_Obj f, Expression_Obj v);
    Supports_Declaration(const Supports_Declaration* ptr);
    virtual bool needs_parens(Supports_Condition_Obj cond) const;
    ATTACH_AST_OPERATIONS(Supports_Declaration)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ///////////////////////////////////////////////
  // An interpolation condition (e.g. `#{$var}`).
  ///////////////////////////////////////////////
  class Supports_Interpolation : public Supports_Condition {
  private:
    ADD_PROPERTY(Expression_Obj, value);
  public:
    Supports_Interpolation(ParserState pstate, Expression_Obj v);
    Supports_Interpolation(const Supports_Interpolation* ptr);
    virtual bool needs_parens(Supports_Condition_Obj cond) const;
    ATTACH_AST_OPERATIONS(Supports_Interpolation)
    ATTACH_CRTP_PERFORM_METHODS()
  };

}

#endif