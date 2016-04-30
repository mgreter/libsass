#ifndef SASS_AST_STATEMENTS_H
#define SASS_AST_STATEMENTS_H

namespace Sass {


  ////////////////////////
  // Blocks of statements.
  ////////////////////////
  class Block : public Statement, public Vectorized<Statement*> {
    ADD_PROPERTY(bool, is_root)
    ADD_PROPERTY(bool, is_at_root);
    // needed for properly formatted CSS emission
    ADD_PROPERTY(bool, has_hoistable)
    ADD_PROPERTY(bool, has_non_hoistable)
  protected:
    void adjust_after_pushing(Statement* s)
    {
      if (s->is_hoistable()) has_hoistable_     = true;
      else                   has_non_hoistable_ = true;
    }
  public:
    Block(ParserState pstate, size_t s = 0, bool r = false)
    : Statement(pstate),
      Vectorized<Statement*>(s),
      is_root_(r),
      is_at_root_(false),
      has_hoistable_(false),
      has_non_hoistable_(false)
    { }
    virtual bool has_content()
    {
      for (size_t i = 0, L = elements().size(); i < L; ++i) {
        if (elements()[i]->has_content()) return true;
      }
      return Statement::has_content();
    }
    Block* block() { return this; }
    ATTACH_OPERATIONS()
  };

  ////////////////////////////////////////////////////////////////////////
  // Abstract base class for statements that contain blocks of statements.
  ////////////////////////////////////////////////////////////////////////
  class Has_Block : public Statement {
    ADD_PROPERTY(Block*, block)
  public:
    Has_Block(ParserState pstate, Block* b)
    : Statement(pstate), block_(b)
    { }
    virtual bool has_content()
    {
      return (block_ && block_->has_content()) || Statement::has_content();
    }
    virtual ~Has_Block() = 0;
  };
  inline Has_Block::~Has_Block() { }


  /////////////////
  // Bubble.
  /////////////////
  class Bubble : public Statement {
    ADD_PROPERTY(Statement*, node)
    ADD_PROPERTY(bool, group_end)
  public:
    Bubble(ParserState pstate, Statement* n, Statement* g = 0, size_t t = 0)
    : Statement(pstate, Statement::BUBBLE, t), node_(n), group_end_(g == 0)
    { }
    bool bubbles() { return true; }
    ATTACH_OPERATIONS()
  };

}

#endif
