#ifndef SASS_AST_H
#define SASS_AST_H

// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include <vector>
#include <typeinfo>
#include <unordered_map>

#include "sass/base.h"
#include "ast_helpers.hpp"
#include "ast_fwd_decl.hpp"
#include "ast_def_macros.hpp"
#include "ordered_map.hpp"

#include "file.hpp"
#include "memory.hpp"
#include "mapping.hpp"
#include "position.hpp"
#include "operation.hpp"
#include "environment.hpp"
#include "fn_utils.hpp"
#include "environment_stack.hpp"

#include "ast_base.hpp"
#include "ast_containers.hpp"

namespace Sass {


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

  class Interpolant : public AST_Node {
  public:
    virtual ~Interpolant() {}
    Interpolant(SourceSpan&& pstate);
    Interpolant(const SourceSpan& pstate);

    ATTACH_EQ_OPERATIONS(Interpolant);

    virtual ItplString* getItplString() { return nullptr; }
    virtual Expression* getExpression() { return nullptr; }
    virtual const ItplString* getItplString() const { return nullptr; }
    virtual const Expression* getExpression() const { return nullptr; }

    virtual const sass::string& getText() const = 0;

    ATTACH_VIRTUAL_COPY_OPERATIONS(Interpolant);
  };

  ///////////////////////////////////////////////////////////////////////
  // A native string wrapped as an expression
  ///////////////////////////////////////////////////////////////////////
  class ItplString final : public Interpolant {
    ADD_CONSTREF(sass::string, text)
  public:
    ItplString(const SourceSpan& pstate, const sass::string& text);
    ItplString(const SourceSpan& pstate, sass::string&& text);

    const sass::string& getText() const override final { return text_; }
    ItplString* getItplString() override final { return this; }
    const ItplString* getItplString() const override final { return this; }

    bool operator==(const ItplString& rhs) const {
      return text_ == rhs.text_;
    }
    bool operator!=(const ItplString& rhs) const {
      return text_ != rhs.text_;
    }


    ATTACH_CLONE_OPERATIONS(ItplString);
    ATTACH_CRTP_PERFORM_METHODS();
  };

  //////////////////////////////////////////////////////////////////////
  // Abstract base class for expressions. This side of the AST hierarchy
  // represents elements in value contexts, which exist primarily to be
  // evaluated and returned.
  //////////////////////////////////////////////////////////////////////
  class Expression : public Interpolant {
  public:
    Expression(SourceSpan&& pstate);
    Expression(const SourceSpan& pstate);
    virtual const sass::string& getText() const override { return Strings::empty; }
    Expression* getExpression() override final { return this; }
    const Expression* getExpression() const override final { return this; }
    virtual operator bool() { return true; }
    virtual ~Expression() { }
    virtual bool is_invisible() const { return false; }

    virtual const sass::string& type() const {
      throw std::runtime_error("Invalid reflection");
    }

    virtual bool is_false() { return false; }
    // virtual bool is_true() { return !is_false(); }
    virtual bool operator== (const Expression& rhs) const { return false; }
    inline bool operator!=(const Expression& rhs) const { return !(rhs == *this); }
    ATTACH_VIRTUAL_COPY_OPERATIONS(Expression);
  };

}

/////////////////////////////////////////////////////////////////////////////////////
// Hash method specializations for std::unordered_map to work with Sass::Expression
/////////////////////////////////////////////////////////////////////////////////////

namespace Sass {


  //////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////

  class CallableInvocation {
    // The arguments passed to the callable.
    ADD_PROPERTY(ArgumentInvocationObj, arguments);
  public:
    CallableInvocation(
      ArgumentInvocation* arguments) :
      arguments_(arguments) {}
  };


  class ArgumentDeclaration : public AST_Node {

    // The arguments that are taken.
    ADD_CONSTREF(sass::vector<ArgumentObj>, arguments);

    // The name of the rest argument (as in `$args...`),
    // or `null` if none was declared.
    ADD_CONSTREF(sass::string, restArg);

  public:

    ArgumentDeclaration(
      const SourceSpan& pstate,
      const sass::vector<ArgumentObj>& arguments,
      const sass::string& restArg = "");

    bool isEmpty() const {
      return arguments_.empty()
        && restArg_.empty();
    }

    static ArgumentDeclaration* parse(
      Context& context,
      const sass::string& contents);

    void verify(
      size_t positional,
      const EnvKeyFlatMap<ValueObj>& names,
      const SourceSpan& pstate,
      const BackTraces& traces);

    bool matches(
      size_t positional,
      const EnvKeyFlatMap<ValueObj>& names);

    ATTACH_CRTP_PERFORM_METHODS();

  };


  /// The result of evaluating arguments to a function or mixin.
  class ArgumentResults {

