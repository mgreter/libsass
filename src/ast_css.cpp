#include "ast_css.hpp"
#include "util.hpp"
#include "ast.hpp"

namespace Sass {

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  CssNode::CssNode(
    const SourceSpan& pstate) :
    Statement(pstate),
    isGroupEnd_(false)
  {}

  CssNode::CssNode(const CssNode* ptr) :
    Statement(ptr),
    isGroupEnd_(ptr->isGroupEnd_)
  {}

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  CssParentNode::CssParentNode(
    const SourceSpan& pstate,
    const sass::vector<StatementObj>& children) :
    CssNode(pstate),
    Vectorized(children),
    isChildless_(false)
  {}

  CssParentNode::CssParentNode(
    const CssParentNode* ptr) :
    CssNode(ptr),
    Vectorized(ptr),
    isChildless_(ptr->isChildless_)
  {}

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  CssString::CssString(const SourceSpan& pstate, const sass::string& text) :
    AST_Node(pstate),
    text_(text)
  {}

  CssString::CssString(const CssString* ptr) :
    AST_Node(ptr),
    text_(ptr->text_)
  {}

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  CssStrings::CssStrings(
    const SourceSpan& pstate,
    const sass::vector<sass::string>& texts) :
    AST_Node(pstate),
    texts_(texts)
  {}

  CssStrings::CssStrings(const CssStrings* ptr) :
    AST_Node(ptr),
    texts_(ptr->texts_)
  {}

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  CssValue::CssValue(const SourceSpan& pstate, Value* value) :
    CssNode(pstate),
    value_(value)
  {}

  CssValue::CssValue(const CssValue* ptr) :
    CssNode(ptr),
    value_(ptr->value_)
  {}

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  CssAtRule::CssAtRule(
    const SourceSpan& pstate,
    CssString* name,
    CssString* value,
    const sass::vector<StatementObj>& children)
    :
    CssParentNode(pstate, children),
    name_(name),
    value_(value)
  {}

  CssAtRule::CssAtRule(
    const CssAtRule* ptr) :
    CssParentNode(ptr),
    name_(ptr->name_),
    value_(ptr->value_)
  {}

  bool CssAtRule::bubbles() const
  {
    return is_keyframes() || is_media();
  }

  bool CssAtRule::is_media() const
  {
    return Util::unvendor(name_->text()) == "media";
  }

  bool CssAtRule::is_keyframes() const
  {
    return Util::unvendor(name_->text()) == "keyframes";
  }

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  CssComment::CssComment(
    const SourceSpan& pstate,
    const sass::string& text,
    bool preserve) :
    CssNode(pstate),
    text_(text),
    isPreserved_(preserve)
  {}
  CssComment::CssComment(
    const CssComment* ptr) :
    CssNode(ptr),
    text_(ptr->text_),
    isPreserved_(ptr->isPreserved_)
  {}

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  CssDeclaration::CssDeclaration(
    const SourceSpan& pstate,
    CssString* name,
    CssValue* value,
    bool is_custom_property) :
    CssNode(pstate),
    name_(name),
    value_(value),
    is_custom_property_(is_custom_property)
  {}
  CssDeclaration::CssDeclaration(
    const CssDeclaration* ptr) :
    CssNode(ptr),
    name_(ptr->name_),
    value_(ptr->value_),
    is_custom_property_(ptr->is_custom_property_)
  {}

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  // Value constructor
  CssImport::CssImport(
    const SourceSpan& pstate) :
    CssNode(pstate)
  {}

  CssImport::CssImport(
    const CssImport* ptr) :
    CssNode(ptr)
  {}

  /////////////////////////////////////////////////////////////////////////////
  // A block within a `@keyframes` rule.
  // For example, `10% {opacity: 0.5}`.
  /////////////////////////////////////////////////////////////////////////////

  // Value constructor
  CssKeyframeBlock::CssKeyframeBlock(
    const SourceSpan& pstate,
    CssStrings* selector,
    const sass::vector<Statement_Obj>& children) :
    CssParentNode(pstate, children),
    selector_(selector)
  {}

  // Copy constructor
  CssKeyframeBlock::CssKeyframeBlock(
    const CssKeyframeBlock* ptr) :
    CssParentNode(ptr),
    selector_(ptr->selector_)
  {}

  // Return a copy with empty children
  CssKeyframeBlock* CssKeyframeBlock::copyWithoutChildren() {
    CssKeyframeBlock* cpy = SASS_MEMORY_COPY(this);
    cpy->clear(); // return with empty children
    return cpy;
  }
  // EO copyWithoutChildren

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  CssStyleRule::CssStyleRule(
    const SourceSpan& pstate,
    SelectorList* selector,
    const sass::vector<StatementObj>& children) :
    CssParentNode(pstate, children),
    selector_(selector)
  {}

  CssStyleRule::CssStyleRule(
    const CssStyleRule* ptr) :
    CssParentNode(ptr),
    selector_(ptr->selector_)
  {}

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  CssStylesheet::CssStylesheet(
    const SourceSpan& pstate) :
    CssParentNode(pstate, {})
  {}
  CssStylesheet::CssStylesheet(
    const CssStylesheet* ptr) :
    CssParentNode(ptr)
  {}

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  CssSupportsRule::CssSupportsRule(
    const SourceSpan& pstate,
    ExpressionObj condition,
    const sass::vector<StatementObj>& children) :
    CssParentNode(pstate, children),
    condition_(condition)
  {}

