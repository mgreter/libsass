#include "sass.hpp"
#include "ast.hpp"
#include "context.hpp"
#include "node.hpp"
#include "eval.hpp"
#include "extend.hpp"
#include "debugger.hpp"
#include "emitter.hpp"
#include "color_maps.hpp"
#include "ast_fwd_decl.hpp"
#include "ast_selectors.hpp"
#include <array>
#include <set>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <functional>
#include <cctype>
#include <locale>

namespace Sass {

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  std::string normName(std::string str) {
    std::string name = str;
    std::replace(name.begin(), name.end(), '_', '-');
    if (name[0] == ':') {
      name = name.substr(1);
    }
    if (name[0] == '-' && name[1] == 'm' && name[2] == 'o' && name[3] == 'z' && name[4] == '-') {
      name = name.substr(5);
    }
    return name;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Selector::Selector(ParserState pstate)
  : Expression(pstate),
    is_optional_(false),
    media_block_(0),
    hash_(0)
  { concrete_type(SELECTOR); }

  Selector::Selector(const Selector* ptr)
  : Expression(ptr),
    is_optional_(ptr->is_optional_),
    media_block_(ptr->media_block_),
    hash_(ptr->hash_)
  { concrete_type(SELECTOR); }

  void Selector::set_media_block(Media_Block* mb)
  {
    media_block(mb);
  }

  bool Selector::has_parent_ref() const
  {
    return false;
  }

  bool Selector::has_real_parent_ref() const
  {
    return false;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Selector_Schema::Selector_Schema(ParserState pstate, String_Obj c)
  : AST_Node(pstate),
    contents_(c),
    connect_parent_(true),
    media_block_(NULL),
    hash_(0)
  { }
  Selector_Schema::Selector_Schema(const Selector_Schema* ptr)
  : AST_Node(ptr),
    contents_(ptr->contents_),
    connect_parent_(ptr->connect_parent_),
    media_block_(ptr->media_block_),
    hash_(ptr->hash_)
  { }

  unsigned long Selector_Schema::specificity() const
  {
    return 0;
  }

  size_t Selector_Schema::hash() const {
    if (hash_ == 0) {
      hash_combine(hash_, contents_->hash());
    }
    return hash_;
  }


  bool Selector_Schema::containsParentRef() const
  {
    if (String_Schema_Obj schema = Cast<String_Schema>(contents())) {
      for (auto item : schema->elements()) {
        if (Cast<Parent_Reference>(item)) return true;
      }
    }
    return false;
  }

  bool Selector_Schema::has_real_parent_ref() const
  {
    if (String_Schema_Obj schema = Cast<String_Schema>(contents())) {
      if (schema->empty()) return false;
      const auto& first = *schema->at(0);
      return typeid(first) == typeid(Parent_Reference);
    }
    return false;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Simple_Selector::Simple_Selector(ParserState pstate, std::string n)
  : Selector(pstate), ns_(""), name_(n), has_ns_(false)
  {
    size_t pos = n.find('|');
    // found some namespace
    if (pos != std::string::npos) {
      has_ns_ = true;
      ns_ = n.substr(0, pos);
      name_ = n.substr(pos + 1);
    }
  }
  Simple_Selector::Simple_Selector(const Simple_Selector* ptr)
  : Selector(ptr),
    ns_(ptr->ns_),
    name_(ptr->name_),
    has_ns_(ptr->has_ns_)
  { }

  std::string Simple_Selector::ns_name() const
  {
    std::string name("");
    if (has_ns_)
      name += ns_ + "|";
    return name + name_;
  }

  size_t Simple_Selector::hash() const
  {
    if (hash_ == 0) {
      hash_combine(hash_, std::hash<int>()(SELECTOR));
      hash_combine(hash_, std::hash<int>()(simple_type()));
      if (!name_.empty()) hash_combine(hash_, std::hash<std::string>()(name()));
      if (has_ns_) hash_combine(hash_, std::hash<std::string>()(ns()));
    }
    return hash_;
  }

  bool Simple_Selector::empty() const {
    return ns().empty() && name().empty();
  }

  // namespace compare functions
  bool Simple_Selector::is_ns_eq(const Simple_Selector& r) const
  {
    return has_ns_ == r.has_ns_ && ns_ == r.ns_;
  }

  // namespace query functions
  bool Simple_Selector::is_universal_ns() const
  {
    return has_ns_ && ns_ == "*";
  }

  bool Simple_Selector::is_empty_ns() const
  {
    return !has_ns_ || ns_ == "";
  }

  bool Simple_Selector::has_empty_ns() const
  {
    return has_ns_ && ns_ == "";
  }

  bool Simple_Selector::has_qualified_ns() const
  {
    return has_ns_ && ns_ != "" && ns_ != "*";
  }

  // name query functions
  bool Simple_Selector::is_universal() const
  {
    return name_ == "*";
  }

  bool Simple_Selector::has_placeholder()
  {
    return false;
  }

  bool Simple_Selector::has_parent_ref() const
  {
    return false;
  };

  bool Simple_Selector::has_real_parent_ref() const
  {
    return false;
  };

  bool Simple_Selector::is_pseudo_element() const
  {
    return false;
  }

  bool Simple_Selector::isSuperselectorOf(const CompoundSelector* sub) const
  {
    return false;
  }

  bool CompoundSelector::isSuperselectorOf(const CompoundSelector* sub, std::string wrapped) const
  {
    CompoundSelector_Obj rhs2 = const_cast<CompoundSelector*>(sub);
    CompoundSelector_Obj lhs2 = const_cast<CompoundSelector*>(this);
    rhs2.detach(); lhs2.detach();
    return compoundIsSuperselector(lhs2, rhs2, {});
  }

  bool CompoundSelector::isSuperselectorOf(const ComplexSelector* sub, std::string wrapped) const
  {
    return false;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Parent_Selector::Parent_Selector(ParserState pstate, bool r)
  : Simple_Selector(pstate, "&"), real_(r)
  { simple_type(PARENT_SEL); }
  Parent_Selector::Parent_Selector(const Parent_Selector* ptr)
  : Simple_Selector(ptr), real_(ptr->real_)
  { simple_type(PARENT_SEL); }

  bool Parent_Selector::has_parent_ref() const
  {
    return true;
  };

  bool Parent_Selector::has_real_parent_ref() const
  {
    return real();
  };

  unsigned long Parent_Selector::specificity() const
  {
    return 0;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Placeholder_Selector::Placeholder_Selector(ParserState pstate, std::string n)
  : Simple_Selector(pstate, n)
  { simple_type(PLACEHOLDER_SEL); }
  Placeholder_Selector::Placeholder_Selector(const Placeholder_Selector* ptr)
  : Simple_Selector(ptr)
  { simple_type(PLACEHOLDER_SEL); }
  unsigned long Placeholder_Selector::specificity() const
  {
    return Constants::Specificity_Base;
  }
  bool Placeholder_Selector::has_placeholder() {
    return true;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Type_Selector::Type_Selector(ParserState pstate, std::string n)
  : Simple_Selector(pstate, n)
  { simple_type(TYPE_SEL); }
  Type_Selector::Type_Selector(const Type_Selector* ptr)
  : Simple_Selector(ptr)
  { simple_type(TYPE_SEL); }

  unsigned long Type_Selector::specificity() const
  {
    if (name() == "*") return 0;
    else return Constants::Specificity_Element;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Class_Selector::Class_Selector(ParserState pstate, std::string n)
  : Simple_Selector(pstate, n)
  { simple_type(CLASS_SEL); }
  Class_Selector::Class_Selector(const Class_Selector* ptr)
  : Simple_Selector(ptr)
  { simple_type(CLASS_SEL); }

  unsigned long Class_Selector::specificity() const
  {
    return Constants::Specificity_Class;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Id_Selector::Id_Selector(ParserState pstate, std::string n)
  : Simple_Selector(pstate, n)
  { simple_type(ID_SEL); }
  Id_Selector::Id_Selector(const Id_Selector* ptr)
  : Simple_Selector(ptr)
  { simple_type(ID_SEL); }

  unsigned long Id_Selector::specificity() const
  {
    return Constants::Specificity_ID;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Attribute_Selector::Attribute_Selector(ParserState pstate, std::string n, std::string m, String_Obj v, char o)
  : Simple_Selector(pstate, n), matcher_(m), value_(v), modifier_(o)
  { simple_type(ATTRIBUTE_SEL); }
  Attribute_Selector::Attribute_Selector(const Attribute_Selector* ptr)
  : Simple_Selector(ptr),
    matcher_(ptr->matcher_),
    value_(ptr->value_),
    modifier_(ptr->modifier_)
  { simple_type(ATTRIBUTE_SEL); }

  size_t Attribute_Selector::hash() const
  {
    if (hash_ == 0) {
      hash_combine(hash_, Simple_Selector::hash());
      hash_combine(hash_, std::hash<std::string>()(matcher()));
      if (value_) hash_combine(hash_, value_->hash());
    }
    return hash_;
  }

  unsigned long Attribute_Selector::specificity() const
  {
    return Constants::Specificity_Attr;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  bool isFakePseudoElement(const std::string& name)
  {
    return equalsLiteral("after", name)
      || equalsLiteral("before", name)
      || equalsLiteral("first-line", name)
      || equalsLiteral("first-letter", name);
  }

  Pseudo_Selector::Pseudo_Selector(ParserState pstate, std::string name, bool element)
  : Simple_Selector(pstate, name),
    normalized_(normName(name)),
    expression_({}),
    selector2_({}),
    // selector_({}),
    isSyntacticClass_(!element),
    isClass_(!element && !isFakePseudoElement(normalized_))
  { simple_type(PSEUDO_SEL); }
  Pseudo_Selector::Pseudo_Selector(const Pseudo_Selector* ptr)
  : Simple_Selector(ptr),
    normalized_(ptr->normalized()),
    expression_(ptr->expression()),
    selector2_(ptr->selector2()),
    // selector_(ptr->selector()),
    isSyntacticClass_(ptr->isSyntacticClass()),
    isClass_(ptr->isClass())
  { simple_type(PSEUDO_SEL); }

  // A pseudo-element is made of two colons (::) followed by the name.
  // The `::` notation is introduced by the current document in order to
  // establish a discrimination between pseudo-classes and pseudo-elements.
  // For compatibility with existing style sheets, user agents must also
  // accept the previous one-colon notation for pseudo-elements introduced
  // in CSS levels 1 and 2 (namely, :first-line, :first-letter, :before and
  // :after). This compatibility is not allowed for the new pseudo-elements
  // introduced in this specification.
  bool Pseudo_Selector::is_pseudo_element() const
  {
    return isElement();
  }

  size_t Pseudo_Selector::hash() const
  {
    if (hash_ == 0) {
      hash_combine(hash_, Simple_Selector::hash());
      if (selector2_) hash_combine(hash_, selector2_->hash());
      if (expression_) hash_combine(hash_, expression_->hash());
    }
    return hash_;
  }

  unsigned long Pseudo_Selector::specificity() const
  {
    if (is_pseudo_element())
      return Constants::Specificity_Element;
    return Constants::Specificity_Pseudo;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  bool Pseudo_Selector::isSuperselectorOf(const Pseudo_Selector* sub) const
  {
    if (this->name() != sub->name()) return false;
    if (this->name() == ":current") return false;
    SelectorList_Obj lhs_list = toSelectorList(selector2());
    SelectorList_Obj rhs_list = toSelectorList(sub->selector2());
    return lhs_list && rhs_list && lhs_list->isSuperselectorOf(rhs_list);
    coreError("is_superselector expected a Selector_List", sub->pstate());
    return false;
  }

  // Selectors inside the negation pseudo-class are counted like any
  // other, but the negation itself does not count as a pseudo-class.

  void Pseudo_Selector::cloneChildren()
  {
    if (selector2().isNull()) selector2({});
    else selector2(SASS_MEMORY_CLONE(selector2()));
  }

  bool Pseudo_Selector::has_parent_ref() const {
    if (!selector2()) return false;
    return selector2()->has_parent_ref();
  }

  bool Pseudo_Selector::has_real_parent_ref() const {
    if (!selector2()) return false;
    return selector2()->has_real_parent_ref();
  }
/*
  unsigned long Wrapped_Selector::specificity() const
  {
    return selector_ ? selector_->specificity() : 0;
  }
  
  bool Wrapped_Selector::find ( bool (*f)(AST_Node_Obj) )
  {
    // check children first
    if (selector_) {
      if (selector_->find(f)) return true;
    }
    // execute last
    return f(this);
  }
  */
  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Compound_Selector::Compound_Selector(ParserState pstate, size_t s)
  : Selector(pstate),
    Vectorized<Simple_Selector_Obj, Compound_Selector>(s),
    extended_(false),
    has_parent_reference_(false),
    hasPostLineBreak_(false)
  { }

  Compound_Selector::Compound_Selector(const Compound_Selector* ptr)
  : Selector(ptr),
    Vectorized<Simple_Selector_Obj, Compound_Selector>(*ptr),
    extended_(ptr->extended_),
    has_parent_reference_(ptr->has_parent_reference_),
    hasPostLineBreak_(ptr->hasPostLineBreak())
  { }

  bool Compound_Selector::contains_placeholder() {
    for (size_t i = 0, L = length(); i < L; ++i) {
      if ((*this)[i]->has_placeholder()) return true;
    }
    return false;
  };
  bool CompoundSelector::contains_placeholder() const {
    for (size_t i = 0, L = length(); i < L; ++i) {
      if ((*this)[i]->has_placeholder()) return true;
    }
    return false;
  };
  bool ComplexSelector::contains_placeholder() const {
    for (size_t i = 0, L = length(); i < L; ++i) {
      if ((*this)[i]->has_placeholder()) return true;
    }
    return false;
  };

  size_t ComplexSelector::hash() const
  {
    if (Selector::hash_ == 0) {
      hash_combine(Selector::hash_, Vectorized::hash());
      hash_combine(Selector::hash_, chroots_);
    }
    return Selector::hash_;
  }

  size_t CompoundSelector::hash() const
  {
    if (Selector::hash_ == 0) {
      hash_combine(Selector::hash_, Vectorized::hash());
      hash_combine(Selector::hash_, hasRealParent_);
    }
    return Selector::hash_;
  }

  bool ComplexSelector::has_placeholder() const {
    for (size_t i = 0, L = length(); i < L; ++i) {
      if ((*this)[i]->has_placeholder()) return true;
    }
    return false;
  };

  void Compound_Selector::cloneChildren()
  {
    for (size_t i = 0, l = length(); i < l; i++) {
      at(i) = SASS_MEMORY_CLONE(at(i));
    }
  }

  bool Compound_Selector::find ( bool (*f)(AST_Node_Obj) )
  {
    // check children first
    for (Simple_Selector_Obj sel : elements()) {
      if (sel->find(f)) return true;
    }
    // execute last
    return f(this);
  }

  bool Compound_Selector::has_parent_ref() const
  {
    for (Simple_Selector_Obj s : *this) {
      if (s && s->has_parent_ref()) return true;
    }
    return false;
  }
  bool CompoundSelector::has_parent_ref() const
  {
    if (hasRealParent()) return true;
    for (Simple_Selector_Obj s : *this) {
      if (s && s->has_parent_ref()) return true;
    }
    return false;
  }

  bool Compound_Selector::has_real_parent_ref() const
  {
    for (Simple_Selector_Obj s : *this) {
      if (s && s->has_real_parent_ref()) return true;
    }
    return false;
  }
  bool CompoundSelector::has_real_parent_ref() const
  {
    if (hasRealParent()) return true;
    for (Simple_Selector_Obj s : *this) {
      if (s && s->has_real_parent_ref()) return true;
    }
    return false;
  }

  bool Compound_Selector::is_universal() const
  {
    return length() == 1 && (*this)[0]->is_universal();
  }

  // create complex selector (ancestor of) from compound selector
  Complex_Selector_Obj Compound_Selector::to_complex()
  {
    // create an intermediate complex selector
    return SASS_MEMORY_NEW(Complex_Selector,
                           pstate(),
                           Complex_Selector::ANCESTOR_OF,
                           this,
                           {});
  }

  Simple_Selector* Compound_Selector::base() const {
    if (length() == 0) return 0;
    // ToDo: why is this needed?
    if (Cast<Type_Selector>((*this)[0]))
      return (*this)[0];
    return 0;
  }

  size_t Compound_Selector::hash() const
  {
    if (Selector::hash_ == 0) {
      hash_combine(Selector::hash_, std::hash<int>()(SELECTOR));
      if (length()) hash_combine(Selector::hash_, Vectorized::hash());
    }
    return Selector::hash_;
  }

  unsigned long Compound_Selector::specificity() const
  {
    int sum = 0;
    for (size_t i = 0, L = length(); i < L; ++i)
    { sum += (*this)[i]->specificity(); }
    return sum;
  }

  bool Compound_Selector::has_placeholder()
  {
    if (length() == 0) return false;
    if (Simple_Selector_Obj ss = elements().front()) {
      if (ss->has_placeholder()) return true;
    }
    return false;
  }
  bool CompoundSelector::has_placeholder() const
  {
    if (length() == 0) return false;
    if (Simple_Selector_Obj ss = elements().front()) {
      if (ss->has_placeholder()) return true;
    }
    return false;
  }

  bool Compound_Selector::is_empty_reference()
  {
    return length() == 1 &&
            Cast<Parent_Selector>((*this)[0]);
  }

  void Compound_Selector::append(Simple_Selector_Obj element)
  {
    Vectorized<Simple_Selector_Obj, Compound_Selector>::append(element);
    pstate_.offset += element->pstate().offset;
  }

  Compound_Selector* Compound_Selector::minus(Compound_Selector* rhs)
  {
    Compound_Selector* result = SASS_MEMORY_NEW(Compound_Selector, pstate());
    // result->has_parent_reference(has_parent_reference());

    // not very efficient because it needs to preserve order
    for (size_t i = 0, L = length(); i < L; ++i)
    {
      bool found = false;
      for (size_t j = 0, M = rhs->length(); j < M; ++j)
      {
        if (*get(i) == *rhs->get(j))
        {
          found = true;
          break;
        }
      }
      if (!found) result->append(get(i));
    }

    return result;
  }

  void Compound_Selector::mergeSources(ComplexSelectorSet& sources)
  {
    for (ComplexSelectorSet::iterator iterator = sources.begin(), endIterator = sources.end(); iterator != endIterator; ++iterator) {
      this->sources_.insert(SASS_MEMORY_CLONE(*iterator));
    }
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Complex_Selector::Complex_Selector(ParserState pstate,
                    Combinator c,
                    Compound_Selector_Obj h,
                    Complex_Selector_Obj t,
                    String_Obj r)
  : Selector(pstate),
    combinator_(c),
    head_(h), tail_(t),
    reference_(r),
    hasPreLineFeed_(false)
  {}
  Complex_Selector::Complex_Selector(const Complex_Selector* ptr)
  : Selector(ptr),
    combinator_(ptr->combinator_),
    head_(ptr->head_), tail_(ptr->tail_),
    reference_(ptr->reference_),
    hasPreLineFeed_(ptr->hasPreLineFeed())
  {}

  bool Complex_Selector::empty() const {
    return (!tail() || tail()->empty())
      && (!head() || head()->empty())
      && combinator_ == ANCESTOR_OF;
  }

  Complex_Selector_Obj Complex_Selector::skip_empty_reference()
  {
    if ((!head_ || !head_->length() || head_->is_empty_reference()) &&
        combinator() == Combinator::ANCESTOR_OF)
    {
      if (!tail_) return {};
      tail_->hasPreLineFeed_ = this->hasPreLineFeed_;
      // tail_->has_line_break_ = this->has_line_break_;
      return tail_->skip_empty_reference();
    }
    return this;
  }

  bool Complex_Selector::is_empty_ancestor() const
  {
    return (!head() || head()->length() == 0) &&
            combinator() == Combinator::ANCESTOR_OF;
  }

  size_t Complex_Selector::hash() const
  {
    if (hash_ == 0) {
      if (head_) {
        hash_combine(hash_, head_->hash());
      } else {
        hash_combine(hash_, std::hash<int>()(SELECTOR));
      }
      if (tail_) hash_combine(hash_, tail_->hash());
      if (combinator_ != ANCESTOR_OF) hash_combine(hash_, std::hash<int>()(combinator_));
    }
    return hash_;
  }

  unsigned long Complex_Selector::specificity() const
  {
    int sum = 0;
    if (head()) sum += head()->specificity();
    if (tail()) sum += tail()->specificity();
    return sum;
  }

  void Complex_Selector::set_media_block(Media_Block* mb) {
    media_block(mb);
    if (tail_) tail_->set_media_block(mb);
    if (head_) head_->set_media_block(mb);
  }

  bool Complex_Selector::has_placeholder() {
    if (head_ && head_->has_placeholder()) return true;
    if (tail_ && tail_->has_placeholder()) return true;
    return false;
  }

  const ComplexSelectorSet Complex_Selector::sources()
  {
    //s = Set.new
    //seq.map {|sseq_or_op| s.merge sseq_or_op.sources if sseq_or_op.is_a?(SimpleSequence)}
    //s

    ComplexSelectorSet srcs;

    Compound_Selector_Obj pHead = head();
    Complex_Selector_Obj  pTail = tail();

    if (pHead) {
      const ComplexSelectorSet& headSources = pHead->sources();
      srcs.insert(headSources.begin(), headSources.end());
    }

    if (pTail) {
      const ComplexSelectorSet& tailSources = pTail->sources();
      srcs.insert(tailSources.begin(), tailSources.end());
    }

    return srcs;
  }

  void Complex_Selector::addSources(ComplexSelectorSet& sources)
  {
    // members.map! {|m| m.is_a?(SimpleSequence) ? m.with_more_sources(sources) : m}
    Complex_Selector* pIter = this;
    while (pIter) {
      Compound_Selector* pHead = pIter->head();

      if (pHead) {
        pHead->mergeSources(sources);
      }

      pIter = pIter->tail();
    }
  }

  void Complex_Selector::clearSources()
  {
    Complex_Selector* pIter = this;
    while (pIter) {
      Compound_Selector* pHead = pIter->head();

      if (pHead) {
        pHead->clearSources();
      }

      pIter = pIter->tail();
    }
  }

  bool Complex_Selector::find ( bool (*f)(AST_Node_Obj) )
  {
    // check children first
    if (head_ && head_->find(f)) return true;
    if (tail_ && tail_->find(f)) return true;
    // execute last
    return f(this);
  }

  bool ComplexSelector::has_parent_ref() const
  {
    if (!chroots()) return true;
    for (auto item : elements()) {
      if (item->has_parent_ref()) return true;
    }
    return false;
  }
  bool ComplexSelector::has_real_parent_ref() const
  {
    for (auto item : elements()) {
      if (item->has_real_parent_ref()) return true;
    }
    return false;
  }

  bool Complex_Selector::has_parent_ref() const
  {
    return (head() && head()->has_parent_ref()) ||
           (tail() && tail()->has_parent_ref());
  }

  bool Complex_Selector::has_real_parent_ref() const
  {
    return (head() && head()->has_real_parent_ref()) ||
           (tail() && tail()->has_real_parent_ref());
  }


  size_t Complex_Selector::length() const
  {
    // TODO: make this iterative
    if (!tail()) return 1;
    return 1 + tail()->length();
  }


  template <class T>
  std::vector<std::vector<T>>
  permutate(std::vector<std::vector<T>> in) {

    size_t L = in.size();
    size_t n = in.size() - 1;
    size_t* state = new size_t[L];
    std::vector< std::vector<T>> out;

    // First initialize all states for every permutation group
    for (size_t i = 0; i < L; i += 1) {
      if (in[i].size() == 0) return {};
      state[i] = in[i].size() - 1;
    }

    while (true) {
      /*
      std::cerr << "PERM: ";
      for (size_t p = 0; p < L; p++)
      { std::cerr << state[p] << " "; }
      std::cerr << "\n";
      */
      std::vector<T> perm;
      // Create one permutation for state
      for (size_t i = 0; i < L; i += 1) {
        perm.push_back(in.at(i).at(in[i].size() - state[i] - 1));
      }
      // Current group finished
      if (state[n] == 0) {
        // Find position of next decrement
        while (n > 0 && state[--n] == 0)
        {

        }
        // Check for end condition
        if (state[n] != 0) {
          // Decrease next on the left side
          state[n] -= 1;
          // Reset all counters to the right
          for (size_t p = n + 1; p < L; p += 1) {
            state[p] = in[p].size() - 1;
          }
          // Restart from end
          n = L - 1;
        }
        else {
          out.push_back(perm);
          break;
        }
      }
      else {
        state[n] -= 1;
      }
      out.push_back(perm);
    }

    delete[] state;
    return out;
  }

  void debug_groups(std::vector<std::vector<std::vector<CompoundOrCombinator_Obj>>> groups) {
    for (size_t i = 0; i < groups.size(); i += 1) {
      std::cerr << i << ": [";
      for (auto items : groups[i]) {
        std::cerr << " { ";
        for (auto item : items) {
          std::cerr << item->to_string() << " ";
        }
        std::cerr << " } ";
      }
      std::cerr << "]\n";
    }
  }

  std::vector<ComplexSelector_Obj>
  CompoundSelector::resolve_parent_refs(SelectorStack2 pstack, Backtraces& traces, bool implicit_parent)
  {

    auto parent = pstack.back();
    std::vector<ComplexSelector_Obj> rv;

    for (Simple_Selector_Obj ss : elements()) {
      if (Pseudo_Selector * ws = Cast<Pseudo_Selector>(ss)) {
        if (SelectorList * asd = Cast<SelectorList>(ws->selector2())) {
          if (parent) {
            // SelectorList_Obj asd = sl->toSelList();
            SelectorList_Obj qwe = asd->resolve_parent_refs(pstack, traces, implicit_parent);
            ws->selector2(qwe);
          }
        }
      }

    }


    // Mix with parents from stack
    if (hasRealParent()) {
      if (parent.isNull() && has_real_parent_ref()) {
        // it turns out that real parent references reach
        // across @at-root rules, which comes unexpected
        int i = pstack.size() - 1;
        while (!parent && i > -1) {
          auto asd = pstack.at(i--);
          if (asd.isNull()) parent = {};
          else parent = asd;
        }
      }

      if (parent.isNull()) {
        auto sel = SASS_MEMORY_NEW(ComplexSelector, pstate());
        sel->append(this);
        return { sel };
      }
      else {
        for (auto complex : parent->elements()) {
          // The parent complex selector has a compound selector
          if (CompoundSelector_Obj tail = Cast<CompoundSelector>(complex->last())) {
            // Create a copy to alter it
            complex = complex->copy();
            tail = tail->copy();

            // CHeck if we can merge front with back
            if (length() > 0 && tail->length() > 0) {
              Simple_Selector_Obj back = tail->last();
              Simple_Selector_Obj front = first();
              auto simple_back = Cast<Simple_Selector>(back);
              auto simple_front = Cast<Type_Selector>(front);
              if (simple_front && simple_back) {
/*
                if (last()->combinator() != ANCESTOR_OF && c != ANCESTOR_OF) {
                  traces.push_back(Backtrace(pstate()));
                  throw Exception::InvalidParent(this, traces, ss);
                }
                */
                simple_back = simple_back->copy();
                auto name = simple_back->name();
                name += simple_front->name();
                simple_back->name(name);
                tail->elements().back() = simple_back;
                for (size_t i = 1; i < length(); i += 1) {
                  tail->append(at(i));
                }
              }
              else {
                tail->concat(this);
              }
            }
            else {
              tail->concat(this);
            }


            // Update the 
            complex->elements().back() = tail;
            // Append to results
            rv.push_back(complex);
          }
          else {
            // Can't insert parent that ends with a combinator
            // where the parent selector is followed by something
            if (parent && length() > 0) {
              throw Exception::InvalidParent(parent, traces, this);
            }
            /*if (length() > 0 && tail->length() > 0) {
              Simple_Selector_Obj back = tail->last();
            }
            std::cerr << "COMBINE\n";*/
            // Create a copy to alter it
            complex = complex->copy();
            // Just append ourself
            complex->append(this);
            // Append to results
            rv.push_back(complex);
          }
        }
      }
    }

    // No parents
    else {
      // Create a new wrapper to wrap ourself
      auto complex = SASS_MEMORY_NEW(ComplexSelector, pstate());
      // Just append ourself
      complex->append(this);
      // Append to results
      rv.push_back(complex);
    }

    return rv;

  }
  
    /* better return std::vector? only - is empty container anyway? */
  SelectorList* ComplexSelector::resolve_parent_refs(SelectorStack2 pstack, Backtraces& traces, bool implicit_parent)
  {

    std::vector<std::vector<ComplexSelector_Obj>> vars;

    // debug_ast(this, "In: ");


    auto parent = pstack.back();
    if (!chroots() && parent) {

      // CHeck why this makes it behave to at-root!?
      if (!has_real_parent_ref() && !implicit_parent) {
        SelectorList* retval = SASS_MEMORY_NEW(SelectorList, pstate(), 1);
        retval->append(this);
        return retval;
      }

      // std::cerr << "DOING IT " << has_real_parent_ref() << "\n";
      vars.push_back(parent->elements());
    }

    for (auto sel : elements()) {
      if (CompoundSelector_Obj comp = Cast<CompoundSelector>(sel)) {
        auto asd = comp->resolve_parent_refs(pstack, traces, implicit_parent);
        if (asd.size() > 0) vars.push_back(asd);
      }
      else {
        // ToDo: merge together sequences whenever possible
        auto cont = SASS_MEMORY_NEW(ComplexSelector, pstate());
        cont->append(sel);
        vars.push_back({ cont });
      }
    }
    /*
    for (auto a : vars) {
      std::cerr << "[ ";
      for (auto b : a) {
        std::cerr << " { " << b->to_string() << " }";
      }
      std::cerr << " ]\n";
    }
    */
    // Need complex selectors to preserve linefeeds
    std::vector<std::vector<ComplexSelector_Obj>> res = permutate(vars);

    auto lst = SASS_MEMORY_NEW(SelectorList, pstate());
    for (auto items : res) {
      if (items.size() > 0) {
        ComplexSelector_Obj first = items[0]->copy();
        first->hasPreLineFeed(first->hasPreLineFeed() || (!has_real_parent_ref() && hasPreLineFeed()));
        // if (has_real_parent_ref()) first->has_line_feed(false);
        // first->has_line_break(first->has_line_break() || has_line_break());
        first->chroots(true); // has been resolved by now
        for (size_t i = 1; i < items.size(); i += 1) {
          first->concat(items[i]);
        }
        lst->append(first);
      }
    }
    // debug_ast(lst);
       return lst;


  }

  SelectorList* SelectorList::resolve_parent_refs(SelectorStack2 pstack, Backtraces& traces, bool implicit_parent)
  {
    if (!has_parent_ref()) {
      // std::cerr << "ret this\n";
      return this;
    }
    SelectorList* rv = SASS_MEMORY_NEW(SelectorList, pstate());
    for (auto sel : elements()) {
      SelectorList_Obj rvs = sel->resolve_parent_refs(pstack, traces, implicit_parent);
      rv->concat(rvs);
    }
    return rv;
  }

  // return the last tail that is defined
  const Complex_Selector* Complex_Selector::first() const
  {
    // declare variables used in loop
    const Complex_Selector* cur = this;
    const Compound_Selector* head;
    // processing loop
    while (cur)
    {
      // get the head
      head = cur->head_.ptr();
      // abort (and return) if it is not a parent selector
      if (!head || head->length() != 1 || !Cast<Parent_Selector>((*head)[0])) {
        break;
      }
      // advance to next
      cur = cur->tail_;
    }
    // result
    return cur;
  }

  Complex_Selector* Complex_Selector::mutable_first()
  {
    return const_cast<Complex_Selector*>(first());
  }

  // return the last tail that is defined
  const Complex_Selector* Complex_Selector::last() const
  {
    const Complex_Selector* cur = this;
    const Complex_Selector* nxt = cur;
    // loop until last
    while (nxt) {
      cur = nxt;
      nxt = cur->tail_.ptr();
    }
    return cur;
  }

  Complex_Selector* Complex_Selector::mutable_last()
  {
    return const_cast<Complex_Selector*>(last());
  }

  Complex_Selector::Combinator Complex_Selector::clear_innermost()
  {
    Combinator c;
    if (!tail() || tail()->tail() == nullptr)
    { c = combinator(); combinator(ANCESTOR_OF); tail({}); }
    else
    { c = tail_->clear_innermost(); }
    return c;
  }

  void Complex_Selector::set_innermost(Complex_Selector_Obj val, Combinator c)
  {
    if (!tail_)
    { tail_ = val; combinator(c); }
    else
    { tail_->set_innermost(val, c); }
  }

  void Complex_Selector::cloneChildren()
  {
    if (head()) head(SASS_MEMORY_CLONE(head()));
    if (tail()) tail(SASS_MEMORY_CLONE(tail()));
  }


  bool Complex_Selector::is_superselector_of(const Complex_Selector* rhs, std::string wrapping) const
  {
    Complex_Selector_Obj rhs2 = const_cast<Complex_Selector*>(rhs);
    Complex_Selector_Obj lhs2 = const_cast<Complex_Selector*>(this);
    ComplexSelector_Obj rhs3 = toComplexSelector(rhs2);
    ComplexSelector_Obj lhs3 = toComplexSelector(lhs2);
    return lhs3->isSuperselectorOf(rhs3);
  }

  bool ComplexSelector::isSuperselectorOf(const CompoundSelector* sub) const
  {
    std::cerr << "Implement cplx comp sup\n";
    return false;
  }

  bool ComplexSelector::isSuperselectorOf(const SelectorList* sub) const
  {
    // Check every rhs selector against left hand list
    for (size_t i = 0, L = sub->length(); i < L; ++i) {
      if (!isSuperselectorOf((*sub)[i])) return false;
    }
    return true;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Selector_List::Selector_List(ParserState pstate, size_t s)
  : Selector(pstate),
    Vectorized<Complex_Selector_Obj, Selector_List>(s),
    schema_({})
  { }
  Selector_List::Selector_List(const Selector_List* ptr)
  : Selector(ptr),
    Vectorized<Complex_Selector_Obj, Selector_List>(*ptr),
    schema_(ptr->schema_)
  { }

  bool Selector_List::find ( bool (*f)(AST_Node_Obj) )
  {
    // check children first
    for (Complex_Selector_Obj sel : elements()) {
      if (sel->find(f)) return true;
    }
    // execute last
    return f(this);
  }

  Selector_List_Obj Selector_List::eval(Eval& eval)
  {
    Selector_List_Obj list = schema() ?
      eval(schema())
      : eval(this);
    list->schema(schema());
    return list;
  }

  Selector_List_Obj Selector_Schema::eval(Eval& eval)
  {
    return eval(this);
  }

  SelectorList_Obj SelectorList::eval(Eval& eval)
  {
    /*
    SelectorList_Obj list = schema() ?
      eval(schema()) : eval(this);
    list->schema(schema());
    return list;
    */
    return eval(this);
  }

  void Selector_List::cloneChildren()
  {
    for (size_t i = 0, l = length(); i < l; i++) {
      at(i) = SASS_MEMORY_CLONE(at(i));
    }
  }

  // remove parent selector references
  // basically unwraps parsed selectors
  void SelectorList::remove_parent_selectors()
  {
    // Check every rhs selector against left hand list
    for (auto complex : elements()) {
      complex->chroots(true);
      for (auto sel : complex->elements()) {
        if (auto compound = Cast<CompoundSelector>(sel)) {
          compound->hasRealParent(false);
        }
      }
    }
  }

  // remove parent selector references
  // basically unwraps parsed selectors
  void Selector_List::remove_parent_selectors()
  {
    // Check every rhs selector against left hand list
    for(size_t i = 0, L = length(); i < L; ++i) {
      if (!(*this)[i]->head()) continue;
      if ((*this)[i]->head()->is_empty_reference()) {
        // simply move to the next tail if we have "no" combinator
        if ((*this)[i]->combinator() == Complex_Selector::ANCESTOR_OF) {
          if ((*this)[i]->tail()) {
            if ((*this)[i]->hasPreLineFeed()) {
              (*this)[i]->tail()->hasPreLineFeed(true);
            }
            (*this)[i] = (*this)[i]->tail();
          }
        }
        // otherwise remove the first item from head
        else {
          (*this)[i]->head()->erase((*this)[i]->head()->begin());
        }
      }
    }
  }

  bool SelectorList::has_parent_ref() const
  {
    for (ComplexSelector_Obj s : elements()) {
      if (s && s->has_parent_ref()) return true;
    }
    return false;
  }
  bool Selector_List::has_parent_ref() const
  {
    for (Complex_Selector_Obj s : elements()) {
      if (s && s->has_parent_ref()) return true;
    }
    return false;
  }

  bool Selector_List::has_real_parent_ref() const
  {
    for (Complex_Selector_Obj s : elements()) {
      if (s && s->has_real_parent_ref()) return true;
    }
    return false;
  }
  bool SelectorList::has_real_parent_ref() const
  {
    for (ComplexSelector_Obj s : elements()) {
      if (s && s->has_real_parent_ref()) return true;
    }
    return false;
  }

  void Selector_List::adjust_after_pushing(Complex_Selector_Obj c)
  {
    // if (c->has_reference())   has_reference(true);
  }

  void Selector_List::populate_extends(Selector_List_Obj extendee, Subset_Map& extends)
  {

    Selector_List* extender = this;
    for (auto complex_sel : extendee->elements()) {
      Complex_Selector_Obj c = complex_sel;


      // Ignore any parent selectors, until we find the first non Selectorerence head
      Compound_Selector_Obj compound_sel = c->head();
      Complex_Selector_Obj pIter = complex_sel;
      while (pIter) {
        Compound_Selector_Obj pHead = pIter->head();
        if (pHead && Cast<Parent_Selector>(pHead->elements()[0]) == NULL) {
          compound_sel = pHead;
          break;
        }

        pIter = pIter->tail();
      }

      if (!pIter->head() || pIter->tail()) {
        coreError("nested selectors may not be extended", c->pstate());
      }

      compound_sel->is_optional(extendee->is_optional());

      for (size_t i = 0, L = extender->length(); i < L; ++i) {
        // std::cerr << "BUT " << compound_sel->to_string() << "\n";
        extends.put(compound_sel, std::make_pair((*extender)[i], compound_sel));
      }
    }
  };

  void SelectorList::populate_extends(SelectorList_Obj extendee, SubsetMap2& extends)
  {

    auto extender = this;
    for (auto complex_sel : extendee->elements()) {
      ComplexSelector_Obj c = complex_sel;


      // Ignore any parent selectors, until we find the first non Selectorerence head
      CompoundSelector_Obj compound_sel = c->first();

      /*
      if (!pIter->head() || pIter->tail()) {
        coreError("nested selectors may not be extended", c->pstate());
      }
      */

      compound_sel->is_optional(extendee->is_optional());

      for (size_t i = 0, L = extender->length(); i < L; ++i) {
        ComplexSelector_Obj lhs = (*extender)[i];
        // std::cerr << "PUT " << compound_sel->to_string() << "\n";
        extends.put(compound_sel, std::make_pair(lhs, compound_sel));
      }
    }
  };

  size_t Selector_List::hash() const
  {
    if (Selector::hash_ == 0) {
      if (empty()) {
        hash_combine(Selector::hash_, std::hash<int>()(SELECTOR));
      } else {
        hash_combine(Selector::hash_, Vectorized::hash());
      }
    }
    return Selector::hash_;
  }

  unsigned long Selector_List::specificity() const
  {
    unsigned long sum = 0;
    unsigned long specificity;
    for (size_t i = 0, L = length(); i < L; ++i)
    {
      specificity = (*this)[i]->specificity();
      if (sum < specificity) sum = specificity;
    }
    return sum;
  }

  void Selector_List::set_media_block(Media_Block* mb)
  {
    media_block(mb);
    for (Complex_Selector_Obj cs : elements()) {
      cs->set_media_block(mb);
    }
  }

  bool Selector_List::has_placeholder()
  {
    for (Complex_Selector_Obj cs : elements()) {
      if (cs->has_placeholder()) return true;
    }
    return false;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  SelectorList::SelectorList(ParserState pstate, size_t s)
  : Selector(pstate),
    Vectorized<ComplexSelector_Obj, SelectorList>(s),
    schemaOnlyToCopy_()
  { }
  SelectorList::SelectorList(const SelectorList* ptr)
    : Selector(ptr),
    Vectorized<ComplexSelector_Obj, SelectorList>(*ptr),
    schemaOnlyToCopy_(ptr->schemaOnlyToCopy_)
  { }

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

  Selector_List_Obj SelectorList::toSelectorList() const {
    Selector_List_Obj list = SASS_MEMORY_NEW(Selector_List, pstate());
    // optional only used for extend?
    list->pstate(pstate());
    list->schema(schemaOnlyToCopy());
    list->is_optional(is_optional());
    list->media_block(media_block());
    // check what we need to preserve
    // list->has_line_feed(has_line_feed());
    // list->has_line_break(has_line_break());
    for (auto& item : elements()) {
      list->append(item->toComplexSelector());
    }
    return list;
  }

  SelectorList_Obj Selector_List::toSelList() const {
    SelectorList_Obj list = SASS_MEMORY_NEW(SelectorList, pstate());
    // optional only used for extend?
    list->pstate(pstate());
    list->is_optional(is_optional());
    list->schemaOnlyToCopy(schema());
    list->media_block(media_block());
    // check what we need to preserve
    // list->has_line_feed(has_line_feed());
    // list->has_line_break(has_line_break());
    // remove_parent_selectors();
    for (auto& item : elements()) {
      list->append(item->toCplxSelector());
    }
    return list;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  ComplexSelector::ComplexSelector(ParserState pstate)
  : Selector(pstate),
    Vectorized<CompoundOrCombinator_Obj, ComplexSelector>(),
    chroots_(false),
    hasPreLineFeed_(false)
  {
  }
  ComplexSelector::ComplexSelector(const ComplexSelector* ptr)
  : Selector(ptr),
    Vectorized<CompoundOrCombinator_Obj, ComplexSelector>(*ptr),
    chroots_(ptr->chroots()),
    hasPreLineFeed_(ptr->hasPreLineFeed())
  {
  }

  void ComplexSelector::cloneChildren()
  {
    /* for (size_t i = 0, l = length(); i < l; i++) {
      at(i) = SASS_MEMORY_CLONE(at(i));
    }*/
  }

  unsigned long ComplexSelector::specificity() const
  {
    int sum = 0;
    for (auto component : elements()) {
      sum += component->specificity();
    }
    return sum;
  }

  Complex_Selector_Obj ComplexSelector::toComplexSelector() {
    size_t count = 0;
    std::vector<CompoundOrCombinator_Obj> els(elements());

    Complex_Selector_Obj sel;
    Complex_Selector_Obj tail;

    while (count < els.size()) {

      Complex_Selector_Obj cur;

      // check if we have only one left
      if (els.size() == count + 1) {
#ifdef DEBUGSEL
        std::cerr << "have one item left\n";
#endif

        if (SelectorCombinator_Obj combinator = Cast<SelectorCombinator>(els[count])) {
          cur = SASS_MEMORY_NEW(Complex_Selector, pstate(), combinator->toComplexCombinator());
#ifdef DEBUGSEL
          std::cerr << "have only a combinator [" << cur->to_string() << "]\n";
#endif
        }
        else if (CompoundSelector_Obj compound = Cast<CompoundSelector>(els[count])) {
          cur = SASS_MEMORY_NEW(Complex_Selector, pstate(),
            Complex_Selector::ANCESTOR_OF, compound->toCompoundSelector());
#ifdef DEBUGSEL
          std::cerr << "have only a compound [" << cur->to_string() << "]\n";
#endif
        }

        count += 1;

      }
      // we have at least two
      else {
#ifdef DEBUGSEL
      std::cerr << "have two or more items\n";
#endif

      // Check if we are starting with a combinator
      if (CompoundSelector_Obj compound = Cast<CompoundSelector>(els[count])) {
        if (SelectorCombinator_Obj combinator = Cast<SelectorCombinator>(els[count + 1])) {
          // Check if we are followed by a compound selector
          cur = SASS_MEMORY_NEW(Complex_Selector, pstate(),
            combinator->toComplexCombinator(), compound->toCompoundSelector());
#ifdef DEBUGSEL
          std::cerr << "have compound + combinator [" << cur->to_string() << "]\n";
#endif
          count += 2;
        }
        else {
          cur = SASS_MEMORY_NEW(Complex_Selector, pstate(),
            Complex_Selector::ANCESTOR_OF, compound->toCompoundSelector());
#ifdef DEBUGSEL
          std::cerr << "have only a compound [" << cur->to_string() << "]\n";
#endif
          count += 1;
        }
      }
      // we have a lonely combinator
      else if (SelectorCombinator_Obj combinator = Cast<SelectorCombinator>(els[count])) {
        cur = SASS_MEMORY_NEW(Complex_Selector, pstate(), combinator->toComplexCombinator());
#ifdef DEBUGSEL
        std::cerr << "have only a combinator [" << cur->to_string() << "]\n";
#endif
        count += 1;
      }
      else {
        std::cerr << "WHAT TYPE?\n";
      }

      }

      if (sel.isNull()) {
#ifdef DEBUGSEL
        std::cerr << "Have first selector\n";
#endif
        tail = cur;
        sel = cur;
      }
      else {
#ifdef DEBUGSEL
        std::cerr << "Append to the tail [" << sel->to_string() << "]\n";
#endif
        tail->tail(cur);
        tail = cur;
      }

    }

#ifdef DEBUGSEL
    std::cerr << "RETURN [" << sel->to_string() << "]\n";
#endif

    if (!chroots()) {

      // create the objects to wrap parent selector reference
      Compound_Selector_Obj head = SASS_MEMORY_NEW(Compound_Selector, pstate());
      Parent_Selector* parent = SASS_MEMORY_NEW(Parent_Selector, pstate(), false);
      parent->media_block(media_block());
      head->media_block(media_block());
      // add simple selector
      head->append(parent);
      // selector may not have any head yet
      if (!sel->head()) { sel->head(head); }
      // otherwise we need to create a new complex selector and set the old one as its tail
      else {
        sel = SASS_MEMORY_NEW(Complex_Selector, pstate(), Complex_Selector::ANCESTOR_OF, head, sel);
        sel->media_block(media_block());
      }

    }

    // check what we need to preserve
    sel->hasPreLineFeed(hasPreLineFeed());
    // sel->has_line_break(has_line_break());
    sel->media_block(media_block());



    return sel;
  }

  ComplexSelector_Obj Complex_Selector::toCplxSelector() {
    ComplexSelector_Obj sel = SASS_MEMORY_NEW(ComplexSelector, pstate());
    // std::cerr << "Convert [" << to_string() << "]\n";
    Complex_Selector_Obj cur = this;

    sel->chroots(true);

    if (cur->head() && cur->head()->length() == 1) {
      if (Parent_Selector_Obj par = Cast<Parent_Selector>(cur->head()->at(0))) {
        if (!par->real()) {
          if (cur->combinator() == Complex_Selector::Combinator::ANCESTOR_OF) {
            cur = cur->tail();
            sel->chroots(false);
          }
          else {
            cur = cur->copy();
            cur->head({});
            sel->chroots(false);
          }
        }
      }
    }

    while (cur) {

      if (cur->head() && !cur->head()->empty()) {
        sel->append(cur->head()->toCompSelector());
        // std::cerr << "Has head\n";
      }
      if (cur->combinator()) {
        // std::cerr << "Has combinator\n";
        switch (cur->combinator()) {
        case Complex_Selector::Combinator::PARENT_OF:
            sel->append(SASS_MEMORY_NEW(SelectorCombinator, pstate(), SelectorCombinator::Combinator::CHILD));
          break;
          case Complex_Selector::Combinator::ADJACENT_TO:
            sel->append(SASS_MEMORY_NEW(SelectorCombinator, pstate(), SelectorCombinator::Combinator::ADJACENT));
            break;
          case Complex_Selector::Combinator::PRECEDES:
            sel->append(SASS_MEMORY_NEW(SelectorCombinator, pstate(), SelectorCombinator::Combinator::GENERAL));
            break;
          case Complex_Selector::Combinator::REFERENCE: break;
          case Complex_Selector::Combinator::ANCESTOR_OF: break;
        }
      }

      cur = cur->tail();
    }

    // check what we need to preserve
    sel->hasPreLineFeed(hasPreLineFeed());
    // sel->has_line_break(has_line_break());
    sel->media_block(media_block());

    return sel;
  }

  Compound_Selector_Obj CompoundSelector::toCompoundSelector() {
    Compound_Selector_Obj sel = SASS_MEMORY_NEW(Compound_Selector, pstate());
    if (hasRealParent()) {
      // std::cerr << "HAVING A REAL PARENT\n";
      sel->append(SASS_MEMORY_NEW(Parent_Selector, pstate()));
    }
    // sel->has_line_feed(has_line_feed());
    sel->hasPostLineBreak(hasPostLineBreak());
    sel->media_block(media_block());
    sel->concat(elements());
    return sel;
  }

  CompoundSelector_Obj Compound_Selector::toCompSelector() {
    CompoundSelector_Obj sel = SASS_MEMORY_NEW(CompoundSelector, pstate());
    // sel->has_line_feed(has_line_feed());
    sel->hasPostLineBreak(hasPostLineBreak());
    sel->media_block(media_block());
    sel->elements(elements());
    if (sel->length() > 0) {
      if (Cast<Parent_Selector>(sel->at(0))) {
        sel->erase(sel->begin());
        sel->hasRealParent(true);
      }
    }
    return sel;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  CompoundOrCombinator::CompoundOrCombinator(ParserState pstate)
  : Selector(pstate)
  {
  }

  CompoundOrCombinator::CompoundOrCombinator(const CompoundOrCombinator* ptr)
  : Selector(ptr)
  { }

  void CompoundOrCombinator::cloneChildren()
  {
    /* for (size_t i = 0, l = length(); i < l; i++) {
      at(i) = SASS_MEMORY_CLONE(at(i));
    }*/
  }

  unsigned long CompoundOrCombinator::specificity() const
  {
    return 0;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  SelectorCombinator::SelectorCombinator(ParserState pstate, SelectorCombinator::Combinator combinator)
    : CompoundOrCombinator(pstate),
    combinator_(combinator)
  {
  }
  SelectorCombinator::SelectorCombinator(const SelectorCombinator* ptr)
    : CompoundOrCombinator(ptr),
      combinator_(ptr->combinator())
  { }

  void SelectorCombinator::cloneChildren()
  {
    /* for (size_t i = 0, l = length(); i < l; i++) {
      at(i) = SASS_MEMORY_CLONE(at(i));
    }*/
  }

  unsigned long SelectorCombinator::specificity() const
  {
    return 0;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  CompoundSelector::CompoundSelector(ParserState pstate)
    : CompoundOrCombinator(pstate),
      Vectorized<Simple_Selector_Obj, CompoundSelector>(),
      hasRealParent_(false),
      extended_(false),
      hasPostLineBreak_(false)
  {
  }
  CompoundSelector::CompoundSelector(const CompoundSelector* ptr)
    : CompoundOrCombinator(ptr),
      Vectorized<Simple_Selector_Obj, CompoundSelector>(*ptr),
      hasRealParent_(ptr->hasRealParent()),
      extended_(ptr->extended()),
      hasPostLineBreak_(ptr->hasPostLineBreak())
  { }

  void CompoundSelector::cloneChildren()
  {
    /* for (size_t i = 0, l = length(); i < l; i++) {
      at(i) = SASS_MEMORY_CLONE(at(i));
    }*/
  }

  // Wrap the compound selector with a complex selector
  ComplexSelector* CompoundOrCombinator::wrapInComplex()
  {
    auto complex = SASS_MEMORY_NEW(ComplexSelector, pstate());
    complex->append(this);
    return complex;
  }


  unsigned long CompoundSelector::specificity() const
  {
    int sum = 0;
    for (size_t i = 0, L = length(); i < L; ++i)
    { sum += get(i)->specificity(); }
    return sum;
  }

  CompoundSelector_Obj Simple_Selector::wrapInCompound()
  {
    CompoundSelector_Obj selector =
      SASS_MEMORY_NEW(CompoundSelector, pstate());
    selector->append(this);
    return selector;
  }
  ComplexSelector_Obj Simple_Selector::wrapInComplex()
  {
    ComplexSelector_Obj selector =
      SASS_MEMORY_NEW(ComplexSelector, pstate());
    selector->append(wrapInCompound());
    return selector;
  }

  SelectorList_Obj ComplexSelector::wrapInList()
  {
    SelectorList_Obj selector =
      SASS_MEMORY_NEW(SelectorList, pstate());
    selector->append(this);
    return selector;
  }

  Pseudo_Selector_Obj Pseudo_Selector::withSelector(SelectorList_Obj selector)
  {
    Pseudo_Selector_Obj pseudo = copy();
    pseudo->selector2(selector);
    return pseudo;
  }

  bool ComplexSelector::canHaveRealParent() const
  {
    if (empty()) { return true; }
    return get(0)->canHaveRealParent();
  }

  bool SelectorList::canHaveRealParent() const
  {
    for (size_t i = 0; i < length(); i += 1) {
      if (!get(i)->canHaveRealParent()) return false;
    }
    return true;
  }

  bool CompoundSelector::canHaveRealParent() const
  {
    if (empty()) { return true; }
    Simple_Selector_Obj front = first();
    // if (Cast<Universal_Selector>(front)) return false;
    if (Cast<Type_Selector>(front)) {
      if (front->ns() != "") return false;
    }
    return true;
  }


  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  IMPLEMENT_AST_OPERATORS(Selector_Schema);
  IMPLEMENT_AST_OPERATORS(Placeholder_Selector);
  IMPLEMENT_AST_OPERATORS(Parent_Selector);
  IMPLEMENT_AST_OPERATORS(Attribute_Selector);
  IMPLEMENT_AST_OPERATORS(Compound_Selector);
  IMPLEMENT_AST_OPERATORS(Complex_Selector);
  IMPLEMENT_AST_OPERATORS(Type_Selector);
  IMPLEMENT_AST_OPERATORS(Class_Selector);
  IMPLEMENT_AST_OPERATORS(Id_Selector);
  IMPLEMENT_AST_OPERATORS(Pseudo_Selector);
  IMPLEMENT_AST_OPERATORS(Selector_List);


  IMPLEMENT_AST_OPERATORS(SelectorCombinator);
  IMPLEMENT_AST_OPERATORS(CompoundSelector);
  IMPLEMENT_AST_OPERATORS(ComplexSelector);
  IMPLEMENT_AST_OPERATORS(SelectorList);

}
