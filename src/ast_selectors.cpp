// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include "ast.hpp"
#include "character.hpp"
#include "permutate.hpp"
#include "util_string.hpp"
#include "debugger.hpp"
#include "ast_selectors.hpp"
#include "dart_helpers.hpp"

namespace Sass {

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Selector::Selector(const SourceSpan& pstate)
  : AST_Node(pstate),
    hash_(0)
  {}

  Selector::Selector(const Selector* ptr)
  : AST_Node(ptr),
    hash_(ptr->hash_)
  {}


  bool Selector::has_real_parent_ref() const
  {
    return false;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  SimpleSelector::SimpleSelector(const SourceSpan& pstate, const sass::string& n)
  : Selector(pstate), name_(n)
  {
  }
  SimpleSelector::SimpleSelector(const SimpleSelector* ptr)
  : Selector(ptr),
    name_(ptr->name_)
  { }

  sass::string NameSpaceSelector::ns_name() const
  {
    if (!has_ns_) return name_;
    else return ns_ + "|" + name_;
  }

  size_t SimpleSelector::hash() const
  {
    if (hash_ == 0) {
      hash_start(hash_, name());
      hash_combine(hash_,
        // Supported by c++ only
        typeid(this).hash_code());
    }
    return hash_;
  }

  bool SimpleSelector::empty() const {
    return name().empty();
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  NameSpaceSelector::NameSpaceSelector(
    const SourceSpan& pstate,
    const sass::string& name) :
    SimpleSelector(pstate, name),
    has_ns_(false),
    ns_("")
  {
    size_t pos = name.find('|');
    // found some namespace
    if (pos != sass::string::npos) {
      has_ns_ = true;
      ns_ = name.substr(0, pos);
      name_ = name.substr(pos + 1);
    }

  }

  NameSpaceSelector::NameSpaceSelector(const NameSpaceSelector* ptr)
    : SimpleSelector(ptr),
    has_ns_(ptr->has_ns_),
    ns_(ptr->ns_)
  { }

  size_t NameSpaceSelector::hash() const
  {
    if (hash_ == 0) {
      hash_start(hash_, SimpleSelector::hash());
      if (has_ns_) hash_combine(hash_, ns());
    }
    return hash_;
  }

  bool NameSpaceSelector::empty() const
  {
    return ns().empty() && SimpleSelector::empty();
  }

  // namespace compare functions
  bool NameSpaceSelector::is_ns_eq(const NameSpaceSelector& r) const
  {
    return has_ns_ == r.has_ns_ && ns_ == r.ns_;
  }

  bool NameSpaceSelector::ns_match(const SimpleSelector& r) const
  {
    if (has_ns() == false || is_universal_ns()) {
      return SimpleSelector::ns_match(r);
    }
    if (auto simple = Cast<NameSpaceSelector>(&r)) {
      return ns_match(*simple);
    }
    return false;
  }

  // namespace compare functions
  bool NameSpaceSelector::ns_match(const NameSpaceSelector& r) const
  {
    if (has_ns_ && ns_ == "*") return true;
    return has_ns_ == r.has_ns_ && ns_ == r.ns_;
  }

  // namespace query functions
  bool NameSpaceSelector::is_universal_ns() const
  {
    return has_ns_ && ns_ == "*";
  }

  // namespace query functions
  bool NameSpaceSelector::is_explicit_ns() const
  {
    return has_ns_ && ns_ != "*";
  }

  // name query functions
  bool SimpleSelector::is_universal() const
  {
    return name_ == "*";
  }

  bool SimpleSelector::has_placeholder()
  {
    return false;
  }

  bool SimpleSelector::has_real_parent_ref() const
  {
    return false;
  }

  CompoundSelectorObj SimpleSelector::wrapInCompound()
  {
    
    return SASS_MEMORY_NEW(CompoundSelector, pstate(), { this }, false);
  }
  ComplexSelectorObj SimpleSelector::wrapInComplex()
  {
    return SASS_MEMORY_NEW(ComplexSelector, pstate(), { wrapInCompound() });
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  PlaceholderSelector::PlaceholderSelector(const SourceSpan& pstate, const sass::string& n)
  : SimpleSelector(pstate, n)
  { }
  PlaceholderSelector::PlaceholderSelector(const PlaceholderSelector* ptr)
  : SimpleSelector(ptr)
  { }
  unsigned long PlaceholderSelector::specificity() const
  {
		return Constants::Specificity::Base;
  }

  bool PlaceholderSelector::has_placeholder() {
    return true;
  }

  // Returns whether this is a private selector.
  // That is, whether it begins with `-` or `_`.
  bool PlaceholderSelector::isPrivate() const
  {
    return name_[0] == Character::$dash
      || name_[0] == Character::$underscore;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  TypeSelector::TypeSelector(const SourceSpan& pstate, const sass::string& n)
  : NameSpaceSelector(pstate, n)
  { }
  TypeSelector::TypeSelector(const TypeSelector* ptr)
  : NameSpaceSelector(ptr)
  { }

  unsigned long TypeSelector::specificity() const
  {
    if (name() == "*") return 0;
    else return Constants::Specificity::Element;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  ClassSelector::ClassSelector(const SourceSpan& pstate, const sass::string& n)
  : SimpleSelector(pstate, n)
  { }
  ClassSelector::ClassSelector(const ClassSelector* ptr)
  : SimpleSelector(ptr)
  { }

  unsigned long ClassSelector::specificity() const
  {
    return Constants::Specificity::Class;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  IDSelector::IDSelector(const SourceSpan& pstate, const sass::string& n)
  : SimpleSelector(pstate, n)
  { }
  IDSelector::IDSelector(const IDSelector* ptr)
  : SimpleSelector(ptr)
  { }

  unsigned long IDSelector::specificity() const
  {
		return Constants::Specificity::ID;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  AttributeSelector::AttributeSelector(const SourceSpan& pstate, sass::string&& n, sass::string&& op, sass::string&& v, bool isIdentifier, char o)
  : NameSpaceSelector(pstate, n), op_(std::move(op)), value_(std::move(v)), modifier_(std::move(o)), isIdentifier_(isIdentifier)
  { }
  AttributeSelector::AttributeSelector(const AttributeSelector* ptr)
  : NameSpaceSelector(ptr),
    op_(ptr->op_),
    value_(ptr->value_),
    modifier_(ptr->modifier_),
    isIdentifier_(ptr->isIdentifier_)
  { }

  unsigned long AttributeSelector::specificity() const
  {
		return Constants::Specificity::Attr;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  PseudoSelector::PseudoSelector(const SourceSpan& pstate, const sass::string& name, bool element)
  : SimpleSelector(pstate, name),
    normalized_(Util::unvendor(name)),
    argument_(),
    selector_(),
    isSyntacticClass_(!element),
    isClass_(!element && !isFakePseudoElement(normalized_))
  { }

  PseudoSelector::PseudoSelector(const PseudoSelector* ptr)
  : SimpleSelector(ptr),
    normalized_(ptr->normalized()),
    argument_(ptr->argument()),
    selector_(ptr->selector()),
    isSyntacticClass_(ptr->isSyntacticClass()),
    isClass_(ptr->isClass())
  { }

  // A pseudo-element is made of two colons (::) followed by the name.
  // The `::` notation is introduced by the current document in order to
  // establish a discrimination between pseudo-classes and pseudo-elements.
  // For compatibility with existing style sheets, user agents must also
  // accept the previous one-colon notation for pseudo-elements introduced
  // in CSS levels 1 and 2 (namely, :first-line, :first-letter, :before and
  // :after). This compatibility is not allowed for the new pseudo-elements
  // introduced in this specification.
  bool PseudoSelector::is_pseudo_element() const
  {
    return isElement();
  }

  bool PseudoSelector::isImpossible() const
  {
    if (selector() && selector()->empty() && name() != "not") return true;
    return false;
  }

  size_t PseudoSelector::hash() const
  {
    if (hash_ == 0) {
      hash_start(hash_, argument_);
      if (selector_) hash_combine(
        hash_, selector_->hash());
    }
    return hash_;
  }

  bool PseudoSelector::is_invisible() const
  {
    return selector() && selector()->isInvisible() && argument_.empty() && name_ != "not";
  }

  unsigned long PseudoSelector::specificity() const
  {
    if (is_pseudo_element())
			return Constants::Specificity::Element;
		return Constants::Specificity::Pseudo;
  }

  PseudoSelector* PseudoSelector::withSelector(SelectorList* selector)
  {
    PseudoSelector* pseudo = SASS_MEMORY_COPY(this);
    pseudo->selector(selector);
    return pseudo;
  }

  bool PseudoSelector::empty() const
  {
    // Only considered empty if selector is
    // available but has no items in it.
    return argument_.empty() && name().empty() &&
      (selector() && selector()->empty());
  }

  bool PseudoSelector::has_real_parent_ref() const {
    if (!selector()) return false;
    return selector()->has_real_parent_ref();
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  SelectorList::SelectorList(
    const SourceSpan& pstate,
    sass::vector<ComplexSelectorObj>&& complexes) :
    Selector(pstate),
    VectorizedNopsi(std::move(complexes))
  { }
  SelectorList::SelectorList(const SelectorList* ptr)
    : Selector(ptr),
    VectorizedNopsi(*ptr)
  { }

  size_t SelectorList::hash() const
  {
    return 0;
  }

  bool SelectorList::has_real_parent_ref() const
  {
    for (ComplexSelectorObj s : elements()) {
      if (s && s->has_real_parent_ref()) return true;
    }
    return false;
  }

  void SelectorList::cloneChildren()
  {
    for (size_t i = 0, l = length(); i < l; i++) {
      at(i) = SASS_MEMORY_CLONE(at(i));
    }
  }

  unsigned long SelectorList::specificity() const
  {
    return 0;
  }

  bool SelectorList::isInvisible() const
  {
    if (empty()) return true;
    for (size_t i = 0; i < length(); i += 1) {
      if (get(i)->isInvisible()) return true;
    }
    return false;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  ComplexSelector::ComplexSelector(const SourceSpan& pstate)
    : Selector(pstate),
    VectorizedNopsi(),
    chroots_(false),
    hasPreLineFeed_(false)
  {
  }

  ComplexSelector::ComplexSelector(const SourceSpan& pstate,
    sass::vector<SelectorComponentObj>&& components) :
    Selector(pstate),
    VectorizedNopsi(std::move(components)),
    chroots_(false),
    hasPreLineFeed_(false)
  {
  }

  

  ComplexSelector::ComplexSelector(const ComplexSelector* ptr)
  : Selector(ptr),
    VectorizedNopsi(ptr),
    chroots_(ptr->chroots()),
    hasPreLineFeed_(ptr->hasPreLineFeed())
  {
  }

  void ComplexSelector::cloneChildren()
  {
    for (size_t i = 0, l = length(); i < l; i++) {
      at(i) = SASS_MEMORY_CLONE(at(i));
    }
  }

  unsigned long ComplexSelector::specificity() const
  {
    int sum = 0;
    for (auto component : elements()) {
      sum += component->specificity();
    }
    return sum;
  }

  bool ComplexSelector::isInvisible() const
  {
    if (length() == 0) return true;
    for (size_t i = 0; i < length(); i += 1) {
      if (get(i)->isInvisible()) return true;
    }
    return false;
  }

  bool ComplexSelector::isImpossible() const
  {
    if (length() == 0) return false;
    for (size_t i = 0; i < length(); i += 1) {
      if (CompoundSelector* compound = get(i)->getCompound()) {
        if (compound->isImpossible()) return true;
      }
    }
    return false;
  }

  SelectorListObj ComplexSelector::wrapInList()
  {
    return SASS_MEMORY_NEW(SelectorList, pstate(), { this });
  }

  size_t ComplexSelector::hash() const
  {
    return 0;
  }

  bool ComplexSelector::has_placeholder() const {
    for (size_t i = 0, L = length(); i < L; ++i) {
      if (get(i)->has_placeholder()) return true;
    }
    return false;
  }

  bool ComplexSelector::has_real_parent_ref() const
  {
    for (SelectorComponent* component : elements()) {
      if (component->has_real_parent_ref()) return true;
    }
    return false;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  SelectorComponent::SelectorComponent(const SourceSpan& pstate, bool postLineBreak)
  : Selector(pstate),
    hasPostLineBreak_(postLineBreak)
  {
  }

  SelectorComponent::SelectorComponent(const SelectorComponent* ptr)
  : Selector(ptr),
    hasPostLineBreak_(ptr->hasPostLineBreak())
  { }

  void SelectorComponent::cloneChildren()
  {
  }

  unsigned long SelectorComponent::specificity() const
  {
    return 0;
  }

  // Wrap the compound selector with a complex selector
  ComplexSelector* SelectorComponent::wrapInComplex()
  {
    auto complex = SASS_MEMORY_NEW(ComplexSelector, pstate());
    complex->append(this);
    return complex;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  SelectorCombinator::SelectorCombinator(const SourceSpan& pstate, SelectorCombinator::Combinator combinator, bool postLineBreak)
    : SelectorComponent(pstate, postLineBreak),
    combinator_(combinator)
  {
  }

  unsigned long SelectorCombinator::specificity() const
  {
    return 0;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  CompoundSelector::CompoundSelector(const SourceSpan& pstate, bool postLineBreak)
    : SelectorComponent(pstate, postLineBreak),
      VectorizedNopsi(),
      hasRealParent_(false),
      extended_(false)
  {
  }

  CompoundSelector::CompoundSelector(
    const SourceSpan& pstate,
    sass::vector<SimpleSelectorObj>&& selectors,
    bool postLineBreak) :
    SelectorComponent(pstate, postLineBreak),
    VectorizedNopsi(std::move(selectors)),
    hasRealParent_(false),
    extended_(false)
  {
  }

  CompoundSelector::CompoundSelector(const CompoundSelector* ptr)
    : SelectorComponent(ptr),
    VectorizedNopsi(*ptr),
      hasRealParent_(ptr->hasRealParent()),
      extended_(ptr->extended())
  { }

  size_t CompoundSelector::hash() const
  {
    return 0;
  }

  bool CompoundSelector::has_real_parent_ref() const
  {
    if (hasRealParent()) return true;
    // ToDo: dart sass has another check?
    // if (Cast<TypeSelector>(front)) {
    //  if (front->ns() != "") return false;
    // }
    for (const SimpleSelector* s : elements()) {
      if (s && s->has_real_parent_ref()) return true;
    }
    return false;
  }

  bool CompoundSelector::has_placeholder() const
  {
    if (length() == 0) return false;
    for (SimpleSelectorObj ss : elements()) {
      if (ss->has_placeholder()) return true;
    }
    return false;
  }

  void CompoundSelector::cloneChildren()
  {
    for (size_t i = 0, l = length(); i < l; i++) {
      at(i) = SASS_MEMORY_CLONE(at(i));
    }
  }

  unsigned long CompoundSelector::specificity() const
  {
    int sum = 0;
    for (size_t i = 0, L = length(); i < L; ++i)
    { sum += get(i)->specificity(); }
    return sum;
  }

  bool CompoundSelector::isInvisible() const
  {
    for (size_t i = 0; i < length(); i += 1) {
      if (!get(i)->isInvisible()) return false;
    }
    return true;
  }

  bool CompoundSelector::isImpossible() const
  {
    for (size_t i = 0; i < length(); i += 1) {
      if (SimpleSelector* simple = get(i)) {
        if (simple->isImpossible()) return true;
      }
    }
    return false;
  }

  bool CompoundSelector::isSuperselectorOf(const CompoundSelector* sub, sass::string wrapped) const
  {
    return compoundIsSuperselector(this, sub, {});
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  MediaRule::MediaRule(const SourceSpan& pstate, InterpolationObj list, Block_Obj block) :
    ParentStatement(pstate, block, false), query_(list)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////



  /////////////////////////////////////////////////////////////////////////
  // ToDo: finalize specificity implementation
  /////////////////////////////////////////////////////////////////////////

  size_t SelectorList::maxSpecificity() const
  {
    size_t specificity = 0;
    for (auto complex : elements()) {
      specificity = std::max(specificity, complex->maxSpecificity());
    }
    return specificity;
  }

  size_t SelectorList::minSpecificity() const
  {
    size_t specificity = 0;
    for (auto complex : elements()) {
      specificity = std::min(specificity, complex->minSpecificity());
    }
    return specificity;
  }

  size_t CompoundSelector::maxSpecificity() const
  {
    size_t specificity = 0;
    for (auto simple : elements()) {
      specificity += simple->maxSpecificity();
    }
    return specificity;
  }

  size_t CompoundSelector::minSpecificity() const
  {
    size_t specificity = 0;
    for (auto simple : elements()) {
      specificity += simple->minSpecificity();
    }
    return specificity;
  }

  size_t ComplexSelector::maxSpecificity() const
  {
    size_t specificity = 0;
    for (auto component : elements()) {
      specificity += component->maxSpecificity();
    }
    return specificity;
  }

  size_t ComplexSelector::minSpecificity() const
  {
    size_t specificity = 0;
    for (auto component : elements()) {
      specificity += component->minSpecificity();
    }
    return specificity;
  }

  /////////////////////////////////////////////////////////////////////////
  // ToDo: this might be done easier with new selector format
  /////////////////////////////////////////////////////////////////////////

  sass::vector<ComplexSelectorObj>
    CompoundSelector::resolveParentSelectors(SelectorList* parent, BackTraces& traces, bool implicit_parent)
  {

    sass::vector<ComplexSelectorObj> rv;

    for (SimpleSelectorObj simple : elements()) {
      if (PseudoSelector * pseudo = simple->getPseudoSelector()) {
        if (SelectorList* sel = pseudo->selector()) {
          if (parent) {
            pseudo->selector(sel->resolveParentSelectors(
              parent, traces, implicit_parent));
          }
        }
      }
    }

    // Mix with parents from stack
    // Equivalent to dart-sass parent selector tail
    if (hasRealParent()) {
      SASS_ASSERT(parent != nullptr, "Parent must be defined");
      for (auto complex : parent->elements()) {
        // The parent complex selector has a compound selector
        if (CompoundSelector* tail = complex->last()->getCompound()) {
          // Create copies to alter them
          tail = SASS_MEMORY_COPY(tail);
          complex = SASS_MEMORY_COPY(complex);

          // Check if we can merge front with back
          if (length() > 0 && tail->length() > 0) {
            SimpleSelector* front = first();
            auto simple_back = tail->last();
            auto simple_front = front->getTypeSelector();
            // If they are type/simple selectors ...
            if (simple_front && simple_back) {
              // ... we can combine the names into one
              simple_back = SASS_MEMORY_COPY(simple_back);
              auto name = simple_back->name();
              name += simple_front->name();
              simple_back->name(name);
              // Replace with modified simple selector
              tail->elements().back() = simple_back;
              // Append rest of selector components
              tail->elements().insert(tail->end(),
                begin() + 1, end());
            }
            else {
              // Append us to parent
              tail->concat(this);
            }
          }
          else {
            // Append us to parent
            tail->concat(this);
          }
          // Reset the parent selector tail with
          // the combination of parent plus ourself
          complex->elements().back() = tail;
          // Append to results
          rv.emplace_back(complex);
        }
        // SelectorCombinator
        else {
          // Can't insert parent that ends with a combinator
          // where the parent selector is followed by something
          callStackFrame frame(traces, complex->last()->pstate());
          if (length() > 0) { throw Exception::InvalidParent(parent, traces, this); }
          // Just append ourself to results
          rv.emplace_back(wrapInComplex());
        }
      }

    }
    // No parent
    else {
      // Wrap and append compound selector
      rv.emplace_back(wrapInComplex());
    }

    return rv;

  }
  // EO CompoundSelector::resolveParentSelectors

  sass::vector<ComplexSelectorObj> ComplexSelector::resolveParentSelectors(SelectorList* parent, BackTraces& traces, bool implicit_parent)
  {

    if (has_real_parent_ref() && !parent) {
      throw Exception::TopLevelParent(traces, pstate());
    }

    sass::vector<sass::vector<ComplexSelectorObj>> selectors;

    // Check if selector should implicit get a parent
    if (!chroots() && !has_real_parent_ref()) {
      // Check if we should never connect to parent
      if (!implicit_parent) { return { this }; }
      // Otherwise add parent selectors at the begining
      if (parent) { selectors.emplace_back(parent->elements()); }
    }

    // Loop all items from the complex selector
    for (SelectorComponent* component : this->elements()) {
      if (CompoundSelector* compound = component->getCompound()) {
        sass::vector<ComplexSelectorObj> complexes =
          compound->resolveParentSelectors(parent, traces, implicit_parent);
        // for (auto sel : complexes) { sel->hasPreLineFeed(hasPreLineFeed()); }
        if (complexes.size() > 0) selectors.emplace_back(complexes);
      }
      else {
        // component->hasPreLineFeed(hasPreLineFeed());
        selectors.push_back({ component->wrapInComplex() });
      }
    }

    // Premutate through all paths
    selectors = permutateAlt(selectors);

    // Create final selectors from path permutations
    sass::vector<ComplexSelectorObj> resolved;
    for (sass::vector<ComplexSelectorObj>& items : selectors) {
      if (items.empty()) continue;
      ComplexSelectorObj first = SASS_MEMORY_COPY(items[0]);
      if (hasPreLineFeed() && !has_real_parent_ref()) {
        first->hasPreLineFeed(true);
      }
      // ToDo: remove once we know how to handle line feeds
      // ToDo: currently a mashup between ruby and dart sass
      // if (has_real_parent_ref()) first->has_line_feed(false);
      // first->has_line_break(first->has_line_break() || has_line_break());
      first->chroots(true); // has been resolved by now
      for (size_t i = 1; i < items.size(); i += 1) {
        if (items[i]->hasPreLineFeed()) {
          first->hasPreLineFeed(true);
        }
        first->concat(items[i]);
      }
      resolved.emplace_back(first);
    }

    return resolved;
  }
  // EO ComplexSelector::resolveParentSelectors

  SelectorList* SelectorList::resolveParentSelectors(SelectorList* parent, BackTraces& traces, bool implicit_parent)
  {
    sass::vector<sass::vector<ComplexSelectorObj>> lists;
    for (ComplexSelector* sel : elements()) {
      lists.emplace_back(sel->resolveParentSelectors
        (parent, traces, implicit_parent));
    }
    return SASS_MEMORY_NEW(SelectorList, pstate(),
      flattenVertically<ComplexSelectorObj>(lists));
  }
  // EO SelectorList::resolveParentSelectors

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // ToDo: IMO only containers should need copy operations
  // All other selectors should be quite static after eval
  IMPLEMENT_COPY_OPERATORS(PlaceholderSelector);
  IMPLEMENT_COPY_OPERATORS(AttributeSelector);
  IMPLEMENT_COPY_OPERATORS(TypeSelector);
  IMPLEMENT_COPY_OPERATORS(ClassSelector);
  IMPLEMENT_COPY_OPERATORS(IDSelector);
  IMPLEMENT_COPY_OPERATORS(PseudoSelector);
  // IMPLEMENT_COPY_OPERATORS(SelectorCombinator);
  IMPLEMENT_COPY_OPERATORS(CompoundSelector);
  IMPLEMENT_COPY_OPERATORS(ComplexSelector);
  IMPLEMENT_COPY_OPERATORS(SelectorList);

}
