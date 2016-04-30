#ifndef SASS_AST_H
#define SASS_AST_H

#include <set>
#include <deque>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <typeinfo>
#include <algorithm>
#include <unordered_map>
#include "sass/base.h"

#ifdef __clang__

/*
 * There are some overloads used here that trigger the clang overload
 * hiding warning. Specifically:
 *
 * Type type() which hides string type() from Expression
 *
 * and
 *
 * Block* block() which hides virtual Block* block() from Statement
 *
 */

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverloaded-virtual"

#endif

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

#include "sass.h"

#include "ast/common.hpp"
#include "ast/nodes.hpp"
#include "ast/containers.hpp"
#include "ast/statements.hpp"
#include "ast/expressions.hpp"
#include "ast/blocks.hpp"
#include "ast/values.hpp"

namespace Sass {



  ////////////
  // Booleans.
  ////////////
  class Boolean : public Value {
    ADD_HASHED(bool, value)
    size_t hash_;
  public:
    Boolean(ParserState pstate, bool val)
    : Value(pstate), value_(val),
      hash_(0)
    { concrete_type(BOOLEAN); }
    virtual operator bool() { return value_; }
    std::string type() { return "bool"; }
    static std::string type_name() { return "bool"; }
    virtual bool is_false() { return !value_; }

    virtual size_t hash()
    {
      if (hash_ == 0) {
        hash_ = std::hash<bool>()(value_);
      }
      return hash_;
    }

    virtual bool operator== (const Expression& rhs) const;

    ATTACH_OPERATIONS()
  };

  ////////////////////////////////////////////////////////////////////////
  // Abstract base class for Sass string values. Includes interpolated and
  // "flat" strings.
  ////////////////////////////////////////////////////////////////////////
  class String : public Value {
  public:
    String(ParserState pstate, bool delayed = false)
    : Value(pstate, delayed)
    { concrete_type(STRING); }
    static std::string type_name() { return "string"; }
    virtual ~String() = 0;
    virtual void rtrim() = 0;
    virtual void ltrim() = 0;
    virtual void trim() = 0;
    virtual bool operator==(const Expression& rhs) const = 0;
    ATTACH_OPERATIONS()
  };
  inline String::~String() { };

  ///////////////////////////////////////////////////////////////////////
  // Interpolated strings. Meant to be reduced to flat strings during the
  // evaluation phase.
  ///////////////////////////////////////////////////////////////////////
  class String_Schema : public String, public Vectorized<Expression*> {
    // ADD_PROPERTY(bool, has_interpolants)
    size_t hash_;
  public:
    String_Schema(ParserState pstate, size_t size = 0, bool has_interpolants = false)
    : String(pstate), Vectorized<Expression*>(size), hash_(0)
    { concrete_type(STRING); }
    std::string type() { return "string"; }
    static std::string type_name() { return "string"; }

    bool is_left_interpolant(void) const;
    bool is_right_interpolant(void) const;
    // void has_interpolants(bool tc) { }
    bool has_interpolants() {
      for (auto el : elements()) {
        if (el->is_interpolant()) return true;
      }
      return false;
    }
    virtual void rtrim();
    virtual void ltrim();
    virtual void trim();

    virtual size_t hash()
    {
      if (hash_ == 0) {
        for (auto string : elements())
          hash_combine(hash_, string->hash());
      }
      return hash_;
    }

    virtual void set_delayed(bool delayed) {
      is_delayed(delayed);
    }

    virtual bool operator==(const Expression& rhs) const;

    ATTACH_OPERATIONS()
  };

  ////////////////////////////////////////////////////////
  // Flat strings -- the lowest level of raw textual data.
  ////////////////////////////////////////////////////////
  class String_Constant : public String {
    ADD_PROPERTY(char, quote_mark)
    ADD_PROPERTY(bool, can_compress_whitespace)
    ADD_HASHED(std::string, value)
  protected:
    size_t hash_;
  public:
    String_Constant(ParserState pstate, std::string val)
    : String(pstate), quote_mark_(0), can_compress_whitespace_(false), value_(read_css_string(val)), hash_(0)
    { }
    String_Constant(ParserState pstate, const char* beg)
    : String(pstate), quote_mark_(0), can_compress_whitespace_(false), value_(read_css_string(std::string(beg))), hash_(0)
    { }
    String_Constant(ParserState pstate, const char* beg, const char* end)
    : String(pstate), quote_mark_(0), can_compress_whitespace_(false), value_(read_css_string(std::string(beg, end-beg))), hash_(0)
    { }
    String_Constant(ParserState pstate, const Token& tok)
    : String(pstate), quote_mark_(0), can_compress_whitespace_(false), value_(read_css_string(std::string(tok.begin, tok.end))), hash_(0)
    { }
    std::string type() { return "string"; }
    static std::string type_name() { return "string"; }
    virtual bool is_invisible() const;
    virtual void rtrim();
    virtual void ltrim();
    virtual void trim();

    virtual size_t hash()
    {
      if (hash_ == 0) {
        hash_ = std::hash<std::string>()(value_);
      }
      return hash_;
    }

    virtual bool operator==(const Expression& rhs) const;
    virtual std::string inspect() const; // quotes are forced on inspection

