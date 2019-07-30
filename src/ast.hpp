#ifndef SASS_AST_H
#define SASS_AST_H

// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include <typeinfo>
#include <unordered_map>

#include "sass/base.h"
#include "ast_helpers.hpp"
#include "ast_fwd_decl.hpp"
#include "ast_def_macros.hpp"

#include "file.hpp"
#include "position.hpp"
#include "operation.hpp"
#include "environment.hpp"
#include "fn_utils.hpp"

namespace Sass {

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

  uint8_t sass_op_to_precedence(Sass_OP op);

  const char* sass_op_to_name(enum Sass_OP op);

  const char* sass_op_separator(enum Sass_OP op);

  //////////////////////////////////////////////////////////
  // Abstract base class for all abstract syntax tree nodes.
  //////////////////////////////////////////////////////////
  class AST_Node : public SharedObj {
    ADD_PROPERTY(ParserState, pstate)
  public:
    AST_Node(ParserState pstate)
    : pstate_(pstate)
    { }
    AST_Node(const AST_Node* ptr)
    : pstate_(ptr->pstate_)
    { }

    // allow implicit conversion to string
    // needed for by SharedPtr implementation
    operator std::string() {
      return to_string();
    }

    // AST_Node(AST_Node& ptr) = delete;

    virtual ~AST_Node() = 0;
    virtual size_t hash() const { return 0; }
    virtual std::string inspect() const { return to_string({ INSPECT, 5 }); }
    virtual std::string to_sass() const { return to_string({ TO_SASS, 5 }); }
    virtual std::string to_string(Sass_Inspect_Options opt) const;
    virtual std::string to_css(Sass_Inspect_Options opt) const;
    virtual std::string to_string() const;
    virtual std::string to_css() const;
    virtual void cloneChildren() {};
    // generic find function (not fully implemented yet)
    // ToDo: add specific implementions to all children
    virtual bool find ( bool (*f)(AST_Node_Obj) ) { return f(this); };
    void update_pstate(const ParserState& pstate);
    Offset off() { return pstate(); }
    Position pos() { return pstate(); }

    // Subclasses should only override these methods
    // The full set is emulated by calling only those
    // Make sure the left side is resonably upcasted!
    virtual bool operator< (const AST_Node& rhs) const {
      throw std::runtime_error("operator< not implemented");
    }
    virtual bool operator== (const AST_Node& rhs) const {
      throw std::runtime_error("operator== not implemented");
    }


    // We can give some reasonable implementations by using
    // inverst operators on the specialized implementations
    virtual bool operator>(const AST_Node& rhs) const { return rhs < *this; };
    virtual bool operator<=(const AST_Node& rhs) const { return !(rhs < *this); };
    virtual bool operator>=(const AST_Node& rhs) const { return !(*this < rhs); };
    virtual bool operator!=(const AST_Node& rhs) const { return !(*this == rhs); }

    ATTACH_ABSTRACT_COPY_OPERATIONS(AST_Node);
    ATTACH_ABSTRACT_CRTP_PERFORM_METHODS()
  };
  inline AST_Node::~AST_Node() { }

  //////////////////////////////////////////////////////////////////////
  // define cast template now (need complete type)
  //////////////////////////////////////////////////////////////////////

  template<class T>
  T* Cast(AST_Node* ptr) {
    return ptr && typeid(T) == typeid(*ptr) ?
           static_cast<T*>(ptr) : NULL;
  };

  template<class T>
  const T* Cast(const AST_Node* ptr) {
    return ptr && typeid(T) == typeid(*ptr) ?
           static_cast<const T*>(ptr) : NULL;
  };

  //////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////

  class SassNode : public AST_Node {
  public:
    SassNode(ParserState pstate) :
      AST_Node(pstate) {};
    ATTACH_VIRTUAL_COPY_OPERATIONS(SassNode);
    ATTACH_CRTP_PERFORM_METHODS();
  };

  class CallableInvocation {
    // The arguments passed to the callable.
    ADD_PROPERTY(ArgumentInvocationObj, arguments);
  public:
    CallableInvocation(
      ArgumentInvocation* arguments) :
      arguments_(arguments) {}
  };

  class ArgumentDeclaration : public SassNode {

    // The arguments that are taken.
    ADD_PROPERTY(std::vector<ArgumentObj>, arguments);

    // The name of the rest argument (as in `$args...`),
    // or `null` if none was declared.
    ADD_PROPERTY(std::string, restArg);

  public:

    ArgumentDeclaration(
      ParserState pstate,
      std::vector<ArgumentObj> arguments,
      std::string restArg = "");

    bool isEmpty() const {
      return arguments_.empty()
        && restArg_.empty();
    }

    static ArgumentDeclaration* parse(
      Context& context,
      std::string contents);

    void verify(
      size_t positional,
      NormalizedMap<ValueObj>& names,
      Backtraces traces);

    bool matches(
      size_t positional,
      NormalizedMap<ValueObj>& names);

  };

  class ArgumentInvocation : public SassNode {

    // The arguments passed by position.
    ADD_CONSTREF(std::vector<ExpressionObj>, positional);

    // The arguments passed by name.
    ADD_CONSTREF(NormalizedMap<ExpressionObj>, named);

    // The first rest argument (as in `$args...`).
    ADD_PROPERTY(ExpressionObj, restArg);

    // The second rest argument, which is expected to only contain a keyword map.
    ADD_PROPERTY(ExpressionObj, kwdRest);

  public:

    ArgumentInvocation(ParserState pstate,
      std::vector<ExpressionObj> positional,
      NormalizedMap<ExpressionObj> named,
      Expression* restArgs = nullptr,
      Expression* kwdRest = nullptr);

    // Returns whether this invocation passes no arguments.
    bool isEmpty() const;

    std::string toString() const;

