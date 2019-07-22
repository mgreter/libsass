#include "ast_css.hpp"
#include "ast.hpp"

namespace Sass {

  CssNode::CssNode(ParserState pstate) :
    Statement(pstate),
    isGroupEnd_(false)
  {}
  CssNode::CssNode(const CssNode* ptr) :
    Statement(ptr),
    isGroupEnd_(ptr->isGroupEnd_)
  {}

  CssParentNode::CssParentNode(ParserState pstate) :
    CssNode(pstate),
    Vectorized(),
    isChildless_(false),
    block_()
  {}

  CssParentNode::CssParentNode(const CssParentNode* ptr) :
    CssNode(ptr),
    Vectorized(ptr),
    isChildless_(ptr->isChildless_),
    block_(ptr->block_)
  {}

  CssString::CssString(ParserState pstate, std::string text) :
    AST_Node(pstate),
    text_(text)
  {}

  CssString::CssString(const CssString* ptr) :
    AST_Node(ptr),
    text_(ptr->text_)
  {}

  CssValue::CssValue(ParserState pstate, Value* value) :
    CssNode(pstate),
    value_(value)
  {}
  CssValue::CssValue(const CssValue* ptr) :
    CssNode(ptr),
    value_(ptr->value_)
  {}

  CssAtRule::CssAtRule(ParserState pstate) :
    CssNode(pstate)
  {}
  CssAtRule::CssAtRule(const CssAtRule* ptr) :
    CssNode(ptr)
  {}

  CssComment::CssComment(ParserState pstate, std::string text, bool preserve) :
    CssNode(pstate),
    text_(text),
    isPreserved_(preserve)
  {}
  CssComment::CssComment(const CssComment* ptr) :
    CssNode(ptr),
    text_(ptr->text_),
    isPreserved_(ptr->isPreserved_)
  {}

  CssDeclaration::CssDeclaration(ParserState pstate, CssString* name, CssValue* value) :
    CssNode(pstate),
    name_(name),
    value_(value)
  {}
  CssDeclaration::CssDeclaration(const CssDeclaration* ptr) :
    CssNode(ptr),
    name_(ptr->name_),
    value_(ptr->value_)
  {}

  CssImport::CssImport(ParserState pstate) :
    CssNode(pstate)
  {}
  CssImport::CssImport(const CssImport* ptr) :
    CssNode(ptr)
  {}

  CssKeyframeBlock::CssKeyframeBlock(ParserState pstate) :
    CssNode(pstate)
  {}
  CssKeyframeBlock::CssKeyframeBlock(const CssKeyframeBlock* ptr) :
    CssNode(ptr)
  {}

  // CssMediaQuery::CssMediaQuery(ParserState pstate) :
  //   CssNode(pstate)
  // {}
  // CssMediaQuery::CssMediaQuery(const CssMediaQuery* ptr) :
  //   CssNode(ptr)
  // {}

  // CssMediaRule::CssMediaRule(ParserState pstate) :
  //   CssNode(pstate)
  // {}
  // CssMediaRule::CssMediaRule(const CssMediaRule* ptr) :
  //   CssNode(ptr)
  // {}

  CssStyleRule::CssStyleRule(ParserState pstate, SelectorList* selector) :
    CssParentNode(pstate),
    selector_(selector)
  {
    statement_type(Statement::RULESET);
  }
  CssStyleRule::CssStyleRule(const CssStyleRule* ptr) :
    CssParentNode(ptr),
    selector_(ptr->selector_)
  {
    statement_type(Statement::RULESET);
  }

  CssStylesheet::CssStylesheet(ParserState pstate) :
    CssNode(pstate)
  {}
  CssStylesheet::CssStylesheet(const CssStylesheet* ptr) :
    CssNode(ptr)
  {}

  CssSupportsRule::CssSupportsRule(ParserState pstate, ExpressionObj condition) :
    CssParentNode(pstate),
    condition_(condition)
  {
    statement_type(Statement::SUPPORTS);
  }

  bool CssSupportsRule::is_invisible() const
  {
    return block_ == nullptr || block_->isInvisible();
  }

  bool CssSupportsRule::bubbles()
  {
    return true;
  }