    // static char auto_quote() { return '*'; }
    static char double_quote() { return '"'; }
    static char single_quote() { return '\''; }

    ATTACH_OPERATIONS()
  };

  ////////////////////////////////////////////////////////
  // Possibly quoted string (unquote on instantiation)
  ////////////////////////////////////////////////////////
  class String_Quoted : public String_Constant {
  public:
    String_Quoted(ParserState pstate, std::string val, char q = 0, bool keep_utf8_escapes = false)
    : String_Constant(pstate, val)
    {
      value_ = unquote(value_, &quote_mark_, keep_utf8_escapes);
      if (q && quote_mark_) quote_mark_ = q;
    }
    virtual bool operator==(const Expression& rhs) const;
    virtual std::string inspect() const; // quotes are forced on inspection
    ATTACH_OPERATIONS()
  };

  /////////////////
  // Media queries.
  /////////////////
  class Media_Query : public Expression,
                      public Vectorized<Media_Query_Expression*> {
    ADD_PROPERTY(String*, media_type)
    ADD_PROPERTY(bool, is_negated)
    ADD_PROPERTY(bool, is_restricted)
  public:
    Media_Query(ParserState pstate,
                String* t = 0, size_t s = 0, bool n = false, bool r = false)
    : Expression(pstate), Vectorized<Media_Query_Expression*>(s),
      media_type_(t), is_negated_(n), is_restricted_(r)
    { }
    ATTACH_OPERATIONS()
  };

  ////////////////////////////////////////////////////
  // Media expressions (for use inside media queries).
  ////////////////////////////////////////////////////
  class Media_Query_Expression : public Expression {
    ADD_PROPERTY(Expression*, feature)
    ADD_PROPERTY(Expression*, value)
    ADD_PROPERTY(bool, is_interpolated)
  public:
    Media_Query_Expression(ParserState pstate,
                           Expression* f, Expression* v, bool i = false)
    : Expression(pstate), feature_(f), value_(v), is_interpolated_(i)
    { }
    ATTACH_OPERATIONS()
  };

  ////////////////////
  // `@supports` rule.
  ////////////////////
  class Supports_Block : public Has_Block {
    ADD_PROPERTY(Supports_Condition*, condition)
  public:
    Supports_Block(ParserState pstate, Supports_Condition* condition, Block* block = 0)
    : Has_Block(pstate, block), condition_(condition)
    { statement_type(SUPPORTS); }
    bool is_hoistable() { return true; }
    bool bubbles() { return true; }
    ATTACH_OPERATIONS()
  };

  //////////////////////////////////////////////////////
  // The abstract superclass of all Supports conditions.
  //////////////////////////////////////////////////////
  class Supports_Condition : public Expression {
  public:
    Supports_Condition(ParserState pstate)
    : Expression(pstate)
    { }
    virtual bool needs_parens(Supports_Condition* cond) const { return false; }
    ATTACH_OPERATIONS()
  };

  ////////////////////////////////////////////////////////////
  // An operator condition (e.g. `CONDITION1 and CONDITION2`).
  ////////////////////////////////////////////////////////////
  class Supports_Operator : public Supports_Condition {
  public:
    enum Operand { AND, OR };
  private:
    ADD_PROPERTY(Supports_Condition*, left);
    ADD_PROPERTY(Supports_Condition*, right);
    ADD_PROPERTY(Operand, operand);
  public:
    Supports_Operator(ParserState pstate, Supports_Condition* l, Supports_Condition* r, Operand o)
    : Supports_Condition(pstate), left_(l), right_(r), operand_(o)
    { }
    virtual bool needs_parens(Supports_Condition* cond) const;
    ATTACH_OPERATIONS()
  };

  //////////////////////////////////////////
  // A negation condition (`not CONDITION`).
  //////////////////////////////////////////
  class Supports_Negation : public Supports_Condition {
  private:
    ADD_PROPERTY(Supports_Condition*, condition);
  public:
    Supports_Negation(ParserState pstate, Supports_Condition* c)
    : Supports_Condition(pstate), condition_(c)
    { }
    virtual bool needs_parens(Supports_Condition* cond) const;
    ATTACH_OPERATIONS()
  };

  /////////////////////////////////////////////////////
  // A declaration condition (e.g. `(feature: value)`).
  /////////////////////////////////////////////////////
  class Supports_Declaration : public Supports_Condition {
  private:
    ADD_PROPERTY(Expression*, feature);
    ADD_PROPERTY(Expression*, value);
  public:
    Supports_Declaration(ParserState pstate, Expression* f, Expression* v)
    : Supports_Condition(pstate), feature_(f), value_(v)
    { }
    virtual bool needs_parens(Supports_Condition* cond) const { return false; }
    ATTACH_OPERATIONS()
  };

  ///////////////////////////////////////////////
  // An interpolation condition (e.g. `#{$var}`).
  ///////////////////////////////////////////////
  class Supports_Interpolation : public Supports_Condition {
  private:
    ADD_PROPERTY(Expression*, value);
  public:
    Supports_Interpolation(ParserState pstate, Expression* v)
    : Supports_Condition(pstate), value_(v)
    { }
    virtual bool needs_parens(Supports_Condition* cond) const { return false; }
    ATTACH_OPERATIONS()
  };

