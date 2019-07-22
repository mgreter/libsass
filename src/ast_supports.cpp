// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"
#include "ast.hpp"


namespace Sass {

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  SupportsRule::SupportsRule(ParserState pstate, SupportsCondition_Obj condition, Block_Obj block)
  : Has_Block(pstate, block), condition_(condition)
  { statement_type(SUPPORTS); }
  SupportsRule::SupportsRule(const SupportsRule* ptr)
  : Has_Block(ptr), condition_(ptr->condition_)
  { statement_type(SUPPORTS); }

  bool SupportsRule::is_invisible() const
  {
    Block* b = block();

    bool hasDeclarations = false;
    bool hasPrintableChildBlocks = false;
    for (size_t i = 0, L = b->length(); i < L; ++i) {
      Statement_Obj stm = b->at(i);
      if (Cast<Declaration>(stm) || Cast<AtRule>(stm)) {
        hasDeclarations = true;
      }
      else if (CssStyleRule * b = Cast<CssStyleRule>(stm)) {
        Block_Obj pChildBlock = b->block();
        if (!b->is_invisible()) {
          if (!pChildBlock->is_invisible()) {
            hasPrintableChildBlocks = true;
          }
        }
      }
      else if (Has_Block * b = Cast<Has_Block>(stm)) {
        Block_Obj pChildBlock = b->block();
        if (!b->is_invisible()) {
          if (!pChildBlock->is_invisible()) {
            hasPrintableChildBlocks = true;
          }
        }
      }

      if (hasDeclarations || hasPrintableChildBlocks) {
        return false;
      }
    }

    return true;

  }

  bool SupportsRule::bubbles() { return true; }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  SupportsCondition::SupportsCondition(ParserState pstate)
  : Expression(pstate)
  { }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  SupportsOperation::SupportsOperation(ParserState pstate, SupportsCondition_Obj l, SupportsCondition_Obj r, Operand o)
  : SupportsCondition(pstate), left_(l), right_(r), operand_(o)
  { }

  bool SupportsOperation::needs_parens(SupportsCondition_Obj cond) const
  {
    if (SupportsOperation_Obj op = Cast<SupportsOperation>(cond)) {
      return op->operand() != operand();
    }
    return Cast<SupportsNegation>(cond) != NULL;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  SupportsNegation::SupportsNegation(ParserState pstate, SupportsCondition_Obj c)
  : SupportsCondition(pstate), condition_(c)
  { }

  bool SupportsNegation::needs_parens(SupportsCondition_Obj cond) const
  {
    return Cast<SupportsNegation>(cond) ||
           Cast<SupportsOperation>(cond);
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  SupportsDeclaration::SupportsDeclaration(ParserState pstate, Expression_Obj f, Expression_Obj v)
  : SupportsCondition(pstate), feature_(f), value_(v)
  { }

  bool SupportsDeclaration::needs_parens(SupportsCondition_Obj cond) const
  {
    return false;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  SupportsInterpolation::SupportsInterpolation(ParserState pstate, Expression_Obj v)
  : SupportsCondition(pstate), value_(v)
  { }

  bool SupportsInterpolation::needs_parens(SupportsCondition_Obj cond) const
  {
    return false;
  }

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