  CssSupportsRule::CssSupportsRule(const CssSupportsRule* ptr) :
    CssParentNode(ptr),
    condition_(ptr->condition_)
  {
    statement_type(Statement::SUPPORTS);
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  CssMediaRule::CssMediaRule(ParserState pstate, Block_Obj block) :
    Has_Block(pstate, block),
    Vectorized()
  {
    statement_type(MEDIA);
  }

  CssMediaRule::CssMediaRule(const CssMediaRule* ptr) :
    Has_Block(ptr),
    Vectorized(*ptr)
  {
    statement_type(MEDIA);
  }

  CssMediaQuery::CssMediaQuery(ParserState pstate) :
    AST_Node(pstate),
    modifier_(""),
    type_(""),
    features_()
  {
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  bool CssMediaQuery::operator==(const CssMediaQuery& rhs) const
  {
    return type_ == rhs.type_
      && modifier_ == rhs.modifier_
      && features_ == rhs.features_;
  }

  // Implemented after dart-sass (maybe move to other class?)
  CssMediaQueryObj CssMediaQuery::merge(CssMediaQueryObj& other)
  {

    std::string ourType = this->type();
    Util::ascii_str_tolower(&ourType);

    std::string theirType = other->type();
    Util::ascii_str_tolower(&theirType);

    std::string ourModifier = this->modifier();
    Util::ascii_str_tolower(&ourModifier);

    std::string theirModifier = other->modifier();
    Util::ascii_str_tolower(&theirModifier);

    std::string type;
    std::string modifier;
    std::vector<std::string> features;

    if (ourType.empty() && theirType.empty()) {
      CssMediaQueryObj query = SASS_MEMORY_NEW(CssMediaQuery, pstate());
      std::vector<std::string> f1(this->features());
      std::vector<std::string> f2(other->features());
      features.insert(features.end(), f1.begin(), f1.end());
      features.insert(features.end(), f2.begin(), f2.end());
      query->features(features);
      return query;
    }

    if ((ourModifier == "not") != (theirModifier == "not")) {
      if (ourType == theirType) {
        std::vector<std::string> negativeFeatures =
          ourModifier == "not" ? this->features() : other->features();
        std::vector<std::string> positiveFeatures =
          ourModifier == "not" ? other->features() : this->features();

        // If the negative features are a subset of the positive features, the
        // query is empty. For example, `not screen and (color)` has no
        // intersection with `screen and (color) and (grid)`.
        // However, `not screen and (color)` *does* intersect with `screen and
        // (grid)`, because it means `not (screen and (color))` and so it allows
        // a screen with no color but with a grid.
        if (listIsSubsetOrEqual(negativeFeatures, positiveFeatures)) {
          return SASS_MEMORY_NEW(CssMediaQuery, pstate());
        }
        else {
          return CssMediaQueryObj();
        }
      }
      else if (this->matchesAllTypes() || other->matchesAllTypes()) {
        return CssMediaQueryObj();
      }

      if (ourModifier == "not") {
        type = theirType;
        modifier = theirModifier;
        features = other->features();
      }
      else {
        type = ourType;
        modifier = ourModifier;
        features = this->features();
      }
    }
    else if (ourModifier == "not") {
      SASS_ASSERT(theirModifier == "not", "modifiers not is sync");

      // CSS has no way of representing "neither screen nor print".
      if (ourType != theirType) return CssMediaQueryObj();

      auto moreFeatures = this->features().size() > other->features().size()
        ? this->features()
        : other->features();
      auto fewerFeatures = this->features().size() > other->features().size()
        ? other->features()
        : this->features();

      // If one set of features is a superset of the other,
      // use those features because they're strictly narrower.
      if (listIsSubsetOrEqual(fewerFeatures, moreFeatures)) {
        modifier = ourModifier; // "not"
        type = ourType;
        features = moreFeatures;
      }
      else {
        // Otherwise, there's no way to
        // represent the intersection.
        return CssMediaQueryObj();
      }

    }
    else if (this->matchesAllTypes()) {
      modifier = theirModifier;
      // Omit the type if either input query did, since that indicates that they
      // aren't targeting a browser that requires "all and".
      type = (other->matchesAllTypes() && ourType.empty()) ? "" : theirType;
      const std::vector<std::string>& f1 = this->features();
      const std::vector<std::string>& f2 = other->features();
      std::copy(f1.begin(), f1.end(), std::back_inserter(features));
      std::copy(f2.begin(), f2.end(), std::back_inserter(features));
    }
    else if (other->matchesAllTypes()) {
      type = ourType;
      modifier = ourModifier;
      const std::vector<std::string>& f1 = this->features();
      const std::vector<std::string>& f2 = other->features();
      std::copy(f1.begin(), f1.end(), std::back_inserter(features));
      std::copy(f2.begin(), f2.end(), std::back_inserter(features));
    }
    else if (ourType != theirType) {
      return SASS_MEMORY_NEW(CssMediaQuery, pstate());
    }
    else {
      type = ourType;
      modifier = ourModifier.empty() ? theirModifier : ourModifier;
      const std::vector<std::string>& f1 = this->features();
      const std::vector<std::string>& f2 = other->features();
      std::copy(f1.begin(), f1.end(), std::back_inserter(features));
      std::copy(f2.begin(), f2.end(), std::back_inserter(features));
    }

    CssMediaQueryObj query = SASS_MEMORY_NEW(CssMediaQuery, pstate());
    query->modifier(modifier == ourModifier ? this->modifier() : other->modifier());
    query->type(type == ourType ? this->type() : other->type());
    query->features(features);
    return query;
  }

  CssMediaQuery::CssMediaQuery(const CssMediaQuery* ptr) :
    AST_Node(*ptr),
    modifier_(ptr->modifier_),
    type_(ptr->type_),
    features_(ptr->features_)
  {
  } 

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // IMPLEMENT_AST_OPERATORS(CssNode);
  IMPLEMENT_AST_OPERATORS(CssString);
  IMPLEMENT_AST_OPERATORS(CssValue);
  IMPLEMENT_AST_OPERATORS(CssAtRule);
  IMPLEMENT_AST_OPERATORS(CssComment);
  IMPLEMENT_AST_OPERATORS(CssDeclaration);
  IMPLEMENT_AST_OPERATORS(CssImport);
  IMPLEMENT_AST_OPERATORS(CssKeyframeBlock);
  IMPLEMENT_AST_OPERATORS(CssMediaQuery);
  IMPLEMENT_AST_OPERATORS(CssMediaRule);
  IMPLEMENT_AST_OPERATORS(CssStyleRule);
  IMPLEMENT_AST_OPERATORS(CssStylesheet);
  IMPLEMENT_AST_OPERATORS(CssSupportsRule);

}