  /////////////////////////////////////////////////
  // At root expressions (for use inside @at-root).
  /////////////////////////////////////////////////
  class At_Root_Query : public Expression {
  private:
    ADD_PROPERTY(Expression*, feature)
    ADD_PROPERTY(Expression*, value)
  public:
    At_Root_Query(ParserState pstate, Expression* f = 0, Expression* v = 0, bool i = false)
    : Expression(pstate), feature_(f), value_(v)
    { }
    bool exclude(std::string str);
    ATTACH_OPERATIONS()
  };

  ///////////
  // At-root.
  ///////////
  class At_Root_Block : public Has_Block {
    ADD_PROPERTY(At_Root_Query*, expression)
  public:
    At_Root_Block(ParserState pstate, Block* b = 0, At_Root_Query* e = 0)
    : Has_Block(pstate, b), expression_(e)
    { statement_type(ATROOT); }
    bool is_hoistable() { return true; }
    bool bubbles() { return true; }
    bool exclude_node(Statement* s) {
      if (s->statement_type() == Statement::DIRECTIVE)
      {
        return expression()->exclude(static_cast<Directive*>(s)->keyword().erase(0, 1));
      }
      if (s->statement_type() == Statement::MEDIA)
      {
        return expression()->exclude("media");
      }
      if (s->statement_type() == Statement::RULESET)
      {
        return expression()->exclude("rule");
      }
      if (s->statement_type() == Statement::SUPPORTS)
      {
        return expression()->exclude("supports");
      }
      if (static_cast<Directive*>(s)->is_keyframes())
      {
        return expression()->exclude("keyframes");
      }
      return false;
    }
    ATTACH_OPERATIONS()
  };

  //////////////////
  // The null value.
  //////////////////
  class Null : public Value {
  public:
    Null(ParserState pstate) : Value(pstate) { concrete_type(NULL_VAL); }
    std::string type() { return "null"; }
    static std::string type_name() { return "null"; }
    bool is_invisible() const { return true; }
    operator bool() { return false; }
    bool is_false() { return true; }

    virtual size_t hash()
    {
      return -1;
    }

    virtual bool operator== (const Expression& rhs) const;

    ATTACH_OPERATIONS()
  };

  /////////////////////////////////
  // Thunks for delayed evaluation.
  /////////////////////////////////
  class Thunk : public Expression {
    ADD_PROPERTY(Expression*, expression)
    ADD_PROPERTY(Env*, environment)
  public:
    Thunk(ParserState pstate, Expression* exp, Env* env = 0)
    : Expression(pstate), expression_(exp), environment_(env)
    { }
  };

  /////////////////////////////////////////////////////////
  // Individual parameter objects for mixins and functions.
  /////////////////////////////////////////////////////////
  class Parameter : public AST_Node {
    ADD_PROPERTY(std::string, name)
    ADD_PROPERTY(Expression*, default_value)
    ADD_PROPERTY(bool, is_rest_parameter)
  public:
    Parameter(ParserState pstate,
              std::string n, Expression* def = 0, bool rest = false)
    : AST_Node(pstate), name_(n), default_value_(def), is_rest_parameter_(rest)
    {
      if (default_value_ && is_rest_parameter_) {
        error("variable-length parameter may not have a default value", pstate);
      }
    }
    ATTACH_OPERATIONS()
  };

  /////////////////////////////////////////////////////////////////////////
  // Parameter lists -- in their own class to facilitate context-sensitive
  // error checking (e.g., ensuring that all optional parameters follow all
  // required parameters).
  /////////////////////////////////////////////////////////////////////////
  class Parameters : public AST_Node, public Vectorized<Parameter*> {
    ADD_PROPERTY(bool, has_optional_parameters)
    ADD_PROPERTY(bool, has_rest_parameter)
  protected:
    void adjust_after_pushing(Parameter* p)
    {
      if (p->default_value()) {
        if (has_rest_parameter_) {
          error("optional parameters may not be combined with variable-length parameters", p->pstate());
        }
        has_optional_parameters_ = true;
      }
      else if (p->is_rest_parameter()) {
        if (has_rest_parameter_) {
          error("functions and mixins cannot have more than one variable-length parameter", p->pstate());
        }
        has_rest_parameter_ = true;
      }
      else {
        if (has_rest_parameter_) {
          error("required parameters must precede variable-length parameters", p->pstate());
        }
        if (has_optional_parameters_) {
          error("required parameters must precede optional parameters", p->pstate());
        }
      }
    }
  public:
    Parameters(ParserState pstate)
    : AST_Node(pstate),
      Vectorized<Parameter*>(),
      has_optional_parameters_(false),
      has_rest_parameter_(false)
    { }
    ATTACH_OPERATIONS()
  };

