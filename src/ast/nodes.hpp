#ifndef SASS_AST_NODES_H
#define SASS_AST_NODES_H

#include "../ast.hpp"

namespace Sass {


  //////////////////////////////////////////////////////////
  // Abstract base class for all abstract syntax tree nodes.
  //////////////////////////////////////////////////////////
  class AST_Node : public Memory_Object {
    ADD_PROPERTY(ParserState, pstate)
  public:
    AST_Node(ParserState pstate)
    : pstate_(pstate)
    { }
    virtual ~AST_Node() = 0;
    virtual size_t hash() { return 0; }
    virtual std::string inspect() const { return to_string({ INSPECT, 5 }); }
    virtual std::string to_sass() const { return to_string({ TO_SASS, 5 }); }
    virtual std::string to_string(Sass_Inspect_Options opt) const;
    virtual std::string to_string() const;
    // virtual Block* block() { return 0; }
  public:
    void update_pstate(const ParserState& pstate);
  public:
    Offset off() { return pstate(); }
    Position pos() { return pstate(); }
    ATTACH_OPERATIONS()
  };
  inline AST_Node::~AST_Node() { }

  //////////////////////////////////////////////////////////////////////
  // Abstract base class for expressions. This side of the AST hierarchy
  // represents elements in value contexts, which exist primarily to be
  // evaluated and returned.
  //////////////////////////////////////////////////////////////////////
  class Expression : public AST_Node {
  public:
    enum Concrete_Type {
      NONE,
      BOOLEAN,
      NUMBER,
      COLOR,
      STRING,
      LIST,
      MAP,
      SELECTOR,
      NULL_VAL,
      C_WARNING,
      C_ERROR,
      NUM_TYPES
    };
  private:
    // expressions in some contexts shouldn't be evaluated
    ADD_PROPERTY(bool, is_delayed)
    ADD_PROPERTY(bool, is_expanded)
    ADD_PROPERTY(bool, is_interpolant)
    ADD_PROPERTY(Concrete_Type, concrete_type)
  public:
    Expression(ParserState pstate,
               bool d = false, bool e = false, bool i = false, Concrete_Type ct = NONE)
    : AST_Node(pstate),
      is_delayed_(d),
      is_expanded_(d),
      is_interpolant_(i),
      concrete_type_(ct)
    { }
    virtual operator bool() { return true; }
    virtual ~Expression() { }
    virtual std::string type() { return ""; /* TODO: raise an error? */ }
    virtual bool is_invisible() const { return false; }
    static std::string type_name() { return ""; }
    virtual bool is_false() { return false; }
    virtual bool operator== (const Expression& rhs) const { return false; }
    virtual void set_delayed(bool delayed) { is_delayed(delayed); }
    virtual bool has_interpolant() const { return is_interpolant(); }
    virtual bool is_left_interpolant() const { return is_interpolant(); }
    virtual bool is_right_interpolant() const { return is_interpolant(); }
    virtual std::string inspect() const { return to_string({ INSPECT, 5 }); }
    virtual std::string to_sass() const { return to_string({ TO_SASS, 5 }); }
    virtual size_t hash() { return 0; }
  };

}



/////////////////////////////////////////////////////////////////////////////////////
// Hash method specializations for std::unordered_map to work with Sass::Expression
/////////////////////////////////////////////////////////////////////////////////////

namespace std {
  template<>
  struct hash<Sass::Expression*>
  {
    size_t operator()(Sass::Expression* s) const
    {
      return s->hash();
    }
  };
  template<>
  struct equal_to<Sass::Expression*>
  {
    bool operator()( Sass::Expression* lhs,  Sass::Expression* rhs) const
    {
      return lhs->hash() == rhs->hash();
    }
  };
}

#endif
