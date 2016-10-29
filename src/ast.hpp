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

namespace Sass {

  // easier to search with name
  const bool DELAYED = true;

  // ToDo: should this really be hardcoded
  // Note: most methods follow precision option
  const double NUMBER_EPSILON = 0.00000000000001;

  // ToDo: where does this fit best?
  // We don't share this with C-API?
  class Operand {
    public:
      Operand(Sass_OP operand, bool ws_before = false, bool ws_after = false)
      : operand(operand), ws_before(ws_before), ws_after(ws_after)
      { }
    public:
      enum Sass_OP operand;
      bool ws_before;
      bool ws_after;
  };

  //////////////////////////////////////////////////////////
  // `hash_combine` comes from boost (functional/hash):
  // http://www.boost.org/doc/libs/1_35_0/doc/html/hash/combine.html
  // Boost Software License - Version 1.0
  // http://www.boost.org/users/license.html
  template <typename T>
  void hash_combine (std::size_t& seed, const T& val)
  {
    seed ^= std::hash<T>()(val) + 0x9e3779b9
             + (seed<<6) + (seed>>2);
  }
  //////////////////////////////////////////////////////////

  //////////////////////////////////////////////////////////
  // Abstract base class for all abstract syntax tree nodes.
  //////////////////////////////////////////////////////////
  class AST_Node_Ref : public Memory_Object {
    ADD_PROPERTY(ParserState, pstate)
  public:
    AST_Node_Ref(ParserState pstate)
    : pstate_(pstate)
    { }
    virtual ~AST_Node_Ref() = 0;
    virtual size_t hash() { return 0; }
    virtual AST_Node_Ptr copy(Memory_Manager& mem, bool recursive = false);
    virtual std::string inspect() const { return to_string({ INSPECT, 5 }); }
    virtual std::string to_sass() const { return to_string({ TO_SASS, 5 }); }
    virtual std::string to_string(Sass_Inspect_Options opt) const;
    virtual std::string to_string() const;
  public:
    void update_pstate(const ParserState& pstate);
    void set_pstate_offset(const Offset& offset);
  public:
    Offset off() { return pstate(); }
    Position pos() { return pstate(); }
    ATTACH_OPERATIONS()
  };
  inline AST_Node_Ref::~AST_Node_Ref() { }


  //////////////////////////////////////////////////////////////////////
  // Abstract base class for expressions. This side of the AST hierarchy
  // represents elements in value contexts, which exist primarily to be
  // evaluated and returned.
  //////////////////////////////////////////////////////////////////////
  class Expression_Ref : public AST_Node_Ref {
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
    Expression_Ref(ParserState pstate,
               bool d = false, bool e = false, bool i = false, Concrete_Type ct = NONE)
    : AST_Node_Ref(pstate),
      is_delayed_(d),
      is_expanded_(e),
      is_interpolant_(i),
      concrete_type_(ct)
    { }
    virtual operator bool() { return true; }
    virtual ~Expression_Ref() { }
    virtual std::string type() { return ""; /* TODO: raise an error? */ }
    virtual bool is_invisible() const { return false; }
    static std::string type_name() { return ""; }
    virtual bool is_false() { return false; }
    virtual bool operator== (const Expression& rhs) const { return false; }
    virtual bool eq(const Expression& rhs) const { return *this == rhs; };
    virtual void set_delayed(bool delayed) { is_delayed(delayed); }
    virtual bool has_interpolant() const { return is_interpolant(); }
    virtual bool is_left_interpolant() const { return is_interpolant(); }
    virtual bool is_right_interpolant() const { return is_interpolant(); }
    virtual std::string inspect() const { return to_string({ INSPECT, 5 }); }
    virtual std::string to_sass() const { return to_string({ TO_SASS, 5 }); }
    virtual Expression_Ptr copy(Memory_Manager& mem, bool recursive = false);
    virtual size_t hash() { return 0; }
  };

  //////////////////////////////////////////////////////////////////////
  // Still just an expression, but with a to_string method
  //////////////////////////////////////////////////////////////////////
  class PreValue_Ref : public Expression_Ref {
  public:
    PreValue_Ref(ParserState pstate,
               bool d = false, bool e = false, bool i = false, Concrete_Type ct = NONE)
    : Expression_Ref(pstate, d, e, i, ct)
    { }
    virtual PreValue_Ptr copy(Memory_Manager& mem, bool recursive = false);
    virtual ~PreValue_Ref() { }
  };

  //////////////////////////////////////////////////////////////////////
  // base class for values that support operations
  //////////////////////////////////////////////////////////////////////
  class Value_Ref : public Expression_Ref {
  public:
    Value_Ref(ParserState pstate,
          bool d = false, bool e = false, bool i = false, Concrete_Type ct = NONE)
    : Expression_Ref(pstate, d, e, i, ct)
    { }
    virtual Value_Ptr copy(Memory_Manager& mem, bool recursive = false);
    virtual bool operator== (const Expression& rhs) const = 0;
  };
}

/////////////////////////////////////////////////////////////////////////////////////
// Hash method specializations for std::unordered_map to work with Sass::Expression
/////////////////////////////////////////////////////////////////////////////////////

namespace std {
  template<>
  struct hash<Sass::Expression_Ptr>
  {
    size_t operator()(Sass::Expression_Ptr s) const
    {
      return s->hash();
    }
  };
  template<>
  struct equal_to<Sass::Expression_Ptr>
  {
    bool operator()( Sass::Expression_Ptr lhs,  Sass::Expression_Ptr rhs) const
    {
      return lhs->hash() == rhs->hash();
    }
  };
}

namespace Sass {

  /////////////////////////////////////////////////////////////////////////////
  // Mixin class for AST nodes that should behave like vectors. Uses the
  // "Template Method" design pattern to allow subclasses to adjust their flags
  // when certain objects are pushed.
  /////////////////////////////////////////////////////////////////////////////
  template <typename T>
  class Vectorized {
    std::vector<T> elements_;
  protected:
    size_t hash_;
    void reset_hash() { hash_ = 0; }
    virtual void adjust_after_pushing(T element) { }
  public:
    Vectorized(size_t s = 0) : elements_(std::vector<T>()), hash_(0)
    { elements_.reserve(s); }
    virtual ~Vectorized() = 0;
    size_t length() const   { return elements_.size(); }
    bool empty() const      { return elements_.empty(); }
    T last() const          { return elements_.back(); }
    T first() const         { return elements_.front(); }
    T& operator[](size_t i) { return elements_[i]; }
    virtual const T& at(size_t i) const { return elements_.at(i); }
    const T& operator[](size_t i) const { return elements_[i]; }
    virtual Vectorized& append(T element)
    {
      if (!element) return *this;
      reset_hash();
      elements_.push_back(element);
      adjust_after_pushing(element);
      return *this;
    }
    virtual Vectorized& operator<<(T element)
    {
      if (!element) return *this;
      reset_hash();
      elements_.push_back(element);
      adjust_after_pushing(element);
      return *this;
    }
    Vectorized& concat(Vectorized* v)
    {
      for (size_t i = 0, L = v->length(); i < L; ++i) *this << (*v)[i];
      return *this;
    }
    Vectorized& operator+=(Vectorized* v)
    {
      for (size_t i = 0, L = v->length(); i < L; ++i) *this << (*v)[i];
      return *this;
    }
    Vectorized& unshift(T element)
    {
      elements_.insert(elements_.begin(), element);
      return *this;
    }
    std::vector<T>& elements() { return elements_; }
    const std::vector<T>& elements() const { return elements_; }
    std::vector<T>& elements(std::vector<T>& e) { elements_ = e; return elements_; }

    virtual size_t hash()
    {
      if (hash_ == 0) {
        for (T& el : elements_) {
          hash_combine(hash_, el->hash());
        }
      }
      return hash_;
    }

    typename std::vector<T>::iterator end() { return elements_.end(); }
    typename std::vector<T>::iterator begin() { return elements_.begin(); }
    typename std::vector<T>::const_iterator end() const { return elements_.end(); }
    typename std::vector<T>::const_iterator begin() const { return elements_.begin(); }
    typename std::vector<T>::iterator erase(typename std::vector<T>::iterator el) { return elements_.erase(el); }
    typename std::vector<T>::const_iterator erase(typename std::vector<T>::const_iterator el) { return elements_.erase(el); }

  };
  template <typename T>
  inline Vectorized<T>::~Vectorized() { }


  /////////////////////////////////////////////////////////////////////////////
  // Mixin class for AST nodes that should behave like a hash table. Uses an
  // extra <std::vector> internally to maintain insertion order for interation.
  /////////////////////////////////////////////////////////////////////////////
  class Hashed {
  struct HashExpression {
    size_t operator() (Expression_Ptr ex) const {
      return ex ? ex->hash() : 0;
    }
  };
  struct CompareExpression {
    bool operator()(const Expression_Ptr lhs, const Expression_Ptr rhs) const {
      return lhs && rhs && lhs->eq(*rhs);
    }
  };
  typedef std::unordered_map<
    Expression_Ptr, // key
    Expression_Ptr, // value
    HashExpression, // hasher
    CompareExpression // compare
  > ExpressionMap;
  private:
    ExpressionMap elements_;
    std::vector<Expression_Ptr> list_;
  protected:
    size_t hash_;
    Expression_Ptr duplicate_key_;
    void reset_hash() { hash_ = 0; }
    void reset_duplicate_key() { duplicate_key_ = 0; }
    virtual void adjust_after_pushing(std::pair<Expression_Ptr, Expression_Ptr> p) { }
  public:
    Hashed(size_t s = 0) : elements_(ExpressionMap(s)), list_(std::vector<Expression_Ptr>())
    { elements_.reserve(s); list_.reserve(s); reset_duplicate_key(); }
    virtual ~Hashed();
    size_t length() const                  { return list_.size(); }
    bool empty() const                     { return list_.empty(); }
    bool has(Expression_Ptr k) const          { return elements_.count(k) == 1; }
    Expression_Ptr at(Expression_Ptr k) const;
    bool has_duplicate_key() const         { return duplicate_key_ != 0; }
    Expression_Ptr get_duplicate_key() const  { return duplicate_key_; }
    const ExpressionMap elements() { return elements_; }
    Hashed& operator<<(std::pair<Expression_Ptr, Expression_Ptr> p)
    {
      reset_hash();

      if (!has(p.first)) list_.push_back(p.first);
      else if (!duplicate_key_) duplicate_key_ = p.first;

      elements_[p.first] = p.second;

      adjust_after_pushing(p);
      return *this;
    }
    Hashed& operator+=(Hashed* h)
    {
      if (length() == 0) {
        this->elements_ = h->elements_;
        this->list_ = h->list_;
        return *this;
      }

      for (auto key : h->keys()) {
        *this << std::make_pair(key, h->at(key));
      }

      reset_duplicate_key();
      return *this;
    }
    const ExpressionMap& pairs() const { return elements_; }
    const std::vector<Expression_Ptr>& keys() const { return list_; }