  /////////////////////////////////////////
  // Abstract base class for CSS selectors.
  /////////////////////////////////////////
  class Selector : public Expression {
    // ADD_PROPERTY(bool, has_reference)
    ADD_PROPERTY(bool, has_placeholder)
    // line break before list separator
    ADD_PROPERTY(bool, has_line_feed)
    // line break after list separator
    ADD_PROPERTY(bool, has_line_break)
    // maybe we have optional flag
    ADD_PROPERTY(bool, is_optional)
    // parent block pointers
    ADD_PROPERTY(Media_Block*, media_block)
  protected:
    size_t hash_;
  public:
    Selector(ParserState pstate, bool r = false, bool h = false)
    : Expression(pstate),
      // has_reference_(r),
      has_placeholder_(h),
      has_line_feed_(false),
      has_line_break_(false),
      is_optional_(false),
      media_block_(0),
      hash_(0)
    { concrete_type(SELECTOR); }
    virtual ~Selector() = 0;
    virtual size_t hash() = 0;
    virtual bool has_parent_ref() {
      return false;
    }
    virtual unsigned long specificity() {
      return Constants::Specificity_Universal;
    }
    virtual void set_media_block(Media_Block* mb) {
      media_block(mb);
    }
    virtual bool has_wrapped_selector() {
      return false;
    }
  };
  inline Selector::~Selector() { }

  /////////////////////////////////////////////////////////////////////////
  // Interpolated selectors -- the interpolated String will be expanded and
  // re-parsed into a normal selector class.
  /////////////////////////////////////////////////////////////////////////
  class Selector_Schema : public Selector {
    ADD_PROPERTY(String*, contents)
    ADD_PROPERTY(bool, at_root);
  public:
    Selector_Schema(ParserState pstate, String* c)
    : Selector(pstate), contents_(c), at_root_(false)
    { }
    virtual bool has_parent_ref();
    virtual size_t hash() {
      if (hash_ == 0) {
        hash_combine(hash_, contents_->hash());
      }
      return hash_;
    }
    ATTACH_OPERATIONS()
  };

  ////////////////////////////////////////////
  // Abstract base class for simple selectors.
  ////////////////////////////////////////////
  class Simple_Selector : public Selector {
    ADD_PROPERTY(std::string, ns);
    ADD_PROPERTY(std::string, name)
    ADD_PROPERTY(bool, has_ns)
  public:
    Simple_Selector(ParserState pstate, std::string n = "")
    : Selector(pstate), ns_(""), name_(n), has_ns_(false)
    {
      size_t pos = n.find('|');
      // found some namespace
      if (pos != std::string::npos) {
        has_ns_ = true;
        ns_ = n.substr(0, pos);
        name_ = n.substr(pos + 1);
      }
    }
    virtual std::string ns_name() const
    {
      std::string name("");
      if (has_ns_)
        name += ns_ + "|";
      return name + name_;
    }
    virtual size_t hash()
    {
      if (hash_ == 0) {
        hash_combine(hash_, std::hash<int>()(SELECTOR));
        hash_combine(hash_, std::hash<std::string>()(ns()));
        hash_combine(hash_, std::hash<std::string>()(name()));
      }
      return hash_;
    }
    // namespace query functions
    bool is_universal_ns() const
    {
      return has_ns_ && ns_ == "*";
    }
    bool has_universal_ns() const
    {
      return !has_ns_ || ns_ == "*";
    }
    bool is_empty_ns() const
    {
      return !has_ns_ || ns_ == "";
    }
    bool has_empty_ns() const
    {
      return has_ns_ && ns_ == "";
    }
    bool has_qualified_ns() const
    {
      return has_ns_ && ns_ != "" && ns_ != "*";
    }
    // name query functions
    bool is_universal() const
    {
      return name_ == "*";
    }

    virtual ~Simple_Selector() = 0;
    virtual Compound_Selector* unify_with(Compound_Selector*, Context&);
    virtual bool has_parent_ref() { return false; };
    virtual bool is_pseudo_element() { return false; }
    virtual bool is_pseudo_class() { return false; }

    virtual bool is_superselector_of(Compound_Selector* sub) { return false; }

    bool operator==(const Simple_Selector& rhs) const;
    inline bool operator!=(const Simple_Selector& rhs) const { return !(*this == rhs); }

    bool operator<(const Simple_Selector& rhs) const;
    // default implementation should work for most of the simple selectors (otherwise overload)
    ATTACH_OPERATIONS();
  };
  inline Simple_Selector::~Simple_Selector() { }


  //////////////////////////////////
  // The Parent Selector Expression.
  //////////////////////////////////
  // parent selectors can occur in selectors but also
  // inside strings in declarations (Compound_Selector).
  // only one simple parent selector means the first case.
  class Parent_Selector : public Simple_Selector {
  public:
    Parent_Selector(ParserState pstate)
    : Simple_Selector(pstate, "&")
    { /* has_reference(true); */ }
    virtual bool has_parent_ref() { return true; };
    virtual unsigned long specificity()
    {
      return 0;
    }
    std::string type() { return "selector"; }
    static std::string type_name() { return "selector"; }
    ATTACH_OPERATIONS()
  };

  /////////////////////////////////////////////////////////////////////////
  // Placeholder selectors (e.g., "%foo") for use in extend-only selectors.
  /////////////////////////////////////////////////////////////////////////
  class Selector_Placeholder : public Simple_Selector {
  public:
    Selector_Placeholder(ParserState pstate, std::string n)
    : Simple_Selector(pstate, n)
    { has_placeholder(true); }
    // virtual Selector_Placeholder* find_placeholder();
    virtual ~Selector_Placeholder() {};
    ATTACH_OPERATIONS()
  };

