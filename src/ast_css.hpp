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

  class CssNode : public Statement {
    // Whether this was generated from the last node in a
    // nested Sass tree that got flattened during evaluation.
    ADD_PROPERTY(bool, isGroupEnd);
  public:
    CssNode(ParserState pstate);
    // Calls the appropriate visit method on [visitor].
    // virtual void serialize(CssVisitor<void>& visitor);
    ATTACH_VIRTUAL_COPY_OPERATIONS(CssNode)
  };

  class CssParentNode : public CssNode, public Vectorized<StatementObj> {
    // Whether the rule has no children and should be emitted
    // without curly braces. This implies `children.isEmpty`,
    // but the reverse is not true—for a rule like `@foo {}`,
    // [children] is empty but [isChildless] is `false`.
    ADD_PROPERTY(bool, isChildless);
    // Backward compatibility
    ADD_PROPERTY(BlockObj, block);

  public:
    /// The child statements of this node.
    // List<CssNode> get children;

    CssParentNode(ParserState pstate);

      // bool get isChildless;
    ATTACH_VIRTUAL_COPY_OPERATIONS(CssParentNode)
  };

  // A plain CSS string
  class CssString : public AST_Node {
    ADD_PROPERTY(std::string, text);
  public:
    CssString(ParserState pstate, std::string text);
    ATTACH_COPY_OPERATIONS(CssString)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  // A plain CSS value
  class CssValue : public CssNode {
    ADD_PROPERTY(ValueObj, value);
  public:
    CssValue(ParserState pstate, Value* value);
    ATTACH_COPY_OPERATIONS(CssValue)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  class CssAtRule : public CssNode { // CssParentNode
    // ADD_PROPERTY(CssString, name);
    // ADD_PROPERTY(CssString, value);
    // ADD_PROPERTY(bool, isChildless);
  public:
    CssAtRule(ParserState pstate);
    ATTACH_COPY_OPERATIONS(CssAtRule)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  class CssComment : public CssNode {
    ADD_PROPERTY(std::string, text);
    ADD_PROPERTY(bool, isPreserved);
  public:
    CssComment(ParserState pstate, std::string text, bool preserve = false);
    ATTACH_COPY_OPERATIONS(CssComment)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  class CssDeclaration : public CssNode {
    // The name of this declaration.
    ADD_PROPERTY(CssStringObj, name);
    // The value of this declaration.
    ADD_PROPERTY(CssValueObj, value);
  public:
    CssDeclaration(ParserState pstate, CssString* name, CssValue* value);
    ATTACH_COPY_OPERATIONS(CssDeclaration)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  class CssImport : public CssNode {
  public:
    CssImport(ParserState pstate);
    ATTACH_COPY_OPERATIONS(CssImport)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  class CssKeyframeBlock : public CssNode {
  public:
    CssKeyframeBlock(ParserState pstate);
    ATTACH_COPY_OPERATIONS(CssKeyframeBlock)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  class CssStyleRule : public CssParentNode {
    // The selector for this ruleset.
    ADD_PROPERTY(SelectorListObj, selector);
  public:
    CssStyleRule(ParserState pstate, SelectorList* selector);
    bool is_invisible() const override;

    bool empty() const {
      return block_.isNull() ||
        block_->empty();
    };

    ATTACH_COPY_OPERATIONS(CssStyleRule)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  class CssStylesheet : public CssNode {
  public:
    CssStylesheet(ParserState pstate);
    ATTACH_COPY_OPERATIONS(CssStylesheet)
      ATTACH_CRTP_PERFORM_METHODS()
  };

  class CssSupportsRule : public CssParentNode {
    ADD_PROPERTY(ExpressionObj, condition);
  public:
    CssSupportsRule(ParserState pstate, ExpressionObj condition);
    bool is_invisible() const override;
    bool bubbles() override;
    ATTACH_COPY_OPERATIONS(CssSupportsRule)
    ATTACH_CRTP_PERFORM_METHODS()
  };


  /// A plain CSS `@import`.
  // class CssImport : public CssNode {

    // The URL being imported.
    // This includes quotes.
    // CssValue<std::string> url;

    // The supports condition attached to this import.
    // CssValue<std::string> supports;

    // The media query attached to this import.
    // Vectorized<CssMediaQueryObj> media;

    // void serialize(CssVisitor<void>& visitor); // = > visitor.visitCssImport(this);
  // };

  // Media Queries after they have been evaluated
  // Representing the static or resulting css
  class CssMediaQuery final : public AST_Node {

    // The modifier, probably either "not" or "only".
    // This may be `null` if no modifier is in use.
    ADD_PROPERTY(std::string, modifier);

    // The media type, for example "screen" or "print".
    // This may be `null`. If so, [features] will not be empty.
    ADD_PROPERTY(std::string, type);

    // Feature queries, including parentheses.
    ADD_PROPERTY(std::vector<std::string>, features);

  public:
    CssMediaQuery(ParserState pstate);

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
      return type_.empty() || Util::equalsLiteral("all", type_);
    }

    // Merges this with [other] and adds a query that matches the intersection
    // of both inputs to [result]. Returns false if the result is unrepresentable
    CssMediaQueryObj merge(CssMediaQueryObj& other);

    ATTACH_COPY_OPERATIONS(CssMediaQuery)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  // A Media Ruleset after it has been evaluated
  // Representing the static or resulting css
  class CssMediaRule final : public Has_Block,
    public Vectorized<CssMediaQueryObj> {
  public:
    CssMediaRule(ParserState pstate, Block_Obj b);
    bool bubbles() override { return true; };
    bool isInvisible() const { return empty(); }
    bool is_invisible() const override { return false; };

  public:
    // Hash and equality implemtation from vector
    size_t hash() const override { return Vectorized::hash(); }
    // Check if two instances are considered equal
    bool operator== (const CssMediaRule& rhs) const {
      return Vectorized::operator== (rhs);
    }

    ATTACH_COPY_OPERATIONS(CssMediaRule)
    ATTACH_CRTP_PERFORM_METHODS()
  };


}


#endif