    ATTACH_CRTP_PERFORM_METHODS();

  };


  /// The result of evaluating arguments to a function or mixin.
  class ArgumentResults : public SassNode {

    // Arguments passed by position.
    ADD_PROPERTY(std::vector<ValueObj>, positional);

    // The [AstNode]s that hold the spans for each [positional]
    // argument, or `null` if source span tracking is disabled. This
    // stores [AstNode]s rather than [FileSpan]s so it can avoid
    // calling [AstNode.span] if the span isn't required, since
    // some nodes need to do real work to manufacture a source span.
    // std::vector<Ast_Node_Obj> positionalNodes;

    // Arguments passed by name.
    ADD_PROPERTY(NormalizedMap<ValueObj>, named);

    // The [AstNode]s that hold the spans for each [named] argument,
    // or `null` if source span tracking is disabled. This stores
    // [AstNode]s rather than [FileSpan]s so it can avoid calling
    // [AstNode.span] if the span isn't required, since some nodes
    // need to do real work to manufacture a source span.
    // NormalizedMap<Ast_Node_Obj> namedNodes;

    // The separator used for the rest argument list, if any.
    ADD_PROPERTY(Sass_Separator, separator);

  public:

    ArgumentResults(
      ParserState pstate,
      std::vector<ValueObj> positional,
      NormalizedMap<ValueObj> named,
      Sass_Separator separator);

  };


  //////////////////////////////////////////////////////////////////////
  // Abstract base class for expressions. This side of the AST hierarchy
  // represents elements in value contexts, which exist primarily to be
  // evaluated and returned.
  //////////////////////////////////////////////////////////////////////
  class Expression : public SassNode {
  public:
    enum Type {
      NONE,
      BOOLEAN,
      NUMBER,
      COLOR,
      STRING,
      LIST,
      MAP,
      NULL_VAL,
      FUNCTION_VAL,
      C_WARNING,
      C_ERROR,
      FUNCTION,
      VARIABLE,
      PARENT,
      NUM_TYPES
    };
  private:
    // expressions in some contexts shouldn't be evaluated
    ADD_PROPERTY(Type, concrete_type)
  public:
    Expression(ParserState pstate, bool d = false, bool e = false, bool i = false, Type ct = NONE);
    virtual operator bool() { return true; }
    virtual ~Expression() { }
    virtual bool is_invisible() const { return false; }

    virtual std::string type() const { return ""; }
    static std::string type_name() { return ""; }

    virtual bool is_false() { return false; }
    // virtual bool is_true() { return !is_false(); }
    virtual bool operator== (const Expression& rhs) const { return false; }
    inline bool operator!=(const Expression& rhs) const { return !(rhs == *this); }
    virtual void set_delayed(bool delayed) { }
    ATTACH_VIRTUAL_COPY_OPERATIONS(Expression);
    size_t hash() const override { return 0; }
  };

}

/////////////////////////////////////////////////////////////////////////////////////
// Hash method specializations for std::unordered_map to work with Sass::Expression
/////////////////////////////////////////////////////////////////////////////////////

namespace std {
  template<>
  struct hash<Sass::Expression_Obj>
  {
    size_t operator()(Sass::Expression_Obj s) const
    {
      return s->hash();
    }
  };
  template<>
  struct equal_to<Sass::Expression_Obj>
  {
    bool operator()( Sass::Expression_Obj lhs,  Sass::Expression_Obj rhs) const
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
    mutable size_t hash_;
    void reset_hash() { hash_ = 0; }
  public:
    Vectorized(size_t s = 0) : hash_(0)
    { elements_.reserve(s); }
    Vectorized(const Vectorized<T>* vec) :
      elements_(vec->elements_),
      hash_(0)
    {}
    Vectorized(std::vector<T> vec) :
      elements_(vec),
      hash_(0)
    {}
    ~Vectorized() {};
    size_t length() const   { return elements_.size(); }
    bool empty() const      { return elements_.empty(); }
    void clear()            { return elements_.clear(); }
    T& last()               { return elements_.back(); }
    T& first()              { return elements_.front(); }
    const T& last() const   { return elements_.back(); }
    const T& first() const  { return elements_.front(); }

    bool operator== (const Vectorized<T>& rhs) const {
      // Abort early if sizes do not match
      if (length() != rhs.length()) return false;
      // Otherwise test each node for object equalicy in order
      return std::equal(begin(), end(), rhs.begin(), ObjEqualityFn<T>);
    }

    bool operator!= (const Vectorized<T>& rhs) const {
      return !(*this == rhs);
    }

    T& operator[](size_t i) { return elements_[i]; }
    virtual const T& at(size_t i) const { return elements_.at(i); }
    virtual T& at(size_t i) { return elements_.at(i); }
    const T& get(size_t i) const { return elements_[i]; }
    // ToDo: might insert am item (update ordered list)
    const T& operator[](size_t i) const { return elements_[i]; }

    // Implicitly get the std::vector from our object
    // Makes the Vector directly assignable to std::vector
    // You are responsible to make a copy if needed
    // Note: since this returns the real object, we can't
    // Note: guarantee that the hash will not get out of sync
    operator std::vector<T>&() { return elements_; }
    operator const std::vector<T>&() const { return elements_; }

    // Explicitly request all elements as a real std::vector
    // You are responsible to make a copy if needed
    // Note: since this returns the real object, we can't
    // Note: guarantee that the hash will not get out of sync
    std::vector<T>& elements() { return elements_; }
    const std::vector<T>& elements() const { return elements_; }

    // Insert all items from compatible vector
    void concat(const std::vector<T>& v)
    {
      if (!v.empty()) reset_hash();
      elements().insert(end(), v.begin(), v.end());
    }

    // Syntatic sugar for pointers
    void concat(const Vectorized<T>* v)
    {
      if (v != nullptr) {
        return concat(*v);
      }
    }