    std::unordered_map<Expression_Ptr, Expression_Ptr>::iterator end() { return elements_.end(); }
    std::unordered_map<Expression_Ptr, Expression_Ptr>::iterator begin() { return elements_.begin(); }
    std::unordered_map<Expression_Ptr, Expression_Ptr>::const_iterator end() const { return elements_.end(); }
    std::unordered_map<Expression_Ptr, Expression_Ptr>::const_iterator begin() const { return elements_.begin(); }

  };
  inline Hashed::~Hashed() { }


  /////////////////////////////////////////////////////////////////////////
  // Abstract base class for statements. This side of the AST hierarchy
  // represents elements in expansion contexts, which exist primarily to be
  // rewritten and macro-expanded.
  /////////////////////////////////////////////////////////////////////////
  class Statement_Ref : public AST_Node_Ref {
  public:
    enum Statement_Type {
      NONE,
      RULESET,
      MEDIA,
      DIRECTIVE,
      SUPPORTS,
      ATROOT,
      BUBBLE,
      CONTENT,
      KEYFRAMERULE,
      DECLARATION,
      ASSIGNMENT,
      IMPORT_STUB,
      IMPORT,
      COMMENT,
      WARNING,
      RETURN,
      EXTEND,
      ERROR,
      DEBUGSTMT,
      WHILE,
      EACH,
      FOR,
      IF
    };
  private:
    ADD_PROPERTY(Statement_Type, statement_type)
    ADD_PROPERTY(size_t, tabs)
    ADD_PROPERTY(bool, group_end)
  public:
    Statement_Ref(ParserState pstate, Statement_Type st = NONE, size_t t = 0)
    : AST_Node_Ref(pstate), statement_type_(st), tabs_(t), group_end_(false)
     { }
    virtual ~Statement_Ref() = 0;
    // needed for rearranging nested rulesets during CSS emission
    virtual bool   is_invisible() const { return false; }
    virtual bool   bubbles() { return false; }
    virtual bool has_content()
    {
      return statement_type_ == CONTENT;
    }
  };
  inline Statement_Ref::~Statement_Ref() { }

  ////////////////////////
  // Blocks of statements.
  ////////////////////////
  class Block_Ref : public Statement_Ref, public Vectorized<Statement_Obj> {
    ADD_PROPERTY(bool, is_root)
    ADD_PROPERTY(bool, is_at_root);
    // needed for properly formatted CSS emission
  protected:
    void adjust_after_pushing(Statement_Obj s)
    {
    }
  public:
    Block_Ref(ParserState pstate, size_t s = 0, bool r = false)
    : Statement_Ref(pstate),
      Vectorized<Statement_Obj>(s),
      is_root_(r),
      is_at_root_(false)
    { }
    virtual bool has_content()
    {
      for (size_t i = 0, L = elements().size(); i < L; ++i) {
        if (elements()[i]->has_content()) return true;
      }
      return Statement_Ref::has_content();
    }
    ATTACH_OPERATIONS()
  };

  ////////////////////////////////////////////////////////////////////////
  // Abstract base class for statements that contain blocks of statements.
  ////////////////////////////////////////////////////////////////////////
  class Has_Block_Ref : public Statement_Ref {
    // ADD_PROPERTY(Block_Ptr, block)
    ADD_PROPERTY(Block_Obj, oblock)
  public:
    Has_Block_Ref(ParserState pstate, Block_Obj b)
    : Statement_Ref(pstate), /* block_(&b), */ oblock_(b)
    { }
    virtual bool has_content()
    {
      return (oblock_ && oblock_->has_content()) || Statement_Ref::has_content();
    }
    virtual ~Has_Block_Ref() = 0;
  };
  inline Has_Block_Ref::~Has_Block_Ref() { }

  /////////////////////////////////////////////////////////////////////////////
  // Rulesets (i.e., sets of styles headed by a selector and containing a block
  // of style declarations.
  /////////////////////////////////////////////////////////////////////////////
  class Ruleset_Ref : public Has_Block_Ref {
    ADD_PROPERTY(Selector_Obj, selector)
    ADD_PROPERTY(bool, at_root);
    ADD_PROPERTY(bool, is_root);
  public:
    Ruleset_Ref(ParserState pstate, Selector_Obj s = 0, Block_Obj b = 0)
    : Has_Block_Ref(pstate, b), selector_(s), at_root_(false), is_root_(false)
    { statement_type(RULESET); }
    bool is_invisible() const;
    ATTACH_OPERATIONS()
  };

  /////////////////
  // Bubble.
  /////////////////
  class Bubble_Ref : public Statement_Ref {
    ADD_PROPERTY(Statement_Obj, node)
    ADD_PROPERTY(bool, group_end)
  public:
    Bubble_Ref(ParserState pstate, Statement_Obj n, Statement_Obj g = 0, size_t t = 0)
    : Statement_Ref(pstate, Statement_Ref::BUBBLE, t), node_(n), group_end_(g == 0)
    { }
    bool bubbles() { return true; }
    ATTACH_OPERATIONS()
  };

  /////////////////
  // Trace.
  /////////////////
  class Trace_Ref : public Has_Block_Ref {
    ADD_PROPERTY(std::string, name)
  public:
    Trace_Ref(ParserState pstate, std::string n, Block_Obj b = 0)
    : Has_Block_Ref(pstate, b), name_(n)
    { }
    ATTACH_OPERATIONS()
  };

  /////////////////
  // Media queries.
  /////////////////
  class Media_Block_Ref : public Has_Block_Ref {
    ADD_PROPERTY(List_Obj, media_queries)
  public:
    Media_Block_Ref(ParserState pstate, List_Obj mqs, Block_Obj b)
    : Has_Block_Ref(pstate, b), media_queries_(mqs)
    { statement_type(MEDIA); }
    Media_Block_Ref(ParserState pstate, List_Obj mqs, Block_Obj b, Selector_Ptr s)
    : Has_Block_Ref(pstate, b), media_queries_(mqs)
    { statement_type(MEDIA); }
    bool bubbles() { return true; }
    bool is_invisible() const;
    ATTACH_OPERATIONS()
  };

  ///////////////////////////////////////////////////////////////////////
  // At-rules -- arbitrary directives beginning with "@" that may have an
  // optional statement block.
  ///////////////////////////////////////////////////////////////////////
  class Directive_Ref : public Has_Block_Ref {
    ADD_PROPERTY(std::string, keyword)
    ADD_PROPERTY(Selector_Obj, selector)
    ADD_PROPERTY(Expression_Obj, value)
  public:
    Directive_Ref(ParserState pstate, std::string kwd, Selector_Obj sel = 0, Block_Obj b = 0, Expression_Obj val = 0)
    : Has_Block_Ref(pstate, b), keyword_(kwd), selector_(sel), value_(val) // set value manually if needed
    { statement_type(DIRECTIVE); }
    bool bubbles() { return is_keyframes() || is_media(); }
    bool is_media() {
      return keyword_.compare("@-webkit-media") == 0 ||
             keyword_.compare("@-moz-media") == 0 ||
             keyword_.compare("@-o-media") == 0 ||
             keyword_.compare("@media") == 0;
    }
    bool is_keyframes() {
      return keyword_.compare("@-webkit-keyframes") == 0 ||
             keyword_.compare("@-moz-keyframes") == 0 ||
             keyword_.compare("@-o-keyframes") == 0 ||
             keyword_.compare("@keyframes") == 0;
    }
    ATTACH_OPERATIONS()
  };

  ///////////////////////////////////////////////////////////////////////
  // Keyframe-rules -- the child blocks of "@keyframes" nodes.
  ///////////////////////////////////////////////////////////////////////
  class Keyframe_Rule_Ref : public Has_Block_Ref {
    ADD_PROPERTY(Selector_Obj, selector2)
  public:
    Keyframe_Rule_Ref(ParserState pstate, Block_Obj b)
    : Has_Block_Ref(pstate, b), selector2_()
    { statement_type(KEYFRAMERULE); }
    ATTACH_OPERATIONS()
  };

  ////////////////////////////////////////////////////////////////////////
  // Declarations -- style rules consisting of a property name and values.
  ////////////////////////////////////////////////////////////////////////
  class Declaration_Ref : public Has_Block_Ref {
    ADD_PROPERTY(String_Ptr, property)
    ADD_PROPERTY(Expression_Ptr, value)
    ADD_PROPERTY(bool, is_important)
    ADD_PROPERTY(bool, is_indented)
  public:
    Declaration_Ref(ParserState pstate,
                String_Ptr prop, Expression_Ptr val, bool i = false, Block_Obj b = 0)
    : Has_Block_Ref(pstate, b), property_(prop), value_(val), is_important_(i), is_indented_(false)
    { statement_type(DECLARATION); }
    ATTACH_OPERATIONS()
  };

