#include "string_utils.hpp"
#include "ast_css.hpp"
#include "strings.hpp"
#include "util.hpp"
#include "ast.hpp"

namespace Sass {

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  CssNode::CssNode(
    const SourceSpan& pstate) :
    AST_Node(pstate),
    isGroupEnd_(false)
  {}

  CssNode::CssNode(const CssNode* ptr, bool childless) :
    AST_Node(ptr),
    isGroupEnd_(ptr->isGroupEnd_)
  {}

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  CssParentNode::CssParentNode(
    const SourceSpan& pstate,
    CssParentNode* parent) :
    CssNode(pstate),
    Vectorized(),
    isChildless_(false),
    parent_(parent)
  {}

  CssParentNode::CssParentNode(
    const SourceSpan& pstate,
    CssParentNode* parent,
    sass::vector<CssNodeObj>&& children) :
    CssNode(pstate),
    Vectorized(std::move(children)),
    isChildless_(false),
    parent_(parent)
  {}

  
  CssParentNode::CssParentNode(
    const CssParentNode* ptr, bool childless) :
    CssNode(ptr),
    Vectorized(),
    isChildless_(ptr->isChildless_),
    parent_(ptr->parent_)
  {
    if (!childless) elements_ = ptr->elements_;
  }

  bool CssParentNode::_isInvisible2(CssNode* asd)
  {
    if (auto node = Cast<CssParentNode>(asd)) {
      // An unknown at-rule is never invisible. Because we don't know the
      // semantics of unknown rules, we can't guarantee that (for example)
      // `@foo {}` isn't meaningful.
      if (Cast<CssAtRule>(node)) {
        //std::cerr << "Saw atRule\n";
        return false;
      }

      auto styleRule = Cast<CssStyleRule>(node);
      if (styleRule && styleRule->selector()->isInvisible()) {
        //std::cerr << "Saw invisible selector\n";
        return true;
      }

      for (auto item : node->elements()) {
        if (!_isInvisible2(item)) {
          //std::cerr << "Saw visible child\n";
          return false;
        }
      }

      return true;
    }
    else {
      return false;
    }
  }

  bool CssParentNode::hasVisibleSibling(CssParentNode* node)
  {
    if (node->parent_ == nullptr) return false;

    CssParentNode* siblings = node->parent_;

    auto it = std::find(siblings->begin(), siblings->end(), node);

    while (++it != siblings->end()) {
      // Special context for invisibility!
      // dart calls this out to the parent
      if (!_isInvisible2(*it)) {
        return true;
      }
    }

    return false;
  }


  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  CssRoot::CssRoot(const SourceSpan& pstate,
    sass::vector<CssNodeObj>&& vec)
    : CssParentNode(pstate, nullptr, std::move(vec))
  {}

//  CssRoot::CssRoot(const CssRoot* ptr)
//    : CssParentNode(ptr, childless)
//  {}

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  CssString::CssString(const SourceSpan& pstate, const sass::string& text) :
    AST_Node(pstate),
    text_(text)
  {}

  CssString::CssString(const CssString* ptr, bool childless) :
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

  CssStrings::CssStrings(const CssStrings* ptr, bool childless) :
    AST_Node(ptr),
    texts_(ptr->texts_)
  {}

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  CssValue::CssValue(const SourceSpan& pstate, Value* value) :
    CssNode(pstate),
    value_(value)
  {}

  CssValue::CssValue(const CssValue* ptr, bool childless) :
    CssNode(ptr),
    value_(ptr->value_)
  {}

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  CssAtRule::CssAtRule(
    const SourceSpan& pstate,
    CssParentNode* parent,
    CssString* name,
    CssString* value)
    :
    CssParentNode(pstate, parent),
    name_(name),
    value_(value)
  {}

  CssAtRule::CssAtRule(
    const SourceSpan& pstate,
    CssParentNode* parent,
    CssString* name,
    CssString* value,
    sass::vector<CssNodeObj>&& children)
    :
    CssParentNode(pstate, parent, std::move(children)),
    name_(name),
    value_(value)
  {}

  bool CssAtRule::is_invisible() const
  {
    return false; //  empty() && !isChildless_;
  }