    // Insert one item on the front
    void unshift(T element)
    {
      reset_hash();
      elements_.insert(begin(), element);
    }

    // Remove and return item on the front
    // ToDo: handle empty vectors
    T shift() {
      reset_hash();
      T first = get(0);
      elements_.erase(begin());
      return first;
    }

    // Insert one item on the back
    // ToDo: rename this to push
    void append(T element)
    {
      if (!element) {
        std::cerr << "APPEND NULL PTR\n";
      }
      reset_hash();
      elements_.insert(end(), element);
    }

    // Check if an item already exists
    // Uses underlying object `operator==`
    // E.g. compares the actual objects
    bool contains(const T& el) const {
      for (const T& rhs : elements_) {
        // Test the underlying objects for equality
        // A std::find checks for pointer equality
        if (ObjEqualityFn(el, rhs)) {
          return true;
        }
      }
      return false;
    }

    // This might be better implemented as `operator=`?
    void elements(std::vector<T> e) {
      reset_hash();
      elements_ = std::move(e);
    }

    virtual size_t hash() const
    {
      if (hash_ == 0) {
        for (const T& el : elements_) {
          hash_combine(hash_, el->hash());
        }
      }
      return hash_;
    }

    template <typename P, typename V>
    typename std::vector<T>::iterator insert(P position, const V& val) {
      reset_hash();
      return elements_.insert(position, val);
    }

    typename std::vector<T>::iterator end() { return elements_.end(); }
    typename std::vector<T>::iterator begin() { return elements_.begin(); }
    typename std::vector<T>::const_iterator end() const { return elements_.end(); }
    typename std::vector<T>::const_iterator begin() const { return elements_.begin(); }
    typename std::vector<T>::iterator erase(typename std::vector<T>::iterator el) { reset_hash(); return elements_.erase(el); }
    typename std::vector<T>::const_iterator erase(typename std::vector<T>::const_iterator el) { reset_hash(); return elements_.erase(el); }

  };

  /////////////////////////////////////////////////////////////////////////////
  // Mixin class for AST nodes that should behave like a hash table. Uses an
  // extra <std::vector> internally to maintain insertion order for interation.
  /////////////////////////////////////////////////////////////////////////////
  template <typename K, typename T, typename U>
  class Hashed {
  private:
    std::unordered_map<
      K, T, ObjHash, ObjEquality
    > elements_;

    std::vector<K> _keys;
    std::vector<T> _values;
  protected:
    mutable size_t hash_;
    K duplicate_key_;
    void reset_hash() { hash_ = 0; }
    void reset_duplicate_key() { duplicate_key_ = {}; }
  public:
    Hashed(size_t s = 0)
    : elements_(),
      _keys(),
      _values(),
      hash_(0), duplicate_key_({})
    {
      _keys.reserve(s);
      _values.reserve(s);
      elements_.reserve(s);
    }
    virtual ~Hashed();
    size_t length() const                  { return _keys.size(); }
    bool empty() const                     { return _keys.empty(); }
    bool has(K k) const          {
      return elements_.find(k) != elements_.end();
    }
    T at(K k) const {
      if (elements_.count(k))
      {
        return elements_.at(k);
      }
      else { return {}; }
    }
    bool has_duplicate_key() const         { return duplicate_key_ != nullptr; }
    K get_duplicate_key() const  { return duplicate_key_; }
    const std::unordered_map<
      K, T, ObjHash, ObjEquality
    >& elements() { return elements_; }
    size_t erase(K key)
    {
      if (elements_.erase(key) == 1) {
        for (size_t i = 0; i < _keys.size(); i++) {
          if (!(*_keys[i] == *key)) continue;
          // Remove items given at position
          _keys.erase(_keys.begin() + i);
          _values.erase(_values.begin() + i);
          return 1;
        }
        throw std::runtime_error("Ordered is out of sync!?");
      }
      return 0;
    }

    void insert(K key, T val)
    {
      reset_hash();
      if (!has(key)) {
        _keys.push_back(key);
        _values.push_back(val);
      }
      else if (!duplicate_key_) {
        duplicate_key_ = key;
      }

      elements_[key] = val;
    }
    Hashed& operator<<(std::pair<K, T> p)
    {
      reset_hash();

      if (!has(p.first)) {
        _keys.push_back(p.first);
        _values.push_back(p.second);
      }
      else if (!duplicate_key_) {
        duplicate_key_ = p.first;
      }

      elements_[p.first] = p.second;

      return *this;
    }
    Hashed& operator+=(Hashed* h)
    {
      if (length() == 0) {
        this->elements_ = h->elements_;
        this->_values = h->_values;
        this->_keys = h->_keys;
        return *this;
      }

      for (auto key : h->keys()) {
        *this << std::make_pair(key, h->at(key));
      }

      reset_duplicate_key();
      return *this;
    }
    const std::unordered_map<
      K, T, ObjHash, ObjEquality
    >& pairs() const { return elements_; }

    K& getKey(size_t pos) { return _keys.at(pos); }
    T& getValue(size_t pos) { return _values.at(pos); }
    const std::vector<K>& keys() const { return _keys; }
    const std::vector<T>& values() const { return _values; }
    
    typename std::unordered_map<K, T, ObjHash, ObjEquality>::iterator end() { return elements_.end(); }
    typename std::unordered_map<K, T, ObjHash, ObjEquality>::iterator begin() { return elements_.begin(); }
    typename std::unordered_map<K, T, ObjHash, ObjEquality>::const_iterator end() const { return elements_.end(); }
    typename std::unordered_map<K, T, ObjHash, ObjEquality>::const_iterator begin() const { return elements_.begin(); }

  };
  template <typename K, typename T, typename U>
  inline Hashed<K, T, U>::~Hashed() { }