    // Arguments passed by position.
    ADD_REF(sass::vector<ValueObj>, positional);

    // The [AstNode]s that hold the spans for each [positional]
    // argument, or `null` if source span tracking is disabled. This
    // stores [AstNode]s rather than [FileSpan]s so it can avoid
    // calling [AstNode.span] if the span isn't required, since
    // some nodes need to do real work to manufacture a source span.
    // sass::vector<Ast_Node_Obj> positionalNodes;

    // Arguments passed by name.
    // A list implementation might be more efficient
    // I dont expect any function to have many arguments
    // Normally the tradeoff is around 8 items in the list
    ADD_REF(EnvKeyFlatMap<ValueObj>, named);

    // The [AstNode]s that hold the spans for each [named] argument,
    // or `null` if source span tracking is disabled. This stores
    // [AstNode]s rather than [FileSpan]s so it can avoid calling
    // [AstNode.span] if the span isn't required, since some nodes
    // need to do real work to manufacture a source span.
    // EnvKeyFlatMap<Ast_Node_Obj> namedNodes;

    // The separator used for the rest argument list, if any.
    ADD_REF(Sass_Separator, separator);

  public:

    ArgumentResults() :
      separator_(SASS_UNDEF)
    {};

    ArgumentResults(
      const ArgumentResults& other);

    ArgumentResults(
      ArgumentResults&& other) noexcept;

    ArgumentResults& operator=(
      ArgumentResults&& other) noexcept;

    ArgumentResults(
      const sass::vector<ValueObj>& positional,
      const EnvKeyFlatMap<ValueObj>& named,
      Sass_Separator separator);

    ArgumentResults(
      sass::vector<ValueObj>&& positional,
      EnvKeyFlatMap<ValueObj>&& named,
      Sass_Separator separator);

    void clear() {
      named_.clear();
      positional_.clear();
    }

  };

  class EnvKeyFlatMap2 : public AST_Node, public EnvKeyFlatMap<ExpressionObj> {

  public:

    EnvKeyFlatMap2(const SourceSpan& pstate,
      EnvKeyFlatMap<ExpressionObj>&& named);

    ATTACH_CRTP_PERFORM_METHODS();

  };

  class ArgumentInvocation : public AST_Node {

    // The arguments passed by position.
    ADD_REF(sass::vector<ExpressionObj>, positional);

    // The argument expressions passed by name.
    ADD_CONSTREF(EnvKeyFlatMap2, named);

    // The first rest argument (as in `$args...`).
    ADD_PROPERTY(ExpressionObj, restArg);

    // The second rest argument, which is expected to only contain a keyword map.
    // This can be an already evaluated Map (via call) or a MapExpression.
    // So we must guarantee that this evaluates to a real Map value.
    ADD_PROPERTY(ExpressionObj, kwdRest);

  public:

    // Cache evaluated results object
    // Re-used for better performance
    ArgumentResults evaluated;

  public:

    ArgumentInvocation(const SourceSpan& pstate,
      const sass::vector<ExpressionObj>& positional,
      const EnvKeyFlatMap2& named,
      Expression* restArgs = nullptr,
      Expression* kwdRest = nullptr);

    ArgumentInvocation(SourceSpan&& pstate,
      sass::vector<ExpressionObj>&& positional,
      EnvKeyFlatMap2&& named,
      Expression* restArgs = nullptr,
      Expression* kwdRest = nullptr);

    // Returns whether this invocation passes no arguments.
    bool isEmpty() const;

    sass::string toString() const;

    ATTACH_CRTP_PERFORM_METHODS();

  };


  /////////////////////////////////////////////////////////////////////////
  // Abstract base class for statements. This side of the AST hierarchy
  // represents elements in expansion contexts, which exist primarily to be
  // rewritten and macro-expanded.
  /////////////////////////////////////////////////////////////////////////
  class Statement : public AST_Node {
  private:
    ADD_PROPERTY(size_t, tabs)
  public:
    Statement(SourceSpan&& pstate, size_t t = 0);
    Statement(const SourceSpan& pstate, size_t t = 0);
    virtual ~Statement() = 0; // virtual destructor
    // needed for rearranging nested rulesets during CSS emission
    virtual bool bubbles() const;
    virtual bool has_content();
    virtual bool is_invisible() const;
    ATTACH_VIRTUAL_COPY_OPERATIONS(Statement)
  };
  inline Statement::~Statement() { }