  /////////////////////////////////////////////////////////////////////
  // Type selectors (and the universal selector) -- e.g., div, span, *.
  /////////////////////////////////////////////////////////////////////
  class Type_Selector : public Simple_Selector {
  public:
    Type_Selector(ParserState pstate, std::string n)
    : Simple_Selector(pstate, n)
    { }
    virtual unsigned long specificity()
    {
      // ToDo: What is the specificity of the star selector?
      if (name() == "*") return Constants::Specificity_Universal;
      else               return Constants::Specificity_Type;
    }
    virtual Simple_Selector* unify_with(Simple_Selector*, Context&);
    virtual Compound_Selector* unify_with(Compound_Selector*, Context&);
    ATTACH_OPERATIONS()
  };

  ////////////////////////////////////////////////
  // Selector qualifiers -- i.e., classes and ids.
  ////////////////////////////////////////////////
  class Selector_Qualifier : public Simple_Selector {
  public:
    Selector_Qualifier(ParserState pstate, std::string n)
    : Simple_Selector(pstate, n)
    { }
    virtual unsigned long specificity()
    {
      if (name()[0] == '#') return Constants::Specificity_ID;
      if (name()[0] == '.') return Constants::Specificity_Class;
      else                  return Constants::Specificity_Type;
    }
    virtual Compound_Selector* unify_with(Compound_Selector*, Context&);
    ATTACH_OPERATIONS()
  };

  ///////////////////////////////////////////////////
  // Attribute selectors -- e.g., [src*=".jpg"], etc.
  ///////////////////////////////////////////////////
  class Attribute_Selector : public Simple_Selector {
    ADD_PROPERTY(std::string, matcher)
    ADD_PROPERTY(String*, value) // might be interpolated
  public:
    Attribute_Selector(ParserState pstate, std::string n, std::string m, String* v)
    : Simple_Selector(pstate, n), matcher_(m), value_(v)
    { }
    virtual size_t hash()
    {
      if (hash_ == 0) {
        hash_combine(hash_, Simple_Selector::hash());
        hash_combine(hash_, std::hash<std::string>()(matcher()));
        if (value_) hash_combine(hash_, value_->hash());
      }
      return hash_;
    }
    virtual unsigned long specificity()
    {
      return Constants::Specificity_Attr;
    }
    bool operator==(const Simple_Selector& rhs) const;
    bool operator==(const Attribute_Selector& rhs) const;
    bool operator<(const Simple_Selector& rhs) const;
    bool operator<(const Attribute_Selector& rhs) const;
    ATTACH_OPERATIONS()
  };

  //////////////////////////////////////////////////////////////////
  // Pseudo selectors -- e.g., :first-child, :nth-of-type(...), etc.
  //////////////////////////////////////////////////////////////////
  /* '::' starts a pseudo-element, ':' a pseudo-class */
  /* Except :first-line, :first-letter, :before and :after */
  /* Note that pseudo-elements are restricted to one per selector */
  /* and occur only in the last simple_selector_sequence. */
  inline bool is_pseudo_class_element(const std::string& name)
  {
    return name == ":before"       ||
           name == ":after"        ||
           name == ":first-line"   ||
           name == ":first-letter";
  }

  // Pseudo Selector cannot have any namespace?
  class Pseudo_Selector : public Simple_Selector {
    ADD_PROPERTY(String*, expression)
  public:
    Pseudo_Selector(ParserState pstate, std::string n, String* expr = 0)
    : Simple_Selector(pstate, n), expression_(expr)
    { }

    // A pseudo-class always consists of a "colon" (:) followed by the name
    // of the pseudo-class and optionally by a value between parentheses.
    virtual bool is_pseudo_class()
    {
      return (name_[0] == ':' && name_[1] != ':')
             && ! is_pseudo_class_element(name_);
    }

    // A pseudo-element is made of two colons (::) followed by the name.
    // The `::` notation is introduced by the current document in order to
    // establish a discrimination between pseudo-classes and pseudo-elements.
    // For compatibility with existing style sheets, user agents must also
    // accept the previous one-colon notation for pseudo-elements introduced
    // in CSS levels 1 and 2 (namely, :first-line, :first-letter, :before and
    // :after). This compatibility is not allowed for the new pseudo-elements
    // introduced in this specification.
    virtual bool is_pseudo_element()
    {
      return (name_[0] == ':' && name_[1] == ':')
             || is_pseudo_class_element(name_);
    }
    virtual size_t hash()
    {
      if (hash_ == 0) {
        hash_combine(hash_, Simple_Selector::hash());
        if (expression_) hash_combine(hash_, expression_->hash());
      }
      return hash_;
    }
    virtual unsigned long specificity()
    {
      if (is_pseudo_element())
        return Constants::Specificity_Type;
      return Constants::Specificity_Pseudo;
    }
    bool operator==(const Simple_Selector& rhs) const;
    bool operator==(const Pseudo_Selector& rhs) const;
    bool operator<(const Simple_Selector& rhs) const;
    bool operator<(const Pseudo_Selector& rhs) const;
    virtual Compound_Selector* unify_with(Compound_Selector*, Context&);
    ATTACH_OPERATIONS()
  };