  bool CssSupportsRule::is_invisible() const
  {
    for (auto stmt : elements()) {
      if (!stmt->is_invisible()) return false;
    }
    return true;
  }

  bool CssSupportsRule::bubbles() const
  {
    return true;
  }

  CssSupportsRule::CssSupportsRule(const CssSupportsRule* ptr) :
    CssParentNode(ptr),
    condition_(ptr->condition_)
  {}

  /////////////////////////////////////////////////////////////////////////
  // A plain CSS `@media` rule after it has been evaluated.
  /////////////////////////////////////////////////////////////////////////

  // Value constructor
  CssMediaRule::CssMediaRule(
    const SourceSpan& pstate,
    const sass::vector<CssMediaQueryObj>& queries,
    const sass::vector<StatementObj>& children) :
    CssParentNode(pstate, children),
    queries_(queries)
  {}

  // Copy constructor for rule of three
  CssMediaRule::CssMediaRule(const CssMediaRule* ptr) :
    CssParentNode(ptr),
    queries_(ptr->queries_)
  {}

  // Equality comparator for rule of three
  bool CssMediaRule::operator== (const CssMediaRule& rhs) const {
    return queries_ == rhs.queries_;
  }
  // EO operator==

  // Append additional media queries
  void CssMediaRule::concat(const sass::vector<CssMediaQueryObj>& queries)
  {
    queries_.concat(queries);
  }
  // EO concat(List<CssMediaQuery>)

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  CssMediaQuery::CssMediaQuery(const SourceSpan& pstate) :
    AST_Node(pstate),
    modifier_(StrEmpty),
    type_(StrEmpty),
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

    sass::string ourType = this->type();
    Util::ascii_str_tolower(&ourType);

    sass::string theirType = other->type();
    Util::ascii_str_tolower(&theirType);

    sass::string ourModifier = this->modifier();
    Util::ascii_str_tolower(&ourModifier);

    sass::string theirModifier = other->modifier();
    Util::ascii_str_tolower(&theirModifier);

    sass::string type;
    sass::string modifier;
    sass::vector<sass::string> features;

    if (ourType.empty() && theirType.empty()) {
      CssMediaQueryObj query = SASS_MEMORY_NEW(CssMediaQuery, pstate());
      sass::vector<sass::string> f1(this->features());
      sass::vector<sass::string> f2(other->features());
      features.insert(features.end(), f1.begin(), f1.end());
      features.insert(features.end(), f2.begin(), f2.end());
      query->features(features);
      return query;
    }

    if ((ourModifier == "not") != (theirModifier == "not")) {
      if (ourType == theirType) {
        sass::vector<sass::string> negativeFeatures =
          ourModifier == "not" ? this->features() : other->features();
        sass::vector<sass::string> positiveFeatures =
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
      const sass::vector<sass::string>& f1 = this->features();
      const sass::vector<sass::string>& f2 = other->features();
      std::copy(f1.begin(), f1.end(), std::back_inserter(features));
      std::copy(f2.begin(), f2.end(), std::back_inserter(features));
    }
    else if (other->matchesAllTypes()) {
      type = ourType;
      modifier = ourModifier;
      const sass::vector<sass::string>& f1 = this->features();
      const sass::vector<sass::string>& f2 = other->features();
      std::copy(f1.begin(), f1.end(), std::back_inserter(features));
      std::copy(f2.begin(), f2.end(), std::back_inserter(features));
    }
    else if (ourType != theirType) {
      return SASS_MEMORY_NEW(CssMediaQuery, pstate());
    }
    else {
      type = ourType;
      modifier = ourModifier.empty() ? theirModifier : ourModifier;
      const sass::vector<sass::string>& f1 = this->features();
      const sass::vector<sass::string>& f2 = other->features();
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

  /////////////////////////////////////////////////////////////////////////////
  // An `@at-root` rule. Moves it contents "up" the tree through parent nodes.
  // Note: This does not exist in dart-sass, as it moves stuff on eval stage.
  /////////////////////////////////////////////////////////////////////////////

  // Value constructor
  CssAtRootRule::CssAtRootRule(
    const SourceSpan& pstate,
    AtRootQueryObj query,
    const sass::vector<StatementObj>& children) :
    CssParentNode(pstate, children),
    query_(query)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Keyframe_Rule::Keyframe_Rule(
    const SourceSpan& pstate,
    const sass::vector<StatementObj>& children)
    : CssParentNode(pstate, children), name2_()
  {}
  Keyframe_Rule::Keyframe_Rule(const Keyframe_Rule* ptr)
    : CssParentNode(ptr), name2_(ptr->name2_)
  {}


  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // IMPLEMENT_AST_OPERATORS(CssNode);
  IMPLEMENT_AST_OPERATORS(CssStrings);
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
  IMPLEMENT_AST_OPERATORS(Keyframe_Rule);

}
