#ifndef SASS_AST_CSS_H
#define SASS_AST_CSS_H

// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"
#include "ast.hpp"

#include <exception>

#include "ast_css.hpp"
#include "visitor_css.hpp"
#include "ast_def_macros.hpp"

namespace Sass {

  class CssNode : public AST_Node {
    // Whether this was generated from the last node in a
    // nested Sass tree that got flattened during evaluation.
    ADD_PROPERTY(bool, isGroupEnd);
  public:
    CssNode(const SourceSpan& pstate);

    virtual bool isInvisibleCss() const { return false; }

    virtual bool isInvisibleSibling() const { return false; }


    // size_t tabs() const { return 0; }
    // void tabs(size_t tabs) const { }

    // Calls the appropriate visit method on [visitor].
    // virtual void serialize(CssVisitor<void>& visitor);
    ATTACH_VIRTUAL_COPY_OPERATIONS(CssNode)
  };

  // [x] CssAtRule
  // [-] CssKeyframeBlock
  // [x] CssMediaRule
  // [x] CssStyleRule
  // [-] CssStylesheet
  // [x] CssSupportsRule
  class CssParentNode : public CssNode,
    private Vectorized<CssNode> {
    // Whether the rule has no children and should be emitted
    // without curly braces. This implies `children.isEmpty`,
    // but the reverse is not true—for a rule like `@foo {}`,
    // [children] is empty but [isChildless] is `false`.
    ADD_PROPERTY(bool, isChildless);
    ADD_PROPERTY(size_t, parentIndex);
  public:
    CssParentNode* parent_;

  public:
    /// The child statements of this node.
    // List<CssNode> get children;

    CssParentNode(const SourceSpan& pstate,
      CssParentNode* parent);

    CssParentNode(const SourceSpan& pstate,
      CssParentNode* parent,
      sass::vector<CssNodeObj>&& children);

    void append(const CssNodeObj& element) = delete;
    void append(CssNodeObj&& element) = delete;

    sass::vector<T>& elements73() { return Vectorized::elements(); }
    const sass::vector<T>& elements73() const { return Vectorized::elements(); }

    typename sass::vector<T>::iterator end() { return Vectorized::end(); }
    typename sass::vector<T>::iterator begin() { return Vectorized::begin(); }
    typename sass::vector<T>::const_iterator end() const { return Vectorized::end(); }
    typename sass::vector<T>::const_iterator begin() const { return Vectorized::begin(); }

    size_t length() const { return Vectorized::length(); }

    const T& at(size_t i) const { return Vectorized::at(i); }
    const T& get(size_t i) const { return Vectorized::get(i); }
    T& operator[](size_t i) { return elements_[i]; }

    void addChild(CssParentNode* element);
    void addNode(CssNode* element);

    bool empty() const
    {
      // Is empty until something is visible
      for (auto child : elements()) {
        if (!child->isInvisibleCss()) return false;
      }
      return true;
    }

    virtual bool isInvisibleSibling() const override;

    // bool get isChildless;
//    ATTACH_VIRTUAL_COPY_OPERATIONS(CssParentNode);

    CssParentNode(const CssParentNode* ptr, bool childless);

    // Must be implemented in derived classes
    virtual CssParentNode* copy(bool childless = false) const {
      throw std::runtime_error("Copy not implemented");
    }

    // 
    virtual bool bubbleThroughStyleRule() const { return false; }
    virtual CssParentNode* bubbleThroughStyleRule2() { return this; }

    ATTACH_CRTP_PERFORM_METHODS();
  };

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  class CssRoot : public CssParentNode {
    // needed for properly formatted CSS emission
  public:
    CssRoot(const SourceSpan& pstate,
      sass::vector<CssNodeObj>&& vec = {});
    ATTACH_CRTP_PERFORM_METHODS();
  };

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  // A plain CSS string
  class CssString : public AST_Node {
    ADD_PROPERTY(sass::string, text);
  public:
    CssString(const SourceSpan& pstate, const sass::string& text);
    bool empty() const { return text_.empty(); }
    ATTACH_COPY_CTOR(CssString);
    ATTACH_CRTP_PERFORM_METHODS();
  };

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  // A plain list of CSS string
  class CssStringList : public AST_Node {
    ADD_PROPERTY(sass::vector<sass::string>, texts);
  public:
    CssStringList(const SourceSpan& pstate,
      sass::vector<sass::string>&& texts);
    ATTACH_COPY_CTOR(CssStringList);
    ATTACH_CRTP_PERFORM_METHODS();
  };

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  // A plain CSS value
  class CssValue : public CssNode {
    ADD_PROPERTY(ValueObj, value);
  public:
    CssValue(const SourceSpan& pstate, Value* value);
    ATTACH_COPY_CTOR(CssValue);
    ATTACH_CRTP_PERFORM_METHODS();
  };

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  class CssAtRule : public CssParentNode {
    ADD_PROPERTY(CssStringObj, name);
    ADD_PROPERTY(CssStringObj, value);
    // ADD_PROPERTY(bool, isChildless);
  public:
    CssAtRule(const SourceSpan& pstate,
      CssParentNode* parent,
      CssString* name,
      CssString* value);

    CssAtRule(const SourceSpan& pstate,
      CssParentNode* parent,
      CssString* name,
      CssString* value,
      sass::vector<CssNodeObj>&& children);

    bool isInvisibleCss() const override final {
      return false;
    }

    bool isInvisibleSibling() const override final {
      return false;
    }

    ATTACH_COPY_CTOR(CssAtRule);
    ATTACH_CRTP_PERFORM_METHODS();
  };

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  class CssComment : public CssNode {
    ADD_PROPERTY(sass::string, text);
    ADD_PROPERTY(bool, isPreserved);
  public:
    CssComment(const SourceSpan& pstate, const sass::string& text, bool preserve = false);
    ATTACH_COPY_CTOR(CssComment);
    ATTACH_CRTP_PERFORM_METHODS();
  };

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  class CssDeclaration : public CssNode {
    // The name of this declaration.
    ADD_PROPERTY(CssStringObj, name);
    // The value of this declaration.
    ADD_PROPERTY(CssValueObj, value);
    ADD_PROPERTY(bool, is_custom_property);
  public:
    CssDeclaration(const SourceSpan& pstate, CssString* name, CssValue* value,
      bool is_custom_property = false);
    ATTACH_COPY_CTOR(CssDeclaration);
    ATTACH_CRTP_PERFORM_METHODS();
  };

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  // A css import is static in nature and
  // can only have one single import url.
  class CssImport : public CssNode {