  /////////////////////////////////////////////////
  // Wrapped selector -- pseudo selector that takes a list of selectors as argument(s) e.g., :not(:first-of-type), :-moz-any(ol p.blah, ul, menu, dir)
  /////////////////////////////////////////////////
  class Wrapped_Selector : public Simple_Selector {
    ADD_PROPERTY(Selector*, selector)
  public:
    Wrapped_Selector(ParserState pstate, std::string n, Selector* sel)
    : Simple_Selector(pstate, n), selector_(sel)
    { }
    virtual bool has_parent_ref() {
      // if (has_reference()) return true;
      if (!selector()) return false;
      return selector()->has_parent_ref();
    }
    virtual bool is_superselector_of(Wrapped_Selector* sub);
    // Selectors inside the negation pseudo-class are counted like any
    // other, but the negation itself does not count as a pseudo-class.
    virtual size_t hash()
    {
      if (hash_ == 0) {
        hash_combine(hash_, Simple_Selector::hash());
        if (selector_) hash_combine(hash_, selector_->hash());
      }
      return hash_;
    }
    virtual bool has_wrapped_selector()
    {
      return true;
    }
    virtual unsigned long specificity()
    {
      return selector_ ? selector_->specificity() : 0;
    }
    bool operator==(const Simple_Selector& rhs) const;
    bool operator==(const Wrapped_Selector& rhs) const;
    bool operator<(const Simple_Selector& rhs) const;
    bool operator<(const Wrapped_Selector& rhs) const;
    ATTACH_OPERATIONS()
  };

  struct Complex_Selector_Pointer_Compare {
    bool operator() (const Complex_Selector* const pLeft, const Complex_Selector* const pRight) const;
  };

  ////////////////////////////////////////////////////////////////////////////
  // Simple selector sequences. Maintains flags indicating whether it contains
  // any parent references or placeholders, to simplify expansion.
  ////////////////////////////////////////////////////////////////////////////
  typedef std::set<Complex_Selector*, Complex_Selector_Pointer_Compare> SourcesSet;
  class Compound_Selector : public Selector, public Vectorized<Simple_Selector*> {
  private:
    SourcesSet sources_;
    ADD_PROPERTY(bool, extended);
    ADD_PROPERTY(bool, has_parent_reference);
  protected:
    void adjust_after_pushing(Simple_Selector* s)
    {
      // if (s->has_reference())   has_reference(true);
      if (s->has_placeholder()) has_placeholder(true);
    }
  public:
    Compound_Selector(ParserState pstate, size_t s = 0)
    : Selector(pstate),
      Vectorized<Simple_Selector*>(s),
      extended_(false),
      has_parent_reference_(false)
    { }
    bool contains_placeholder() {
      for (size_t i = 0, L = length(); i < L; ++i) {
        if ((*this)[i]->has_placeholder()) return true;
      }
      return false;
    };

    bool is_universal() const
    {
      return length() == 1 && (*this)[0]->is_universal();
    }

    Complex_Selector* to_complex(Memory_Manager& mem);
    Compound_Selector* unify_with(Compound_Selector* rhs, Context& ctx);
    // virtual Selector_Placeholder* find_placeholder();
    virtual bool has_parent_ref();
    Simple_Selector* base()
    {
      // Implement non-const in terms of const. Safe to const_cast since this method is non-const
      return const_cast<Simple_Selector*>(static_cast<const Compound_Selector*>(this)->base());
    }
    const Simple_Selector* base() const {
      if (length() == 0) return 0;
      // ToDo: why is this needed?
      if (dynamic_cast<Type_Selector*>((*this)[0]))
        return (*this)[0];
      return 0;
    }
    virtual bool is_superselector_of(Compound_Selector* sub, std::string wrapped = "");
    virtual bool is_superselector_of(Complex_Selector* sub, std::string wrapped = "");
    virtual bool is_superselector_of(Selector_List* sub, std::string wrapped = "");
    virtual size_t hash()
    {
      if (Selector::hash_ == 0) {
        hash_combine(Selector::hash_, std::hash<int>()(SELECTOR));
        if (length()) hash_combine(Selector::hash_, Vectorized::hash());
      }
      return Selector::hash_;
    }
    virtual unsigned long specificity()
    {
      int sum = 0;
      for (size_t i = 0, L = length(); i < L; ++i)
      { sum += (*this)[i]->specificity(); }
      return sum;
    }

    virtual bool has_wrapped_selector()
    {
      if (length() == 0) return false;
      if (Simple_Selector* ss = elements().front()) {
        if (ss->has_wrapped_selector()) return true;
      }
      return false;
    }

    bool is_empty_reference()
    {
      return length() == 1 &&
             dynamic_cast<Parent_Selector*>((*this)[0]);
    }
    std::vector<std::string> to_str_vec(); // sometimes need to convert to a flat "by-value" data structure

    bool operator<(const Compound_Selector& rhs) const;

    bool operator==(const Compound_Selector& rhs) const;
    inline bool operator!=(const Compound_Selector& rhs) const { return !(*this == rhs); }