  ////////////////////////
  // Blocks of statements.
  ////////////////////////
  class Block final : public Statement, public VectorizedBase<Statement> {
    ADD_POINTER(IDXS*, idxs);
    ADD_PROPERTY(bool, is_root);
    // needed for properly formatted CSS emission
  public:
    Block(const SourceSpan& pstate, size_t s = 0, bool r = false);
    Block(const SourceSpan& pstate, const sass::vector<StatementObj>& vec, bool r = false);
    Block(const SourceSpan& pstate, sass::vector<StatementObj>&& vec, bool r = false);
    bool isInvisible() const;
    bool is_invisible() const override {
      return isInvisible();
    }
    bool has_content() override;
    // ATTACH_CLONE_OPERATIONS(Block)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ////////////////////////////////////////////////////////////////////////
  // Abstract base class for statements that contain blocks of statements.
  ////////////////////////////////////////////////////////////////////////

  // [X] AtRootRule
  // [X] AtRule
  // [X] CallableDeclaration
  // [X] Declaration
  // [X] EachRule
  // [X] ForRule
  // [X] MediaRule
  // [X] StyleRule
  // [ ] Stylesheet
  // [X] SupportsRule
  // [X] WhileRule
  class ParentStatement : public Statement {
    ADD_PROPERTY(Block_Obj, block)
  public:
    void concat(const sass::vector<StatementObj>& vec) {
      if (block_.ptr() == nullptr) {
        block_ = SASS_MEMORY_NEW(Block, pstate_);
      }
      block_->concat(vec);
    }
    void concat(sass::vector<StatementObj>&& vec) {
      if (block_.ptr() == nullptr) {
        block_ = SASS_MEMORY_NEW(Block, pstate_);
      }
      block_->concat(std::move(vec));
    }
    ParentStatement(SourceSpan&& pstate, Block_Obj b);
    ParentStatement(const SourceSpan& pstate, Block_Obj b);
    ParentStatement(const ParentStatement* ptr); // copy constructor
    virtual ~ParentStatement() = 0; // virtual destructor
    virtual bool has_content() override;
  };
  inline ParentStatement::~ParentStatement() { }

  /////////////////////////////////////////////////////////////////////////////
  // A style rule. This applies style declarations to elements 
  // that match a given selector. Formerly known as `Ruleset`.
  /////////////////////////////////////////////////////////////////////////////
  class StyleRule final : public ParentStatement {
    // The selector to which the declaration will be applied.
    // This is only parsed after the interpolation has been resolved.
    ADD_PROPERTY(InterpolationObj, interpolation);
    ADD_POINTER(IDXS*, idxs);
  public:
    StyleRule(SourceSpan&& pstate, Interpolation* s, Block_Obj b = {});
    bool empty() const { return block().isNull() || block()->empty(); }
    // ATTACH_CLONE_OPERATIONS(StyleRule)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  // ToDo: ParentStatement
  ///////////////////////////////////////////////////////////////////////
  // At-rules -- arbitrary directives beginning with "@" that may have an
  // optional statement block.
  ///////////////////////////////////////////////////////////////////////
  class AtRule final : public ParentStatement {
    ADD_PROPERTY(InterpolationObj, name);
    ADD_PROPERTY(InterpolationObj, value);
  public:
    AtRule(const SourceSpan& pstate,
      InterpolationObj name,
      ExpressionObj value,
      Block_Obj b = {});
    ATTACH_CLONE_OPERATIONS(AtRule);
    ATTACH_CRTP_PERFORM_METHODS();
  };

  /////////////////
  // Bubble.
  /////////////////
  class Bubble final : public Statement {
    ADD_PROPERTY(Statement_Obj, node)
  public:
    Bubble(const SourceSpan& pstate, Statement_Obj n, Statement_Obj g = {}, size_t t = 0);
    bool bubbles() const override final;
    // ATTACH_CLONE_OPERATIONS(Bubble)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  /////////////////
  // Trace.
  /////////////////
  class Trace final : public ParentStatement {
    ADD_CONSTREF(char, type)
    ADD_CONSTREF(sass::string, name)
  public:
    Trace(const SourceSpan& pstate, const sass::string& name, Block_Obj b = {}, char type = 'm');
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
      const SourceSpan& pstate,
      ValueObj value);
    ATTACH_CRTP_PERFORM_METHODS();
  };

  class ListExpression : public Expression {
    ADD_CONSTREF(sass::vector<ExpressionObj>, contents);
    ADD_PROPERTY(Sass_Separator, separator);
    ADD_PROPERTY(bool, hasBrackets);
  public:
    ListExpression(const SourceSpan& pstate, Sass_Separator separator = SASS_UNDEF);
    void concat(sass::vector<ExpressionObj>& expressions) {
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
      contents_.emplace_back(expression);
    }
    sass::string toString() {
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
    ADD_CONSTREF(sass::vector<ExpressionObj>, kvlist);
  public:
    void append(Expression* kv) {
      kvlist_.emplace_back(kv);
    }
    size_t size() const {
      return kvlist_.size();
    }
    Expression* get(size_t i) {
      return kvlist_[i];
    }
    MapExpression(const SourceSpan& pstate);
    ATTACH_CRTP_PERFORM_METHODS();
  };