  /////////////////////////////////////////////////////////////////////////
  // Abstract base class for statements. This side of the AST hierarchy
  // represents elements in expansion contexts, which exist primarily to be
  // rewritten and macro-expanded.
  /////////////////////////////////////////////////////////////////////////
  class Statement : public AST_Node {
  public:
    enum Type {
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
    ADD_PROPERTY(Type, statement_type)
    ADD_PROPERTY(size_t, tabs)
    ADD_PROPERTY(bool, group_end)
  public:
    Statement(ParserState pstate, Type st = NONE, size_t t = 0);
    virtual ~Statement() = 0; // virtual destructor
    // needed for rearranging nested rulesets during CSS emission
    virtual bool bubbles();
    virtual bool has_content();
    virtual bool is_invisible() const;
    ATTACH_VIRTUAL_COPY_OPERATIONS(Statement)
  };
  inline Statement::~Statement() { }

  ////////////////////////
  // Blocks of statements.
  ////////////////////////
  class Block final : public Statement, public Vectorized<Statement_Obj> {
    ADD_PROPERTY(bool, is_root)
    // needed for properly formatted CSS emission
  public:
    Block(ParserState pstate, size_t s = 0, bool r = false);
    Block(ParserState pstate, std::vector<StatementObj> vec, bool r = false);
    bool isInvisible() const;
    bool is_invisible() const override {
      return isInvisible();
    }
    bool has_content() override;
    // ATTACH_COPY_OPERATIONS(Block)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ////////////////////////////////////////////////////////////////////////
  // Abstract base class for statements that contain blocks of statements.
  ////////////////////////////////////////////////////////////////////////
  class Has_Block : public Statement {
    ADD_PROPERTY(Block_Obj, block)
  public:
    void concat(std::vector<StatementObj>& vec) {
      if (!block()) {
        block(SASS_MEMORY_NEW(Block, pstate_));
      }
      block()->concat(vec);
    }
    Has_Block(ParserState pstate, Block_Obj b);
    Has_Block(const Has_Block* ptr); // copy constructor
    virtual ~Has_Block() = 0; // virtual destructor
    virtual bool has_content() override;
  };
  inline Has_Block::~Has_Block() { }

  // ToDo: ParentStatement
  /////////////////////////////////////////////////////////////////////////////
  // A style rule. This applies style declarations to elements 
  // that match a given selector. Formerly known as `Ruleset`.
  /////////////////////////////////////////////////////////////////////////////
  class StyleRule final : public Has_Block {
    // The selector to which the declaration will be applied.
    // This is only parsed after the interpolation has been resolved.
    ADD_PROPERTY(InterpolationObj, interpolation)
  public:
    StyleRule(ParserState pstate, Interpolation* s, Block_Obj b = {});
    bool empty() const { return block().isNull() || block()->empty(); }
    // ATTACH_COPY_OPERATIONS(StyleRule)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  // ToDo: ParentStatement
  ///////////////////////////////////////////////////////////////////////
  // At-rules -- arbitrary directives beginning with "@" that may have an
  // optional statement block.
  ///////////////////////////////////////////////////////////////////////
  class AtRule final : public Has_Block {
    ADD_CONSTREF(std::string, keyword)
      ADD_PROPERTY(InterpolationObj, interpolation)
      ADD_PROPERTY(Expression_Obj, value)

      ADD_PROPERTY(InterpolationObj, name2)
      ADD_PROPERTY(InterpolationObj, value2)

  public:
    AtRule(ParserState pstate, std::string kwd, Block_Obj b = {}, Expression_Obj val = {});
    AtRule(ParserState pstate, InterpolationObj itpl, Block_Obj b = {}, Expression_Obj val = {});
    bool bubbles() override;
    bool is_media();
    bool is_keyframes();
    ATTACH_COPY_OPERATIONS(AtRule)
      ATTACH_CRTP_PERFORM_METHODS()
  };

  /////////////////
  // Bubble.
  /////////////////
  class Bubble final : public Statement {
    ADD_PROPERTY(Statement_Obj, node)
    ADD_PROPERTY(bool, group_end)
  public:
    Bubble(ParserState pstate, Statement_Obj n, Statement_Obj g = {}, size_t t = 0);
    bool bubbles() override;
    // ATTACH_COPY_OPERATIONS(Bubble)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  /////////////////
  // Trace.
  /////////////////
  class Trace final : public Has_Block {
    ADD_CONSTREF(char, type)
    ADD_CONSTREF(std::string, name)
  public:
    Trace(ParserState pstate, std::string n, Block_Obj b = {}, char type = 'm');
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ///////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////

  // An expression that directly embeds a [Value]. This is never
  // constructed by the parser. It's only used when ASTs are
  // constructed dynamically, as for the `call()` function.
  class ValueExpression : public Expression {
    ADD_PROPERTY(ValueObj, value);
  public:
    ValueExpression(
      ParserState pstate,
      ValueObj value);
    ATTACH_CRTP_PERFORM_METHODS();
  };