    SourcesSet& sources() { return sources_; }
    void clearSources() { sources_.clear(); }
    void mergeSources(SourcesSet& sources, Context& ctx);

    Compound_Selector* clone(Context&) const; // does not clone the Simple_Selector*s

    Compound_Selector* minus(Compound_Selector* rhs, Context& ctx);
    ATTACH_OPERATIONS()
  };

  ////////////////////////////////////////////////////////////////////////////
  // General selectors -- i.e., simple sequences combined with one of the four
  // CSS selector combinators (">", "+", "~", and whitespace). Essentially a
  // linked list.
  ////////////////////////////////////////////////////////////////////////////
  class Complex_Selector : public Selector {
  public:
    enum Combinator { ANCESTOR_OF, PARENT_OF, PRECEDES, ADJACENT_TO, REFERENCE };
  private:
    ADD_PROPERTY(Combinator, combinator)
    ADD_PROPERTY(Compound_Selector*, head)
    ADD_PROPERTY(Complex_Selector*, tail)
    ADD_PROPERTY(String*, reference);
  public:
    bool contains_placeholder() {
      if (head() && head()->contains_placeholder()) return true;
      if (tail() && tail()->contains_placeholder()) return true;
      return false;
    };
    Complex_Selector(ParserState pstate,
                     Combinator c = ANCESTOR_OF,
                     Compound_Selector* h = 0,
                     Complex_Selector* t = 0,
                     String* r = 0)
    : Selector(pstate),
      combinator_(c),
      head_(h), tail_(t),
      reference_(r)
    {
      // if ((h && h->has_reference())   || (t && t->has_reference()))   has_reference(true);
      if ((h && h->has_placeholder()) || (t && t->has_placeholder())) has_placeholder(true);
    }
    virtual bool has_parent_ref();

    Complex_Selector* skip_empty_reference()
    {
      if ((!head_ || !head_->length() || head_->is_empty_reference()) &&
          combinator() == Combinator::ANCESTOR_OF)
      {
        if (!tail_) return 0;
        tail_->has_line_feed_ = this->has_line_feed_;
        // tail_->has_line_break_ = this->has_line_break_;
        return tail_->skip_empty_reference();
      }
      return this;
    }

    // can still have a tail
    bool is_empty_ancestor() const
    {
      return (!head() || head()->length() == 0) &&
             combinator() == Combinator::ANCESTOR_OF;
    }

    Complex_Selector* context(Context&);


    // front returns the first real tail
    // skips over parent and empty ones
    const Complex_Selector* first() const;

    // last returns the last real tail
    const Complex_Selector* last() const;

    Selector_List* tails(Context& ctx, Selector_List* tails);

    // unconstant accessors
    Complex_Selector* first();
    Complex_Selector* last();

    // some shortcuts that should be removed
    const Complex_Selector* innermost() const { return last(); };
    Complex_Selector* innermost() { return last(); };

    size_t length() const;
    Selector_List* parentize(Selector_List* parents, Context& ctx);
    virtual bool is_superselector_of(Compound_Selector* sub, std::string wrapping = "");
    virtual bool is_superselector_of(Complex_Selector* sub, std::string wrapping = "");
    virtual bool is_superselector_of(Selector_List* sub, std::string wrapping = "");
    // virtual Selector_Placeholder* find_placeholder();
    Selector_List* unify_with(Complex_Selector* rhs, Context& ctx);
    Combinator clear_innermost();
    void append(Context&, Complex_Selector*);
    void set_innermost(Complex_Selector*, Combinator);
    virtual size_t hash()
    {
      if (hash_ == 0) {
        hash_combine(hash_, std::hash<int>()(SELECTOR));
        hash_combine(hash_, std::hash<int>()(combinator_));
        if (head_) hash_combine(hash_, head_->hash());
        if (tail_) hash_combine(hash_, tail_->hash());
      }
      return hash_;
    }
    virtual unsigned long specificity() const
    {
      int sum = 0;
      if (head()) sum += head()->specificity();
      if (tail()) sum += tail()->specificity();
      return sum;
    }
    virtual void set_media_block(Media_Block* mb) {
      media_block(mb);
      if (tail_) tail_->set_media_block(mb);
      if (head_) head_->set_media_block(mb);
    }
    virtual bool has_wrapped_selector() {
      if (head_ && head_->has_wrapped_selector()) return true;
      if (tail_ && tail_->has_wrapped_selector()) return true;
      return false;
    }
    bool operator<(const Complex_Selector& rhs) const;
    bool operator==(const Complex_Selector& rhs) const;
    inline bool operator!=(const Complex_Selector& rhs) const { return !(*this == rhs); }
    SourcesSet sources()
    {
      //s = Set.new
      //seq.map {|sseq_or_op| s.merge sseq_or_op.sources if sseq_or_op.is_a?(SimpleSequence)}
      //s

      SourcesSet srcs;

      Compound_Selector* pHead = head();
      Complex_Selector*  pTail = tail();

      if (pHead) {
        SourcesSet& headSources = pHead->sources();
        srcs.insert(headSources.begin(), headSources.end());
      }

      if (pTail) {
        SourcesSet tailSources = pTail->sources();
        srcs.insert(tailSources.begin(), tailSources.end());
      }

      return srcs;
    }
    void addSources(SourcesSet& sources, Context& ctx) {
      // members.map! {|m| m.is_a?(SimpleSequence) ? m.with_more_sources(sources) : m}
      Complex_Selector* pIter = this;
      while (pIter) {
        Compound_Selector* pHead = pIter->head();

        if (pHead) {
          pHead->mergeSources(sources, ctx);
        }

        pIter = pIter->tail();
      }
    }
    void clearSources() {
      Complex_Selector* pIter = this;
      while (pIter) {
        Compound_Selector* pHead = pIter->head();

        if (pHead) {
          pHead->clearSources();
        }

        pIter = pIter->tail();
      }
    }
    Complex_Selector* clone(Context&) const;      // does not clone Compound_Selector*s
    Complex_Selector* cloneFully(Context&) const; // clones Compound_Selector*s
    // std::vector<Compound_Selector*> to_vector();
    ATTACH_OPERATIONS()
  };