  /////////////////////////////////////
  // Assignments -- variable and value.
  /////////////////////////////////////
  class Assignment_Ref : public Statement_Ref {
    ADD_PROPERTY(std::string, variable)
    ADD_PROPERTY(Expression_Ptr, value)
    ADD_PROPERTY(bool, is_default)
    ADD_PROPERTY(bool, is_global)
  public:
    Assignment_Ref(ParserState pstate,
               std::string var, Expression_Ptr val,
               bool is_default = false,
               bool is_global = false)
    : Statement_Ref(pstate), variable_(var), value_(val), is_default_(is_default), is_global_(is_global)
    { statement_type(ASSIGNMENT); }
    ATTACH_OPERATIONS()
  };

  ////////////////////////////////////////////////////////////////////////////
  // Import directives. CSS and Sass import lists can be intermingled, so it's
  // necessary to store a list of each in an Import node.
  ////////////////////////////////////////////////////////////////////////////
  class Import_Ref : public Statement_Ref {
    std::vector<Expression_Ptr> urls_;
    std::vector<Include>     incs_;
    ADD_PROPERTY(List_Obj,      import_queries);
  public:
    Import_Ref(ParserState pstate)
    : Statement_Ref(pstate),
      urls_(std::vector<Expression_Ptr>()),
      incs_(std::vector<Include>()),
      import_queries_()
    { statement_type(IMPORT); }
    std::vector<Expression_Ptr>& urls() { return urls_; }
    std::vector<Include>& incs() { return incs_; }
    ATTACH_OPERATIONS()
  };

  // not yet resolved single import
  // so far we only know requested name
  class Import_Stub_Ref : public Statement_Ref {
    Include resource_;
  public:
    std::string abs_path() { return resource_.abs_path; };
    std::string imp_path() { return resource_.imp_path; };
    Include resource() { return resource_; };

    Import_Stub_Ref(ParserState pstate, Include res)
    : Statement_Ref(pstate), resource_(res)
    { statement_type(IMPORT_STUB); }
    ATTACH_OPERATIONS()
  };

  //////////////////////////////
  // The Sass `@warn` directive.
  //////////////////////////////
  class Warning_Ref : public Statement_Ref {
    ADD_PROPERTY(Expression_Obj, message)
  public:
    Warning_Ref(ParserState pstate, Expression_Obj msg)
    : Statement_Ref(pstate), message_(msg)
    { statement_type(WARNING); }
    ATTACH_OPERATIONS()
  };

  ///////////////////////////////
  // The Sass `@error` directive.
  ///////////////////////////////
  class Error_Ref : public Statement_Ref {
    ADD_PROPERTY(Expression_Obj, message)
  public:
    Error_Ref(ParserState pstate, Expression_Obj msg)
    : Statement_Ref(pstate), message_(msg)
    { statement_type(ERROR); }
    ATTACH_OPERATIONS()
  };

  ///////////////////////////////
  // The Sass `@debug` directive.
  ///////////////////////////////
  class Debug_Ref : public Statement_Ref {
    ADD_PROPERTY(Expression_Obj, value)
  public:
    Debug_Ref(ParserState pstate, Expression_Obj val)
    : Statement_Ref(pstate), value_(val)
    { statement_type(DEBUGSTMT); }
    ATTACH_OPERATIONS()
  };

  ///////////////////////////////////////////
  // CSS comments. These may be interpolated.
  ///////////////////////////////////////////
  class Comment_Ref : public Statement_Ref {
    ADD_PROPERTY(String_Obj, text)
    ADD_PROPERTY(bool, is_important)
  public:
    Comment_Ref(ParserState pstate, String_Obj txt, bool is_important)
    : Statement_Ref(pstate), text_(txt), is_important_(is_important)
    { statement_type(COMMENT); }
    virtual bool is_invisible() const
    { return /* is_important() == */ false; }
    ATTACH_OPERATIONS()
  };

  ////////////////////////////////////
  // The Sass `@if` control directive.
  ////////////////////////////////////
  class If_Ref : public Has_Block_Ref {
    ADD_PROPERTY(Expression_Obj, predicate)
    ADD_PROPERTY(Block_Obj, alternative)
  public:
    If_Ref(ParserState pstate, Expression_Obj pred, Block_Obj con, Block_Obj alt = 0)
    : Has_Block_Ref(pstate, &con), predicate_(pred), alternative_(alt)
    { statement_type(IF); }
    virtual bool has_content()
    {
      return Has_Block_Ref::has_content() || (alternative_ && alternative_->has_content());
    }
    ATTACH_OPERATIONS()
  };

  /////////////////////////////////////
  // The Sass `@for` control directive.
  /////////////////////////////////////
  class For_Ref : public Has_Block_Ref {
    ADD_PROPERTY(std::string, variable)
    ADD_PROPERTY(Expression_Obj, lower_bound)
    ADD_PROPERTY(Expression_Obj, upper_bound)
    ADD_PROPERTY(bool, is_inclusive)
  public:
    For_Ref(ParserState pstate,
        std::string var, Expression_Obj lo, Expression_Obj hi, Block_Obj b, bool inc)
    : Has_Block_Ref(pstate, b),
      variable_(var), lower_bound_(lo), upper_bound_(hi), is_inclusive_(inc)
    { statement_type(FOR); }
    ATTACH_OPERATIONS()
  };

  //////////////////////////////////////
  // The Sass `@each` control directive.
  //////////////////////////////////////
  class Each_Ref : public Has_Block_Ref {
    ADD_PROPERTY(std::vector<std::string>, variables)
    ADD_PROPERTY(Expression_Obj, list)
  public:
    Each_Ref(ParserState pstate, std::vector<std::string> vars, Expression_Obj lst, Block_Obj b)
    : Has_Block_Ref(pstate, b), variables_(vars), list_(lst)
    { statement_type(EACH); }
    ATTACH_OPERATIONS()
  };

  ///////////////////////////////////////
  // The Sass `@while` control directive.
  ///////////////////////////////////////
  class While_Ref : public Has_Block_Ref {
    ADD_PROPERTY(Expression_Obj, predicate)
  public:
    While_Ref(ParserState pstate, Expression_Obj pred, Block_Obj b)
    : Has_Block_Ref(pstate, b), predicate_(pred)
    { statement_type(WHILE); }
    ATTACH_OPERATIONS()
  };

  /////////////////////////////////////////////////////////////
  // The @return directive for use inside SassScript functions.
  /////////////////////////////////////////////////////////////
  class Return_Ref : public Statement_Ref {
    ADD_PROPERTY(Expression_Obj, value)
  public:
    Return_Ref(ParserState pstate, Expression_Obj val)
    : Statement_Ref(pstate), value_(val)
    { statement_type(RETURN); }
    ATTACH_OPERATIONS()
  };

  ////////////////////////////////
  // The Sass `@extend` directive.
  ////////////////////////////////
  class Extension_Ref : public Statement_Ref {
    ADD_PROPERTY(Selector_Obj, selector)
  public:
    Extension_Ref(ParserState pstate, Selector_Obj s)
    : Statement_Ref(pstate), selector_(s)
    { statement_type(EXTEND); }
    ATTACH_OPERATIONS()
  };

  /////////////////////////////////////////////////////////////////////////////
  // Definitions for both mixins and functions. The two cases are distinguished
  // by a type tag.
  /////////////////////////////////////////////////////////////////////////////
  struct Backtrace;
  typedef Environment<AST_Node_Obj> Env;
  typedef const char* Signature;
  typedef Expression_Ptr (*Native_Function)(Env&, Env&, Context&, Signature, ParserState, Backtrace*, std::vector<CommaComplex_Selector_Obj>);
  typedef const char* Signature;
  class Definition_Ref : public Has_Block_Ref {
  public:
    enum Type { MIXIN, FUNCTION };
    ADD_PROPERTY(std::string, name)
    ADD_PROPERTY(Parameters_Ptr, parameters)
    ADD_PROPERTY(Env*, environment)
    ADD_PROPERTY(Type, type)
    ADD_PROPERTY(Native_Function, native_function)
    ADD_PROPERTY(Sass_Function_Entry, c_function)
    ADD_PROPERTY(void*, cookie)
    ADD_PROPERTY(bool, is_overload_stub)
    ADD_PROPERTY(Signature, signature)
  public:
    Definition_Ref(ParserState pstate,
               std::string n,
               Parameters_Ptr params,
               Block_Obj b,
               Type t)
    : Has_Block_Ref(pstate, b),
      name_(n),
      parameters_(params),
      environment_(0),
      type_(t),
      native_function_(0),
      c_function_(0),
      cookie_(0),
      is_overload_stub_(false),
      signature_(0)
    { }
    Definition_Ref(ParserState pstate,
               Signature sig,
               std::string n,
               Parameters_Ptr params,
               Native_Function func_ptr,
               bool overload_stub = false)
    : Has_Block_Ref(pstate, 0),
      name_(n),
      parameters_(params),
      environment_(0),
      type_(FUNCTION),
      native_function_(func_ptr),
      c_function_(0),
      cookie_(0),
      is_overload_stub_(overload_stub),
      signature_(sig)
    { }
    Definition_Ref(ParserState pstate,
               Signature sig,
               std::string n,
               Parameters_Ptr params,
               Sass_Function_Entry c_func,
               bool whatever,
               bool whatever2)
    : Has_Block_Ref(pstate, 0),
      name_(n),
      parameters_(params),
      environment_(0),
      type_(FUNCTION),
      native_function_(0),
      c_function_(c_func),
      cookie_(sass_function_get_cookie(c_func)),
      is_overload_stub_(false),
      signature_(sig)
    { }
    ATTACH_OPERATIONS()
  };

  //////////////////////////////////////
  // Mixin calls (i.e., `@include ...`).
  //////////////////////////////////////
  class Mixin_Call_Ref : public Has_Block_Ref {
    ADD_PROPERTY(std::string, name)
    ADD_PROPERTY(Arguments_Obj, arguments)
  public:
    Mixin_Call_Ref(ParserState pstate, std::string n, Arguments_Obj args, Block_Obj b = 0)
    : Has_Block_Ref(pstate, b), name_(n), arguments_(args)
    { }
    ATTACH_OPERATIONS()
  };