  class ListExpression : public Expression {
    ADD_PROPERTY(std::vector<ExpressionObj>, contents);
    ADD_PROPERTY(Sass_Separator, separator);
    ADD_PROPERTY(bool, hasBrackets);
  public:
    ListExpression(ParserState pstate, Sass_Separator separator = SASS_UNDEF);
    void concat(std::vector<ExpressionObj>& expressions) {
      std::copy(
        expressions.begin(), expressions.end(),
        std::back_inserter(contents_)
      );
    }
    size_t size() const {
      return contents_.size();
    }
    Expression* get(size_t i) {
      return contents_[i];
    }
    void append(Expression* expression) {
      contents_.push_back(expression);
    }
    std::string toString() {
      // var buffer = StringBuffer();
      // if (hasBrackets) buffer.writeCharCode($lbracket);
      // buffer.write(contents
      //   .map((element) = >
      //     _elementNeedsParens(element) ? "($element)" : element.toString())
      //   .join(separator == ListSeparator.comma ? ", " : " "));
      // if (hasBrackets) buffer.writeCharCode($rbracket);
      // return buffer.toString();
      return "ListExpression";
    }
    // Returns whether [expression], contained in [this],
    // needs parentheses when printed as Sass source.
    bool _elementNeedsParens(Expression* expression) {
      /*
      if (expression is ListExpression) {
        if (expression.contents.length < 2) return false;
        if (expression.hasBrackets) return false;
        return separator == ListSeparator.comma
            ? separator == ListSeparator.comma
            : separator != ListSeparator.undecided;
      }

      if (separator != ListSeparator.space) return false;

      if (expression is UnaryOperationExpression) {
        return expression.operator == UnaryOperator.plus ||
            expression.operator == UnaryOperator.minus;
      }

      */
      return false;
    }
    ATTACH_CRTP_PERFORM_METHODS();
  };

  ///////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////

  class MapExpression final : public Expression {
    ADD_PROPERTY(std::vector<ExpressionObj>, kvlist);
  public:
    void append(Expression* kv) {
      kvlist_.push_back(kv);
    }
    size_t size() const {
      return kvlist_.size();
    }
    Expression* get(size_t i) {
      return kvlist_[i];
    }
    MapExpression(ParserState pstate);
    ATTACH_CRTP_PERFORM_METHODS();
  };

  ///////////////////////////////////////////////////////////////////////
  // Keyframe-rules -- the child blocks of "@keyframes" nodes.
  ///////////////////////////////////////////////////////////////////////
  class Keyframe_Rule final : public Has_Block {
    // according to css spec, this should be <keyframes-name>
    // <keyframes-name> = <custom-ident> | <string>
    ADD_PROPERTY(SelectorListObj, name)
    ADD_PROPERTY(StringLiteralObj, name2)

  public:
    Keyframe_Rule(ParserState pstate, Block_Obj b);
    ATTACH_COPY_OPERATIONS(Keyframe_Rule)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ////////////////////////////////////////////////////////////////////////
  // Declarations -- style rules consisting of a property name and values.
  ////////////////////////////////////////////////////////////////////////
  class Declaration final : public Has_Block {
    ADD_PROPERTY(String_Obj, property)
      ADD_PROPERTY(Expression_Obj, value)
      ADD_PROPERTY(bool, is_custom_property)
  public:
    Declaration(ParserState pstate, String_Obj prop, Expression_Obj val = {}, bool c = false, Block_Obj b = {});
    bool is_invisible() const override;
    // ATTACH_COPY_OPERATIONS(Declaration)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  /////////////////////////////////////
  // Assignments -- variable and value.
  /////////////////////////////////////
  class Assignment final : public Statement {
    ADD_CONSTREF(std::string, variable)
    ADD_PROPERTY(Expression_Obj, value)
    ADD_PROPERTY(bool, is_default)
    ADD_PROPERTY(bool, is_global)
  public:
    Assignment(ParserState pstate, std::string var, Expression_Obj val, bool is_default = false, bool is_global = false);
    // ATTACH_COPY_OPERATIONS(Assignment)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  class ImportBase : public Statement {
  public:
    ImportBase(ParserState pstate);
    ATTACH_VIRTUAL_COPY_OPERATIONS(ImportBase);
  };

  class StaticImport final : public ImportBase {
    ADD_PROPERTY(InterpolationObj, url);
    ADD_PROPERTY(SupportsCondition_Obj, supports);
    ADD_PROPERTY(InterpolationObj, media);
    ADD_PROPERTY(bool, outOfOrder);
  public:
    StaticImport(ParserState pstate, InterpolationObj url, SupportsCondition_Obj supports = {}, InterpolationObj media = {});
    // ATTACH_COPY_OPERATIONS(StaticImport);
    ATTACH_CRTP_PERFORM_METHODS();
  };

  class DynamicImport final : public ImportBase {
    ADD_PROPERTY(std::string, url);
  public:
    DynamicImport(ParserState pstate, std::string url);
    // ATTACH_COPY_OPERATIONS(DynamicImport);
    ATTACH_CRTP_PERFORM_METHODS();
  };


  class ImportRule final : public Statement, public Vectorized<ImportBaseObj> {
  public:
    ImportRule(ParserState pstate);
    // ATTACH_COPY_OPERATIONS(ImportRule);
    ATTACH_CRTP_PERFORM_METHODS();
  };

