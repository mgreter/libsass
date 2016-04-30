#ifndef SASS_AST_BLOCKS_H
#define SASS_AST_BLOCKS_H

namespace Sass {

  /////////////////////////////////////////////////////////////////////////////
  // Rulesets (i.e., sets of styles headed by a selector and containing a block
  // of style declarations.
  /////////////////////////////////////////////////////////////////////////////
  class Ruleset : public Has_Block {
    ADD_PROPERTY(Selector*, selector)
    ADD_PROPERTY(bool, at_root);
    ADD_PROPERTY(bool, is_root);
  public:
    Ruleset(ParserState pstate, Selector* s = 0, Block* b = 0)
    : Has_Block(pstate, b), selector_(s), at_root_(false), is_root_(false)
    { statement_type(RULESET); }
    bool is_invisible() const;
    // nested rulesets need to be hoisted out of their enclosing blocks
    bool is_hoistable() { return true; }
    ATTACH_OPERATIONS()
  };

  /////////////////////////////////////////////////////////
  // Nested declaration sets (i.e., namespaced properties).
  /////////////////////////////////////////////////////////
  class Propset : public Has_Block {
    ADD_PROPERTY(String*, property_fragment)
  public:
    Propset(ParserState pstate, String* pf, Block* b = 0)
    : Has_Block(pstate, b), property_fragment_(pf)
    { }
    ATTACH_OPERATIONS()
  };

}

#endif
