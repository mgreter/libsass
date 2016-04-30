#ifndef SASS_AST_VALUES_H
#define SASS_AST_VALUES_H

#include "../ast.hpp"

namespace Sass {


  ///////////////////////////////////////////////////////////////////////
  // Lists of values, both comma- and space-separated (distinguished by a
  // type-tag.) Also used to represent variable-length argument lists.
  ///////////////////////////////////////////////////////////////////////
  class List : public Value, public Vectorized<Expression*> {
    void adjust_after_pushing(Expression* e) { is_expanded(false); }
  private:
    ADD_PROPERTY(enum Sass_Separator, separator)
    ADD_PROPERTY(bool, is_arglist)
    ADD_PROPERTY(bool, from_selector)
  public:
    List(ParserState pstate,
         size_t size = 0, enum Sass_Separator sep = SASS_SPACE, bool argl = false)
    : Value(pstate),
      Vectorized<Expression*>(size),
      separator_(sep),
      is_arglist_(argl),
      from_selector_(false)
    { concrete_type(LIST); }
    std::string type() { return is_arglist_ ? "arglist" : "list"; }
    static std::string type_name() { return "list"; }
    const char* sep_string(bool compressed = false) const {
      return separator() == SASS_SPACE ?
        " " : (compressed ? "," : ", ");
    }
    bool is_invisible() const { return empty(); }
    Expression* value_at_index(size_t i);

    virtual size_t size() const;

    virtual size_t hash()
    {
      if (hash_ == 0) {
        hash_ = std::hash<std::string>()(sep_string());
        for (size_t i = 0, L = length(); i < L; ++i)
          hash_combine(hash_, (elements()[i])->hash());
      }
      return hash_;
    }

    virtual void set_delayed(bool delayed)
    {
      is_delayed(delayed);
      // don't set children
    }

    virtual bool operator== (const Expression& rhs) const;

    ATTACH_OPERATIONS()
  };

  ///////////////////////////////////////////////////////////////////////
  // Key value paris.
  ///////////////////////////////////////////////////////////////////////
  class Map : public Value, public Hashed {
    void adjust_after_pushing(std::pair<Expression*, Expression*> p) { is_expanded(false); }
  public:
    Map(ParserState pstate,
         size_t size = 0)
    : Value(pstate),
      Hashed(size)
    { concrete_type(MAP); }
    std::string type() { return "map"; }
    static std::string type_name() { return "map"; }
    bool is_invisible() const { return empty(); }

    virtual size_t hash()
    {
      if (hash_ == 0) {
        for (auto key : keys()) {
          hash_combine(hash_, key->hash());
          hash_combine(hash_, at(key)->hash());
        }
      }

      return hash_;
    }

    virtual bool operator== (const Expression& rhs) const;

    ATTACH_OPERATIONS()
  };

  inline static const std::string sass_op_to_name(enum Sass_OP op) {
    switch (op) {
      case AND: return "and"; break;
      case OR: return "or"; break;
      case EQ: return "eq"; break;
      case NEQ: return "neq"; break;
      case GT: return "gt"; break;
      case GTE: return "gte"; break;
      case LT: return "lt"; break;
      case LTE: return "lte"; break;
      case ADD: return "plus"; break;
      case SUB: return "sub"; break;
      case MUL: return "times"; break;
      case DIV: return "div"; break;
      case MOD: return "mod"; break;
      // this is only used internally!
      case NUM_OPS: return "[OPS]"; break;
      default: return "invalid"; break;
    }
  }