  ////////////////////////////////////////////////////////////////////////////
  // Import directives. CSS and Sass import lists can be intermingled, so it's
  // necessary to store a list of each in an Import node.
  ////////////////////////////////////////////////////////////////////////////
  class Import final : public ImportBase {
    std::vector<Expression_Obj> urls_;
    std::vector<Include>        incs_;
    std::vector<ImportBaseObj> imports_;
    ADD_PROPERTY(std::vector<ExpressionObj>, import_queries);
    ADD_PROPERTY(std::vector<CssMediaQueryObj>, queries);
  public:
    Import(ParserState pstate);
    std::vector<Include>& incs();
    std::vector<ExpressionObj>& urls();
    std::vector<ImportBaseObj>& imports();
    std::vector<ExpressionObj>& queries2();
    bool is_invisible() const override;
    // ATTACH_COPY_OPERATIONS(Import)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  // not yet resolved single import
  // so far we only know requested name
  class Import_Stub final : public ImportBase {
    Include resource_;
    // Sass_Import_Entry import_;
  public:
    Import_Stub(ParserState pstate, Include res/*,
      Sass_Import_Entry import*/);
    Include resource();
    // Sass_Import_Entry import();
    std::string imp_path();
    std::string abs_path();
    // ATTACH_COPY_OPERATIONS(Import_Stub)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  //////////////////////////////
  // The Sass `@warn` directive.
  //////////////////////////////
  class Warning final : public Statement {
    ADD_PROPERTY(Expression_Obj, message)
  public:
    Warning(ParserState pstate, Expression_Obj msg);
    // ATTACH_COPY_OPERATIONS(Warning)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ///////////////////////////////
  // The Sass `@error` directive.
  ///////////////////////////////
  class Error final : public Statement {
    ADD_PROPERTY(Expression_Obj, message)
  public:
    Error(ParserState pstate, Expression_Obj msg);
    // ATTACH_COPY_OPERATIONS(Error)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ///////////////////////////////
  // The Sass `@debug` directive.
  ///////////////////////////////
  class Debug final : public Statement {
    ADD_PROPERTY(Expression_Obj, value)
  public:
    Debug(ParserState pstate, Expression_Obj val);
    // ATTACH_COPY_OPERATIONS(Debug)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ///////////////////////////////////////////
  // CSS comments. These may be interpolated.
  ///////////////////////////////////////////
  class LoudComment final : public Statement {
    // The interpolated text of this comment, including comment characters.
    ADD_PROPERTY(InterpolationObj, text)
  public:
    LoudComment(ParserState pstate, InterpolationObj itpl);
    // ATTACH_COPY_OPERATIONS(LoudComment)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  class SilentComment final : public Statement {
    // The text of this comment, including comment characters.
    ADD_PROPERTY(std::string, text)
  public:
    SilentComment(ParserState pstate, std::string text);
    // not used in dart sass beside tests!?
    // std::string getDocComment() const;
    // ATTACH_COPY_OPERATIONS(SilentComment)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ////////////////////////////////////
  // The Sass `@if` control directive.
  ////////////////////////////////////
  class If final : public Has_Block {
    ADD_PROPERTY(Expression_Obj, predicate)
    ADD_PROPERTY(Block_Obj, alternative)
  public:
    If(ParserState pstate, Expression_Obj pred, Block_Obj con, Block_Obj alt = {});
    virtual bool has_content() override;
    // ATTACH_COPY_OPERATIONS(If)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  /////////////////////////////////////
  // The Sass `@for` control directive.
  /////////////////////////////////////
  class For final : public Has_Block {
    ADD_CONSTREF(std::string, variable)
    ADD_PROPERTY(Expression_Obj, lower_bound)
    ADD_PROPERTY(Expression_Obj, upper_bound)
    ADD_PROPERTY(bool, is_inclusive)
  public:
    For(ParserState pstate, std::string var, Expression_Obj lo, Expression_Obj hi, bool inc = false, Block_Obj b = {});
    // ATTACH_COPY_OPERATIONS(For)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  //////////////////////////////////////
  // The Sass `@each` control directive.
  //////////////////////////////////////
  class Each final : public Has_Block {
    ADD_PROPERTY(std::vector<std::string>, variables)
    ADD_PROPERTY(Expression_Obj, list)
  public:
    Each(ParserState pstate, std::vector<std::string> vars, Expression_Obj lst, Block_Obj b = {});
    // ATTACH_COPY_OPERATIONS(Each)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ///////////////////////////////////////
  // The Sass `@while` control directive.
  ///////////////////////////////////////
  class While final : public Has_Block {
    ADD_PROPERTY(Expression_Obj, predicate)
  public:
    While(ParserState pstate, Expression_Obj pred, Block_Obj b = {});
    // ATTACH_COPY_OPERATIONS(While)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  /////////////////////////////////////////////////////////////
  // The @return directive for use inside SassScript functions.
  /////////////////////////////////////////////////////////////
  class Return final : public Statement {
    ADD_PROPERTY(Expression_Obj, value);
  public:
    Return(ParserState pstate, Expression_Obj val);
    // ATTACH_COPY_OPERATIONS(Return)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  class InvocationExpression :
    public Expression,
    public CallableInvocation {
  public:
    InvocationExpression(ParserState pstate,
      ArgumentInvocation* arguments) :
      Expression(pstate),
      CallableInvocation(arguments)
    {
    }
  };

  class InvocationStatement :
    public Statement,
    public CallableInvocation {
  public:
    InvocationStatement(ParserState pstate,
      ArgumentInvocation* arguments) :
      Statement(pstate),
      CallableInvocation(arguments)
    {
    }
  };

  /// A function invocation.
  ///
  /// This may be a plain CSS function or a Sass function.
  class IfExpression : public InvocationExpression {

  public:
    IfExpression(ParserState pstate,
      ArgumentInvocation* arguments) :
      InvocationExpression(pstate, arguments)
    {
    }

    std::string toString() const {
      return "if" + arguments_->toString();
    }

  };

  /// A function invocation.
  ///
  /// This may be a plain CSS function or a Sass function.
  class FunctionExpression2 : public InvocationExpression {

    // The namespace of the function being invoked,
    // or `null` if it's invoked without a namespace.
    ADD_PROPERTY(std::string, ns);

    // The name of the function being invoked. If this is
    // interpolated, the function will be interpreted as plain
    // CSS, even if it has the same name as a Sass function.
    ADD_PROPERTY(InterpolationObj, name);

  public:
    FunctionExpression2(ParserState pstate,
      Interpolation* name,
      ArgumentInvocation* arguments,
      std::string ns = "") :
      InvocationExpression(pstate, arguments),
      ns_(ns), name_(name)
    {

    }

    ATTACH_CRTP_PERFORM_METHODS();
  };

  class MixinExpression : public InvocationExpression {

  };