  ///////////////////////////////////////////////////
  // The @content directive for mixin content blocks.
  ///////////////////////////////////////////////////
  class Content_Ref : public Statement_Ref {
    ADD_PROPERTY(Media_Block_Obj, media_block)
  public:
    Content_Ref(ParserState pstate) : Statement_Ref(pstate)
    { statement_type(CONTENT); }
    ATTACH_OPERATIONS()
  };

  ///////////////////////////////////////////////////////////////////////
  // Lists of values, both comma- and space-separated (distinguished by a
  // type-tag.) Also used to represent variable-length argument lists.
  ///////////////////////////////////////////////////////////////////////
  class List_Ref : public Value_Ref, public Vectorized<Expression_Obj> {
    void adjust_after_pushing(Expression_Obj e) { is_expanded(false); }
  private:
    ADD_PROPERTY(enum Sass_Separator, separator)
    ADD_PROPERTY(bool, is_arglist)
    ADD_PROPERTY(bool, from_selector)
  public:
    List_Ref(ParserState pstate,
         size_t size = 0, enum Sass_Separator sep = SASS_SPACE, bool argl = false)
    : Value_Ref(pstate),
      Vectorized<Expression_Obj>(size),
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
    Expression_Ptr value_at_index(size_t i);

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
    virtual List_Ptr copy(Memory_Manager& mem, bool recursive = false);

    ATTACH_OPERATIONS()
  };


  ///////////////////////////////////////////////////////////////////////
  // Lists of values, both comma- and space-separated (distinguished by a
  // type-tag.) Also used to represent variable-length argument lists.
  ///////////////////////////////////////////////////////////////////////
  class List2_Ref : public Value_Ref, public Vectorized<Expression_Obj> {
    void adjust_after_pushing(Expression_Obj e) { is_expanded(false); }
  private:
    ADD_PROPERTY(enum Sass_Separator, separator)
    ADD_PROPERTY(bool, is_arglist)
    ADD_PROPERTY(bool, from_selector)
  public:
    List2_Ref(ParserState pstate,
         size_t size = 0, enum Sass_Separator sep = SASS_SPACE, bool argl = false)
    : Value_Ref(pstate),
      Vectorized<Expression_Obj>(size),
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
    Expression_Obj value_at_index(size_t i);

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
    virtual List2_Ptr copy(Memory_Manager& mem, bool recursive = false);

    ATTACH_OPERATIONS()
  };
  ///////////////////////////////////////////////////////////////////////
  // Key value paris.
  ///////////////////////////////////////////////////////////////////////
  class Map_Ref : public Value_Ref, public Hashed {
    void adjust_after_pushing(std::pair<Expression_Ptr, Expression_Ptr> p) { is_expanded(false); }
  public:
    Map_Ref(ParserState pstate,
         size_t size = 0)
    : Value_Ref(pstate),
      Hashed(size)
    { concrete_type(MAP); }
    std::string type() { return "map"; }
    static std::string type_name() { return "map"; }
    bool is_invisible() const { return empty(); }
    List_Ptr to_list(Context& ctx, ParserState& pstate);

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
    virtual Map_Ptr copy(Memory_Manager& mem, bool recursive = false);

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
  class Binary_Expression_Ref : public PreValue_Ref {
  private:
    ADD_HASHED(Operand, op)
    ADD_HASHED(Expression_Obj, left)
    ADD_HASHED(Expression_Obj, right)
    size_t hash_;
  public:
    Binary_Expression_Ref(ParserState pstate,
                      Operand op, Expression_Obj lhs, Expression_Obj rhs)
    : PreValue_Ref(pstate), op_(op), left_(lhs), right_(rhs), hash_(0)
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
        Binary_Expression_Ptr_Const m = dynamic_cast<Binary_Expression_Ptr_Const>(&rhs);
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
    virtual Binary_Expression_Ptr copy(Memory_Manager& mem, bool recursive = false);
    enum Sass_OP type() const { return op_.operand; }
    ATTACH_OPERATIONS()
  };

  ////////////////////////////////////////////////////////////////////////////
  // Arithmetic negation (logical negation is just an ordinary function call).
  ////////////////////////////////////////////////////////////////////////////
  class Unary_Expression_Ref : public Expression_Ref {
  public:
    enum Type { PLUS, MINUS, NOT };
  private:
    ADD_HASHED(Type, type)
    ADD_HASHED(Expression_Obj, operand)
    size_t hash_;
  public:
    Unary_Expression_Ref(ParserState pstate, Type t, Expression_Obj o)
    : Expression_Ref(pstate), type_(t), operand_(o), hash_(0)
    { }
    const std::string type_name() {
      switch (type_) {
        case PLUS: return "plus"; break;
        case MINUS: return "minus"; break;
        case NOT: return "not"; break;
        default: return "invalid"; break;
      }
    }
    virtual bool operator==(const Expression& rhs) const
    {
      try
      {
        Unary_Expression_Ptr_Const m = dynamic_cast<Unary_Expression_Ptr_Const>(&rhs);
        if (m == 0) return false;
        return type() == m->type() &&
               operand() == m->operand();
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
        hash_ = std::hash<size_t>()(type_);
        hash_combine(hash_, operand()->hash());
      };
      return hash_;
    }
    ATTACH_OPERATIONS()
  };

  ////////////////////////////////////////////////////////////
  // Individual argument objects for mixin and function calls.
  ////////////////////////////////////////////////////////////
  class Argument_Ref : public Expression_Ref {
    ADD_HASHED(Expression_Ptr, value)
    ADD_HASHED(std::string, name)
    ADD_PROPERTY(bool, is_rest_argument)
    ADD_PROPERTY(bool, is_keyword_argument)
    size_t hash_;
  public:
    Argument_Ref(ParserState pstate, Expression_Obj val, std::string n = "", bool rest = false, bool keyword = false)
    : Expression_Ref(pstate), value_(&val), name_(n), is_rest_argument_(rest), is_keyword_argument_(keyword), hash_(0)
    {
      if (!name_.empty() && is_rest_argument_) {
        error("variable-length argument may not be passed by name", pstate);
      }
    }

    virtual void set_delayed(bool delayed);
    virtual bool operator==(const Expression& rhs) const
    {
      try
      {
        Argument_Ptr_Const m = dynamic_cast<Argument_Ptr_Const>(&rhs);
        if (!(m && name() == m->name())) return false;
        return *value() == *m->value();
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
        hash_combine(hash_, value()->hash());
      }
      return hash_;
    }

    ATTACH_OPERATIONS()
  };

  ////////////////////////////////////////////////////////////////////////
  // Argument lists -- in their own class to facilitate context-sensitive
  // error checking (e.g., ensuring that all ordinal arguments precede all
  // named arguments).
  ////////////////////////////////////////////////////////////////////////
  class Arguments_Ref : public Expression_Ref, public Vectorized<Argument_Obj> {
    ADD_PROPERTY(bool, has_named_arguments)
    ADD_PROPERTY(bool, has_rest_argument)
    ADD_PROPERTY(bool, has_keyword_argument)
  protected:
    void adjust_after_pushing(Argument_Obj a);
  public:
    Arguments_Ref(ParserState pstate)
    : Expression_Ref(pstate),
      Vectorized<Argument_Obj>(),
      has_named_arguments_(false),
      has_rest_argument_(false),
      has_keyword_argument_(false)
    { }

    virtual void set_delayed(bool delayed);

    Argument_Obj get_rest_argument();
    Argument_Obj get_keyword_argument();

    ATTACH_OPERATIONS()
  };

  //////////////////
  // Function calls.
  //////////////////
  class Function_Call_Ref : public PreValue_Ref {
    ADD_HASHED(std::string, name)
    ADD_HASHED(Arguments_Obj, arguments)
    ADD_PROPERTY(void*, cookie)
    size_t hash_;
  public:
    Function_Call_Ref(ParserState pstate, std::string n, Arguments_Obj args, void* cookie)
    : PreValue_Ref(pstate), name_(n), arguments_(args), cookie_(cookie), hash_(0)
    { concrete_type(STRING); }
    Function_Call_Ref(ParserState pstate, std::string n, Arguments_Obj args)
    : PreValue_Ref(pstate), name_(n), arguments_(args), cookie_(0), hash_(0)
    { concrete_type(STRING); }