  //////////////////////////////////////////////////////////////////////////
  // Binary expressions. Represents logical, relational, and arithmetic
  // operations. Templatized to avoid large switch statements and repetitive
  // subclassing.
  //////////////////////////////////////////////////////////////////////////
  class Binary_Expression : public PreValue {
  private:
    ADD_HASHED(Operand, op)
    ADD_HASHED(Expression*, left)
    ADD_HASHED(Expression*, right)
    size_t hash_;
  public:
    Binary_Expression(ParserState pstate,
                      Operand op, Expression* lhs, Expression* rhs)
    : PreValue(pstate), op_(op), left_(lhs), right_(rhs), hash_(0)
    { }
    const std::string type_name() {
      switch (type()) {
        case AND: return "and"; break;
        case OR: return "or"; break;
        case EQ: return "eq"; break;
        case NEQ: return "neq"; break;
        case GT: return "gt"; break;
        case GTE: return "gte"; break;
        case LT: return "lt"; break;
        case LTE: return "lte"; break;
        case ADD: return "add"; break;
        case SUB: return "sub"; break;
        case MUL: return "mul"; break;
        case DIV: return "div"; break;
        case MOD: return "mod"; break;
        // this is only used internally!
        case NUM_OPS: return "[OPS]"; break;
        default: return "invalid"; break;
      }
    }
    const std::string separator() {
      switch (type()) {
        case AND: return "&&"; break;
        case OR: return "||"; break;
        case EQ: return "=="; break;
        case NEQ: return "!="; break;
        case GT: return ">"; break;
        case GTE: return ">="; break;
        case LT: return "<"; break;
        case LTE: return "<="; break;
        case ADD: return "+"; break;
        case SUB: return "-"; break;
        case MUL: return "*"; break;
        case DIV: return "/"; break;
        case MOD: return "%"; break;
        // this is only used internally!
        case NUM_OPS: return "[OPS]"; break;
        default: return "invalid"; break;
      }
    }
    bool is_left_interpolant(void) const;
    bool is_right_interpolant(void) const;
    bool has_interpolant() const
    {
      return is_left_interpolant() ||
             is_right_interpolant();
    }
    virtual void set_delayed(bool delayed)
    {
      right()->set_delayed(delayed);
      left()->set_delayed(delayed);
      is_delayed(delayed);
    }
    virtual bool operator==(const Expression& rhs) const
    {
      try
      {
        const Binary_Expression* m = dynamic_cast<const Binary_Expression*>(&rhs);
        if (m == 0) return false;
        return type() == m->type() &&
               left() == m->left() &&
               right() == m->right();
      }
      catch (std::bad_cast&)
      {
        return false;
      }
      catch (...) { throw; }
    }
    virtual size_t hash()
    {
      if (hash_ == 0) {
        hash_ = std::hash<size_t>()(type());
        hash_combine(hash_, left()->hash());
        hash_combine(hash_, right()->hash());
      }
      return hash_;
    }
    enum Sass_OP type() const { return op_.operand; }
    ATTACH_OPERATIONS()
  };


  //////////////////
  // Function calls.
  //////////////////
  class Function_Call : public PreValue {
    ADD_HASHED(std::string, name)
    ADD_HASHED(Arguments*, arguments)
    ADD_PROPERTY(void*, cookie)
    size_t hash_;
  public:
    Function_Call(ParserState pstate, std::string n, Arguments* args, void* cookie)
    : PreValue(pstate), name_(n), arguments_(args), cookie_(cookie), hash_(0)
    { concrete_type(STRING); }
    Function_Call(ParserState pstate, std::string n, Arguments* args)
    : PreValue(pstate), name_(n), arguments_(args), cookie_(0), hash_(0)
    { concrete_type(STRING); }

    virtual bool operator==(const Expression& rhs) const
    {
      try
      {
        const Function_Call* m = dynamic_cast<const Function_Call*>(&rhs);
        if (!(m && name() == m->name())) return false;
        if (!(m && arguments()->length() == m->arguments()->length())) return false;
        for (size_t i =0, L = arguments()->length(); i < L; ++i)
          if (!((*arguments())[i] == (*m->arguments())[i])) return false;
        return true;
      }
      catch (std::bad_cast&)
      {
        return false;
      }
      catch (...) { throw; }
    }

    virtual size_t hash()
    {
      if (hash_ == 0) {
        hash_ = std::hash<std::string>()(name());
        for (auto argument : arguments()->elements())
          hash_combine(hash_, argument->hash());
      }
      return hash_;
    }

    ATTACH_OPERATIONS()
  };


  ///////////////////////
  // Variable references.
  ///////////////////////
  class Variable : public PreValue {
    ADD_PROPERTY(std::string, name)
  public:
    Variable(ParserState pstate, std::string n)
    : PreValue(pstate), name_(n)
    { }

    virtual bool operator==(const Expression& rhs) const
    {
      try
      {
        const Variable* e = dynamic_cast<const Variable*>(&rhs);
        return e && name() == e->name();
      }
      catch (std::bad_cast&)
      {
        return false;
      }
      catch (...) { throw; }
    }

    virtual size_t hash()
    {
      return std::hash<std::string>()(name());
    }

    ATTACH_OPERATIONS()
  };


}

#endif