  /////////////////////////////////////////////////////////////////////////////
  // Definitions for both mixins and functions. The two cases are distinguished
  // by a type tag.
  /////////////////////////////////////////////////////////////////////////////
  class CallableDeclaration : public Has_Block {
    // The name of this callable.
    // This may be empty for callables without names.
    ADD_PROPERTY(std::string, name);
    // The comment immediately preceding this declaration.
    ADD_PROPERTY(SilentCommentObj, comment);
    // The declared arguments this callable accepts.
    ADD_PROPERTY(ArgumentDeclarationObj, arguments);
  public:
    CallableDeclaration(
      ParserState pstate,
      std::string name,
      ArgumentDeclaration* arguments,
      SilentComment* comment = nullptr,
      Block* block = nullptr);
    ATTACH_ABSTRACT_CRTP_PERFORM_METHODS();
  };

  class ContentBlock :
    public CallableDeclaration {
  public:
    ContentBlock(
      ParserState pstate,
      ArgumentDeclaration* arguments = nullptr,
      std::vector<StatementObj> children = {});
    ATTACH_CRTP_PERFORM_METHODS();
  };

  class FunctionRule final : public CallableDeclaration {
  public:
    FunctionRule(
      ParserState pstate,
      std::string name,
      ArgumentDeclaration* arguments,
      SilentComment* comment = nullptr,
      Block* block = nullptr);
    ATTACH_CRTP_PERFORM_METHODS();
  };

  class MixinRule final : public CallableDeclaration {
  public:
    MixinRule(
      ParserState pstate,
      std::string name,
      ArgumentDeclaration* arguments,
      SilentComment* comment = nullptr,
      Block* block = nullptr);
    ATTACH_CRTP_PERFORM_METHODS();
  };

  class IncludeRule final : public InvocationStatement {

    // The namespace of the mixin being invoked, or
    // `null` if it's invoked without a namespace.
    ADD_PROPERTY(std::string, ns);

    // The name of the mixin being invoked.
    ADD_PROPERTY(std::string, name);

    // The block that will be invoked for [ContentRule]s in the mixin
    // being invoked, or `null` if this doesn't pass a content block.
    ADD_PROPERTY(ContentBlockObj, content);

  public:

    IncludeRule(
      ParserState pstate,
      std::string name,
      ArgumentInvocation* arguments,
      std::string ns = "",
      ContentBlock* content = nullptr,
      Block* block = nullptr);

    ATTACH_CRTP_PERFORM_METHODS();
  };

  class Definition final : public Has_Block {
  public:
    enum Type { MIXIN, FUNCTION };
    ADD_CONSTREF(std::string, name)
    ADD_PROPERTY(Parameters_Obj, parameters)
    ADD_PROPERTY(Env*, environment)
    ADD_PROPERTY(Type, type)
    ADD_PROPERTY(Native_Function, native_function)
    ADD_PROPERTY(Sass_Function_Entry, c_function)
    ADD_PROPERTY(void*, cookie)
    ADD_PROPERTY(bool, is_overload_stub)
    ADD_PROPERTY(size_t, defaultParams)
    ADD_PROPERTY(Signature, signature)
  public:
    Definition(ParserState pstate,
               std::string n,
               Parameters_Obj params,
               Block_Obj b,
               Type t);

    Definition(ParserState pstate,
               Signature sig,
               std::string n,
               Parameters_Obj params,
               Native_Function func_ptr,
               bool overload_stub = false);
    Definition(ParserState pstate,
               Signature sig,
               std::string n,
               Parameters_Obj params,
               Sass_Function_Entry c_func);

    ATTACH_COPY_OPERATIONS(Definition)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  //////////////////////////////////////
  // Mixin calls (i.e., `@include ...`).
  //////////////////////////////////////
  class Mixin_Call final : public Has_Block {
    ADD_CONSTREF(std::string, name)
    ADD_PROPERTY(ArgumentInvocationObj, arguments)
    ADD_PROPERTY(Parameters_Obj, block_parameters)
  public:
    Mixin_Call(ParserState pstate, std::string n, ArgumentInvocation* args, Parameters_Obj b_params = {}, Block_Obj b = {});
    // ATTACH_COPY_OPERATIONS(Mixin_Call)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ///////////////////////////////////////////////////
  // The @content directive for mixin content blocks.
  ///////////////////////////////////////////////////
  class Content final : public Statement {
    ADD_PROPERTY(ArgumentInvocationObj, arguments)
  public:
    Content(ParserState pstate, ArgumentInvocation* args);
    // ATTACH_COPY_OPERATIONS(Content)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ////////////////////////////////////////////////////////////////////////////
  class ParenthesizedExpression final : public Expression {
    ADD_PROPERTY(ExpressionObj, expression)
  public:
    ParenthesizedExpression(ParserState pstate, Expression* expression);
    // ATTACH_COPY_OPERATIONS(ParenthesizedExpression);
    ATTACH_CRTP_PERFORM_METHODS();
  };
  ////////////////////////////////////////////////////////////////////////////

  ////////////////////////////////////////////////////////////////////////////
  // Arithmetic negation (logical negation is just an ordinary function call).
  ////////////////////////////////////////////////////////////////////////////
  class Unary_Expression final : public Expression {
  public:
    enum Type { PLUS, MINUS, NOT, SLASH, END };
  private:
    ADD_PROPERTY(Type, optype)
    ADD_PROPERTY(Expression_Obj, operand)
  public:
    Unary_Expression(ParserState pstate, Type t, Expression_Obj o);
    // ATTACH_COPY_OPERATIONS(Unary_Expression)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  // A Media Ruleset before it has been evaluated
  // Could be already final or an interpolation
  class MediaRule final : public Has_Block {
    ADD_PROPERTY(InterpolationObj, query)
  public:
    // The query that determines on which platforms the styles will be in effect.
    // This is only parsed after the interpolation has been resolved.
    MediaRule(ParserState pstate, InterpolationObj query, Block_Obj block = {});

