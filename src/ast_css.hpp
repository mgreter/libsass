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

    virtual bool bubbles() const { return false; }
    virtual bool is_invisible() const { return false; }
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
    public VectorizedNopsi<CssNode> {
    // Whether the rule has no children and should be emitted
    // without curly braces. This implies `children.isEmpty`,
    // but the reverse is not true—for a rule like `@foo {}`,
    // [children] is empty but [isChildless] is `false`.
    ADD_PROPERTY(bool, isChildless);
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

    // bool get isChildless;
    ATTACH_VIRTUAL_COPY_OPERATIONS(CssParentNode);
    ATTACH_CRTP_PERFORM_METHODS()
  };

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  class CssRoot : public CssParentNode {
    // needed for properly formatted CSS emission
  public:
    CssRoot(const SourceSpan& pstate,
      sass::vector<CssNodeObj>&& vec = {});
    // ATTACH_CLONE_OPERATIONS(CssRoot);
    ATTACH_CRTP_PERFORM_METHODS();
  };

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  // A plain CSS string
  class CssString : public AST_Node {
    ADD_CONSTREF(sass::string, text);
  public:
    CssString(const SourceSpan& pstate, const sass::string& text);
    bool empty() const { return text_.empty(); }
    ATTACH_CLONE_OPERATIONS(CssString)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  // A plain list of CSS string
  class CssStrings : public AST_Node {
    ADD_CONSTREF(sass::vector<sass::string>, texts);
  public:
    CssStrings(const SourceSpan& pstate,
      const sass::vector<sass::string>& texts);
    ATTACH_CLONE_OPERATIONS(CssStrings)
      ATTACH_CRTP_PERFORM_METHODS()
  };

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  // A plain CSS value
  class CssValue : public CssNode {
    ADD_PROPERTY(ValueObj, value);
  public:
    CssValue(const SourceSpan& pstate, Value* value);
    ATTACH_CLONE_OPERATIONS(CssValue)
    ATTACH_CRTP_PERFORM_METHODS()
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

    bool bubbles() const override final;
    ATTACH_CLONE_OPERATIONS(CssAtRule)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  class CssComment : public CssNode {
    ADD_CONSTREF(sass::string, text);
    ADD_PROPERTY(bool, isPreserved);
  public:
    CssComment(const SourceSpan& pstate, const sass::string& text, bool preserve = false);
    ATTACH_CLONE_OPERATIONS(CssComment)
    ATTACH_CRTP_PERFORM_METHODS()
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
    ATTACH_CLONE_OPERATIONS(CssDeclaration)
    ATTACH_CRTP_PERFORM_METHODS()
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

    ATTACH_CLONE_OPERATIONS(CssImport);
    ATTACH_CRTP_PERFORM_METHODS();
  };

  /////////////////////////////////////////////////////////////////////////////
  // A block within a `@keyframes` rule.
  // For example, `10% {opacity: 0.5}`.
  /////////////////////////////////////////////////////////////////////////////
  class CssKeyframeBlock : public CssParentNode {

    // The selector for this block.
    ADD_CONSTREF(CssStringsObj, selector);

  public:

    // Value constructor
    CssKeyframeBlock(
      const SourceSpan& pstate,
      CssParentNode* parent,
      CssStrings* selector);

    // Value constructor
    CssKeyframeBlock(
      const SourceSpan& pstate,
      CssParentNode* parent,
      CssStrings* selector,
      sass::vector<CssNodeObj>&& children);

    // Dispatch to visitor
    template <typename T>
    T accept(CssVisitor<T>& visitor) {
      return visitor.visitCssKeyframeBlock(this);
    }

    // Return a copy with empty children
    CssKeyframeBlock* copyWithoutChildren();

    ATTACH_CLONE_OPERATIONS(CssKeyframeBlock)
    ATTACH_CRTP_PERFORM_METHODS()

  };
  // EO CssKeyframeBlock

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  class CssStyleRule : public CssParentNode {
    // The selector for this ruleset.
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

    bool is_invisible() const override;
    bool bubbles() const override final { return true; }

    ATTACH_CLONE_OPERATIONS(CssStyleRule)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  class CssStylesheet : public CssParentNode {
  public:
    CssStylesheet(const SourceSpan& pstate,
      CssParentNode* parent);
    ATTACH_CLONE_OPERATIONS(CssStylesheet)
      ATTACH_CRTP_PERFORM_METHODS()
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

    bool is_invisible() const override;
    bool bubbles() const override final;
    ATTACH_CLONE_OPERATIONS(CssSupportsRule);
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
    ADD_CONSTREF(sass::string, modifier);

    // The media type, for example "screen" or "print".
    // This may be `null`. If so, [features] will not be empty.
    ADD_CONSTREF(sass::string, type);

    // Feature queries, including parentheses.
    ADD_CONSTREF(sass::vector<sass::string>, features);

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

    ATTACH_CLONE_OPERATIONS(CssMediaQuery)
    ATTACH_CRTP_PERFORM_METHODS()
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

    // Tell cssize that we can bubble up
    bool bubbles() const override final { return true; }

    ATTACH_CLONE_OPERATIONS(CssAtRootRule);
    ATTACH_CRTP_PERFORM_METHODS();

  };
  // EO CssAtRootRule

  /////////////////////////////////////////////////////////////////////////////
  // A plain CSS `@media` rule after it has been evaluated.
  /////////////////////////////////////////////////////////////////////////////
  class CssMediaRule final :
    public CssParentNode {

    // The queries for this rule (this is never empty).
    ADD_CONSTREF(VectorizedNopsi<CssMediaQuery>, queries);

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

    // Tell cssize that we can bubble up
    bool bubbles() const override final { return true; }

    bool isInvisible() const { return queries_.empty(); }
    bool is_invisible() const override { return false; };

    // Append additional media queries
    void concat(const sass::vector<CssMediaQueryObj>& queries);

    // Check if two instances are considered equal
    bool operator== (const CssMediaRule& rhs) const;

    ATTACH_CLONE_OPERATIONS(CssMediaRule)
    ATTACH_CRTP_PERFORM_METHODS()

  };
  // EO CssMediaRule



    ///////////////////////////////////////////////////////////////////////
  // Keyframe-rules -- the child blocks of "@keyframes" nodes.
  ///////////////////////////////////////////////////////////////////////
  class Keyframe_Rule final : public CssParentNode {
    // according to css spec, this should be <keyframes-name>
    // <keyframes-name> = <custom-ident> | <string>
    // ADD_PROPERTY(SelectorListObj, name)
    ADD_PROPERTY(SassStringObj, name2)

  public:
    Keyframe_Rule(const SourceSpan& pstate,
      CssParentNode* parent);
    Keyframe_Rule(const SourceSpan& pstate,
      CssParentNode* parent,
      sass::vector<CssNodeObj>&& children);

    ATTACH_CLONE_OPERATIONS(Keyframe_Rule)
      ATTACH_CRTP_PERFORM_METHODS()
  };


  /////////////////
  // Bubble.
  /////////////////
  class Bubble final : public CssNode {
    ADD_PROPERTY(CssNodeObj, node)
  public:
    Bubble(const SourceSpan& pstate, CssNodeObj n, CssNodeObj g = {});
    bool bubbles() const override final;
    // ATTACH_CLONE_OPERATIONS(Bubble)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  /////////////////
  // Trace.
  /////////////////
  class Trace final : public CssParentNode {
    ADD_CONSTREF(char, type)
    ADD_CONSTREF(sass::string, name)
  public:
    Trace(const SourceSpan& pstate,
      CssParentNode* parent,
      const sass::string& name, char type = 'm');
    Trace(const SourceSpan& pstate,
      CssParentNode* parent,
      const sass::string& name, sass::vector<CssNodeObj>&& b, char type = 'm');
    virtual bool is_invisible() const { return empty(); }
    ATTACH_CRTP_PERFORM_METHODS()
  };
}


#endif
