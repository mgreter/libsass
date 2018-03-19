#include "sass.hpp"
#include "ast.hpp"
#include "context.hpp"
#include "node.hpp"
#include "eval.hpp"
#include "extend.hpp"
#include "emitter.hpp"
#include "color_maps.hpp"
#include "ast_fwd_decl.hpp"
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

  Selector::Selector(ParserState pstate)
  : Expression(pstate),
    has_line_feed_(false),
    has_line_break_(false),
    is_optional_(false),
    media_block_(0),
    hash_(0)
  { concrete_type(SELECTOR); }

  Selector::Selector(const Selector* ptr)
  : Expression(ptr),
    has_line_feed_(ptr->has_line_feed_),
    has_line_break_(ptr->has_line_break_),
    is_optional_(ptr->is_optional_),
    media_block_(ptr->media_block_),
    hash_(ptr->hash_)
  { concrete_type(SELECTOR); }

  void Selector::set_media_block(Media_Block_Ptr mb)
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

  size_t Selector_Schema::hash() {
    if (hash_ == 0) {
      hash_combine(hash_, contents_->hash());
    }
    return hash_;
  }

  bool Selector_Schema::has_parent_ref() const
  {
    if (String_Schema_Obj schema = Cast<String_Schema>(contents())) {
      return schema->length() > 0 && Cast<Parent_Selector>(schema->at(0)) != NULL;
    }
    return false;
  }

  bool Selector_Schema::has_real_parent_ref() const
  {
    if (String_Schema_Obj schema = Cast<String_Schema>(contents())) {
      if (schema->length() == 0) return false;
      return Cast<Parent_Reference>(schema->at(0));
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

  size_t Simple_Selector::hash()
  {
    if (hash_ == 0) {
      hash_combine(hash_, std::hash<int>()(SELECTOR));
      hash_combine(hash_, std::hash<int>()(simple_type()));
      hash_combine(hash_, std::hash<std::string>()(ns()));
      hash_combine(hash_, std::hash<std::string>()(name()));
    }
    return hash_;
  }

  bool Simple_Selector::empty() const {
    return ns().empty() && name().empty();
  }

  // namespace compare functions
  bool Simple_Selector::is_ns_eq(const Simple_Selector& r) const
  {
    // https://github.com/sass/sass/issues/2229
    if ((has_ns_ == r.has_ns_) ||
        (has_ns_ && ns_.empty()) ||
        (r.has_ns_ && r.ns_.empty())
    ) {
      if (ns_.empty() && r.ns() == "*") return false;
      else if (r.ns().empty() && ns() == "*") return false;
      else return ns() == r.ns();
    }
    return false;
  }

  // namespace query functions
  bool Simple_Selector::is_universal_ns() const
  {
    return has_ns_ && ns_ == "*";
  }

  bool Simple_Selector::has_universal_ns() const
  {
    return !has_ns_ || ns_ == "*";
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

  bool Simple_Selector::is_superselector_of(Compound_Selector_Obj sub)
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

  size_t Attribute_Selector::hash()
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

  Pseudo_Selector::Pseudo_Selector(ParserState pstate, std::string n, String_Obj expr)
  : Simple_Selector(pstate, n), expression_(expr)
  { simple_type(PSEUDO_SEL); }
  Pseudo_Selector::Pseudo_Selector(const Pseudo_Selector* ptr)
  : Simple_Selector(ptr), expression_(ptr->expression_)
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
    return (name_[0] == ':' && name_[1] == ':')
            || is_pseudo_class_element(name_);
  }

  size_t Pseudo_Selector::hash()
  {
    if (hash_ == 0) {
      hash_combine(hash_, Simple_Selector::hash());
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

  Wrapped_Selector::Wrapped_Selector(ParserState pstate, std::string n, Selector_List_Obj sel)
  : Simple_Selector(pstate, n), selector_(sel)
  { simple_type(WRAPPED_SEL); }
  Wrapped_Selector::Wrapped_Selector(const Wrapped_Selector* ptr)
  : Simple_Selector(ptr), selector_(ptr->selector_)
  { simple_type(WRAPPED_SEL); }

  bool Wrapped_Selector::is_superselector_of(Wrapped_Selector_Obj sub)
  {
    if (this->name() != sub->name()) return false;
    if (this->name() == ":current") return false;
    if (Selector_List_Obj rhs_list = Cast<Selector_List>(sub->selector())) {
      if (Selector_List_Obj lhs_list = Cast<Selector_List>(selector())) {
        return lhs_list->is_superselector_of(rhs_list);
      }
    }
    coreError("is_superselector expected a Selector_List", sub->pstate());
    return false;
  }

  // Selectors inside the negation pseudo-class are counted like any
  // other, but the negation itself does not count as a pseudo-class.

  void Wrapped_Selector::cloneChildren()
  {
    selector(SASS_MEMORY_CLONE(selector()));
  }

  size_t Wrapped_Selector::hash()
  {
    if (hash_ == 0) {
      hash_combine(hash_, Simple_Selector::hash());
      if (selector_) hash_combine(hash_, selector_->hash());
    }
    return hash_;
  }

  bool Wrapped_Selector::has_parent_ref() const {
    if (!selector()) return false;
    return selector()->has_parent_ref();
  }

  bool Wrapped_Selector::has_real_parent_ref() const {
    if (!selector()) return false;
    return selector()->has_real_parent_ref();
  }

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

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Compound_Selector::Compound_Selector(ParserState pstate, size_t s)
  : Selector(pstate),
    Vectorized<Simple_Selector_Obj>(s),
    extended_(false),
    has_parent_reference_(false)
  { }

  Compound_Selector::Compound_Selector(const Compound_Selector* ptr)
  : Selector(ptr),
    Vectorized<Simple_Selector_Obj>(*ptr),
    extended_(ptr->extended_),
    has_parent_reference_(ptr->has_parent_reference_)
  { }

  bool Compound_Selector::contains_placeholder() {
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

  bool Compound_Selector::has_real_parent_ref() const
  {
    for (Simple_Selector_Obj s : *this) {
      if (s && s->has_real_parent_ref()) return true;
    }
    return false;
  }

  bool Compound_Selector::is_superselector_of(Selector_List_Obj rhs, std::string wrapped)
  {
    for (Complex_Selector_Obj item : rhs->elements()) {
      if (is_superselector_of(item, wrapped)) return true;
    }
    return false;
  }

  bool Compound_Selector::is_superselector_of(Complex_Selector_Obj rhs, std::string wrapped)
  {
    if (rhs->head()) return is_superselector_of(rhs->head(), wrapped);
    return false;
  }

  bool Compound_Selector::is_superselector_of(Compound_Selector_Obj rhs, std::string wrapping)
  {
    Compound_Selector_Ptr lhs = this;
    Simple_Selector_Ptr lbase = lhs->base();
    Simple_Selector_Ptr rbase = rhs->base();

    // Check if pseudo-elements are the same between the selectors

    std::set<std::string> lpsuedoset, rpsuedoset;
    for (size_t i = 0, L = length(); i < L; ++i)
    {
      if ((*this)[i]->is_pseudo_element()) {
        std::string pseudo((*this)[i]->to_string());
        pseudo = pseudo.substr(pseudo.find_first_not_of(":")); // strip off colons to ensure :after matches ::after since ruby sass is forgiving
        lpsuedoset.insert(pseudo);
      }
    }
    for (size_t i = 0, L = rhs->length(); i < L; ++i)
    {
      if ((*rhs)[i]->is_pseudo_element()) {
        std::string pseudo((*rhs)[i]->to_string());
        pseudo = pseudo.substr(pseudo.find_first_not_of(":")); // strip off colons to ensure :after matches ::after since ruby sass is forgiving
        rpsuedoset.insert(pseudo);
      }
    }
    if (lpsuedoset != rpsuedoset) {
      return false;
    }

    // replaced compare without stringification
    // https://github.com/sass/sass/issues/2229
    SelectorSet lset, rset;

    if (lbase && rbase)
    {
      if (*lbase == *rbase) {
        // create ordered sets for includes query
        lset.insert(this->begin(), this->end());
        rset.insert(rhs->begin(), rhs->end());
        return includes(rset.begin(), rset.end(), lset.begin(), lset.end());
      }
      return false;
    }

    for (size_t i = 0, iL = length(); i < iL; ++i)
    {
      Selector_Obj wlhs = (*this)[i];
      // very special case for wrapped matches selector
      if (Wrapped_Selector_Obj wrapped = Cast<Wrapped_Selector>(wlhs)) {
        if (wrapped->name() == ":not") {
          if (Selector_List_Obj not_list = Cast<Selector_List>(wrapped->selector())) {
            if (not_list->is_superselector_of(rhs, wrapped->name())) return false;
          } else {
            throw std::runtime_error("wrapped not selector is not a list");
          }
        }
        if (wrapped->name() == ":matches" || wrapped->name() == ":-moz-any") {
          wlhs = wrapped->selector();
          if (Selector_List_Obj list = Cast<Selector_List>(wrapped->selector())) {
            if (Compound_Selector_Obj comp = Cast<Compound_Selector>(rhs)) {
              if (!wrapping.empty() && wrapping != wrapped->name()) return false;
              if (wrapping.empty() || wrapping != wrapped->name()) {;
                if (list->is_superselector_of(comp, wrapped->name())) return true;
              }
            }
          }
        }
        Simple_Selector_Ptr rhs_sel = NULL;
        if (rhs->elements().size() > i) rhs_sel = (*rhs)[i];
        if (Wrapped_Selector_Ptr wrapped_r = Cast<Wrapped_Selector>(rhs_sel)) {
          if (wrapped->name() == wrapped_r->name()) {
          if (wrapped->is_superselector_of(wrapped_r)) {
             continue;
          }}
        }
      }
      lset.insert(wlhs);
    }

    for (size_t n = 0, nL = rhs->length(); n < nL; ++n)
    {
      Selector_Obj r = (*rhs)[n];
      if (Wrapped_Selector_Obj wrapped = Cast<Wrapped_Selector>(r)) {
        if (wrapped->name() == ":not") {
          if (Selector_List_Obj ls = Cast<Selector_List>(wrapped->selector())) {
            ls->remove_parent_selectors(); // unverified
            if (is_superselector_of(ls, wrapped->name())) return false;
          }
        }
        if (wrapped->name() == ":matches" || wrapped->name() == ":-moz-any") {
          if (!wrapping.empty()) {
            if (wrapping != wrapped->name()) return false;
          }
          if (Selector_List_Obj ls = Cast<Selector_List>(wrapped->selector())) {
            ls->remove_parent_selectors(); // unverified
            return (is_superselector_of(ls, wrapped->name()));
          }
        }
      }
      rset.insert(r);
    }

    //for (auto l : lset) { cerr << "l: " << l << endl; }
    //for (auto r : rset) { cerr << "r: " << r << endl; }

    if (lset.empty()) return true;
    // return true if rset contains all the elements of lset
    return includes(rset.begin(), rset.end(), lset.begin(), lset.end());

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
                           0);
  }

  Simple_Selector_Ptr Compound_Selector::base() const {
    if (length() == 0) return 0;
    // ToDo: why is this needed?
    if (Cast<Type_Selector>((*this)[0]))
      return (*this)[0];
    return 0;
  }

  size_t Compound_Selector::hash()
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

  bool Compound_Selector::is_empty_reference()
  {
    return length() == 1 &&
            Cast<Parent_Selector>((*this)[0]);
  }

  void Compound_Selector::append(Simple_Selector_Ptr element)
  {
    Vectorized<Simple_Selector_Obj>::append(element);
    pstate_.offset += element->pstate().offset;
  }

  Compound_Selector_Ptr Compound_Selector::minus(Compound_Selector_Ptr rhs)
  {
    Compound_Selector_Ptr result = SASS_MEMORY_NEW(Compound_Selector, pstate());
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
    reference_(r)
  {}
  Complex_Selector::Complex_Selector(const Complex_Selector* ptr)
  : Selector(ptr),
    combinator_(ptr->combinator_),
    head_(ptr->head_), tail_(ptr->tail_),
    reference_(ptr->reference_)
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
      if (!tail_) return 0;
      tail_->has_line_feed_ = this->has_line_feed_;
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

  size_t Complex_Selector::hash()
  {
    if (hash_ == 0) {
      hash_combine(hash_, std::hash<int>()(SELECTOR));
      hash_combine(hash_, std::hash<int>()(combinator_));
      if (head_) hash_combine(hash_, head_->hash());
      if (tail_) hash_combine(hash_, tail_->hash());
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

  void Complex_Selector::set_media_block(Media_Block_Ptr mb) {
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
    Complex_Selector_Ptr pIter = this;
    while (pIter) {
      Compound_Selector_Ptr pHead = pIter->head();

      if (pHead) {
        pHead->mergeSources(sources);
      }

      pIter = pIter->tail();
    }
  }

  void Complex_Selector::clearSources()
  {
    Complex_Selector_Ptr pIter = this;
    while (pIter) {
      Compound_Selector_Ptr pHead = pIter->head();

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

  bool Complex_Selector::is_superselector_of(Compound_Selector_Obj rhs, std::string wrapping)
  {
    return last()->head() && last()->head()->is_superselector_of(rhs, wrapping);
  }

  bool Complex_Selector::is_superselector_of(Complex_Selector_Obj rhs, std::string wrapping)
  {
    Complex_Selector_Ptr lhs = this;
    // check for selectors with leading or trailing combinators
    if (!lhs->head() || !rhs->head())
    { return false; }
    Complex_Selector_Obj l_innermost = lhs->innermost();
    if (l_innermost->combinator() != Complex_Selector::ANCESTOR_OF)
    { return false; }
    Complex_Selector_Obj r_innermost = rhs->innermost();
    if (r_innermost->combinator() != Complex_Selector::ANCESTOR_OF)
    { return false; }
    // more complex (i.e., longer) selectors are always more specific
    size_t l_len = lhs->length(), r_len = rhs->length();
    if (l_len > r_len)
    { return false; }

    if (l_len == 1)
    { return lhs->head()->is_superselector_of(rhs->last()->head(), wrapping); }

    // we have to look one tail deeper, since we cary the
    // combinator around for it (which is important here)
    if (rhs->tail() && lhs->tail() && combinator() != Complex_Selector::ANCESTOR_OF) {
      Complex_Selector_Obj lhs_tail = lhs->tail();
      Complex_Selector_Obj rhs_tail = rhs->tail();
      if (lhs_tail->combinator() != rhs_tail->combinator()) return false;
      if (lhs_tail->head() && !rhs_tail->head()) return false;
      if (!lhs_tail->head() && rhs_tail->head()) return false;
      if (lhs_tail->head() && rhs_tail->head()) {
        if (!lhs_tail->head()->is_superselector_of(rhs_tail->head())) return false;
      }
    }

    bool found = false;
    Complex_Selector_Obj marker = rhs;
    for (size_t i = 0, L = rhs->length(); i < L; ++i) {
      if (i == L-1)
      { return false; }
      if (lhs->head() && marker->head() && lhs->head()->is_superselector_of(marker->head(), wrapping))
      { found = true; break; }
      marker = marker->tail();
    }
    if (!found)
    { return false; }

    /*
      Hmm, I hope I have the logic right:

      if lhs has a combinator:
        if !(marker has a combinator) return false
        if !(lhs.combinator == '~' ? marker.combinator != '>' : lhs.combinator == marker.combinator) return false
        return lhs.tail-without-innermost.is_superselector_of(marker.tail-without-innermost)
      else if marker has a combinator:
        if !(marker.combinator == ">") return false
        return lhs.tail.is_superselector_of(marker.tail)
      else
        return lhs.tail.is_superselector_of(marker.tail)
    */
    if (lhs->combinator() != Complex_Selector::ANCESTOR_OF)
    {
      if (marker->combinator() == Complex_Selector::ANCESTOR_OF)
      { return false; }
      if (!(lhs->combinator() == Complex_Selector::PRECEDES ? marker->combinator() != Complex_Selector::PARENT_OF : lhs->combinator() == marker->combinator()))
      { return false; }
      return lhs->tail()->is_superselector_of(marker->tail());
    }
    else if (marker->combinator() != Complex_Selector::ANCESTOR_OF)
    {
      if (marker->combinator() != Complex_Selector::PARENT_OF)
      { return false; }
      return lhs->tail()->is_superselector_of(marker->tail());
    }
    return lhs->tail()->is_superselector_of(marker->tail());
  }

  size_t Complex_Selector::length() const
  {
    // TODO: make this iterative
    if (!tail()) return 1;
    return 1 + tail()->length();
  }

  // append another complex selector at the end
  // check if we need to append some headers
  // then we need to check for the combinator
  // only then we can safely set the new tail
  void Complex_Selector::append(Complex_Selector_Obj ss, Backtraces& traces)
  {

    Complex_Selector_Obj t = ss->tail();
    Combinator c = ss->combinator();
    String_Obj r = ss->reference();
    Compound_Selector_Obj h = ss->head();

    if (ss->has_line_feed()) has_line_feed(true);
    if (ss->has_line_break()) has_line_break(true);

    // append old headers
    if (h && h->length()) {
      if (last()->combinator() != ANCESTOR_OF && c != ANCESTOR_OF) {
        traces.push_back(Backtrace(pstate()));
        throw Exception::InvalidParent(this, traces, ss);
      } else if (last()->head_ && last()->head_->length()) {
        Compound_Selector_Obj rh = last()->head();
        size_t i;
        size_t L = h->length();
        if (Cast<Type_Selector>(h->first())) {
          if (Class_Selector_Ptr cs = Cast<Class_Selector>(rh->last())) {
            Class_Selector_Ptr sqs = SASS_MEMORY_COPY(cs);
            sqs->name(sqs->name() + (*h)[0]->name());
            sqs->pstate((*h)[0]->pstate());
            (*rh)[rh->length()-1] = sqs;
            rh->pstate(h->pstate());
            for (i = 1; i < L; ++i) rh->append((*h)[i]);
          } else if (Id_Selector_Ptr is = Cast<Id_Selector>(rh->last())) {
            Id_Selector_Ptr sqs = SASS_MEMORY_COPY(is);
            sqs->name(sqs->name() + (*h)[0]->name());
            sqs->pstate((*h)[0]->pstate());
            (*rh)[rh->length()-1] = sqs;
            rh->pstate(h->pstate());
            for (i = 1; i < L; ++i) rh->append((*h)[i]);
          } else if (Type_Selector_Ptr ts = Cast<Type_Selector>(rh->last())) {
            Type_Selector_Ptr tss = SASS_MEMORY_COPY(ts);
            tss->name(tss->name() + (*h)[0]->name());
            tss->pstate((*h)[0]->pstate());
            (*rh)[rh->length()-1] = tss;
            rh->pstate(h->pstate());
            for (i = 1; i < L; ++i) rh->append((*h)[i]);
          } else if (Placeholder_Selector_Ptr ps = Cast<Placeholder_Selector>(rh->last())) {
            Placeholder_Selector_Ptr pss = SASS_MEMORY_COPY(ps);
            pss->name(pss->name() + (*h)[0]->name());
            pss->pstate((*h)[0]->pstate());
            (*rh)[rh->length()-1] = pss;
            rh->pstate(h->pstate());
            for (i = 1; i < L; ++i) rh->append((*h)[i]);
          } else {
            last()->head_->concat(h);
          }
        } else {
          last()->head_->concat(h);
        }
      } else if (last()->head_) {
        last()->head_->concat(h);
      }
    } else {
      // std::cerr << "has no or empty head\n";
    }

    if (last()) {
      if (last()->combinator() != ANCESTOR_OF && c != ANCESTOR_OF) {
        Complex_Selector_Ptr inter = SASS_MEMORY_NEW(Complex_Selector, pstate());
        inter->reference(r);
        inter->combinator(c);
        inter->tail(t);
        last()->tail(inter);
      } else {
        if (last()->combinator() == ANCESTOR_OF) {
          last()->combinator(c);
          last()->reference(r);
        }
        last()->tail(t);
      }
    }

  }

  Selector_List_Ptr Complex_Selector::resolve_parent_refs(SelectorStack& pstack, Backtraces& traces, bool implicit_parent)
  {
    Complex_Selector_Obj tail = this->tail();
    Compound_Selector_Obj head = this->head();
    Selector_List_Ptr parents = pstack.back();

    if (!this->has_real_parent_ref() && !implicit_parent) {
      Selector_List_Ptr retval = SASS_MEMORY_NEW(Selector_List, pstate());
      retval->append(this);
      return retval;
    }

    // first resolve_parent_refs the tail (which may return an expanded list)
    Selector_List_Obj tails = tail ? tail->resolve_parent_refs(pstack, traces, implicit_parent) : 0;

    if (head && head->length() > 0) {

      Selector_List_Obj retval;
      // we have a parent selector in a simple compound list
      // mix parent complex selector into the compound list
      if (Cast<Parent_Selector>((*head)[0])) {
        retval = SASS_MEMORY_NEW(Selector_List, pstate());

        // it turns out that real parent references reach
        // across @at-root rules, which comes unexpected
        if (parents == NULL && head->has_real_parent_ref()) {
          int i = pstack.size() - 1;
          while (!parents && i > -1) {
            parents = pstack.at(i--);
          }
        }

        if (parents && parents->length()) {
          if (tails && tails->length() > 0) {
            for (size_t n = 0, nL = tails->length(); n < nL; ++n) {
              for (size_t i = 0, iL = parents->length(); i < iL; ++i) {
                Complex_Selector_Obj t = (*tails)[n];
                Complex_Selector_Obj parent = (*parents)[i];
                Complex_Selector_Obj s = SASS_MEMORY_CLONE(parent);
                Complex_Selector_Obj ss = SASS_MEMORY_CLONE(this);
                ss->tail(t ? SASS_MEMORY_CLONE(t) : NULL);
                Compound_Selector_Obj h = SASS_MEMORY_COPY(head_);
                // remove parent selector from sequence
                if (h->length()) {
                  h->erase(h->begin());
                  ss->head(h);
                } else {
                  ss->head(NULL);
                }
                // adjust for parent selector (1 char)
                // if (h->length()) {
                //   ParserState state(h->at(0)->pstate());
                //   state.offset.column += 1;
                //   state.column -= 1;
                //   (*h)[0]->pstate(state);
                // }
                // keep old parser state
                s->pstate(pstate());
                // append new tail
                s->append(ss, traces);
                retval->append(s);
              }
            }
          }
          // have no tails but parents
          // loop above is inside out
          else {
            for (size_t i = 0, iL = parents->length(); i < iL; ++i) {
              Complex_Selector_Obj parent = (*parents)[i];
              Complex_Selector_Obj s = SASS_MEMORY_CLONE(parent);
              Complex_Selector_Obj ss = SASS_MEMORY_CLONE(this);
              // this is only if valid if the parent has no trailing op
              // otherwise we cannot append more simple selectors to head
              if (parent->last()->combinator() != ANCESTOR_OF) {
                traces.push_back(Backtrace(pstate()));
                throw Exception::InvalidParent(parent, traces, ss);
              }
              ss->tail(tail ? SASS_MEMORY_CLONE(tail) : NULL);
              Compound_Selector_Obj h = SASS_MEMORY_COPY(head_);
              // remove parent selector from sequence
              if (h->length()) {
                h->erase(h->begin());
                ss->head(h);
              } else {
                ss->head(NULL);
              }
              // \/ IMO ruby sass bug \/
              ss->has_line_feed(false);
              // adjust for parent selector (1 char)
              // if (h->length()) {
              //   ParserState state(h->at(0)->pstate());
              //   state.offset.column += 1;
              //   state.column -= 1;
              //   (*h)[0]->pstate(state);
              // }
              // keep old parser state
              s->pstate(pstate());
              // append new tail
              s->append(ss, traces);
              retval->append(s);
            }
          }
        }
        // have no parent but some tails
        else {
          if (tails && tails->length() > 0) {
            for (size_t n = 0, nL = tails->length(); n < nL; ++n) {
              Complex_Selector_Obj cpy = SASS_MEMORY_CLONE(this);
              cpy->tail(SASS_MEMORY_CLONE(tails->at(n)));
              cpy->head(SASS_MEMORY_NEW(Compound_Selector, head->pstate()));
              for (size_t i = 1, L = this->head()->length(); i < L; ++i)
                cpy->head()->append((*this->head())[i]);
              if (!cpy->head()->length()) cpy->head(0);
              retval->append(cpy->skip_empty_reference());
            }
          }
          // have no parent nor tails
          else {
            Complex_Selector_Obj cpy = SASS_MEMORY_CLONE(this);
            cpy->head(SASS_MEMORY_NEW(Compound_Selector, head->pstate()));
            for (size_t i = 1, L = this->head()->length(); i < L; ++i)
              cpy->head()->append((*this->head())[i]);
            if (!cpy->head()->length()) cpy->head(0);
            retval->append(cpy->skip_empty_reference());
          }
        }
      }
      // no parent selector in head
      else {
        retval = this->tails(tails);
      }

      for (Simple_Selector_Obj ss : head->elements()) {
        if (Wrapped_Selector_Ptr ws = Cast<Wrapped_Selector>(ss)) {
          if (Selector_List_Ptr sl = Cast<Selector_List>(ws->selector())) {
            if (parents) ws->selector(sl->resolve_parent_refs(pstack, traces, implicit_parent));
          }
        }
      }

      return retval.detach();

    }
    // has no head
    return this->tails(tails);
  }

  Selector_List_Ptr Complex_Selector::tails(Selector_List_Ptr tails)
  {
    Selector_List_Ptr rv = SASS_MEMORY_NEW(Selector_List, pstate_);
    if (tails && tails->length()) {
      for (size_t i = 0, iL = tails->length(); i < iL; ++i) {
        Complex_Selector_Obj pr = SASS_MEMORY_CLONE(this);
        pr->tail(tails->at(i));
        rv->append(pr);
      }
    }
    else {
      rv->append(this);
    }
    return rv;
  }

  // return the last tail that is defined
  Complex_Selector_Obj Complex_Selector::first()
  {
    // declare variables used in loop
    Complex_Selector_Obj cur = this;
    Compound_Selector_Obj head;
    // processing loop
    while (cur)
    {
      // get the head
      head = cur->head_;
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

  // return the last tail that is defined
  Complex_Selector_Obj Complex_Selector::last()
  {
    Complex_Selector_Ptr cur = this;
    Complex_Selector_Ptr nxt = cur;
    // loop until last
    while (nxt) {
      cur = nxt;
      nxt = cur->tail();
    }
    return cur;
  }

  Complex_Selector::Combinator Complex_Selector::clear_innermost()
  {
    Combinator c;
    if (!tail() || tail()->tail() == 0)
    { c = combinator(); combinator(ANCESTOR_OF); tail(0); }
    else
    { c = tail()->clear_innermost(); }
    return c;
  }

  void Complex_Selector::set_innermost(Complex_Selector_Obj val, Combinator c)
  {
    if (!tail())
    { tail(val); combinator(c); }
    else
    { tail()->set_innermost(val, c); }
  }

  void Complex_Selector::cloneChildren()
  {
    if (head()) head(SASS_MEMORY_CLONE(head()));
    if (tail()) tail(SASS_MEMORY_CLONE(tail()));
  }

  // it's a superselector if every selector of the right side
  // list is a superselector of the given left side selector
  bool Complex_Selector::is_superselector_of(Selector_List_Obj sub, std::string wrapping)
  {
    // Check every rhs selector against left hand list
    for(size_t i = 0, L = sub->length(); i < L; ++i) {
      if (!is_superselector_of((*sub)[i], wrapping)) return false;
    }
    return true;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Selector_List::Selector_List(ParserState pstate, size_t s)
  : Selector(pstate),
    Vectorized<Complex_Selector_Obj>(s),
    schema_(NULL),
    wspace_(0)
  { }
  Selector_List::Selector_List(const Selector_List* ptr)
  : Selector(ptr),
    Vectorized<Complex_Selector_Obj>(*ptr),
    schema_(ptr->schema_),
    wspace_(ptr->wspace_)
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
      eval(schema()) : eval(this);
    list->schema(schema());
    return list;
  }

  Selector_List_Ptr Selector_List::resolve_parent_refs(SelectorStack& pstack, Backtraces& traces, bool implicit_parent)
  {
    if (!this->has_parent_ref()) return this;
    Selector_List_Ptr ss = SASS_MEMORY_NEW(Selector_List, pstate());
    Selector_List_Ptr ps = pstack.back();
    for (size_t pi = 0, pL = ps->length(); pi < pL; ++pi) {
      for (size_t si = 0, sL = this->length(); si < sL; ++si) {
        Selector_List_Obj rv = at(si)->resolve_parent_refs(pstack, traces, implicit_parent);
        ss->concat(rv);
      }
    }
    return ss;
  }

  void Selector_List::cloneChildren()
  {
    for (size_t i = 0, l = length(); i < l; i++) {
      at(i) = SASS_MEMORY_CLONE(at(i));
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
            if ((*this)[i]->has_line_feed()) {
              (*this)[i]->tail()->has_line_feed(true);
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

  void Selector_List::adjust_after_pushing(Complex_Selector_Obj c)
  {
    // if (c->has_reference())   has_reference(true);
  }

  // it's a superselector if every selector of the right side
  // list is a superselector of the given left side selector
  bool Selector_List::is_superselector_of(Selector_List_Obj sub, std::string wrapping)
  {
    // Check every rhs selector against left hand list
    for(size_t i = 0, L = sub->length(); i < L; ++i) {
      if (!is_superselector_of((*sub)[i], wrapping)) return false;
    }
    return true;
  }

  // it's a superselector if every selector on the right side
  // is a superselector of any one of the left side selectors
  bool Selector_List::is_superselector_of(Compound_Selector_Obj sub, std::string wrapping)
  {
    // Check every lhs selector against right hand
    for(size_t i = 0, L = length(); i < L; ++i) {
      if ((*this)[i]->is_superselector_of(sub, wrapping)) return true;
    }
    return false;
  }

  // it's a superselector if every selector on the right side
  // is a superselector of any one of the left side selectors
  bool Selector_List::is_superselector_of(Complex_Selector_Obj sub, std::string wrapping)
  {
    // Check every lhs selector against right hand
    for(size_t i = 0, L = length(); i < L; ++i) {
      if ((*this)[i]->is_superselector_of(sub)) return true;
    }
    return false;
  }


  void Selector_List::populate_extends(Selector_List_Obj extendee, Subset_Map& extends)
  {

    Selector_List_Ptr extender = this;
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
        extends.put(compound_sel, std::make_pair((*extender)[i], compound_sel));
      }
    }
  };

  size_t Selector_List::hash()
  {
    if (Selector::hash_ == 0) {
      hash_combine(Selector::hash_, std::hash<int>()(SELECTOR));
      hash_combine(Selector::hash_, Vectorized::hash());
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

  void Selector_List::set_media_block(Media_Block_Ptr mb)
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

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

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
  IMPLEMENT_AST_OPERATORS(Wrapped_Selector);
  IMPLEMENT_AST_OPERATORS(Selector_List);

}