  ////////////////////////////////////////////////////////////////////////
  // Declarations -- style rules consisting of a property name and values.
  ////////////////////////////////////////////////////////////////////////
  class Declaration final : public ParentStatement {
    ADD_PROPERTY(InterpolationObj, name);
    ADD_PROPERTY(ExpressionObj, value);
    ADD_PROPERTY(bool, is_custom_property);
  public:
    Declaration(const SourceSpan& pstate, InterpolationObj name, ExpressionObj value = {}, bool c = false, Block_Obj b = {});
    bool is_invisible() const override;
    // ATTACH_CLONE_OPERATIONS(Declaration)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  class MapMerge final : public Statement {
    ADD_CONSTREF(EnvKey, variable);
    ADD_PROPERTY(ExpressionObj, value);
    ADD_PROPERTY(IdxRef, vidx);
    ADD_PROPERTY(bool, is_default);
    ADD_PROPERTY(bool, is_global);
  public:
    MapMerge(const SourceSpan& pstate, const sass::string& var, IdxRef vidx, ExpressionObj val, bool is_default = false, bool is_global = false);
    // ATTACH_CLONE_OPERATIONS(Assignment)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  /////////////////////////////////////
  // Assignments -- variable and value.
  /////////////////////////////////////
  class Assignment final : public Statement {
    ADD_CONSTREF(EnvKey, variable);
    ADD_PROPERTY(ExpressionObj, value);
    ADD_PROPERTY(IdxRef, vidx);
    ADD_PROPERTY(bool, is_default);
    ADD_PROPERTY(bool, is_global);
  public:
    Assignment(const SourceSpan& pstate, const sass::string& var, IdxRef vidx, ExpressionObj val, bool is_default = false, bool is_global = false);
    // ATTACH_CLONE_OPERATIONS(Assignment)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  class ImportBase : public Statement {
  public:
    ImportBase(const SourceSpan& pstate);
    ATTACH_VIRTUAL_COPY_OPERATIONS(ImportBase);
  };

  class StaticImport final : public ImportBase {

    // The URL for this import.
    // This already contains quotes.
    ADD_PROPERTY(InterpolationObj, url);

    // The supports condition attached to this import,
    // or `null` if no condition is attached.
    ADD_PROPERTY(SupportsConditionObj, supports);

    // The media query attached to this import,
    // or `null` if no condition is attached.
    ADD_PROPERTY(InterpolationObj, media);

    ADD_CONSTREF(sass::vector<ExpressionObj>, queries44);

    // Flag to hoist import to the top.
    ADD_PROPERTY(bool, outOfOrder);

  public:

    // Object constructor by values
    StaticImport(const SourceSpan& pstate,
      InterpolationObj url,
      SupportsConditionObj supports = {},
      InterpolationObj media = {});

    // Attach public CRTP methods
    ATTACH_CRTP_PERFORM_METHODS();

  };
  // EO class StaticImport

  // Dynamic import beside its name must have a static url
  // We do not support to load sass partials programmatic
  // They also don't allow any supports or media queries.
  class DynamicImport : public ImportBase {
    ADD_CONSTREF(sass::string, url);
  public:
    DynamicImport(const SourceSpan& pstate, const sass::string& url);
    // ATTACH_CLONE_OPERATIONS(DynamicImport);
    ATTACH_CRTP_PERFORM_METHODS();
  };

  // After DynamicImport is loaded
  class IncludeImport final : public DynamicImport {
    ADD_CONSTREF(Include, include);
  public:
    IncludeImport(DynamicImport* import, Include include);
    // ATTACH_CLONE_OPERATIONS(DynamicImport);
    ATTACH_CRTP_PERFORM_METHODS();
  };

  class ImportRule final : public ImportBase, public VectorizedBase<ImportBase> {
  public:
    ImportRule(const SourceSpan& pstate);
    // ATTACH_CLONE_OPERATIONS(ImportRule);
    ATTACH_CRTP_PERFORM_METHODS();
  };

  ////////////////////////////////////////////////////////////////////////////
  // Import directives. CSS and Sass import lists can be intermingled, so it's
  // necessary to store a list of each in an Import node.
  ////////////////////////////////////////////////////////////////////////////
  class Import final : public ImportBase {
    sass::vector<Include> incs_;
  public:
    Import(const SourceSpan& pstate);
    sass::vector<Include>& incs();
    ATTACH_CRTP_PERFORM_METHODS()
  };