    // The url including quotes.
    ADD_PROPERTY(CssStringObj, url);

    // The supports condition attached to this import.
    ADD_PROPERTY(CssStringObj, supports);

    // The media query attached to this import.
    ADD_PROPERTY(sass::vector<CssMediaQueryObj>, media);

    // Flag to hoist import to the top.
    ADD_PROPERTY(bool, outOfOrder);

  public:

    // Standard value constructor
    CssImport(const SourceSpan& pstate,
      CssString* url = nullptr,
      CssString* supports = nullptr,
      sass::vector<CssMediaQueryObj> media = {});

    ATTACH_COPY_CTOR(CssImport);
    ATTACH_CRTP_PERFORM_METHODS();
  };

  /////////////////////////////////////////////////////////////////////////////
  // A block within a `@keyframes` rule.
  // For example, `10% {opacity: 0.5}`.
  /////////////////////////////////////////////////////////////////////////////
  class CssKeyframeBlock : public CssParentNode {

    // The selector for this block.
    ADD_PROPERTY(CssStringListObj, selector);

  public:

    // Value constructor
    CssKeyframeBlock(
      const SourceSpan& pstate,
      CssParentNode* parent,
      CssStringList* selector);

    // Value constructor
    CssKeyframeBlock(
      const SourceSpan& pstate,
      CssParentNode* parent,
      CssStringList* selector,
      sass::vector<CssNodeObj>&& children);

    // Dispatch to visitor
    template <typename T>
    T accept(CssVisitor<T>& visitor) {
      return visitor.visitCssKeyframeBlock(this);
    }

    // Return a copy with empty children
    // CssKeyframeBlock* copyWithoutChildren();

    ATTACH_COPY_CTOR(CssKeyframeBlock);
    ATTACH_CRTP_PERFORM_METHODS();

  };
  // EO CssKeyframeBlock

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  class CssStyleRule : public CssParentNode {
    ADD_PROPERTY(SelectorListObj, selector);
  public:

    CssStyleRule(
      const SourceSpan& pstate,
      CssParentNode* parent,
      SelectorList* selector);

    CssStyleRule(
      const SourceSpan& pstate,
      CssParentNode* parent,
      SelectorList* selector,
      sass::vector<CssNodeObj>&& children);

    // Selector and one child must be visible
    bool isInvisibleCss() const override final;

    bool isInvisibleSibling() const override final;

    bool bubbleThroughStyleRule() const override final { return true; }
    CssParentNode* bubbleThroughStyleRule2() override final {
      return parent_ ? parent_->bubbleThroughStyleRule2() : this;
    }

    ATTACH_COPY_OPERATIONS2(CssStyleRule);
    ATTACH_CRTP_PERFORM_METHODS();
  };

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  class CssSupportsRule : public CssParentNode {
    ADD_PROPERTY(ExpressionObj, condition);
  public:
    CssSupportsRule(const SourceSpan& pstate,
      CssParentNode* parent,
      ExpressionObj condition);

    CssSupportsRule(
      const SourceSpan& pstate,
      CssParentNode* parent,
      ExpressionObj condition,
      sass::vector<CssNodeObj>&& children);

    bool isInvisibleCss() const override;
    ATTACH_COPY_OPERATIONS2(CssSupportsRule);
    ATTACH_CRTP_PERFORM_METHODS();
  };

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  /// A plain CSS `@import`.
  // class CssImport : public CssNode {