  CssAtRule::CssAtRule(
    const CssAtRule* ptr, bool childless) :
    CssParentNode(ptr, childless),
    name_(ptr->name_),
    value_(ptr->value_)
  {}

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
    const CssComment* ptr, bool childless) :
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
    const CssDeclaration* ptr, bool childless) :
    CssNode(ptr),
    name_(ptr->name_),
    value_(ptr->value_),
    is_custom_property_(ptr->is_custom_property_)
  {}

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  // Value constructor
  CssImport::CssImport(
    const SourceSpan& pstate,
    CssString* url,
    CssString* supports,
    sass::vector<CssMediaQueryObj> media) :
    CssNode(pstate),
    url_(url),
    supports_(supports),
    media_(media),
    outOfOrder_(false)
  {
  }

  CssImport::CssImport(
    const CssImport* ptr, bool childless) :
    CssNode(ptr),
    url_(ptr->url_),
    supports_(ptr->supports_),
    media_(ptr->media_),
    outOfOrder_(ptr->outOfOrder_)
  {}

  /////////////////////////////////////////////////////////////////////////////
  // A block within a `@keyframes` rule.
  // For example, `10% {opacity: 0.5}`.
  /////////////////////////////////////////////////////////////////////////////

  // Value constructor
  CssKeyframeBlock::CssKeyframeBlock(
    const SourceSpan& pstate,
    CssParentNode* parent,
    CssStrings* selector) :
    CssParentNode(pstate, parent),
    selector_(selector)
  {}

  // Value constructor
  CssKeyframeBlock::CssKeyframeBlock(
    const SourceSpan& pstate,
    CssParentNode* parent,
    CssStrings* selector,
    sass::vector<CssNodeObj>&& children) :
    CssParentNode(pstate, parent, std::move(children)),
    selector_(selector)
  {}

  // Copy constructor
  CssKeyframeBlock::CssKeyframeBlock(
    const CssKeyframeBlock* ptr, bool childless) :
    CssParentNode(ptr, childless),
    selector_(ptr->selector_)
  {}

  // Return a copy with empty children
  // CssKeyframeBlock* CssKeyframeBlock::copyWithoutChildren() {
  //   CssKeyframeBlock* cpy = SASS_MEMORY_COPY(this);
  //   cpy->clear(); // return with empty children
  //   return cpy;
  // }
  // EO copyWithoutChildren

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  CssStyleRule::CssStyleRule(
    const SourceSpan& pstate,
    CssParentNode* parent,
    SelectorList* selector) :
    CssParentNode(pstate, parent),
    selector_(selector)
  {}

  CssStyleRule::CssStyleRule(
    const SourceSpan& pstate,
    CssParentNode* parent,
    SelectorList* selector,
    sass::vector<CssNodeObj>&& children) :
    CssParentNode(pstate, parent, std::move(children)),
    selector_(selector)
  {}

  CssStyleRule::CssStyleRule(
    const CssStyleRule* ptr, bool childless) :
    CssParentNode(ptr, childless),
    selector_(ptr->selector_)
  {}

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  CssSupportsRule::CssSupportsRule(
    const SourceSpan& pstate,
    CssParentNode* parent,
    ExpressionObj condition) :
    CssParentNode(pstate, parent),
    condition_(condition)
  {}

  CssSupportsRule::CssSupportsRule(
    const SourceSpan& pstate,
    CssParentNode* parent,
    ExpressionObj condition,
    sass::vector<CssNodeObj>&& children) :
    CssParentNode(pstate, parent, std::move(children)),
    condition_(condition)
  {}

  bool CssSupportsRule::is_invisible() const
  {
    for (auto stmt : elements()) {
      if (!stmt->is_invisible()) return false;
    }
    return true;
  }

  CssSupportsRule::CssSupportsRule(const CssSupportsRule* ptr, bool childless) :
    CssParentNode(ptr, childless),
    condition_(ptr->condition_)
  {}

  /////////////////////////////////////////////////////////////////////////
  // A plain CSS `@media` rule after it has been evaluated.
  /////////////////////////////////////////////////////////////////////////

  // Value constructor
  CssMediaRule::CssMediaRule(
    const SourceSpan& pstate,
    CssParentNode* parent,
    const sass::vector<CssMediaQueryObj>& queries) :
    CssParentNode(pstate, parent),
    queries_(queries)
  {}

  // Value constructor
  CssMediaRule::CssMediaRule(
    const SourceSpan& pstate,
    CssParentNode* parent,
    const sass::vector<CssMediaQueryObj>& queries,
    sass::vector<CssNodeObj>&& children) :
    CssParentNode(pstate, parent, std::move(children)),
    queries_(queries)
  {}

  // Copy constructor for rule of three
  CssMediaRule::CssMediaRule(const CssMediaRule* ptr, bool childless) :
    CssParentNode(ptr, childless),
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

    sass::string ourType(this->type());
    StringUtils::makeLowerCase(ourType);

    sass::string theirType(other->type());
    StringUtils::makeLowerCase(theirType);

    sass::string ourModifier(this->modifier());
    StringUtils::makeLowerCase(ourModifier);

    sass::string theirModifier(other->modifier());
    StringUtils::makeLowerCase(theirModifier);

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

    bool ourModifierIsNot = StringUtils::equalsIgnoreCase(ourModifier, "not", 3);
    bool theirModifierIsNot = StringUtils::equalsIgnoreCase(theirModifier, "not", 3);

    if (ourModifierIsNot != theirModifierIsNot) {
      if (ourType == theirType) {
        sass::vector<sass::string> negativeFeatures =
          ourModifierIsNot ? this->features() : other->features();
        sass::vector<sass::string> positiveFeatures =
          ourModifierIsNot ? other->features() : this->features();

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

      if (ourModifierIsNot) {
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
    else if (ourModifierIsNot) {

      SASS_ASSERT(theirModifierIsNot, "modifiers not is sync");

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

  CssMediaQuery::CssMediaQuery(const CssMediaQuery* ptr, bool childless) :
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
    CssParentNode* parent,
    AtRootQueryObj query) :
    CssParentNode(pstate, parent),
    query_(query)
  {}

  // Value constructor
  CssAtRootRule::CssAtRootRule(
    const SourceSpan& pstate,
    CssParentNode* parent,
    AtRootQueryObj query,
    sass::vector<CssNodeObj>&& children) :
    CssParentNode(pstate, parent, std::move(children)),
    query_(query)
  {}

  // Value constructor
  CssAtRootRule::CssAtRootRule(
    const CssAtRootRule* ptr, bool childless) :
    CssParentNode(ptr, childless),
    query_(ptr->query_)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Bubble::Bubble(const SourceSpan& pstate, CssNodeObj n, CssNodeObj g)
  : CssNode(pstate), node_(n)
  { }


  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  CssImportTrace::CssImportTrace(const SourceSpan& pstate,
    CssParentNode* parent,
    const sass::string& n, char type)
    : CssParentNode(pstate, parent), type_(type), name_(n)
  {}
  CssImportTrace::CssImportTrace(const SourceSpan& pstate,
    CssParentNode* parent,
    const sass::string& n, sass::vector<CssNodeObj>&& b, char type)
    : CssParentNode(pstate, parent, std::move(b)), type_(type), name_(n)
  {}
  CssImportTrace::CssImportTrace(const CssImportTrace* ptr, bool childless) :
    CssParentNode(ptr, childless),
    type_(ptr->type_),
    name_(ptr->name_)
  {}


  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // IMPLEMENT_AST_OPERATORS(CssNode);
  //IMPLEMENT_COPY_OPERATORS(CssStrings);
  // IMPLEMENT_COPY_OPERATORS(CssString);
//  IMPLEMENT_COPY_OPERATORS(CssValue);
  //IMPLEMENT_COPY_OPERATORS(CssAtRule);
  //IMPLEMENT_COPY_OPERATORS(CssComment);
  //IMPLEMENT_COPY_OPERATORS(CssDeclaration);
  // IMPLEMENT_COPY_OPERATORS(CssImport); 
  // IMPLEMENT_COPY_OPERATORS(CssKeyframeBlock);
  // IMPLEMENT_COPY_OPERATORS(CssMediaQuery);
  IMPLEMENT_COPY_OPERATORS(CssMediaRule);
  IMPLEMENT_COPY_OPERATORS(CssStyleRule);
  IMPLEMENT_COPY_OPERATORS(CssSupportsRule);
  IMPLEMENT_COPY_OPERATORS(CssAtRootRule);
  IMPLEMENT_COPY_OPERATORS(CssImportTrace);

}