  //////////////////////////////
  // The Sass `@warn` directive.
  //////////////////////////////
  class WarnRule final : public Statement {
    ADD_PROPERTY(ExpressionObj, expression);
  public:
    WarnRule(const SourceSpan& pstate,
      ExpressionObj expression);
    // String toString() => "@warn $expression;";
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ///////////////////////////////
  // The Sass `@error` directive.
  ///////////////////////////////
  class ErrorRule final : public Statement {
    ADD_PROPERTY(ExpressionObj, expression);
  public:
    ErrorRule(const SourceSpan& pstate,
      ExpressionObj expression);
    // String toString() => "@error $expression;";
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ///////////////////////////////
  // The Sass `@debug` directive.
  ///////////////////////////////
  class DebugRule final : public Statement {
    ADD_PROPERTY(ExpressionObj, expression);
  public:
    DebugRule(const SourceSpan& pstate,
      ExpressionObj expression);
    // String toString() => "@debug $expression;";
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ///////////////////////////////////////////
  // CSS comments. These may be interpolated.
  ///////////////////////////////////////////
  class LoudComment final : public Statement {
    // The interpolated text of this comment, including comment characters.
    ADD_PROPERTY(InterpolationObj, text)
  public:
    LoudComment(const SourceSpan& pstate, InterpolationObj itpl);
    // ATTACH_CLONE_OPERATIONS(LoudComment)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  class SilentComment final : public Statement {
    // The text of this comment, including comment characters.
    ADD_CONSTREF(sass::string, text)
  public:
    SilentComment(const SourceSpan& pstate, const sass::string& text);
    // not used in dart sass beside tests!?
    // sass::string getDocComment() const;
    // ATTACH_CLONE_OPERATIONS(SilentComment)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ////////////////////////////////////
  // The Sass `@if` control directive.
  ////////////////////////////////////
  class If final : public ParentStatement {
    ADD_POINTER(IDXS*, idxs);
    ADD_PROPERTY(ExpressionObj, predicate);
    ADD_PROPERTY(Block_Obj, alternative);
  public:
    If(const SourceSpan& pstate, ExpressionObj pred, Block_Obj con, Block_Obj alt = {});
    virtual bool has_content() override;
    // ATTACH_CLONE_OPERATIONS(If)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  /////////////////////////////////////
  // The Sass `@for` control directive.
  /////////////////////////////////////
  class For final : public ParentStatement {
    ADD_CONSTREF(EnvKey, variable);
    ADD_PROPERTY(ExpressionObj, lower_bound);
    ADD_PROPERTY(ExpressionObj, upper_bound);
    ADD_POINTER(IDXS*, idxs);
    ADD_PROPERTY(bool, is_inclusive);
  public:
    For(const SourceSpan& pstate, const EnvKey& var, ExpressionObj lo, ExpressionObj hi, bool inc = false, Block_Obj b = {});
    // ATTACH_CLONE_OPERATIONS(For)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  //////////////////////////////////////
  // The Sass `@each` control directive.
  //////////////////////////////////////
  class Each final : public ParentStatement {
    ADD_CONSTREF(sass::vector<EnvKey>, variables);
    ADD_POINTER(IDXS*, idxs);
    ADD_PROPERTY(ExpressionObj, list);
  public:
    Each(const SourceSpan& pstate, const sass::vector<EnvKey>& vars, ExpressionObj lst, Block_Obj b = {});
    // ATTACH_CLONE_OPERATIONS(Each)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ///////////////////////////////////////
  // The Sass `@while` control directive.
  ///////////////////////////////////////
  class WhileRule final : public ParentStatement {
    ADD_PROPERTY(ExpressionObj, condition);
    ADD_POINTER(IDXS*, idxs);
  public:
    WhileRule(const SourceSpan& pstate,
      ExpressionObj condition,
      Block_Obj b = {});
    // String toString() = > "@while $condition {${children.join(" ")}}";
    ATTACH_CRTP_PERFORM_METHODS()
  };