    virtual bool operator==(const Expression& rhs) const
    {
      try
      {
        Function_Call_Ptr_Const m = dynamic_cast<Function_Call_Ptr_Const>(&rhs);
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

  /////////////////////////
  // Function call schemas.
  /////////////////////////
  class Function_Call_Schema_Ref : public Expression_Ref {
    ADD_PROPERTY(String_Obj, name)
    ADD_PROPERTY(Arguments_Obj, arguments)
  public:
    Function_Call_Schema_Ref(ParserState pstate, String_Obj n, Arguments_Obj args)
    : Expression_Ref(pstate), name_(n), arguments_(args)
    { concrete_type(STRING); }
    ATTACH_OPERATIONS()
  };

  ///////////////////////
  // Variable references.
  ///////////////////////
  class Variable_Ref : public PreValue_Ref {
    ADD_PROPERTY(std::string, name)
  public:
    Variable_Ref(ParserState pstate, std::string n)
    : PreValue_Ref(pstate), name_(n)
    { }

    virtual bool operator==(const Expression& rhs) const
    {
      try
      {
        Variable_Ptr_Const e = dynamic_cast<Variable_Ptr_Const>(&rhs);
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

  ////////////////////////////////////////////////////////////////////////////
  // Textual (i.e., unevaluated) numeric data. Variants are distinguished with
  // a type tag.
  ////////////////////////////////////////////////////////////////////////////
  class Textual_Ref : public Expression_Ref {
  public:
    enum Type { NUMBER, PERCENTAGE, DIMENSION, HEX };
  private:
    ADD_HASHED(Type, type)
    ADD_HASHED(std::string, value)
    size_t hash_;
  public:
    Textual_Ref(ParserState pstate, Type t, std::string val)
    : Expression_Ref(pstate, DELAYED), type_(t), value_(val),
      hash_(0)
    { }

    virtual bool operator==(const Expression& rhs) const
    {
      try
      {
        Textual_Ptr_Const e = dynamic_cast<Textual_Ptr_Const>(&rhs);
        return e && value() == e->value() && type() == e->type();
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
        hash_ = std::hash<std::string>()(value_);
        hash_combine(hash_, std::hash<int>()(type_));
      }
      return hash_;
    }

    ATTACH_OPERATIONS()
  };

  ////////////////////////////////////////////////
  // Numbers, percentages, dimensions, and colors.
  ////////////////////////////////////////////////
  class Number_Ref : public Value_Ref {
    ADD_HASHED(double, value)
    ADD_PROPERTY(bool, zero)
    std::vector<std::string> numerator_units_;
    std::vector<std::string> denominator_units_;
    size_t hash_;
  public:
    Number_Ref(ParserState pstate, double val, std::string u = "", bool zero = true);
    bool zero() { return zero_; }
    bool is_valid_css_unit() const;
    std::vector<std::string>& numerator_units()   { return numerator_units_; }
    std::vector<std::string>& denominator_units() { return denominator_units_; }
    const std::vector<std::string>& numerator_units() const   { return numerator_units_; }
    const std::vector<std::string>& denominator_units() const { return denominator_units_; }
    std::string type() { return "number"; }
    static std::string type_name() { return "number"; }
    std::string unit() const;

    bool is_unitless() const;
    double convert_factor(const Number_Ref&) const;
    bool convert(const std::string& unit = "", bool strict = false);
    void normalize(const std::string& unit = "", bool strict = false);
    // useful for making one number compatible with another
    std::string find_convertible_unit() const;

    virtual size_t hash()
    {
      if (hash_ == 0) {
        hash_ = std::hash<double>()(value_);
        for (const auto numerator : numerator_units())
          hash_combine(hash_, std::hash<std::string>()(numerator));
        for (const auto denominator : denominator_units())
          hash_combine(hash_, std::hash<std::string>()(denominator));
      }
      return hash_;
    }

    virtual bool operator< (const Number_Ref& rhs) const;
    virtual bool operator== (const Expression& rhs) const;
    virtual bool eq(const Expression& rhs) const;

    ATTACH_OPERATIONS()
  };

  //////////
  // Colors.
  //////////
  class Color_Ref : public Value_Ref {
    ADD_HASHED(double, r)
    ADD_HASHED(double, g)
    ADD_HASHED(double, b)
    ADD_HASHED(double, a)
    ADD_PROPERTY(std::string, disp)
    size_t hash_;
  public:
    Color_Ref(ParserState pstate, double r, double g, double b, double a = 1, const std::string disp = "")
    : Value_Ref(pstate), r_(r), g_(g), b_(b), a_(a), disp_(disp),
      hash_(0)
    { concrete_type(COLOR); }
    std::string type() { return "color"; }
    static std::string type_name() { return "color"; }

    virtual size_t hash()
    {
      if (hash_ == 0) {
        hash_ = std::hash<double>()(a_);
        hash_combine(hash_, std::hash<double>()(r_));
        hash_combine(hash_, std::hash<double>()(g_));
        hash_combine(hash_, std::hash<double>()(b_));
      }
      return hash_;
    }

    virtual bool operator== (const Expression& rhs) const;

    ATTACH_OPERATIONS()
  };

  //////////////////////////////
  // Errors from Sass_Values.
  //////////////////////////////
  class Custom_Error_Ref : public Value_Ref {
    ADD_PROPERTY(std::string, message)
  public:
    Custom_Error_Ref(ParserState pstate, std::string msg)
    : Value_Ref(pstate), message_(msg)
    { concrete_type(C_ERROR); }
    virtual bool operator== (const Expression& rhs) const;
    ATTACH_OPERATIONS()
  };

  //////////////////////////////
  // Warnings from Sass_Values.
  //////////////////////////////
  class Custom_Warning_Ref : public Value_Ref {
    ADD_PROPERTY(std::string, message)
  public:
    Custom_Warning_Ref(ParserState pstate, std::string msg)
    : Value_Ref(pstate), message_(msg)
    { concrete_type(C_WARNING); }
    virtual bool operator== (const Expression& rhs) const;
    ATTACH_OPERATIONS()
  };

  ////////////
  // Booleans.
  ////////////
  class Boolean_Ref : public Value_Ref {
    ADD_HASHED(bool, value)
    size_t hash_;
  public:
    Boolean_Ref(ParserState pstate, bool val)
    : Value_Ref(pstate), value_(val),
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
  class String_Ref : public Value_Ref {
  public:
    String_Ref(ParserState pstate, bool delayed = false)
    : Value_Ref(pstate, delayed)
    { concrete_type(STRING); }
    static std::string type_name() { return "string"; }
    virtual ~String_Ref() = 0;
    virtual void rtrim() = 0;
    virtual void ltrim() = 0;
    virtual void trim() = 0;
    virtual bool operator==(const Expression& rhs) const = 0;
    virtual String_Ptr copy(Memory_Manager& mem, bool recursive = false);
    ATTACH_OPERATIONS()
  };
  inline String_Ref::~String_Ref() { };

  ///////////////////////////////////////////////////////////////////////
  // Interpolated strings. Meant to be reduced to flat strings during the
  // evaluation phase.
  ///////////////////////////////////////////////////////////////////////
  class String_Schema_Ref : public String_Ref, public Vectorized<Expression_Obj> {
    // ADD_PROPERTY(bool, has_interpolants)
    size_t hash_;
  public:
    String_Schema_Ref(ParserState pstate, size_t size = 0, bool has_interpolants = false)
    : String_Ref(pstate), Vectorized<Expression_Obj>(size), hash_(0)
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
  class String_Constant_Ref : public String_Ref {
    ADD_PROPERTY(char, quote_mark)
    ADD_PROPERTY(bool, can_compress_whitespace)
    ADD_HASHED(std::string, value)
  protected:
    size_t hash_;
  public:
    String_Constant_Ref(ParserState pstate, std::string val)
    : String_Ref(pstate), quote_mark_(0), can_compress_whitespace_(false), value_(read_css_string(val)), hash_(0)
    { }
    String_Constant_Ref(ParserState pstate, const char* beg)
    : String_Ref(pstate), quote_mark_(0), can_compress_whitespace_(false), value_(read_css_string(std::string(beg))), hash_(0)
    { }
    String_Constant_Ref(ParserState pstate, const char* beg, const char* end)
    : String_Ref(pstate), quote_mark_(0), can_compress_whitespace_(false), value_(read_css_string(std::string(beg, end-beg))), hash_(0)
    { }
    String_Constant_Ref(ParserState pstate, const Token& tok)
    : String_Ref(pstate), quote_mark_(0), can_compress_whitespace_(false), value_(read_css_string(std::string(tok.begin, tok.end))), hash_(0)
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
    virtual String_Constant_Ptr copy(Memory_Manager& mem, bool recursive = false);
    virtual std::string inspect() const; // quotes are forced on inspection

    // static char auto_quote() { return '*'; }
    static char double_quote() { return '"'; }
    static char single_quote() { return '\''; }

    ATTACH_OPERATIONS()
  };

  ////////////////////////////////////////////////////////
  // Possibly quoted string (unquote on instantiation)
  ////////////////////////////////////////////////////////
  class String_Quoted_Ref : public String_Constant_Ref {
  public:
    String_Quoted_Ref(ParserState pstate, std::string val, char q = 0,
      bool keep_utf8_escapes = false, bool skip_unquoting = false,
      bool strict_unquoting = true)
    : String_Constant(pstate, val)
    {
      if (skip_unquoting == false) {
        value_ = unquote(value_, &quote_mark_, keep_utf8_escapes, strict_unquoting);
      }
      if (q && quote_mark_) quote_mark_ = q;
    }
    virtual bool operator==(const Expression& rhs) const;
    virtual String_Quoted_Ptr copy(Memory_Manager& mem, bool recursive = false);
    virtual std::string inspect() const; // quotes are forced on inspection
    ATTACH_OPERATIONS()
  };

  /////////////////
  // Media queries.
  /////////////////
  class Media_Query_Ref : public Expression_Ref,
                      public Vectorized<Media_Query_Expression_Obj> {
    ADD_PROPERTY(String_Obj, media_type)
    ADD_PROPERTY(bool, is_negated)
    ADD_PROPERTY(bool, is_restricted)
  public:
    Media_Query_Ref(ParserState pstate,
                String_Obj t = 0, size_t s = 0, bool n = false, bool r = false)
    : Expression_Ref(pstate), Vectorized<Media_Query_Expression_Obj>(s),
      media_type_(t), is_negated_(n), is_restricted_(r)
    { }
    ATTACH_OPERATIONS()
  };

  ////////////////////////////////////////////////////
  // Media expressions (for use inside media queries).
  ////////////////////////////////////////////////////
  class Media_Query_Expression_Ref : public Expression_Ref {
    ADD_PROPERTY(Expression_Obj, feature)
    ADD_PROPERTY(Expression_Obj, value)
    ADD_PROPERTY(bool, is_interpolated)
  public:
    Media_Query_Expression_Ref(ParserState pstate,
                           Expression_Obj f, Expression_Obj v, bool i = false)
    : Expression_Ref(pstate), feature_(f), value_(v), is_interpolated_(i)
    { }
    ATTACH_OPERATIONS()
  };

  ////////////////////
  // `@supports` rule.
  ////////////////////
  class Supports_Block_Ref : public Has_Block_Ref {
    ADD_PROPERTY(Supports_Condition_Obj, condition)
  public:
    Supports_Block_Ref(ParserState pstate, Supports_Condition_Obj condition, Block_Obj block = 0)
    : Has_Block_Ref(pstate, block), condition_(condition)
    { statement_type(SUPPORTS); }
    bool bubbles() { return true; }
    ATTACH_OPERATIONS()
  };

  //////////////////////////////////////////////////////
  // The abstract superclass of all Supports conditions.
  //////////////////////////////////////////////////////
  class Supports_Condition_Ref : public Expression_Ref {
  public:
    Supports_Condition_Ref(ParserState pstate)
    : Expression_Ref(pstate)
    { }
    virtual bool needs_parens(Supports_Condition_Obj cond) const { return false; }
    ATTACH_OPERATIONS()
  };

  ////////////////////////////////////////////////////////////
  // An operator condition (e.g. `CONDITION1 and CONDITION2`).
  ////////////////////////////////////////////////////////////
  class Supports_Operator_Ref : public Supports_Condition_Ref {
  public:
    enum Operand { AND, OR };
  private:
    ADD_PROPERTY(Supports_Condition_Obj, left);
    ADD_PROPERTY(Supports_Condition_Obj, right);
    ADD_PROPERTY(Operand, operand);
  public:
    Supports_Operator_Ref(ParserState pstate, Supports_Condition_Obj l, Supports_Condition_Obj r, Operand o)
    : Supports_Condition_Ref(pstate), left_(l), right_(r), operand_(o)
    { }
    virtual bool needs_parens(Supports_Condition_Obj cond) const;
    ATTACH_OPERATIONS()
  };

  //////////////////////////////////////////
  // A negation condition (`not CONDITION`).
  //////////////////////////////////////////
  class Supports_Negation_Ref : public Supports_Condition_Ref {
  private:
    ADD_PROPERTY(Supports_Condition_Obj, condition);
  public:
    Supports_Negation_Ref(ParserState pstate, Supports_Condition_Obj c)
    : Supports_Condition_Ref(pstate), condition_(c)
    { }
    virtual bool needs_parens(Supports_Condition_Obj cond) const;
    ATTACH_OPERATIONS()
  };

  /////////////////////////////////////////////////////
  // A declaration condition (e.g. `(feature: value)`).
  /////////////////////////////////////////////////////
  class Supports_Declaration_Ref : public Supports_Condition_Ref {
  private:
    ADD_PROPERTY(Expression_Obj, feature);
    ADD_PROPERTY(Expression_Obj, value);
  public:
    Supports_Declaration_Ref(ParserState pstate, Expression_Obj f, Expression_Obj v)
    : Supports_Condition_Ref(pstate), feature_(f), value_(v)
    { }
    virtual bool needs_parens(Supports_Condition_Obj cond) const { return false; }
    ATTACH_OPERATIONS()
  };

  ///////////////////////////////////////////////
  // An interpolation condition (e.g. `#{$var}`).
  ///////////////////////////////////////////////
  class Supports_Interpolation_Ref : public Supports_Condition_Ref {
  private:
    ADD_PROPERTY(Expression_Obj, value);
  public:
    Supports_Interpolation_Ref(ParserState pstate, Expression_Obj v)
    : Supports_Condition_Ref(pstate), value_(v)
    { }
    virtual bool needs_parens(Supports_Condition_Obj cond) const { return false; }
    ATTACH_OPERATIONS()
  };

  /////////////////////////////////////////////////
  // At root expressions (for use inside @at-root).
  /////////////////////////////////////////////////
  class At_Root_Query_Ref : public Expression_Ref {
  private:
    ADD_PROPERTY(Expression_Obj, feature)
    ADD_PROPERTY(Expression_Obj, value)
  public:
    At_Root_Query_Ref(ParserState pstate, Expression_Obj f = 0, Expression_Obj v = 0, bool i = false)
    : Expression_Ref(pstate), feature_(f), value_(v)
    { }
    bool exclude(std::string str);
    ATTACH_OPERATIONS()
  };

  ///////////
  // At-root.
  ///////////
  class At_Root_Block_Ref : public Has_Block_Ref {
    ADD_PROPERTY(At_Root_Query_Ptr, expression)
  public:
    At_Root_Block_Ref(ParserState pstate, Block_Obj b = 0, At_Root_Query_Ptr e = 0)
    : Has_Block_Ref(pstate, b), expression_(e)
    { statement_type(ATROOT); }
    bool bubbles() { return true; }
    bool exclude_node(Statement_Ptr s) {
      if (expression() == 0)
      {
        return s->statement_type() == Statement_Ref::RULESET;
      }

      if (s->statement_type() == Statement_Ref::DIRECTIVE)
      {
        if (Directive_Ptr dir = dynamic_cast<Directive_Ptr>(s))
        {
          std::string keyword(dir->keyword());
          if (keyword.length() > 0) keyword.erase(0, 1);
          return expression()->exclude(keyword);
        }
      }
      if (s->statement_type() == Statement_Ref::MEDIA)
      {
        return expression()->exclude("media");
      }
      if (s->statement_type() == Statement_Ref::RULESET)
      {
        return expression()->exclude("rule");
      }
      if (s->statement_type() == Statement_Ref::SUPPORTS)
      {
        return expression()->exclude("supports");
      }
      if (Directive_Ptr dir = dynamic_cast<Directive_Ptr>(s))
      {
        if (dir->is_keyframes()) return expression()->exclude("keyframes");
      }
      return false;
    }
    ATTACH_OPERATIONS()
  };

  //////////////////
  // The null value.
  //////////////////
  class Null_Ref : public Value_Ref {
  public:
    Null_Ref(ParserState pstate) : Value_Ref(pstate) { concrete_type(NULL_VAL); }
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
  class Thunk_Ref : public Expression_Ref {
    ADD_PROPERTY(Expression_Ptr, expression)
    ADD_PROPERTY(Env*, environment)
  public:
    Thunk_Ref(ParserState pstate, Expression_Ptr exp, Env* env = 0)
    : Expression_Ref(pstate), expression_(exp), environment_(env)
    { }
  };

  /////////////////////////////////////////////////////////
  // Individual parameter objects for mixins and functions.
  /////////////////////////////////////////////////////////
  class Parameter_Ref : public AST_Node_Ref {
    ADD_PROPERTY(std::string, name)
    ADD_PROPERTY(Expression_Ptr, default_value)
    ADD_PROPERTY(bool, is_rest_parameter)
  public:
    Parameter_Ref(ParserState pstate,
              std::string n, Expression_Ptr def = 0, bool rest = false)
    : AST_Node_Ref(pstate), name_(n), default_value_(def), is_rest_parameter_(rest)
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
  class Parameters_Ref : public AST_Node_Ref, public Vectorized<Parameter_Obj> {
    ADD_PROPERTY(bool, has_optional_parameters)
    ADD_PROPERTY(bool, has_rest_parameter)
  protected:
    void adjust_after_pushing(Parameter_Obj p)
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
    Parameters_Ref(ParserState pstate)
    : AST_Node_Ref(pstate),
      Vectorized<Parameter_Obj>(),
      has_optional_parameters_(false),
      has_rest_parameter_(false)
    { }
    ATTACH_OPERATIONS()
  };

  /////////////////////////////////////////
  // Abstract base class for CSS selectors.
  /////////////////////////////////////////
  class Selector_Ref : public Expression_Ref {
    // ADD_PROPERTY(bool, has_reference)
    // line break before list separator
    ADD_PROPERTY(bool, has_line_feed)
    // line break after list separator
    ADD_PROPERTY(bool, has_line_break)
    // maybe we have optional flag
    ADD_PROPERTY(bool, is_optional)
    // parent block pointers
    ADD_PROPERTY(Media_Block_Ptr, media_block)
  protected:
    size_t hash_;
  public:
    Selector_Ref(ParserState pstate, bool r = false, bool h = false)
    : Expression_Ref(pstate),
      // has_reference_(r),
      has_line_feed_(false),
      has_line_break_(false),
      is_optional_(false),
      media_block_(0),
      hash_(0)
    { concrete_type(SELECTOR); }
    virtual ~Selector_Ref() = 0;
    virtual size_t hash() = 0;
    virtual unsigned long specificity() {
      return 0;
    }
    virtual void set_media_block(Media_Block_Ptr mb) {
      media_block(mb);
    }
    virtual bool has_wrapped_selector() {
      return false;
    }
    virtual bool has_placeholder() {
      return false;
    }
    virtual bool has_parent_ref() {
      return false;
    }
    virtual bool has_real_parent_ref() {
      return false;
    }
  };
  inline Selector_Ref::~Selector_Ref() { }

  /////////////////////////////////////////////////////////////////////////
  // Interpolated selectors -- the interpolated String will be expanded and
  // re-parsed into a normal selector class.
  /////////////////////////////////////////////////////////////////////////
  class Selector_Schema_Ref : public Selector_Ref {
    ADD_PROPERTY(String_Ptr, contents)
    ADD_PROPERTY(bool, at_root);
  public:
    Selector_Schema_Ref(ParserState pstate, String_Ptr c)
    : Selector_Ref(pstate), contents_(c), at_root_(false)
    { }
    virtual bool has_parent_ref();
    virtual bool has_real_parent_ref();
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
  class Simple_Selector_Ref : public Selector_Ref {
    ADD_PROPERTY(std::string, ns);
    ADD_PROPERTY(std::string, name)
    ADD_PROPERTY(bool, has_ns)
  public:
    Simple_Selector_Ref(ParserState pstate, std::string n = "")
    : Selector_Ref(pstate), ns_(""), name_(n), has_ns_(false)
    {
      size_t pos = n.find('|');
      // found some namespace
      if (pos != std::string::npos) {
        has_ns_ = true;
        ns_ = n.substr(0, pos);
        name_ = n.substr(pos + 1);
      }
    }
    virtual bool unique() const
    {
      return false;
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

    virtual ~Simple_Selector_Ref() = 0;
    virtual Compound_Selector_Ptr unify_with(Compound_Selector_Ptr, Context&);
    virtual bool has_parent_ref() { return false; };
    virtual bool has_real_parent_ref() { return false; };
    virtual bool is_pseudo_element() { return false; }
    virtual bool is_pseudo_class() { return false; }

    virtual bool is_superselector_of(Compound_Selector_Ptr sub) { return false; }

    bool operator==(const Simple_Selector_Ref& rhs) const;
    inline bool operator!=(const Simple_Selector_Ref& rhs) const { return !(*this == rhs); }

    bool operator<(const Simple_Selector_Ref& rhs) const;
    // default implementation should work for most of the simple selectors (otherwise overload)
    ATTACH_OPERATIONS();
  };
  inline Simple_Selector_Ref::~Simple_Selector_Ref() { }


  //////////////////////////////////
  // The Parent Selector Expression.
  //////////////////////////////////
  // parent selectors can occur in selectors but also
  // inside strings in declarations (Compound_Selector).
  // only one simple parent selector means the first case.
  class Parent_Selector_Ref : public Simple_Selector_Ref {
    ADD_PROPERTY(bool, real)
  public:
    Parent_Selector_Ref(ParserState pstate, bool r = true)
    : Simple_Selector_Ref(pstate, "&"), real_(r)
    { /* has_reference(true); */ }
    bool is_real_parent_ref() { return real(); };
    virtual bool has_parent_ref() { return true; };
    virtual bool has_real_parent_ref() { return is_real_parent_ref(); };
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
  class Placeholder_Selector_Ref : public Simple_Selector_Ref {
  public:
    Placeholder_Selector_Ref(ParserState pstate, std::string n)
    : Simple_Selector_Ref(pstate, n)
    { }
    virtual unsigned long specificity()
    {
      return Constants::Specificity_Base;
    }
    virtual bool has_placeholder() {
      return true;
    }
    // virtual Placeholder_Selector_Ptr find_placeholder();
    virtual ~Placeholder_Selector_Ref() {};
    ATTACH_OPERATIONS()
  };

  /////////////////////////////////////////////////////////////////////
  // Element selectors (and the universal selector) -- e.g., div, span, *.
  /////////////////////////////////////////////////////////////////////
  class Element_Selector_Ref : public Simple_Selector_Ref {
  public:
    Element_Selector_Ref(ParserState pstate, std::string n)
    : Simple_Selector_Ref(pstate, n)
    { }
    virtual unsigned long specificity()
    {
      if (name() == "*") return 0;
      else               return Constants::Specificity_Element;
    }
    virtual Simple_Selector_Ptr unify_with(Simple_Selector_Ptr, Context&);
    virtual Compound_Selector_Ptr unify_with(Compound_Selector_Ptr, Context&);
    ATTACH_OPERATIONS()
  };

  ////////////////////////////////////////////////
  // Class selectors  -- i.e., .foo.
  ////////////////////////////////////////////////
  class Class_Selector_Ref : public Simple_Selector_Ref {
  public:
    Class_Selector_Ref(ParserState pstate, std::string n)
    : Simple_Selector_Ref(pstate, n)
    { }
    virtual bool unique() const
    {
      return false;
    }
    virtual unsigned long specificity()
    {
      return Constants::Specificity_Class;
    }
    virtual Compound_Selector_Ptr unify_with(Compound_Selector_Ptr, Context&);
    ATTACH_OPERATIONS()
  };

  ////////////////////////////////////////////////
  // ID selectors -- i.e., #foo.
  ////////////////////////////////////////////////
  class Id_Selector_Ref : public Simple_Selector_Ref {
  public:
    Id_Selector_Ref(ParserState pstate, std::string n)
    : Simple_Selector_Ref(pstate, n)
    { }
    virtual bool unique() const
    {
      return true;
    }
    virtual unsigned long specificity()
    {
      return Constants::Specificity_ID;
    }
    virtual Compound_Selector_Ptr unify_with(Compound_Selector_Ptr, Context&);
    ATTACH_OPERATIONS()
  };

  ///////////////////////////////////////////////////
  // Attribute selectors -- e.g., [src*=".jpg"], etc.
  ///////////////////////////////////////////////////
  class Attribute_Selector_Ref : public Simple_Selector_Ref {
    ADD_PROPERTY(std::string, matcher)
    ADD_PROPERTY(String_Ptr, value) // might be interpolated
  public:
    Attribute_Selector_Ref(ParserState pstate, std::string n, std::string m, String_Ptr v)
    : Simple_Selector_Ref(pstate, n), matcher_(m), value_(v)
    { }
    virtual size_t hash()
    {
      if (hash_ == 0) {
        hash_combine(hash_, Simple_Selector_Ref::hash());
        hash_combine(hash_, std::hash<std::string>()(matcher()));
        if (value_) hash_combine(hash_, value_->hash());
      }
      return hash_;
    }
    virtual unsigned long specificity()
    {
      return Constants::Specificity_Attr;
    }
    bool operator==(const Simple_Selector_Ref& rhs) const;
    bool operator==(const Attribute_Selector_Ref& rhs) const;
    bool operator<(const Simple_Selector_Ref& rhs) const;
    bool operator<(const Attribute_Selector_Ref& rhs) const;
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
  class Pseudo_Selector_Ref : public Simple_Selector_Ref {
    ADD_PROPERTY(String_Ptr, expression)
  public:
    Pseudo_Selector_Ref(ParserState pstate, std::string n, String_Ptr expr = 0)
    : Simple_Selector_Ref(pstate, n), expression_(expr)
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
        hash_combine(hash_, Simple_Selector_Ref::hash());
        if (expression_) hash_combine(hash_, expression_->hash());
      }
      return hash_;
    }
    virtual unsigned long specificity()
    {
      if (is_pseudo_element())
        return Constants::Specificity_Element;
      return Constants::Specificity_Pseudo;
    }
    bool operator==(const Simple_Selector_Ref& rhs) const;
    bool operator==(const Pseudo_Selector_Ref& rhs) const;
    bool operator<(const Simple_Selector_Ref& rhs) const;
    bool operator<(const Pseudo_Selector_Ref& rhs) const;
    virtual Compound_Selector_Ptr unify_with(Compound_Selector_Ptr, Context&);
    ATTACH_OPERATIONS()
  };

  /////////////////////////////////////////////////
  // Wrapped selector -- pseudo selector that takes a list of selectors as argument(s) e.g., :not(:first-of-type), :-moz-any(ol p.blah, ul, menu, dir)
  /////////////////////////////////////////////////
  class Wrapped_Selector_Ref : public Simple_Selector_Ref {
    ADD_PROPERTY(Selector_Ptr, selector)
  public:
    Wrapped_Selector_Ref(ParserState pstate, std::string n, Selector_Ptr sel)
    : Simple_Selector_Ref(pstate, n), selector_(sel)
    { }
    virtual bool is_superselector_of(Wrapped_Selector_Ptr sub);
    // Selectors inside the negation pseudo-class are counted like any
    // other, but the negation itself does not count as a pseudo-class.
    virtual size_t hash()
    {
      if (hash_ == 0) {
        hash_combine(hash_, Simple_Selector_Ref::hash());
        if (selector_) hash_combine(hash_, selector_->hash());
      }
      return hash_;
    }
    virtual bool has_parent_ref() {
      // if (has_reference()) return true;
      if (!selector()) return false;
      return selector()->has_parent_ref();
    }
    virtual bool has_real_parent_ref() {
      // if (has_reference()) return true;
      if (!selector()) return false;
      return selector()->has_real_parent_ref();
    }
    virtual bool has_wrapped_selector()
    {
      return true;
    }
    virtual unsigned long specificity()
    {
      return selector_ ? selector_->specificity() : 0;
    }
    bool operator==(const Simple_Selector_Ref& rhs) const;
    bool operator==(const Wrapped_Selector_Ref& rhs) const;
    bool operator<(const Simple_Selector_Ref& rhs) const;
    bool operator<(const Wrapped_Selector_Ref& rhs) const;
    ATTACH_OPERATIONS()
  };

  struct Complex_Selector_Pointer_Compare {
    bool operator() (Complex_Selector_Ptr_Const const pLeft, Complex_Selector_Ptr_Const const pRight) const;
  };

  ////////////////////////////////////////////////////////////////////////////
  // Simple selector sequences. Maintains flags indicating whether it contains
  // any parent references or placeholders, to simplify expansion.
  ////////////////////////////////////////////////////////////////////////////
  typedef std::set<Complex_Selector_Ptr, Complex_Selector_Pointer_Compare> SourcesSet;
  class Compound_Selector_Ref : public Selector_Ref, public Vectorized<Simple_Selector_Obj> {
  private:
    SourcesSet sources_;
    ADD_PROPERTY(bool, extended);
    ADD_PROPERTY(bool, has_parent_reference);
  protected:
    void adjust_after_pushing(Simple_Selector_Obj s)
    {
      // if (s->has_reference())   has_reference(true);
      // if (s->has_placeholder()) has_placeholder(true);
    }
  public:
    Compound_Selector_Ref(ParserState pstate, size_t s = 0)
    : Selector_Ref(pstate),
      Vectorized<Simple_Selector_Obj>(s),
      extended_(false),
      has_parent_reference_(false)
    { }
    bool contains_placeholder() {
      for (size_t i = 0, L = length(); i < L; ++i) {
        if ((*this)[i]->has_placeholder()) return true;
      }
      return false;
    };

    Compound_Selector_Ref& operator<<(Simple_Selector_Ptr element);

    bool is_universal() const
    {
      return length() == 1 && (*this)[0]->is_universal();
    }

    Complex_Selector_Ptr to_complex(Memory_Manager& mem);
    Compound_Selector_Ptr unify_with(Compound_Selector_Ptr rhs, Context& ctx);
    // virtual Placeholder_Selector_Ptr find_placeholder();
    virtual bool has_parent_ref();
    virtual bool has_real_parent_ref();
    Simple_Selector_Ptr base()
    {
      // Implement non-const in terms of const. Safe to const_cast since this method is non-const
      return const_cast<Simple_Selector_Ptr>(static_cast<Compound_Selector_Ptr_Const>(this)->base());
    }
    Simple_Selector_Ptr_Const base() const {
      if (length() == 0) return 0;
      // ToDo: why is this needed?
      if (SASS_MEMORY_CAST(Element_Selector, (*this)[0]))
        return &(*this)[0];
      return 0;
    }
    virtual bool is_superselector_of(Compound_Selector_Ptr sub, std::string wrapped = "");
    virtual bool is_superselector_of(Complex_Selector_Ptr sub, std::string wrapped = "");
    virtual bool is_superselector_of(CommaComplex_Selector_Ptr sub, std::string wrapped = "");
    virtual size_t hash()
    {
      if (Selector::hash_ == 0) {
        hash_combine(Selector::hash_, std::hash<int>()(SELECTOR));
        if (length()) hash_combine(Selector_Ref::hash_, Vectorized::hash());
      }
      return Selector_Ref::hash_;
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
      if (Simple_Selector_Obj ss = elements().front()) {
        if (ss->has_wrapped_selector()) return true;
      }
      return false;
    }

    virtual bool has_placeholder()
    {
      if (length() == 0) return false;
      if (Simple_Selector_Obj ss = elements().front()) {
        if (ss->has_placeholder()) return true;
      }
      return false;
    }

    bool is_empty_reference()
    {
      return length() == 1 &&
             SASS_MEMORY_CAST(Parent_Selector, (*this)[0]);
    }
    std::vector<std::string> to_str_vec(); // sometimes need to convert to a flat "by-value" data structure

    bool operator<(const Compound_Selector& rhs) const;

    bool operator==(const Compound_Selector& rhs) const;
    inline bool operator!=(const Compound_Selector& rhs) const { return !(*this == rhs); }

    SourcesSet& sources() { return sources_; }
    void clearSources() { sources_.clear(); }
    void mergeSources(SourcesSet& sources, Context& ctx);

    Compound_Selector_Ptr clone(Context&) const; // does not clone the Simple_Selectors

    Compound_Selector_Ptr minus(Compound_Selector_Ptr rhs, Context& ctx);
    ATTACH_OPERATIONS()
  };

  ////////////////////////////////////////////////////////////////////////////
  // General selectors -- i.e., simple sequences combined with one of the four
  // CSS selector combinators (">", "+", "~", and whitespace). Essentially a
  // linked list.
  ////////////////////////////////////////////////////////////////////////////
  class Complex_Selector_Ref : public Selector_Ref {
  public:
    enum Combinator { ANCESTOR_OF, PARENT_OF, PRECEDES, ADJACENT_TO, REFERENCE };
  private:
    ADD_PROPERTY(Combinator, combinator)
    ADD_PROPERTY(Compound_Selector_Obj, head)
    ADD_PROPERTY(Complex_Selector_Obj, tail)
    ADD_PROPERTY(String_Ptr, reference);
  public:
    bool contains_placeholder() {
      if (head() && head()->contains_placeholder()) return true;
      if (tail() && tail()->contains_placeholder()) return true;
      return false;
    };
    Complex_Selector_Ref(ParserState pstate,
                     Combinator c = ANCESTOR_OF,
                     Compound_Selector_Obj h = 0,
                     Complex_Selector_Obj t = 0,
                     String_Ptr r = 0)
    : Selector_Ref(pstate),
      combinator_(c),
      head_(h), tail_(t),
      reference_(r)
    {
      // if ((h && h->has_reference())   || (t && t->has_reference()))   has_reference(true);
      // if ((h && h->has_placeholder()) || (t && t->has_placeholder())) has_placeholder(true);
    }
    virtual bool has_parent_ref();
    virtual bool has_real_parent_ref();

    Complex_Selector_Ptr skip_empty_reference()
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

    Complex_Selector_Ptr context(Context&);


    // front returns the first real tail
    // skips over parent and empty ones
    Complex_Selector_Ptr_Const first() const;

    // last returns the last real tail
    Complex_Selector_Ptr_Const last() const;

    CommaComplex_Selector_Ptr tails(Context& ctx, CommaComplex_Selector_Ptr tails);

    // unconstant accessors
    Complex_Selector_Ptr first();
    Complex_Selector_Ptr last();

    // some shortcuts that should be removed
    Complex_Selector_Ptr_Const innermost() const { return last(); };
    Complex_Selector_Ptr innermost() { return last(); };

    size_t length() const;
    CommaComplex_Selector_Ptr resolve_parent_refs(Context& ctx, CommaComplex_Selector_Ptr parents, bool implicit_parent = true);
    virtual bool is_superselector_of(Compound_Selector_Ptr sub, std::string wrapping = "");
    virtual bool is_superselector_of(Complex_Selector_Ptr sub, std::string wrapping = "");
    virtual bool is_superselector_of(CommaComplex_Selector_Ptr sub, std::string wrapping = "");
    // virtual Placeholder_Selector_Ptr find_placeholder();
    CommaComplex_Selector_Ptr unify_with(Complex_Selector_Ptr rhs, Context& ctx);
    Combinator clear_innermost();
    void append(Context&, Complex_Selector_Ptr);
    void set_innermost(Complex_Selector_Ptr, Combinator);
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
    virtual void set_media_block(Media_Block_Ptr mb) {
      media_block(mb);
      if (tail_) tail_->set_media_block(mb);
      if (head_) head_->set_media_block(mb);
    }
    virtual bool has_wrapped_selector() {
      if (head_ && head_->has_wrapped_selector()) return true;
      if (tail_ && tail_->has_wrapped_selector()) return true;
      return false;
    }
    virtual bool has_placeholder() {
      if (head_ && head_->has_placeholder()) return true;
      if (tail_ && tail_->has_placeholder()) return true;
      return false;
    }
    bool operator<(const Complex_Selector_Ref& rhs) const;
    bool operator==(const Complex_Selector_Ref& rhs) const;
    inline bool operator!=(const Complex_Selector_Ref& rhs) const { return !(*this == rhs); }
    SourcesSet sources()
    {
      //s = Set.new
      //seq.map {|sseq_or_op| s.merge sseq_or_op.sources if sseq_or_op.is_a?(SimpleSequence)}
      //s

      SourcesSet srcs;

      Compound_Selector_Obj pHead = head();
      Complex_Selector_Obj  pTail = tail();

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
      Complex_Selector_Obj pIter = this;
      while (pIter) {
        Compound_Selector_Obj pHead = pIter->head();

        if (pHead) {
          pHead->mergeSources(sources, ctx);
        }

        pIter = pIter->tail();
      }
    }
    void clearSources() {
      Complex_Selector_Obj pIter = this;
      while (pIter) {
        Compound_Selector_Obj pHead = pIter->head();

        if (pHead) {
          pHead->clearSources();
        }

        pIter = pIter->tail();
      }
    }
    Complex_Selector_Ptr clone(Context&) const;      // does not clone Compound_Selector_Ptrs
    Complex_Selector_Ptr cloneFully(Context&) const; // clones Compound_Selector_Ptrs
    // std::vector<Compound_Selector_Ptr> to_vector();
    ATTACH_OPERATIONS()
  };

  typedef std::deque<Complex_Selector_Ptr> ComplexSelectorDeque;
  typedef Subset_Map<std::string, std::pair<Complex_Selector_Ptr, Compound_Selector_Ptr> > ExtensionSubsetMap;

  ///////////////////////////////////
  // Comma-separated selector groups.
  ///////////////////////////////////
  class CommaComplex_Selector_Ref : public Selector_Ref, public Vectorized<Complex_Selector_Obj> {
    ADD_PROPERTY(std::vector<std::string>, wspace)
  protected:
    void adjust_after_pushing(Complex_Selector_Ptr c);
  public:
    CommaComplex_Selector_Ref(ParserState pstate, size_t s = 0)
    : Selector_Ref(pstate), Vectorized<Complex_Selector_Obj>(s), wspace_(0)
    { }
    std::string type() { return "list"; }
    // remove parent selector references
    // basically unwraps parsed selectors
    virtual bool has_parent_ref();
    virtual bool has_real_parent_ref();
    void remove_parent_selectors();
    // virtual Placeholder_Selector_Ptr find_placeholder();
    CommaComplex_Selector_Ptr resolve_parent_refs(Context& ctx, CommaComplex_Selector_Ptr parents, bool implicit_parent = true);
    virtual bool is_superselector_of(Compound_Selector_Ptr sub, std::string wrapping = "");
    virtual bool is_superselector_of(Complex_Selector_Ptr sub, std::string wrapping = "");
    virtual bool is_superselector_of(CommaComplex_Selector_Ptr sub, std::string wrapping = "");
    CommaComplex_Selector_Ptr unify_with(CommaComplex_Selector_Ptr, Context&);
    void populate_extends(CommaComplex_Selector_Ptr, Context&, ExtensionSubsetMap&);
    virtual size_t hash()
    {
      if (Selector_Ref::hash_ == 0) {
        hash_combine(Selector_Ref::hash_, std::hash<int>()(SELECTOR));
        hash_combine(Selector_Ref::hash_, Vectorized::hash());
      }
      return Selector_Ref::hash_;
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
    virtual void set_media_block(Media_Block_Ptr mb) {
      media_block(mb);
      for (Complex_Selector_Obj cs : elements()) {
        cs->set_media_block(mb);
      }
    }
    virtual bool has_wrapped_selector() {
      for (Complex_Selector_Obj cs : elements()) {
        if (cs->has_wrapped_selector()) return true;
      }
      return false;
    }
    virtual bool has_placeholder() {
      for (Complex_Selector_Obj cs : elements()) {
        if (cs->has_placeholder()) return true;
      }
      return false;
    }
    CommaComplex_Selector_Ptr clone(Context&) const;      // does not clone Compound_Selector_Ptrs
    CommaComplex_Selector_Ptr cloneFully(Context&) const; // clones Compound_Selector_Ptrs
    virtual bool operator==(const Selector_Ref& rhs) const;
    virtual bool operator==(const CommaComplex_Selector& rhs) const;
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
  struct cmp_complex_selector { inline bool operator() (const Complex_Selector_Obj l, const Complex_Selector_Obj r) { return (*l < *r); } };
  struct cmp_compound_selector { inline bool operator() (Compound_Selector_Ptr_Const l, Compound_Selector_Ptr_Const r) { return (*l < *r); } };
  struct cmp_simple_selector { inline bool operator() (const Simple_Selector_Obj l, const Simple_Selector_Obj r) { return (*l < *r); } };


}

#ifdef __clang__

#pragma clang diagnostic pop

#endif

#endif