    // The URL being imported.
    // This includes quotes.
    // CssValue<sass::string> url;

    // The supports condition attached to this import.
    // CssValue<sass::string> supports;

    // The media query attached to this import.
    // Vectorized<CssMediaQueryObj> media;

    // void serialize(CssVisitor<void>& visitor); // = > visitor.visitCssImport(this);
  // };
  
  // Media Queries after they have been evaluated
  // Representing the static or resulting css
  class CssMediaQuery final : public AST_Node {

    // The modifier, probably either "not" or "only".
    // This may be `null` if no modifier is in use.
    ADD_PROPERTY(sass::string, modifier);

    // The media type, for example "screen" or "print".
    // This may be `null`. If so, [features] will not be empty.
    ADD_PROPERTY(sass::string, type);

    // Feature queries, including parentheses.
    ADD_PROPERTY(sass::vector<sass::string>, features);

  public:
    CssMediaQuery(const SourceSpan& pstate);

    // Check if two instances are considered equal
    bool operator== (const CssMediaQuery& rhs) const;

    // Returns true if this query is empty
    // Meaning it has no type and features
    bool empty() const {
      return type_.empty()
        && modifier_.empty()
        && features_.empty();
    }

    // Whether this media query matches all media types.
    bool matchesAllTypes() const {
      return type_.empty() || StringUtils::equalsIgnoreCase(type_, "all", 3);
    }

    // Merges this with [other] and adds a query that matches the intersection
    // of both inputs to [result]. Returns false if the result is unrepresentable
    CssMediaQueryObj merge(CssMediaQueryObj& other);

    sass::vector<CssMediaQueryObj> parseList(
      const sass::string& contents);

    ATTACH_COPY_CTOR(CssMediaQuery);
    ATTACH_CRTP_PERFORM_METHODS();
  };

  /////////////////////////////////////////////////////////////////////////////
  // An `@at-root` rule. Moves it contents "up" the tree through parent nodes.
  // Note: This does not exist in dart-sass, as it moves stuff on eval stage.
  /////////////////////////////////////////////////////////////////////////////
  class CssAtRootRule final :
    public CssParentNode {

    // The "with" or "without" queries
    ADD_PROPERTY(AtRootQueryObj, query);

  public:

    // Value constructor
    CssAtRootRule(
      const SourceSpan& pstate,
      CssParentNode* parent,
      AtRootQueryObj query);

    // Value constructor
    CssAtRootRule(
      const SourceSpan& pstate,
      CssParentNode* parent,
      AtRootQueryObj query,
      sass::vector<CssNodeObj>&& children);

    ATTACH_COPY_OPERATIONS2(CssAtRootRule);
    ATTACH_CRTP_PERFORM_METHODS();

  };
  // EO CssAtRootRule

  /////////////////////////////////////////////////////////////////////////////
  // A plain CSS `@media` rule after it has been evaluated.
  /////////////////////////////////////////////////////////////////////////////
  class CssMediaRule final :
    public CssParentNode {

    // The queries for this rule (this is never empty).
    ADD_PROPERTY(Vectorized<CssMediaQuery>, queries);

  public:

    // Value constructor
    CssMediaRule(const SourceSpan& pstate,
      CssParentNode* parent,
      const sass::vector<CssMediaQueryObj>& queries);

    // Value constructor
    CssMediaRule(const SourceSpan& pstate,
      CssParentNode* parent,
      const sass::vector<CssMediaQueryObj>& queries,
      sass::vector<CssNodeObj>&& children);

    // Dispatch to visitor
    template <typename T>
    T accept(CssVisitor<T>& visitor) {
      return visitor.visitCssMediaRule(this);
    }

    bool isInvisible5() const { return queries_.empty(); };

    bool isInvisibleCss() const override final {
      return queries_.empty() || CssParentNode::isInvisibleCss();
    }

    // Check if two instances are considered equal
    bool operator== (const CssMediaRule& rhs) const;

    ATTACH_COPY_OPERATIONS2(CssMediaRule);
    ATTACH_CRTP_PERFORM_METHODS();

  };
  // EO CssMediaRule

  /////////////////
  // CssImportTrace.
  /////////////////
  class CssImportTrace final : public CssParentNode {
    ADD_PROPERTY(char, type)
    ADD_PROPERTY(sass::string, name)
  public:
    CssImportTrace(const SourceSpan& pstate,
      CssParentNode* parent,
      const sass::string& name, char type = 'm');
    CssImportTrace(const SourceSpan& pstate,
      CssParentNode* parent,
      const sass::string& name, sass::vector<CssNodeObj>&& b, char type = 'm');
    bool bubbleThroughStyleRule() const override final { return true; }

    CssParentNode* bubbleThroughStyleRule2() override final {
      return parent_ ? parent_->bubbleThroughStyleRule2() : this;
    }

    bool isInvisibleCss() const override final { return empty(); }
    ATTACH_COPY_OPERATIONS2(CssImportTrace);
    ATTACH_CRTP_PERFORM_METHODS();
  };
}


#endif