  /////////////////////////////////////////////////////////////
  // The @return directive for use inside SassScript functions.
  /////////////////////////////////////////////////////////////
  class Return final : public Statement {
    ADD_PROPERTY(ExpressionObj, value);
  public:
    Return(const SourceSpan& pstate, ExpressionObj val);
    // ATTACH_CLONE_OPERATIONS(Return)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  class InvocationExpression :
    public Expression,
    public CallableInvocation {
  public:
    InvocationExpression(const SourceSpan& pstate,
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
    InvocationStatement(const SourceSpan& pstate,
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
    IfExpression(const SourceSpan& pstate,
      ArgumentInvocation* arguments) :
      InvocationExpression(pstate, arguments)
    {
    }

    sass::string toString() const {
      return "if" + arguments_->toString();
    }

    ATTACH_CRTP_PERFORM_METHODS();
  };

  /// A function invocation.
  ///
  /// This may be a plain CSS function or a Sass function.
  class FunctionExpression : public InvocationExpression {

    // The namespace of the function being invoked,
    // or `null` if it's invoked without a namespace.
    ADD_CONSTREF(sass::string, ns);

    ADD_PROPERTY(IdxRef, fidx);

    // The name of the function being invoked. If this is
    // interpolated, the function will be interpreted as plain
    // CSS, even if it has the same name as a Sass function.
    ADD_PROPERTY(InterpolationObj, name);

    ADD_PROPERTY(bool, selfAssign);

  public:
    FunctionExpression(const SourceSpan& pstate,
      Interpolation* name,
      ArgumentInvocation* arguments,
      const sass::string& ns = "") :
      InvocationExpression(pstate, arguments),
      ns_(ns), name_(name), selfAssign_(false)
    {

    }

    ATTACH_CRTP_PERFORM_METHODS();
  };

  /////////////////////////////////////////////////////////////////////////////
  // Definitions for both mixins and functions. The two cases are distinguished
  // by a type tag.
  /////////////////////////////////////////////////////////////////////////////
  class CallableDeclaration : public ParentStatement {
    // The name of this callable.
    // This may be empty for callables without names.
    ADD_CONSTREF(EnvKey, name);
    ADD_POINTER(IDXS*, idxs);
    ADD_PROPERTY(IdxRef, fidx);
    ADD_PROPERTY(IdxRef, cidx);

    // The comment immediately preceding this declaration.
    ADD_PROPERTY(SilentCommentObj, comment);
    // The declared arguments this callable accepts.
    ADD_PROPERTY(ArgumentDeclarationObj, arguments);
  public:
    CallableDeclaration(
      const SourceSpan& pstate,
      const EnvKey& name,
      ArgumentDeclaration* arguments,
      SilentComment* comment = nullptr,
      Block* block = nullptr);

    // Stringify declarations etc. (dart)
    virtual sass::string toString1() const = 0;

    ATTACH_ABSTRACT_CRTP_PERFORM_METHODS();
  };

  class ContentBlock :
    public CallableDeclaration {
  public:
    ContentBlock(
      const SourceSpan& pstate,
      ArgumentDeclaration* arguments = nullptr,
      const sass::vector<StatementObj>& children = {});
    sass::string toString1() const override final;
    ATTACH_CRTP_PERFORM_METHODS();
  };

  class FunctionRule final :
    public CallableDeclaration {
  public:
    FunctionRule(
      const SourceSpan& pstate,
      const EnvKey& name,
      ArgumentDeclaration* arguments,
      SilentComment* comment = nullptr,
      Block* block = nullptr);
    sass::string toString1() const override final;
    ATTACH_CRTP_PERFORM_METHODS();
  };

  class MixinRule final :
    public CallableDeclaration {
    ADD_CONSTREF(IdxRef, cidx);
  public:
    MixinRule(
      const SourceSpan& pstate,
      const sass::string& name,
      ArgumentDeclaration* arguments,
      SilentComment* comment = nullptr,
      Block* block = nullptr);
    sass::string toString1() const override final;
    ATTACH_CRTP_PERFORM_METHODS();
  };

  class IncludeRule final : public InvocationStatement {

    // The namespace of the mixin being invoked, or
    // `null` if it's invoked without a namespace.
    ADD_CONSTREF(sass::string, ns);

    // The name of the mixin being invoked.
    ADD_CONSTREF(EnvKey, name);

    // The block that will be invoked for [ContentRule]s in the mixin
    // being invoked, or `null` if this doesn't pass a content block.
    ADD_PROPERTY(ContentBlockObj, content);

    ADD_CONSTREF(IdxRef, midx);

  public:

    IncludeRule(
      const SourceSpan& pstate,
      const EnvKey& name,
      ArgumentInvocation* arguments,
      const sass::string& ns = "",
      ContentBlock* content = nullptr,
      Block* block = nullptr);

    bool has_content() override final;

    ATTACH_CRTP_PERFORM_METHODS();
  };

  ///////////////////////////////////////////////////
  // The @content directive for mixin content blocks.
  ///////////////////////////////////////////////////
  class ContentRule final : public Statement {
    ADD_PROPERTY(ArgumentInvocationObj, arguments);
  public:
    ContentRule(const SourceSpan& pstate,
      ArgumentInvocation* arguments);
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ////////////////////////////////////////////////////////////////////////////
  class ParenthesizedExpression final : public Expression {
    ADD_PROPERTY(ExpressionObj, expression)
  public:
    ParenthesizedExpression(const SourceSpan& pstate, Expression* expression);
    // ATTACH_CLONE_OPERATIONS(ParenthesizedExpression);
    ATTACH_CRTP_PERFORM_METHODS();
  };
  ////////////////////////////////////////////////////////////////////////////

  ////////////////////////////////////////////////////////////////////////////
  // Arithmetic negation (logical negation is just an ordinary function call).
  ////////////////////////////////////////////////////////////////////////////
  class Unary_Expression final : public Expression {
  public:
    enum Type { PLUS, MINUS, NOT, SLASH };
  private:
    ADD_PROPERTY(Type, optype)
    ADD_PROPERTY(ExpressionObj, operand)
  public:
    Unary_Expression(const SourceSpan& pstate, Type t, ExpressionObj o);
    // ATTACH_CLONE_OPERATIONS(Unary_Expression)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  // A Media Ruleset before it has been evaluated
  // Could be already final or an interpolation
  class MediaRule final : public ParentStatement {
    ADD_PROPERTY(InterpolationObj, query)
  public:
    // The query that determines on which platforms the styles will be in effect.
    // This is only parsed after the interpolation has been resolved.
    MediaRule(const SourceSpan& pstate, InterpolationObj query, Block_Obj block = {});

    bool bubbles() const override final { return true; };
    bool is_invisible() const override { return false; };
    // ATTACH_CLONE_OPERATIONS(MediaRule)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  /////////////////////////////////////////////////
  // A query for the `@at-root` rule.
  /////////////////////////////////////////////////
  class AtRootQuery final : public AST_Node {
  private:
    // The names of the rules included or excluded by this query. There are 
    // two special names. "all" indicates that all rules are included or
    // excluded, and "rule" indicates style rules are included or excluded.
    ADD_PROPERTY(StringSet, names);
    // Whether the query includes or excludes rules with the specified names.
    ADD_PROPERTY(bool, include);

  public:

    AtRootQuery(
      const SourceSpan& pstate,
      const StringSet& names,
      bool include);

    // Whether this includes or excludes *all* rules.
    bool all() const;

    // Whether this includes or excludes style rules.
    bool rule() const;

    // Whether this includes or excludes media rules.
    bool media() const;

    // Returns the at-rule name for [node], or `null` if it's not an at-rule.
    sass::string _nameFor(Statement* node) const;

    // Returns whether [this] excludes a node with the given [name].
    bool excludesName(const sass::string& name) const;

    // Returns whether [this] excludes [node].
    bool excludes(CssParentNode* node) const;

    // Whether this excludes `@media` rules.
    // Note that this takes [include] into account.
    bool excludesMedia() const;

    // Whether this excludes style rules.
    // Note that this takes [include] into account.
    bool excludesStyleRules() const;

    // Parses an at-root query from [contents]. If passed, [url]
    // is the name of the file from which [contents] comes.
    // Throws a [SassFormatException] if parsing fails.
    static AtRootQuery* parse(
      const sass::string& contents, Context& ctx);

    // The default at-root query, which excludes only style rules.
    // ToDo: check out how to make this static
    static AtRootQuery* defaultQuery(const SourceSpan& pstate);

    // Only for debug purposes
    sass::string toString() const;

    ATTACH_CRTP_PERFORM_METHODS()
  };

  ///////////
  // At-root.
  ///////////
  class AtRootRule final : public ParentStatement {
    ADD_PROPERTY(InterpolationObj, query);
    ADD_POINTER(IDXS*, idxs);
  public:
    AtRootRule(const SourceSpan& pstate, InterpolationObj query = {}, Block_Obj b = {});
    AtRootRule(SourceSpan&& pstate, InterpolationObj query = {}, Block_Obj b = {});
    // ATTACH_CLONE_OPERATIONS(CssAtRootRule)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ////////////////////////////////////////////////////////////
  // Individual argument objects for mixin and function calls.
  ////////////////////////////////////////////////////////////
  class Argument final : public Expression {
    HASH_PROPERTY(ExpressionObj, value);
    HASH_CONSTREF(EnvKey, name);
    ADD_PROPERTY(bool, is_rest_argument);
    ADD_PROPERTY(bool, is_keyword_argument);
    mutable size_t hash_;
  public:
    Argument(const SourceSpan& pstate, ExpressionObj val,
      const EnvKey& n, bool rest = false, bool keyword = false);
    ATTACH_CLONE_OPERATIONS(Argument)
      ATTACH_CRTP_PERFORM_METHODS()
  };

  /////////////////////////////////////////////////////////////////////////
  // Parameter lists -- in their own class to facilitate context-sensitive
  // error checking (e.g., ensuring that all optional parameters follow all
  // required parameters).
  /////////////////////////////////////////////////////////////////////////
  typedef Value* (*SassFnSig)(FN_PROTOTYPE2);
  typedef std::pair<ArgumentDeclarationObj, SassFnSig> SassFnPair;
  typedef sass::vector<SassFnPair> SassFnPairs;

  class Callable : public AST_Node {
  public:
    Callable(const SourceSpan& pstate);
    virtual Value* execute(Eval& eval, ArgumentInvocation* arguments, const SourceSpan& pstate, bool selfAssign) = 0;
    virtual bool operator== (const Callable& rhs) const = 0;
    ATTACH_CRTP_PERFORM_METHODS()
  };

  class UserDefinedCallable : public Callable {
    // Name of this callable (used for reporting)
    ADD_CONSTREF(EnvKey, name);
    // The declaration (parameters this function takes).
    ADD_PROPERTY(CallableDeclarationObj, declaration);
    // The environment in which this callable was declared.
    ADD_POINTER(UserDefinedCallable*, content);
  public:
    UserDefinedCallable(
      const SourceSpan& pstate,
      const EnvKey& name,
      CallableDeclarationObj declaration,
      UserDefinedCallable* content);
    Value* execute(Eval& eval, ArgumentInvocation* arguments, const SourceSpan& pstate, bool selfAssign) override final;
    bool operator== (const Callable& rhs) const override final;
    ATTACH_CRTP_PERFORM_METHODS()
  };

  class PlainCssCallable : public Callable {
    ADD_CONSTREF(sass::string, name);
  public:
    PlainCssCallable(const SourceSpan& pstate, const sass::string& name);
    Value* execute(Eval& eval, ArgumentInvocation* arguments, const SourceSpan& pstate, bool selfAssign) override final;
    bool operator== (const Callable& rhs) const override final;
    ATTACH_CRTP_PERFORM_METHODS()
  };

  class BuiltInCallable : public Callable {

    // The function name
    ADD_CONSTREF(EnvKey, name);

    ADD_PROPERTY(ArgumentDeclarationObj, parameters);

    ADD_CONSTREF(SassFnPair, function);

  public:

    Value* execute(Eval& eval, ArgumentInvocation* arguments, const SourceSpan& pstate, bool selfAssign) override final;

    // Value* execute(ArgumentInvocation* arguments) {
    //   // return callback(arguments);
    //   return nullptr;
    // }

    // Creates a callable with a single [arguments] declaration
    // and a single [callback]. The argument declaration is parsed
    // from [arguments], which should not include parentheses.
    // Throws a [SassFormatException] if parsing fails.
    BuiltInCallable(
      const EnvKey& name,
      ArgumentDeclaration* parameters,
      const SassFnSig& callback);

    virtual const SassFnPair& callbackFor(
      size_t positional,
      const EnvKeyFlatMap<ValueObj>& names);

    bool operator== (const Callable& rhs) const override final;

    ATTACH_CRTP_PERFORM_METHODS()
  };

  class BuiltInCallables : public Callable {

    // The function name
    ADD_CONSTREF(EnvKey, name);

    // The overloads declared for this callable.
    ADD_PROPERTY(SassFnPairs, overloads);

  public:

    Value* execute(Eval& eval, ArgumentInvocation* arguments, const SourceSpan& pstate, bool selfAssign) override final;

    // Value* execute(ArgumentInvocation* arguments) {
    //   // return callback(arguments);
    //   return nullptr;
    // }

    // Creates a callable with multiple implementations. Each
    // key/value pair in [overloads] defines the argument declaration
    // for the overload (which should not include parentheses), and
    // the callback to execute if that argument declaration matches.
    // Throws a [SassFormatException] if parsing fails.
    BuiltInCallables(
      const EnvKey& name,
      const SassFnPairs& overloads);

    const SassFnPair& callbackFor(
      size_t positional,
      const EnvKeyFlatMap<ValueObj>& names); // override final;

    bool operator== (const Callable& rhs) const override final;

    ATTACH_CRTP_PERFORM_METHODS()
  };

  class ExternalCallable : public Callable {

    // The function name
    // ToDo: should be EnvKey too?
    ADD_CONSTREF(sass::string, name);

    ADD_PROPERTY(ArgumentDeclarationObj, declaration);

    ADD_PROPERTY(Sass_Function_Entry, function);

    ADD_POINTER(IDXS*, idxs);

  public:

    ExternalCallable(
      const sass::string& name,
      ArgumentDeclaration* parameters,
      Sass_Function_Entry function);
    Value* execute(Eval& eval, ArgumentInvocation* arguments, const SourceSpan& pstate, bool selfAssign) override final;
    bool operator== (const Callable& rhs) const override final;

    ATTACH_CRTP_PERFORM_METHODS()
  };


}

#include "ast_css.hpp"
#include "ast_values.hpp"
#include "ast_supports.hpp"
#include "ast_selectors.hpp"

#ifdef __clang__

// #pragma clang diagnostic pop
// #pragma clang diagnostic push

#endif

#endif