    bool bubbles() override { return true; };
    bool is_invisible() const override { return false; };
    // ATTACH_COPY_OPERATIONS(MediaRule)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  /////////////////////////////////////////////////
  // At root expressions (for use inside @at-root).
  /////////////////////////////////////////////////
  class At_Root_Query final : public Expression {
  private:
    ADD_PROPERTY(Expression_Obj, feature)
    ADD_PROPERTY(Expression_Obj, value)
  public:
    At_Root_Query(ParserState pstate, Expression_Obj f = {}, Expression_Obj v = {}, bool i = false);
    bool exclude(std::string str);
    // ATTACH_COPY_OPERATIONS(At_Root_Query)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ///////////
  // At-root.
  ///////////
  class At_Root_Block final : public Has_Block {
    ADD_PROPERTY(At_Root_Query_Obj, expression)
  public:
    At_Root_Block(ParserState pstate, At_Root_Query_Obj e = {}, Block_Obj b = {});
    bool bubbles() override;
    bool exclude_node(Statement_Obj s);
    // ATTACH_COPY_OPERATIONS(At_Root_Block)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ////////////////////////////////////////////////////////////
  // Individual argument objects for mixin and function calls.
  ////////////////////////////////////////////////////////////
  class Argument final : public Expression {
    HASH_PROPERTY(Expression_Obj, value)
      HASH_CONSTREF(std::string, name)
      ADD_PROPERTY(bool, is_rest_argument)
      ADD_PROPERTY(bool, is_keyword_argument)
      mutable size_t hash_;
  public:
    Argument(ParserState pstate, Expression_Obj val, std::string n = "", bool rest = false, bool keyword = false);
    size_t hash() const override;
    ATTACH_COPY_OPERATIONS(Argument)
      ATTACH_CRTP_PERFORM_METHODS()
  };

  ////////////////////////////////////////////////////////////////////////
  // Argument lists -- in their own class to facilitate context-sensitive
  // error checking (e.g., ensuring that all ordinal arguments precede all
  // named arguments).
  ////////////////////////////////////////////////////////////////////////
  class Arguments final : public Expression, public Vectorized<Argument_Obj> {
  public:
    bool hasRestArgument() const;
    bool hasNamedArgument() const;
    bool hasKeywordArgument() const;
    Arguments(ParserState pstate);
    Argument_Obj get_rest_argument();
    Argument_Obj get_keyword_argument();
    // ATTACH_COPY_OPERATIONS(Arguments)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  /////////////////////////////////////////////////////////
  // Individual parameter objects for mixins and functions.
  /////////////////////////////////////////////////////////
  class Parameter final : public AST_Node {
    ADD_CONSTREF(std::string, name)
    ADD_PROPERTY(Expression_Obj, default_value)
    ADD_PROPERTY(bool, is_rest_parameter)
  public:
    Parameter(ParserState pstate, std::string n, Expression_Obj def = {}, bool rest = false);
    // ATTACH_COPY_OPERATIONS(Parameter)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  /////////////////////////////////////////////////////////////////////////
  // Parameter lists -- in their own class to facilitate context-sensitive
  // error checking (e.g., ensuring that all optional parameters follow all
  // required parameters).
  /////////////////////////////////////////////////////////////////////////
  class Parameters final : public AST_Node, public Vectorized<Parameter_Obj> {
    ADD_PROPERTY(bool, has_optional_parameters)
    ADD_PROPERTY(bool, has_rest_parameter)
  public:
    Parameters(ParserState pstate);
    // ATTACH_COPY_OPERATIONS(Parameters)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  typedef Value* (*SassFnSig)(FN_PROTOTYPE2);
  typedef std::pair<ArgumentDeclaration*, SassFnSig> SassFnPair;
  typedef std::vector<SassFnPair> SassFnPairs;

  class Callable : public SassNode {
  public:
    Callable(ParserState pstate);
  };

  class UserDefinedCallable : public Callable {
    // The declaration.
    ADD_PROPERTY(CallableDeclarationObj, declaration);
    // The environment in which this callable was declared.
    ADD_PROPERTY(Env*, environment);
  public:
    UserDefinedCallable(ParserState pstate,
      CallableDeclarationObj declaration,
      Env* environment);
  };

  class PlainCssCallable : public Callable {
    ADD_PROPERTY(std::string, name);
  public:
    PlainCssCallable(ParserState pstate, std::string name);
  };

  class BuiltInCallable : public Callable {

    // The function name
    ADD_PROPERTY(std::string, name);

    // The overloads declared for this callable.
    ADD_PROPERTY(SassFnPairs, overloads);

  public:

    Value* execute(ArgumentInvocation* arguments) {
      // return callback(arguments);
      return nullptr;
    }

    // Creates a callable with a single [arguments] declaration
    // and a single [callback]. The argument declaration is parsed
    // from [arguments], which should not include parentheses.
    // Throws a [SassFormatException] if parsing fails.
    BuiltInCallable(
      std::string name,
      ArgumentDeclaration* parameters,
      SassFnSig callback);

    // Creates a callable with multiple implementations. Each
    // key/value pair in [overloads] defines the argument declaration
    // for the overload (which should not include parentheses), and
    // the callback to execute if that argument declaration matches.
    // Throws a [SassFormatException] if parsing fails.
    BuiltInCallable(
      std::string name,
      SassFnPairs overloads);

    SassFnPair callbackFor(
      size_t positional,
      NormalizedMap<ValueObj> names);

  };


}

#include "ast_css.hpp"
#include "ast_values.hpp"
#include "ast_supports.hpp"
#include "ast_selectors.hpp"

namespace Sass {

  List* sass_list_to_list(SassList* list);
  SassList* list_to_sass_list(List* list);

}



#ifdef __clang__

// #pragma clang diagnostic pop
// #pragma clang diagnostic push

#endif

#endif