  typedef std::deque<Complex_Selector*> ComplexSelectorDeque;
  typedef Subset_Map<std::string, std::pair<Complex_Selector*, Compound_Selector*> > ExtensionSubsetMap;

  ///////////////////////////////////
  // Comma-separated selector groups.
  ///////////////////////////////////
  class Selector_List : public Selector, public Vectorized<Complex_Selector*> {
    ADD_PROPERTY(std::vector<std::string>, wspace)
  protected:
    void adjust_after_pushing(Complex_Selector* c);
  public:
    Selector_List(ParserState pstate, size_t s = 0)
    : Selector(pstate), Vectorized<Complex_Selector*>(s), wspace_(0)
    { }
    std::string type() { return "list"; }
    // remove parent selector references
    // basically unwraps parsed selectors
    virtual bool has_parent_ref();
    void remove_parent_selectors();
    // virtual Selector_Placeholder* find_placeholder();
    Selector_List* parentize(Selector_List* parents, Context& ctx);
    virtual bool is_superselector_of(Compound_Selector* sub, std::string wrapping = "");
    virtual bool is_superselector_of(Complex_Selector* sub, std::string wrapping = "");
    virtual bool is_superselector_of(Selector_List* sub, std::string wrapping = "");
    Selector_List* unify_with(Selector_List*, Context&);
    void populate_extends(Selector_List*, Context&, ExtensionSubsetMap&);
    virtual size_t hash()
    {
      if (Selector::hash_ == 0) {
        hash_combine(Selector::hash_, std::hash<int>()(SELECTOR));
        hash_combine(Selector::hash_, Vectorized::hash());
      }
      return Selector::hash_;
    }
    virtual unsigned long specificity()
    {
      unsigned long sum = 0;
      unsigned long specificity = 0;
      for (size_t i = 0, L = length(); i < L; ++i)
      {
        specificity = (*this)[i]->specificity();
        if (sum < specificity) sum = specificity;
      }
      return sum;
    }
    virtual void set_media_block(Media_Block* mb) {
      media_block(mb);
      for (Complex_Selector* cs : elements()) {
        cs->set_media_block(mb);
      }
    }
    virtual bool has_wrapped_selector() {
      for (Complex_Selector* cs : elements()) {
        if (cs->has_wrapped_selector()) return true;
      }
      return false;
    }
    Selector_List* clone(Context&) const;      // does not clone Compound_Selector*s
    Selector_List* cloneFully(Context&) const; // clones Compound_Selector*s
    virtual bool operator==(const Selector& rhs) const;
    virtual bool operator==(const Selector_List& rhs) const;
    // Selector Lists can be compared to comma lists
    virtual bool operator==(const Expression& rhs) const;
    ATTACH_OPERATIONS()
  };

  template<typename SelectorType>
  bool selectors_equal(const SelectorType& one, const SelectorType& two, bool simpleSelectorOrderDependent) {
    // Test for equality among selectors while differentiating between checks that demand the underlying Simple_Selector
    // ordering to be the same or not. This works because operator< (which doesn't make a whole lot of sense for selectors, but
    // is required for proper stl collection ordering) is implemented using string comparision. This gives stable sorting
    // behavior, and can be used to determine if the selectors would have exactly idential output. operator== matches the
    // ruby sass implementations for eql, which sometimes perform order independent comparisions (like set comparisons of the
    // members of a SimpleSequence (Compound_Selector)).
    //
    // Due to the reliance on operator== and operater< behavior, this templated method is currently only intended for
    // use with Compound_Selector and Complex_Selector objects.
    if (simpleSelectorOrderDependent) {
      return !(one < two) && !(two < one);
    } else {
      return one == two;
    }
  }

  // compare function for sorting and probably other other uses
  struct cmp_complex_selector { inline bool operator() (const Complex_Selector* l, const Complex_Selector* r) { return (*l < *r); } };
  struct cmp_compound_selector { inline bool operator() (const Compound_Selector* l, const Compound_Selector* r) { return (*l < *r); } };
  struct cmp_simple_selector { inline bool operator() (const Simple_Selector* l, const Simple_Selector* r) { return (*l < *r); } };

}

#ifdef __clang__

#pragma clang diagnostic pop

#endif

#endif
