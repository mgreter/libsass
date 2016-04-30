#ifndef SASS_AST_EXPRESSIONS_H
#define SASS_AST_EXPRESSIONS_H

namespace Sass {


  //////////////////////////////////////////////////////////////////////
  // Still just an expression, but with a to_string method
  //////////////////////////////////////////////////////////////////////
  class PreValue : public Expression {
  public:
    PreValue(ParserState pstate,
               bool d = false, bool e = false, bool i = false, Concrete_Type ct = NONE)
    : Expression(pstate, d, e, i, ct)
    { }
    virtual ~PreValue() { }
  };

  //////////////////////////////////////////////////////////////////////
  // base class for values that support operations
  //////////////////////////////////////////////////////////////////////
  class Value : public Expression {
  public:
    Value(ParserState pstate,
          bool d = false, bool e = false, bool i = false, Concrete_Type ct = NONE)
    : Expression(pstate, d, e, i, ct)
    { }
    virtual bool operator== (const Expression& rhs) const = 0;
  };

}

#endif
