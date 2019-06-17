// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"
#include "ast.hpp"


namespace Sass {

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Supports_Block::Supports_Block(ParserState pstate, Supports_Condition_Obj condition, Block_Obj block)
  : Has_Block(pstate, block), condition_(condition)
  { statement_type(SUPPORTS); }
  Supports_Block::Supports_Block(const Supports_Block* ptr)
  : Has_Block(ptr), condition_(ptr->condition_)
  { statement_type(SUPPORTS); }

  bool Supports_Block::is_invisible() const
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

  bool Supports_Block::bubbles() { return true; }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Supports_Condition::Supports_Condition(ParserState pstate)
  : Expression(pstate)
  { }

  Supports_Condition::Supports_Condition(const Supports_Condition* ptr)
  : Expression(ptr)
  { }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Supports_Operator::Supports_Operator(ParserState pstate, Supports_Condition_Obj l, Supports_Condition_Obj r, Operand o)
  : Supports_Condition(pstate), left_(l), right_(r), operand_(o)
  { }
  Supports_Operator::Supports_Operator(const Supports_Operator* ptr)
  : Supports_Condition(ptr),
    left_(ptr->left_),
    right_(ptr->right_),
    operand_(ptr->operand_)
  { }

  bool Supports_Operator::needs_parens(Supports_Condition_Obj cond) const
  {
    if (Supports_Operator_Obj op = Cast<Supports_Operator>(cond)) {
      return op->operand() != operand();
    }
    return Cast<Supports_Negation>(cond) != NULL;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Supports_Negation::Supports_Negation(ParserState pstate, Supports_Condition_Obj c)
  : Supports_Condition(pstate), condition_(c)
  { }
  Supports_Negation::Supports_Negation(const Supports_Negation* ptr)
  : Supports_Condition(ptr), condition_(ptr->condition_)
  { }

  bool Supports_Negation::needs_parens(Supports_Condition_Obj cond) const
  {
    return Cast<Supports_Negation>(cond) ||
           Cast<Supports_Operator>(cond);
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Supports_Declaration::Supports_Declaration(ParserState pstate, Expression_Obj f, Expression_Obj v)
  : Supports_Condition(pstate), feature_(f), value_(v)
  { }
  Supports_Declaration::Supports_Declaration(const Supports_Declaration* ptr)
  : Supports_Condition(ptr),
    feature_(ptr->feature_),
    value_(ptr->value_)
  { }

  bool Supports_Declaration::needs_parens(Supports_Condition_Obj cond) const
  {
    return false;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Supports_Interpolation::Supports_Interpolation(ParserState pstate, Expression_Obj v)
  : Supports_Condition(pstate), value_(v)
  { }
  Supports_Interpolation::Supports_Interpolation(const Supports_Interpolation* ptr)
  : Supports_Condition(ptr),
    value_(ptr->value_)
  { }

  bool Supports_Interpolation::needs_parens(Supports_Condition_Obj cond) const
  {
    return false;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  IMPLEMENT_AST_OPERATORS(Supports_Block);
  IMPLEMENT_AST_OPERATORS(Supports_Condition);
  IMPLEMENT_AST_OPERATORS(Supports_Operator);
  IMPLEMENT_AST_OPERATORS(Supports_Negation);
  IMPLEMENT_AST_OPERATORS(Supports_Declaration);
  IMPLEMENT_AST_OPERATORS(Supports_Interpolation);

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

}
