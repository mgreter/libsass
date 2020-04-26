// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"
#include "ast.hpp"


namespace Sass {

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  SupportsRule::SupportsRule(const SourceSpan& pstate, SupportsCondition_Obj condition)
  : ParentStatement(pstate), condition_(condition), idxs_(0)
  {}
  SupportsRule::SupportsRule(const SourceSpan& pstate, SupportsCondition_Obj condition, sass::vector<StatementObj>&& els)
    : ParentStatement(pstate, std::move(els)), condition_(condition), idxs_(0)
  {}
  SupportsRule::SupportsRule(const SupportsRule* ptr, bool childless)
  : ParentStatement(ptr, childless), condition_(ptr->condition_), idxs_(ptr->idxs_)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  SupportsCondition::SupportsCondition(const SourceSpan& pstate)
  : Expression(pstate)
  { }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  SupportsOperation::SupportsOperation(const SourceSpan& pstate, SupportsCondition_Obj l, SupportsCondition_Obj r, Operand o)
  : SupportsCondition(pstate), left_(l), right_(r), operand_(o)
  { }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  SupportsNegation::SupportsNegation(const SourceSpan& pstate, SupportsCondition_Obj c)
  : SupportsCondition(pstate), condition_(c)
  { }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  SupportsDeclaration::SupportsDeclaration(const SourceSpan& pstate, ExpressionObj f, ExpressionObj v)
  : SupportsCondition(pstate), feature_(f), value_(v)
  { }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  SupportsInterpolation::SupportsInterpolation(const SourceSpan& pstate, ExpressionObj v)
  : SupportsCondition(pstate), value_(v)
  { }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  IMPLEMENT_AST_OPERATORS(SupportsRule);
  // IMPLEMENT_AST_OPERATORS(SupportsCondition);
  // IMPLEMENT_AST_OPERATORS(SupportsOperation);
  // IMPLEMENT_AST_OPERATORS(SupportsNegation);
  // IMPLEMENT_AST_OPERATORS(SupportsDeclaration);
  // IMPLEMENT_AST_OPERATORS(SupportsInterpolation);

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

}
