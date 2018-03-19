#include "sass.hpp"
#include "ast.hpp"
#include "context.hpp"
#include "node.hpp"
#include "eval.hpp"
#include "extend.hpp"
#include "emitter.hpp"
#include "color_maps.hpp"
#include "ast_fwd_decl.hpp"
#include <set>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <functional>
#include <cctype>
#include <locale>

#include "ast_values.hpp"

namespace Sass {

  void str_rtrim(std::string& str, const std::string& delimiters = " \f\n\r\t\v")
  {
    str.erase( str.find_last_not_of( delimiters ) + 1 );
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  PreValue::PreValue(ParserState pstate, bool d, bool e, bool i, Type ct)
  : Expression(pstate, d, e, i, ct)
  { }
  PreValue::PreValue(const PreValue* ptr)
  : Expression(ptr)
  { }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Value::Value(ParserState pstate, bool d, bool e, bool i, Type ct)
  : PreValue(pstate, d, e, i, ct)
  { }
  Value::Value(const Value* ptr)
  : PreValue(ptr)
  { }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  List::List(ParserState pstate, size_t size, enum Sass_Separator sep, bool argl, bool bracket)
  : Value(pstate),
    Vectorized<Expression_Obj>(size),
    separator_(sep),
    is_arglist_(argl),
    is_bracketed_(bracket),
    from_selector_(false)
  { concrete_type(LIST); }

  List::List(const List* ptr)
  : Value(ptr),
    Vectorized<Expression_Obj>(*ptr),
    separator_(ptr->separator_),
    is_arglist_(ptr->is_arglist_),
    is_bracketed_(ptr->is_bracketed_),
    from_selector_(ptr->from_selector_)
  { concrete_type(LIST); }

  size_t List::hash()
  {
    if (hash_ == 0) {
      hash_ = std::hash<std::string>()(sep_string());
      hash_combine(hash_, std::hash<bool>()(is_bracketed()));
      for (size_t i = 0, L = length(); i < L; ++i)
        hash_combine(hash_, (elements()[i])->hash());
    }
    return hash_;
  }

  void List::set_delayed(bool delayed)
  {
    is_delayed(delayed);
    // don't set children
  }

  bool List::operator== (const Expression& rhs) const
  {
    if (auto r = Cast<List>(&rhs)) {
      if (length() != r->length()) return false;
      if (separator() != r->separator()) return false;
      if (is_bracketed() != r->is_bracketed()) return false;
      for (size_t i = 0, L = length(); i < L; ++i) {
        auto rv = r->get(i);
        auto lv = this->get(i);
        if (*lv != *rv) return false;
      }
      return true;
    }
    return false;
  }

  size_t List::size() const {
    if (!is_arglist_) return length();
    // arglist expects a list of arguments
    // so we need to break before keywords
    for (size_t i = 0, L = length(); i < L; ++i) {
      Expression_Obj obj = this->at(i);
      if (Argument_Ptr arg = Cast<Argument>(obj)) {
        if (!arg->name().empty()) return i;
      }
    }
    return length();
  }


  Expression_Obj List::value_at_index(size_t i) {
    Expression_Obj obj = this->at(i);
    if (is_arglist_) {
      if (Argument_Ptr arg = Cast<Argument>(obj)) {
        return arg->value();
      } else {
        return obj;
      }
    } else {
      return obj;
    }
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Map::Map(ParserState pstate, size_t size)
  : Value(pstate),
    Hashed(size)
  { concrete_type(MAP); }

  Map::Map(const Map* ptr)
  : Value(ptr),
    Hashed(*ptr)
  { concrete_type(MAP); }

  bool Map::operator== (const Expression& rhs) const
  {
    if (auto r = Cast<Map>(&rhs)) {
      if (length() != r->length()) return false;
      for (auto key : keys()) {
        auto lv = at(key);
        auto rv = r->at(key);
        if (*lv != *rv) return false;
      }
      return true;
    }
    return false;
  }

  List_Obj Map::to_list(ParserState& pstate) {
    List_Obj ret = SASS_MEMORY_NEW(List, pstate, length(), SASS_COMMA);

    for (auto key : keys()) {
      List_Obj l = SASS_MEMORY_NEW(List, pstate, 2);
      l->append(key);
      l->append(at(key));
      ret->append(l);
    }

    return ret;
  }

  size_t Map::hash()
  {
    if (hash_ == 0) {
      for (auto key : keys()) {
        hash_combine(hash_, key->hash());
        hash_combine(hash_, at(key)->hash());
      }
    }

    return hash_;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Binary_Expression::Binary_Expression(ParserState pstate,
                    Operand op, Expression_Obj lhs, Expression_Obj rhs)
  : PreValue(pstate), op_(op), left_(lhs), right_(rhs), hash_(0)
  { }

  Binary_Expression::Binary_Expression(const Binary_Expression* ptr)
  : PreValue(ptr),
    op_(ptr->op_),
    left_(ptr->left_),
    right_(ptr->right_),
    hash_(ptr->hash_)
  { }

  bool Binary_Expression::is_left_interpolant(void) const
  {
    return is_interpolant() || (left() && left()->is_left_interpolant());
  }
  bool Binary_Expression::is_right_interpolant(void) const
  {
    return is_interpolant() || (right() && right()->is_right_interpolant());
  }

  const std::string Binary_Expression::type_name()
  {
    return sass_op_to_name(optype());
  }
  
  const std::string Binary_Expression::separator()
  {
    return sass_op_separator(optype());
  }

  bool Binary_Expression::has_interpolant() const
  {
    return is_left_interpolant() ||
            is_right_interpolant();
  }
  
  void Binary_Expression::set_delayed(bool delayed)
  {
    right()->set_delayed(delayed);
    left()->set_delayed(delayed);
    is_delayed(delayed);
  }

  bool Binary_Expression::operator==(const Expression& rhs) const
  {
    if (auto m = Cast<Binary_Expression>(&rhs)) {
      return type() == m->type() &&
             *left() == *m->left() &&
             *right() == *m->right();
    }
    return false;
  }

  size_t Binary_Expression::hash()
  {
    if (hash_ == 0) {
      hash_ = std::hash<size_t>()(optype());
      hash_combine(hash_, left()->hash());
      hash_combine(hash_, right()->hash());
    }
    return hash_;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Function::Function(ParserState pstate, Definition_Obj def, bool css)
  : Value(pstate), definition_(def), is_css_(css)
  { concrete_type(FUNCTION_VAL); }

  Function::Function(const Function* ptr)
  : Value(ptr), definition_(ptr->definition_), is_css_(ptr->is_css_)
  { concrete_type(FUNCTION_VAL); }

  bool Function::operator== (const Expression& rhs) const
  {
    if (auto r = Cast<Function>(&rhs)) {
      auto d1 = Cast<Definition>(definition());
      auto d2 = Cast<Definition>(r->definition());
      return d1 && d2 && d1 == d2 && is_css() == r->is_css();
    }
    return false;
  }

  std::string Function::name() {
    if (definition_) {
      return definition_->name();
    }
    return "";
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Function_Call::Function_Call(ParserState pstate, String_Obj n, Arguments_Obj args, void* cookie)
  : PreValue(pstate), sname_(n), arguments_(args), func_(0), via_call_(false), cookie_(cookie), hash_(0)
  { concrete_type(FUNCTION); }
  Function_Call::Function_Call(ParserState pstate, String_Obj n, Arguments_Obj args, Function_Obj func)
  : PreValue(pstate), sname_(n), arguments_(args), func_(func), via_call_(false), cookie_(0), hash_(0)
  { concrete_type(FUNCTION); }
  Function_Call::Function_Call(ParserState pstate, String_Obj n, Arguments_Obj args)
  : PreValue(pstate), sname_(n), arguments_(args), via_call_(false), cookie_(0), hash_(0)
  { concrete_type(FUNCTION); }

  Function_Call::Function_Call(ParserState pstate, std::string n, Arguments_Obj args, void* cookie)
  : PreValue(pstate), sname_(SASS_MEMORY_NEW(String_Constant, pstate, n)), arguments_(args), func_(0), via_call_(false), cookie_(cookie), hash_(0)
  { concrete_type(FUNCTION); }
  Function_Call::Function_Call(ParserState pstate, std::string n, Arguments_Obj args, Function_Obj func)
  : PreValue(pstate), sname_(SASS_MEMORY_NEW(String_Constant, pstate, n)), arguments_(args), func_(func), via_call_(false), cookie_(0), hash_(0)
  { concrete_type(FUNCTION); }
  Function_Call::Function_Call(ParserState pstate, std::string n, Arguments_Obj args)
  : PreValue(pstate), sname_(SASS_MEMORY_NEW(String_Constant, pstate, n)), arguments_(args), via_call_(false), cookie_(0), hash_(0)
  { concrete_type(FUNCTION); }

  Function_Call::Function_Call(const Function_Call* ptr)
  : PreValue(ptr),
    sname_(ptr->sname_),
    arguments_(ptr->arguments_),
    func_(ptr->func_),
    via_call_(ptr->via_call_),
    cookie_(ptr->cookie_),
    hash_(ptr->hash_)
  { concrete_type(FUNCTION); }

  bool Function_Call::operator==(const Expression& rhs) const
  {
    if (auto m = Cast<Function_Call>(&rhs)) {
      if (*sname() != *m->sname()) return false;
      if (arguments()->length() != m->arguments()->length()) return false;
      for (size_t i = 0, L = arguments()->length(); i < L; ++i)
        if (*arguments()->get(i) != *m->arguments()->get(i)) return false;
      return true;
    }
    return false;
  }

  size_t Function_Call::hash()
  {
    if (hash_ == 0) {
      hash_ = std::hash<std::string>()(name());
      for (auto argument : arguments()->elements())
        hash_combine(hash_, argument->hash());
    }
    return hash_;
  }

  std::string Function_Call::name()
  {
    return sname();
  }

  bool Function_Call::is_css() {
    if (func_) return func_->is_css();
    return false;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Variable::Variable(ParserState pstate, std::string n)
  : PreValue(pstate), name_(n)
  { concrete_type(VARIABLE); }

  Variable::Variable(const Variable* ptr)
  : PreValue(ptr), name_(ptr->name_)
  { concrete_type(VARIABLE); }

  bool Variable::operator==(const Expression& rhs) const
  {
    if (auto e = Cast<Variable>(&rhs)) {
      return name() == e->name();
    }
    return false;
  }

  size_t Variable::hash()
  {
    return std::hash<std::string>()(name());
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Number::Number(ParserState pstate, double val, std::string u, bool zero)
  : Value(pstate),
    Units(),
    value_(val),
    zero_(zero),
    hash_(0)
  {
    size_t l = 0;
    size_t r;
    if (!u.empty()) {
      bool nominator = true;
      while (true) {
        r = u.find_first_of("*/", l);
        std::string unit(u.substr(l, r == std::string::npos ? r : r - l));
        if (!unit.empty()) {
          if (nominator) numerators.push_back(unit);
          else denominators.push_back(unit);
        }
        if (r == std::string::npos) break;
        // ToDo: should error for multiple slashes
        // if (!nominator && u[r] == '/') error(...)
        if (u[r] == '/')
          nominator = false;
        // strange math parsing?
        // else if (u[r] == '*')
        //  nominator = true;
        l = r + 1;
      }
    }
    concrete_type(NUMBER);
  }

  Number::Number(const Number* ptr)
  : Value(ptr),
    Units(ptr),
    value_(ptr->value_), zero_(ptr->zero_),
    hash_(ptr->hash_)
  { concrete_type(NUMBER); }

  // cancel out unnecessary units
  void Number::reduce()
  {
    // apply conversion factor
    value_ *= this->Units::reduce();
  }

  void Number::normalize()
  {
    // apply conversion factor
    value_ *= this->Units::normalize();
  }

  size_t Number::hash()
  {
    if (hash_ == 0) {
      hash_ = std::hash<double>()(value_);
      for (const auto numerator : numerators)
        hash_combine(hash_, std::hash<std::string>()(numerator));
      for (const auto denominator : denominators)
        hash_combine(hash_, std::hash<std::string>()(denominator));
    }
    return hash_;
  }

  bool Number::operator== (const Expression& rhs) const
  {
    if (auto n = Cast<Number>(&rhs)) {
      return *this == *n;
    }
    return false;
  }

  bool Number::operator== (const Number& rhs) const
  {
    // unitless or only having one unit are equivalent (3.4)
    // therefore we need to reduce the units beforehand
    Number l(*this), r(rhs); l.reduce(); r.reduce();
    size_t lhs_units = l.numerators.size() + l.denominators.size();
    size_t rhs_units = r.numerators.size() + r.denominators.size();
    if (!lhs_units || !rhs_units) {
      return NEAR_EQUAL(l.value(), r.value());;
    }
    // ensure both have same units
    l.normalize(); r.normalize();
    Units &lhs_unit = l, &rhs_unit = r;
    return lhs_unit == rhs_unit &&
      NEAR_EQUAL(l.value(), r.value());
  }

  bool Number::operator< (const Number& rhs) const
  {
    // unitless or only having one unit are equivalent (3.4)
    // therefore we need to reduce the units beforehand
    Number l(*this), r(rhs); l.reduce(); r.reduce();
    size_t lhs_units = l.numerators.size() + l.denominators.size();
    size_t rhs_units = r.numerators.size() + r.denominators.size();
    if (!lhs_units || !rhs_units) {
      return l.value() < r.value();
    }
    // ensure both have same units
    l.normalize(); r.normalize();
    Units &lhs_unit = l, &rhs_unit = r;
    if (!(lhs_unit == rhs_unit)) {
      /* ToDo: do we always get usefull backtraces? */
      throw Exception::IncompatibleUnits(rhs, *this);
    }
    if (lhs_unit == rhs_unit) {
      return l.value() < r.value();
    } else {
      return lhs_unit < rhs_unit;
    }
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Color::Color(ParserState pstate, double r, double g, double b, double a, const std::string disp)
  : Value(pstate), r_(r), g_(g), b_(b), a_(a), disp_(disp),
    hash_(0)
  { concrete_type(COLOR); }

  Color::Color(const Color* ptr)
  : Value(ptr),
    r_(ptr->r_),
    g_(ptr->g_),
    b_(ptr->b_),
    a_(ptr->a_),
    disp_(ptr->disp_),
    hash_(ptr->hash_)
  { concrete_type(COLOR); }

  bool Color::operator== (const Expression& rhs) const
  {
    if (auto r = Cast<Color>(&rhs)) {
      return r_ == r->r() &&
             g_ == r->g() &&
             b_ == r->b() &&
             a_ == r->a();
    }
    return false;
  }

  size_t Color::hash()
  {
    if (hash_ == 0) {
      hash_ = std::hash<double>()(a_);
      hash_combine(hash_, std::hash<double>()(r_));
      hash_combine(hash_, std::hash<double>()(g_));
      hash_combine(hash_, std::hash<double>()(b_));
    }
    return hash_;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Custom_Error::Custom_Error(ParserState pstate, std::string msg)
  : Value(pstate), message_(msg)
  { concrete_type(C_ERROR); }

  Custom_Error::Custom_Error(const Custom_Error* ptr)
  : Value(ptr), message_(ptr->message_)
  { concrete_type(C_ERROR); }

  bool Custom_Error::operator== (const Expression& rhs) const
  {
    if (auto r = Cast<Custom_Error>(&rhs)) {
      return message() == r->message();
    }
    return false;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Custom_Warning::Custom_Warning(ParserState pstate, std::string msg)
  : Value(pstate), message_(msg)
  { concrete_type(C_WARNING); }

  Custom_Warning::Custom_Warning(const Custom_Warning* ptr)
  : Value(ptr), message_(ptr->message_)
  { concrete_type(C_WARNING); }

  bool Custom_Warning::operator== (const Expression& rhs) const
  {
    if (auto r = Cast<Custom_Warning>(&rhs)) {
      return message() == r->message();
    }
    return false;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Boolean::Boolean(ParserState pstate, bool val)
  : Value(pstate), value_(val),
    hash_(0)
  { concrete_type(BOOLEAN); }

  Boolean::Boolean(const Boolean* ptr)
  : Value(ptr),
    value_(ptr->value_),
    hash_(ptr->hash_)
  { concrete_type(BOOLEAN); }

 bool Boolean::operator== (const Expression& rhs) const
  {
    if (auto r = Cast<Boolean>(&rhs)) {
      return (value() == r->value());
    }
    return false;
  }

  size_t Boolean::hash()
  {
    if (hash_ == 0) {
      hash_ = std::hash<bool>()(value_);
    }
    return hash_;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  String::String(ParserState pstate, bool delayed)
  : Value(pstate, delayed)
  { concrete_type(STRING); }
  String::String(const String* ptr)
  : Value(ptr)
  { concrete_type(STRING); }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  String_Schema::String_Schema(ParserState pstate, size_t size, bool css)
  : String(pstate), Vectorized<PreValue_Obj>(size), css_(css), hash_(0)
  { concrete_type(STRING); }

  String_Schema::String_Schema(const String_Schema* ptr)
  : String(ptr),
    Vectorized<PreValue_Obj>(*ptr),
    css_(ptr->css_),
    hash_(ptr->hash_)
  { concrete_type(STRING); }

  void String_Schema::rtrim()
  {
    if (!empty()) {
      if (String_Ptr str = Cast<String>(last())) str->rtrim();
    }
  }

  bool String_Schema::is_left_interpolant(void) const
  {
    return length() && first()->is_left_interpolant();
  }
  bool String_Schema::is_right_interpolant(void) const
  {
    return length() && last()->is_right_interpolant();
  }

  bool String_Schema::operator== (const Expression& rhs) const
  {
    if (auto r = Cast<String_Schema>(&rhs)) {
      if (length() != r->length()) return false;
      for (size_t i = 0, L = length(); i < L; ++i) {
        auto rv = (*r)[i];
        auto lv = (*this)[i];
        if (*lv != *rv) return false;
      }
      return true;
    }
    return false;
  }

  bool String_Schema::has_interpolants()
  {
    for (auto el : elements()) {
      if (el->is_interpolant()) return true;
    }
    return false;
  }

  size_t String_Schema::hash()
  {
    if (hash_ == 0) {
      for (auto string : elements())
        hash_combine(hash_, string->hash());
    }
    return hash_;
  }

  void String_Schema::set_delayed(bool delayed)
  {
    is_delayed(delayed);
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  String_Constant::String_Constant(ParserState pstate, std::string val, bool css)
  : String(pstate), quote_mark_(0), can_compress_whitespace_(false), value_(read_css_string(val, css)), hash_(0)
  { }
  String_Constant::String_Constant(ParserState pstate, const char* beg, bool css)
  : String(pstate), quote_mark_(0), can_compress_whitespace_(false), value_(read_css_string(std::string(beg), css)), hash_(0)
  { }
  String_Constant::String_Constant(ParserState pstate, const char* beg, const char* end, bool css)
  : String(pstate), quote_mark_(0), can_compress_whitespace_(false), value_(read_css_string(std::string(beg, end-beg), css)), hash_(0)
  { }
  String_Constant::String_Constant(ParserState pstate, const Token& tok, bool css)
  : String(pstate), quote_mark_(0), can_compress_whitespace_(false), value_(read_css_string(std::string(tok.begin, tok.end), css)), hash_(0)
  { }

  String_Constant::String_Constant(const String_Constant* ptr)
  : String(ptr),
    quote_mark_(ptr->quote_mark_),
    can_compress_whitespace_(ptr->can_compress_whitespace_),
    value_(ptr->value_),
    hash_(ptr->hash_)
  { }

  bool String_Constant::is_invisible() const {
    return value_.empty() && quote_mark_ == 0;
  }

  bool String_Constant::operator== (const Expression& rhs) const
  {
    if (auto qstr = Cast<String_Quoted>(&rhs)) {
      return value() == qstr->value();
    } else if (auto cstr = Cast<String_Constant>(&rhs)) {
      return value() == cstr->value();
    }
    return false;
  }

  std::string String_Constant::inspect() const
  {
    return quote(value_, '*');
  }

  void String_Constant::rtrim()
  {
    str_rtrim(value_);
  }

  size_t String_Constant::hash()
  {
    if (hash_ == 0) {
      hash_ = std::hash<std::string>()(value_);
    }
    return hash_;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  String_Quoted::String_Quoted(ParserState pstate, std::string val, char q,
    bool keep_utf8_escapes, bool skip_unquoting,
    bool strict_unquoting, bool css)
  : String_Constant(pstate, val, css)
  {
    if (skip_unquoting == false) {
      value_ = unquote(value_, &quote_mark_, keep_utf8_escapes, strict_unquoting);
    }
    if (q && quote_mark_) quote_mark_ = q;
  }

  String_Quoted::String_Quoted(const String_Quoted* ptr)
  : String_Constant(ptr)
  { }

  bool String_Quoted::operator== (const Expression& rhs) const
  {
    if (auto qstr = Cast<String_Quoted>(&rhs)) {
      return value() == qstr->value();
    } else if (auto cstr = Cast<String_Constant>(&rhs)) {
      return value() == cstr->value();
    }
    return false;
  }

  std::string String_Quoted::inspect() const
  {
    return quote(value_, '*');
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Null::Null(ParserState pstate)
  : Value(pstate)
  { concrete_type(NULL_VAL); }

  Null::Null(const Null* ptr) : Value(ptr)
  { concrete_type(NULL_VAL); }

  bool Null::operator== (const Expression& rhs) const
  {
    return Cast<Null>(&rhs) != NULL;
  }

  size_t Null::hash()
  {
    return -1;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Parent_Reference::Parent_Reference(ParserState pstate)
  : Value(pstate)
  { concrete_type(PARENT); }

  Parent_Reference::Parent_Reference(const Parent_Reference* ptr)
  : Value(ptr)
  { concrete_type(PARENT); }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  IMPLEMENT_AST_OPERATORS(List);
  IMPLEMENT_AST_OPERATORS(Map);
  IMPLEMENT_AST_OPERATORS(Binary_Expression);
  IMPLEMENT_AST_OPERATORS(Function);
  IMPLEMENT_AST_OPERATORS(Function_Call);
  IMPLEMENT_AST_OPERATORS(Variable);
  IMPLEMENT_AST_OPERATORS(Number);
  IMPLEMENT_AST_OPERATORS(Color);
  IMPLEMENT_AST_OPERATORS(Custom_Error);
  IMPLEMENT_AST_OPERATORS(Custom_Warning);
  IMPLEMENT_AST_OPERATORS(Boolean);
  IMPLEMENT_AST_OPERATORS(String_Schema);
  IMPLEMENT_AST_OPERATORS(String_Constant);
  IMPLEMENT_AST_OPERATORS(String_Quoted);
  IMPLEMENT_AST_OPERATORS(Null);
  IMPLEMENT_AST_OPERATORS(Parent_Reference);

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

}