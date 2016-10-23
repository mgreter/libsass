#include "sass.hpp"
#include "ast.hpp"
#include "context.hpp"
#include "node.hpp"
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

  static Null sass_null(Sass::Null(ParserState("null")));

  bool Supports_Operator_Ref::needs_parens(Supports_Condition_Ptr cond) const {
    return dynamic_cast<Supports_Negation_Ptr>(cond) ||
          (dynamic_cast<Supports_Operator_Ptr>(cond) &&
           dynamic_cast<Supports_Operator_Ptr>(cond)->operand() != operand());
  }

  bool Supports_Negation_Ref::needs_parens(Supports_Condition_Ptr cond) const {
    return dynamic_cast<Supports_Negation_Ptr>(cond) ||
          dynamic_cast<Supports_Operator_Ptr>(cond);
  }

  std::string & str_ltrim(std::string & str)
  {
    auto it2 =  std::find_if( str.begin() , str.end() , [](char ch){ return !std::isspace<char>(ch , std::locale::classic() ) ; } );
    str.erase( str.begin() , it2);
    return str;
  }

  std::string & str_rtrim(std::string & str)
  {
    auto it1 =  std::find_if( str.rbegin() , str.rend() , [](char ch){ return !std::isspace<char>(ch , std::locale::classic() ) ; } );
    str.erase( it1.base() , str.end() );
    return str;
  }

  void String_Constant_Ref::rtrim()
  {
    value_ = str_rtrim(value_);
  }
  void String_Constant_Ref::ltrim()
  {
    value_ = str_ltrim(value_);
  }
  void String_Constant_Ref::trim()
  {
    rtrim();
    ltrim();
  }

  void String_Schema_Ref::rtrim()
  {
    if (!empty()) {
      if (String_Ptr str = dynamic_cast<String_Ptr>(last())) str->rtrim();
    }
  }
  void String_Schema_Ref::ltrim()
  {
    if (!empty()) {
      if (String_Ptr str = dynamic_cast<String_Ptr>(first())) str->ltrim();
    }
  }
  void String_Schema_Ref::trim()
  {
    rtrim();
    ltrim();
  }

  void Argument_Ref::set_delayed(bool delayed)
  {
    if (value_) value_->set_delayed(delayed);
    is_delayed(delayed);
  }

  void Arguments_Ref::set_delayed(bool delayed)
  {
    for (Argument_Ptr arg : elements()) {
      if (arg) arg->set_delayed(delayed);
    }
    is_delayed(delayed);
  }


  bool At_Root_Query_Ref::exclude(std::string str)
  {
    bool with = feature() && unquote(feature()->to_string()).compare("with") == 0;
    List_Ptr l = static_cast<List_Ptr>(value());
    std::string v;

    if (with)
    {
      if (!l || l->length() == 0) return str.compare("rule") != 0;
      for (size_t i = 0, L = l->length(); i < L; ++i)
      {
        v = unquote((*l)[i]->to_string());
        if (v.compare("all") == 0 || v == str) return false;
      }
      return true;
    }
    else
    {
      if (!l || !l->length()) return str.compare("rule") == 0;
      for (size_t i = 0, L = l->length(); i < L; ++i)
      {
        v = unquote((*l)[i]->to_string());
        if (v.compare("all") == 0 || v == str) return true;
      }
      return false;
    }
  }

  void AST_Node_Ref::update_pstate(const ParserState& pstate)
  {
    pstate_.offset += pstate - pstate_ + pstate.offset;
  }

  void AST_Node_Ref::set_pstate_offset(const Offset& offset)
  {
    pstate_.offset = offset;
  }

  inline bool is_ns_eq(const std::string& l, const std::string& r)
  {
    if (l.empty() && r.empty()) return true;
    else if (l.empty() && r == "*") return true;
    else if (r.empty() && l == "*") return true;
    else return l == r;
  }



  bool SimpleSequence_Selector_Ref::operator< (const SimpleSequence_Selector& rhs) const
  {
    size_t L = std::min(length(), rhs.length());
    for (size_t i = 0; i < L; ++i)
    {
      Simple_Selector* l = (*this)[i];
      Simple_Selector* r = rhs[i];
      if (!l && !r) return false;
      else if (!r) return false;
      else if (!l) return true;
      else if (*l != *r)
      { return *l < *r; }
    }
    // just compare the length now
    return length() < rhs.length();
  }

  bool SimpleSequence_Selector_Ref::has_parent_ref()
  {
    for (Simple_Selector* s : *this) {
      if (s && s->has_parent_ref()) return true;
    }
    return false;
  }

  bool SimpleSequence_Selector_Ref::has_real_parent_ref()
  {
    for (Simple_Selector* s : *this) {
      if (s && s->has_real_parent_ref()) return true;
    }
    return false;
  }

  bool Sequence_Selector_Ref::has_parent_ref()
  {
    return (head() && head()->has_parent_ref()) ||
           (tail() && tail()->has_parent_ref());
  }

  bool Sequence_Selector_Ref::has_real_parent_ref()
  {
    return (head() && head()->has_real_parent_ref()) ||
           (tail() && tail()->has_real_parent_ref());
  }

  bool Sequence_Selector_Ref::operator< (const Sequence_Selector& rhs) const
  {
    // const iterators for tails
    Sequence_Selector_Ptr_Const l = this;
    Sequence_Selector_Ptr_Const r = &rhs;
    SimpleSequence_Selector_Ptr l_h = l ? l->head() : 0;
    SimpleSequence_Selector_Ptr r_h = r ? r->head() : 0;
    // process all tails
    while (true)
    {
      // skip empty ancestor first
      if (l && l->is_empty_ancestor())
      {
        l = l->tail();
        l_h = l ? l->head() : 0;
        continue;
      }
      // skip empty ancestor first
      if (r && r->is_empty_ancestor())
      {
        r = r->tail();
        r_h = r ? r->head() : 0;
        continue;
      }
      // check for valid selectors
      if (!l) return !!r;
      if (!r) return false;
      // both are null
      else if (!l_h && !r_h)
      {
        // check combinator after heads
        if (l->combinator() != r->combinator())
        { return l->combinator() < r->combinator(); }
        // advance to next tails
        l = l->tail();
        r = r->tail();
        // fetch the next headers
        l_h = l ? l->head() : 0;
        r_h = r ? r->head() : 0;
      }
      // one side is null
      else if (!r_h) return true;
      else if (!l_h) return false;
      // heads ok and equal
      else if (*l_h == *r_h)
      {
        // check combinator after heads
        if (l->combinator() != r->combinator())
        { return l->combinator() < r->combinator(); }
        // advance to next tails
        l = l->tail();
        r = r->tail();
        // fetch the next headers
        l_h = l ? l->head() : 0;
        r_h = r ? r->head() : 0;
      }
      // heads are not equal
      else return *l_h < *r_h;
    }
    return true;
  }

  bool Sequence_Selector_Ref::operator== (const Sequence_Selector& rhs) const
  {
    // const iterators for tails
    Sequence_Selector_Ptr_Const l = this;
    Sequence_Selector_Ptr_Const r = &rhs;
    SimpleSequence_Selector_Ptr l_h = l ? l->head() : 0;
    SimpleSequence_Selector_Ptr r_h = r ? r->head() : 0;
    // process all tails
    while (true)
    {
      // skip empty ancestor first
      if (l && l->is_empty_ancestor())
      {
        l = l->tail();
        l_h = l ? l->head() : 0;
        continue;
      }
      // skip empty ancestor first
      if (r && r->is_empty_ancestor())
      {
        r = r->tail();
        r_h = r ? r->head() : 0;
        continue;
      }
      // check the pointers
      if (!r) return !l;
      if (!l) return !r;
      // both are null
      if (!l_h && !r_h)
      {
        // check combinator after heads
        if (l->combinator() != r->combinator())
        { return l->combinator() < r->combinator(); }
        // advance to next tails
        l = l->tail();
        r = r->tail();
        // fetch the next heads
        l_h = l ? l->head() : 0;
        r_h = r ? r->head() : 0;
      }
      // fail if only one is null
      else if (!r_h) return !l_h;
      else if (!l_h) return !r_h;
      // heads ok and equal
      else if (*l_h == *r_h)
      {
        // check combinator after heads
        if (l->combinator() != r->combinator())
        { return l->combinator() == r->combinator(); }
        // advance to next tails
        l = l->tail();
        r = r->tail();
        // fetch the next heads
        l_h = l ? l->head() : 0;
        r_h = r ? r->head() : 0;
      }
      // abort
      else break;
    }
    // unreachable
    return false;
  }

  SimpleSequence_Selector_Ptr SimpleSequence_Selector_Ref::unify_with(SimpleSequence_Selector_Ptr rhs, Context& ctx)
  {
    SimpleSequence_Selector_Ptr unified = rhs;
    for (size_t i = 0, L = length(); i < L; ++i)
    {
      if (!unified) break;
      unified = (*this)[i]->unify_with(unified, ctx);
    }
    return unified;
  }

  bool Simple_Selector::operator== (const Simple_Selector& rhs) const
  {
    if (Pseudo_Selector_Ptr_Const lp = dynamic_cast<Pseudo_Selector_Ptr_Const>(this)) return *lp == rhs;
    if (Wrapped_Selector_Ptr_Const lw = dynamic_cast<Wrapped_Selector_Ptr_Const>(this)) return *lw == rhs;
    if (Attribute_Selector_Ptr_Const la = dynamic_cast<Attribute_Selector_Ptr_Const>(this)) return *la == rhs;
    if (is_ns_eq(ns(), rhs.ns()))
    { return name() == rhs.name(); }
    return ns() == rhs.ns();
  }

  bool Simple_Selector::operator< (const Simple_Selector& rhs) const
  {
    if (Pseudo_Selector_Ptr_Const lp = dynamic_cast<Pseudo_Selector_Ptr_Const>(this)) return *lp == rhs;
    if (Wrapped_Selector_Ptr_Const lw = dynamic_cast<Wrapped_Selector_Ptr_Const>(this)) return *lw < rhs;
    if (Attribute_Selector_Ptr_Const la = dynamic_cast<Attribute_Selector_Ptr_Const>(this)) return *la < rhs;
    if (is_ns_eq(ns(), rhs.ns()))
    { return name() < rhs.name(); }
    return ns() < rhs.ns();
  }

  bool CommaSequence_Selector_Ref::operator== (const Selector& rhs) const
  {
    // solve the double dispatch problem by using RTTI information via dynamic cast
    if (CommaSequence_Selector_Ptr_Const ls = dynamic_cast<CommaSequence_Selector_Ptr_Const>(&rhs)) { return *this == *ls; }
    else if (Sequence_Selector_Ptr_Const ls = dynamic_cast<Sequence_Selector_Ptr_Const>(&rhs)) { return *this == *ls; }
    else if (SimpleSequence_Selector_Ptr_Const ls = dynamic_cast<SimpleSequence_Selector_Ptr_Const>(&rhs)) { return *this == *ls; }
    // no compare method
    return this == &rhs;
  }

  // Selector lists can be compared to comma lists
  bool CommaSequence_Selector_Ref::operator==(const Expression& rhs) const
  {
    // solve the double dispatch problem by using RTTI information via dynamic cast
    if (List_Ptr_Const ls = dynamic_cast<List_Ptr_Const>(&rhs)) { return *this == *ls; }
    if (Selector_Ptr_Const ls = dynamic_cast<Selector_Ptr_Const>(&rhs)) { return *this == *ls; }
    // compare invalid (maybe we should error?)
    return false;
  }

  bool CommaSequence_Selector_Ref::operator== (const CommaSequence_Selector& rhs) const
  {
    // for array access
    size_t i = 0, n = 0;
    size_t iL = length();
    size_t nL = rhs.length();
    // create temporary vectors and sort them
    std::vector<Sequence_Selector_Ptr> l_lst = this->elements();
    std::vector<Sequence_Selector_Ptr> r_lst = rhs.elements();
    std::sort(l_lst.begin(), l_lst.end(), cmp_complex_selector());
    std::sort(r_lst.begin(), r_lst.end(), cmp_complex_selector());
    // process loop
    while (true)
    {
      // first check for valid index
      if (i == iL) return iL == nL;
      else if (n == nL) return iL == nL;
      // the access the vector items
      Sequence_Selector_Ptr l = l_lst[i];
      Sequence_Selector_Ptr r = r_lst[n];
      // skip nulls
      if (!l) ++i;
      else if (!r) ++n;
      // do the check
      else if (*l != *r)
      { return false; }
      // advance
      ++i; ++n;
    }
    // no mismatch
    return true;
  }

  SimpleSequence_Selector_Ptr Simple_Selector::unify_with(SimpleSequence_Selector_Ptr rhs, Context& ctx)
  {
    for (size_t i = 0, L = rhs->length(); i < L; ++i)
    { if (to_string(ctx.c_options) == (*rhs)[i]->to_string(ctx.c_options)) return rhs; }

    // check for pseudo elements because they are always last
    size_t i, L;
    bool found = false;
    if (typeid(*this) == typeid(Pseudo_Selector) || typeid(*this) == typeid(Wrapped_Selector))
    {
      for (i = 0, L = rhs->length(); i < L; ++i)
      {
        if ((dynamic_cast<Pseudo_Selector_Ptr>((*rhs)[i]) || dynamic_cast<Wrapped_Selector_Ptr>((*rhs)[i])) && (*rhs)[L-1]->is_pseudo_element())
        { found = true; break; }
      }
    }
    else
    {
      for (i = 0, L = rhs->length(); i < L; ++i)
      {
        if (dynamic_cast<Pseudo_Selector_Ptr>((*rhs)[i]) || dynamic_cast<Wrapped_Selector_Ptr>((*rhs)[i]))
        { found = true; break; }
      }
    }
    if (!found)
    {
      SimpleSequence_Selector_Ptr cpy = SASS_MEMORY_NEW(ctx.mem, SimpleSequence_Selector, *rhs);
      (*cpy) << this;
      return cpy;
    }
    SimpleSequence_Selector_Ptr cpy = SASS_MEMORY_NEW(ctx.mem, SimpleSequence_Selector, rhs->pstate());
    for (size_t j = 0; j < i; ++j)
    { (*cpy) << (*rhs)[j]; }
    (*cpy) << this;
    for (size_t j = i; j < L; ++j)
    { (*cpy) << (*rhs)[j]; }
    return cpy;
  }

  Simple_Selector* Element_Selector::unify_with(Simple_Selector* rhs, Context& ctx)
  {
    // check if ns can be extended
    // true for no ns or universal
    if (has_universal_ns())
    {
      // but dont extend with universal
      // true for valid ns and universal
      if (!rhs->is_universal_ns())
      {
        // creaty the copy inside (avoid unnecessary copies)
        Element_Selector_Ptr ts = SASS_MEMORY_NEW(ctx.mem, Element_Selector, *this);
        // overwrite the name if star is given as name
        if (ts->name() == "*") { ts->name(rhs->name()); }
        // now overwrite the namespace name and flag
        ts->ns(rhs->ns()); ts->has_ns(rhs->has_ns());
        // return copy
        return ts;
      }
    }
    // namespace may changed, check the name now
    // overwrite star (but not with another star)
    if (name() == "*" && rhs->name() != "*")
    {
      // creaty the copy inside (avoid unnecessary copies)
      Element_Selector_Ptr ts = SASS_MEMORY_NEW(ctx.mem, Element_Selector, *this);
      // simply set the new name
      ts->name(rhs->name());
      // return copy
      return ts;
    }
    // return original
    return this;
  }

  SimpleSequence_Selector_Ptr Element_Selector::unify_with(SimpleSequence_Selector_Ptr rhs, Context& ctx)
  {
    // TODO: handle namespaces

    // if the rhs is empty, just return a copy of this
    if (rhs->length() == 0) {
      SimpleSequence_Selector_Ptr cpy = SASS_MEMORY_NEW(ctx.mem, SimpleSequence_Selector, rhs->pstate());
      (*cpy) << this;
      return cpy;
    }

    Simple_Selector* rhs_0 = (*rhs)[0];
    // otherwise, this is a tag name
    if (name() == "*")
    {
      if (typeid(*rhs_0) == typeid(Element_Selector))
      {
        // if rhs is universal, just return this tagname + rhs's qualifiers
        SimpleSequence_Selector_Ptr cpy = SASS_MEMORY_NEW(ctx.mem, SimpleSequence_Selector, *rhs);
        Element_Selector_Ptr ts = static_cast<Element_Selector_Ptr>(rhs_0);
        (*cpy)[0] = this->unify_with(ts, ctx);
        return cpy;
      }
      else if (dynamic_cast<Class_Selector_Ptr>(rhs_0) || dynamic_cast<Id_Selector_Ptr>(rhs_0)) {
        // qualifier is `.class`, so we can prefix with `ns|*.class`
        SimpleSequence_Selector_Ptr cpy = SASS_MEMORY_NEW(ctx.mem, SimpleSequence_Selector, rhs->pstate());
        if (has_ns() && !rhs_0->has_ns()) {
          if (ns() != "*") (*cpy) << this;
        }
        for (size_t i = 0, L = rhs->length(); i < L; ++i)
        { (*cpy) << (*rhs)[i]; }
        return cpy;
      }


      return rhs;
    }

    if (typeid(*rhs_0) == typeid(Element_Selector))
    {
      // if rhs is universal, just return this tagname + rhs's qualifiers
      if (rhs_0->name() != "*" && rhs_0->ns() != "*" && rhs_0->name() != name()) return 0;
      // otherwise create new compound and unify first simple selector
      SimpleSequence_Selector_Ptr copy = SASS_MEMORY_NEW(ctx.mem, SimpleSequence_Selector, *rhs);
      (*copy)[0] = this->unify_with(rhs_0, ctx);
      return copy;

    }
    // else it's a tag name and a bunch of qualifiers -- just append them
    SimpleSequence_Selector_Ptr cpy = SASS_MEMORY_NEW(ctx.mem, SimpleSequence_Selector, rhs->pstate());
    if (name() != "*") (*cpy) << this;
    (*cpy) += rhs;
    return cpy;
  }

  SimpleSequence_Selector_Ptr Class_Selector_Ref::unify_with(SimpleSequence_Selector_Ptr rhs, Context& ctx)
  {
    rhs->has_line_break(has_line_break());
    return Simple_Selector::unify_with(rhs, ctx);
  }

  SimpleSequence_Selector_Ptr Id_Selector_Ref::unify_with(SimpleSequence_Selector_Ptr rhs, Context& ctx)
  {
    for (size_t i = 0, L = rhs->length(); i < L; ++i)
    {
      Simple_Selector* rhs_i = (*rhs)[i];
      if (typeid(*rhs_i) == typeid(Id_Selector) && static_cast<Id_Selector_Ptr>(rhs_i)->name() != name()) {
        return 0;
      }
    }
    rhs->has_line_break(has_line_break());
    return Simple_Selector::unify_with(rhs, ctx);
  }

  SimpleSequence_Selector_Ptr Pseudo_Selector_Ref::unify_with(SimpleSequence_Selector_Ptr rhs, Context& ctx)
  {
    if (is_pseudo_element())
    {
      for (size_t i = 0, L = rhs->length(); i < L; ++i)
      {
        Simple_Selector* rhs_i = (*rhs)[i];
        if (typeid(*rhs_i) == typeid(Pseudo_Selector) &&
            static_cast<Pseudo_Selector_Ptr>(rhs_i)->is_pseudo_element() &&
            static_cast<Pseudo_Selector_Ptr>(rhs_i)->name() != name())
        { return 0; }
      }
    }
    return Simple_Selector::unify_with(rhs, ctx);
  }

  bool Attribute_Selector_Ref::operator< (const Attribute_Selector& rhs) const
  {
    if (is_ns_eq(ns(), rhs.ns())) {
      if (name() == rhs.name()) {
        if (matcher() == rhs.matcher()) {
          return value() < rhs.value();
        } else { return matcher() < rhs.matcher(); }
      } else { return name() < rhs.name(); }
    }
    else return false;
  }

  bool Attribute_Selector_Ref::operator< (const Simple_Selector& rhs) const
  {
    if (Attribute_Selector_Ptr_Const w = dynamic_cast<Attribute_Selector_Ptr_Const>(&rhs))
    {
      return *this < *w;
    }
    if (is_ns_eq(ns(), rhs.ns()))
    { return name() < rhs.name(); }
    return ns() < rhs.ns();
  }

  bool Attribute_Selector_Ref::operator== (const Attribute_Selector& rhs) const
  {
    if (is_ns_eq(ns(), rhs.ns()) && name() == rhs.name())
    { return matcher() == rhs.matcher() && value() == rhs.value(); }
    else return false;
  }

  bool Attribute_Selector_Ref::operator== (const Simple_Selector& rhs) const
  {
    if (Attribute_Selector_Ptr_Const w = dynamic_cast<Attribute_Selector_Ptr_Const>(&rhs))
    {
      return *this == *w;
    }
    if (is_ns_eq(ns(), rhs.ns()))
    { return name() == rhs.name(); }
    return ns() == rhs.ns();
  }

  bool Pseudo_Selector_Ref::operator== (const Pseudo_Selector& rhs) const
  {
    if (is_ns_eq(ns(), rhs.ns()) && name() == rhs.name())
    {
      Expression_Ptr lhs_ex = expression();
      Expression_Ptr rhs_ex = rhs.expression();
      if (rhs_ex && lhs_ex) return *lhs_ex == *rhs_ex;
      else return lhs_ex == rhs_ex;
    }
    else return false;
  }

  bool Pseudo_Selector_Ref::operator== (const Simple_Selector& rhs) const
  {
    if (Pseudo_Selector_Ptr_Const w = dynamic_cast<Pseudo_Selector_Ptr_Const>(&rhs))
    {
      return *this == *w;
    }
    if (is_ns_eq(ns(), rhs.ns()))
    { return name() == rhs.name(); }
    return ns() == rhs.ns();
  }

  bool Pseudo_Selector_Ref::operator< (const Pseudo_Selector& rhs) const
  {
    if (is_ns_eq(ns(), rhs.ns()) && name() == rhs.name())
    { return *(expression()) < *(rhs.expression()); }
    if (is_ns_eq(ns(), rhs.ns()))
    { return name() < rhs.name(); }
    return ns() < rhs.ns();
  }

  bool Pseudo_Selector_Ref::operator< (const Simple_Selector& rhs) const
  {
    if (Pseudo_Selector_Ptr_Const w = dynamic_cast<Pseudo_Selector_Ptr_Const>(&rhs))
    {
      return *this < *w;
    }
    if (is_ns_eq(ns(), rhs.ns()))
    { return name() < rhs.name(); }
    return ns() < rhs.ns();
  }

  bool Wrapped_Selector_Ref::operator== (const Wrapped_Selector& rhs) const
  {
    if (is_ns_eq(ns(), rhs.ns()) && name() == rhs.name())
    { return *(selector()) == *(rhs.selector()); }
    else return false;
  }

  bool Wrapped_Selector_Ref::operator== (const Simple_Selector& rhs) const
  {
    if (Wrapped_Selector_Ptr_Const w = dynamic_cast<Wrapped_Selector_Ptr_Const>(&rhs))
    {
      return *this == *w;
    }
    if (is_ns_eq(ns(), rhs.ns()))
    { return name() == rhs.name(); }
    return ns() == rhs.ns();
  }

  bool Wrapped_Selector_Ref::operator< (const Wrapped_Selector& rhs) const
  {
    if (is_ns_eq(ns(), rhs.ns()) && name() == rhs.name())
    { return *(selector()) < *(rhs.selector()); }
    if (is_ns_eq(ns(), rhs.ns()))
    { return name() < rhs.name(); }
    return ns() < rhs.ns();
  }

  bool Wrapped_Selector_Ref::operator< (const Simple_Selector& rhs) const
  {
    if (Wrapped_Selector_Ptr_Const w = dynamic_cast<Wrapped_Selector_Ptr_Const>(&rhs))
    {
      return *this < *w;
    }
    if (is_ns_eq(ns(), rhs.ns()))
    { return name() < rhs.name(); }
    return ns() < rhs.ns();
  }

  bool Wrapped_Selector_Ref::is_superselector_of(Wrapped_Selector_Ptr sub)
  {
    if (this->name() != sub->name()) return false;
    if (this->name() == ":current") return false;
    if (CommaSequence_Selector_Ptr rhs_list = dynamic_cast<CommaSequence_Selector_Ptr>(sub->selector())) {
      if (CommaSequence_Selector_Ptr lhs_list = dynamic_cast<CommaSequence_Selector_Ptr>(selector())) {
        return lhs_list->is_superselector_of(rhs_list);
      }
      error("is_superselector expected a CommaSequence_Selector", sub->pstate());
    } else {
      error("is_superselector expected a CommaSequence_Selector", sub->pstate());
    }
    return false;
  }

  bool SimpleSequence_Selector_Ref::is_superselector_of(CommaSequence_Selector_Ptr rhs, std::string wrapped)
  {
    for (Sequence_Selector_Ptr item : rhs->elements()) {
      if (is_superselector_of(item, wrapped)) return true;
    }
    return false;
  }

  bool SimpleSequence_Selector_Ref::is_superselector_of(Sequence_Selector_Ptr rhs, std::string wrapped)
  {
    if (rhs->head()) return is_superselector_of(rhs->head(), wrapped);
    return false;
  }

  bool SimpleSequence_Selector_Ref::is_superselector_of(SimpleSequence_Selector_Ptr rhs, std::string wrapping)
  {
    SimpleSequence_Selector_Ptr lhs = this;
    Simple_Selector* lbase = lhs->base();
    Simple_Selector* rbase = rhs->base();

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

    std::set<std::string> lset, rset;

    if (lbase && rbase)
    {
      if (lbase->to_string() == rbase->to_string()) {
        for (size_t i = 1, L = length(); i < L; ++i)
        { lset.insert((*this)[i]->to_string()); }
        for (size_t i = 1, L = rhs->length(); i < L; ++i)
        { rset.insert((*rhs)[i]->to_string()); }
        return includes(rset.begin(), rset.end(), lset.begin(), lset.end());
      }
      return false;
    }

    for (size_t i = 0, iL = length(); i < iL; ++i)
    {
      Selector_Ptr lhs = (*this)[i];
      // very special case for wrapped matches selector
      if (Wrapped_Selector_Ptr wrapped = dynamic_cast<Wrapped_Selector_Ptr>(lhs)) {
        if (wrapped->name() == ":not") {
          if (CommaSequence_Selector_Ptr not_list = dynamic_cast<CommaSequence_Selector_Ptr>(wrapped->selector())) {
            if (not_list->is_superselector_of(rhs, wrapped->name())) return false;
          } else {
            throw std::runtime_error("wrapped not selector is not a list");
          }
        }
        if (wrapped->name() == ":matches" || wrapped->name() == ":-moz-any") {
          lhs = wrapped->selector();
          if (CommaSequence_Selector_Ptr list = dynamic_cast<CommaSequence_Selector_Ptr>(wrapped->selector())) {
            if (SimpleSequence_Selector_Ptr comp = dynamic_cast<SimpleSequence_Selector_Ptr>(rhs)) {
              if (!wrapping.empty() && wrapping != wrapped->name()) return false;
              if (wrapping.empty() || wrapping != wrapped->name()) {;
                if (list->is_superselector_of(comp, wrapped->name())) return true;
              }
            }
          }
        }
        Simple_Selector* rhs_sel = rhs->elements().size() > i ? (*rhs)[i] : 0;
        if (Wrapped_Selector_Ptr wrapped_r = dynamic_cast<Wrapped_Selector_Ptr>(rhs_sel)) {
          if (wrapped->name() == wrapped_r->name()) {
          if (wrapped->is_superselector_of(wrapped_r)) {
             continue;
             rset.insert(lhs->to_string());

          }}
        }
      }
      // match from here on as strings
      lset.insert(lhs->to_string());
    }

    for (size_t n = 0, nL = rhs->length(); n < nL; ++n)
    {
      auto r = (*rhs)[n];
      if (Wrapped_Selector_Ptr wrapped = dynamic_cast<Wrapped_Selector_Ptr>(r)) {
        if (wrapped->name() == ":not") {
          if (CommaSequence_Selector_Ptr ls = dynamic_cast<CommaSequence_Selector_Ptr>(wrapped->selector())) {
            ls->remove_parent_selectors();
            if (is_superselector_of(ls, wrapped->name())) return false;
          }
        }
        if (wrapped->name() == ":matches" || wrapped->name() == ":-moz-any") {
          if (!wrapping.empty()) {
            if (wrapping != wrapped->name()) return false;
          }
          if (CommaSequence_Selector_Ptr ls = dynamic_cast<CommaSequence_Selector_Ptr>(wrapped->selector())) {
            ls->remove_parent_selectors();
            return (is_superselector_of(ls, wrapped->name()));
          }
        }
      }
      rset.insert(r->to_string());
    }

    //for (auto l : lset) { cerr << "l: " << l << endl; }
    //for (auto r : rset) { cerr << "r: " << r << endl; }

    if (lset.empty()) return true;
    // return true if rset contains all the elements of lset
    return includes(rset.begin(), rset.end(), lset.begin(), lset.end());

  }

  // create complex selector (ancestor of) from compound selector
  Sequence_Selector_Ptr SimpleSequence_Selector_Ref::to_complex(Memory_Manager& mem)
  {
    // create an intermediate complex selector
    return SASS_MEMORY_NEW(mem, Sequence_Selector,
                           pstate(),
                           Sequence_Selector_Ref::ANCESTOR_OF,
                           this,
                           0);
  }

  CommaSequence_Selector_Ptr Sequence_Selector_Ref::unify_with(Sequence_Selector_Ptr other, Context& ctx)
  {

    // get last tails (on the right side)
    Sequence_Selector_Ptr l_last = this->last();
    Sequence_Selector_Ptr r_last = other->last();

    // check valid pointers (assertion)
    SASS_ASSERT(l_last, "lhs is null");
    SASS_ASSERT(r_last, "rhs is null");

    // Not sure about this check, but closest way I could check
    // was to see if this is a ruby 'SimpleSequence' equivalent.
    // It seems to do the job correctly as some specs react to this
    if (l_last->combinator() != Combinator::ANCESTOR_OF) return 0;
    if (r_last->combinator() != Combinator::ANCESTOR_OF ) return 0;

    // get the headers for the last tails
    SimpleSequence_Selector_Ptr l_last_head = l_last->head();
    SimpleSequence_Selector_Ptr r_last_head = r_last->head();

    // check valid head pointers (assertion)
    SASS_ASSERT(l_last_head, "lhs head is null");
    SASS_ASSERT(r_last_head, "rhs head is null");

    // get the unification of the last compound selectors
    SimpleSequence_Selector_Ptr unified = r_last_head->unify_with(l_last_head, ctx);

    // abort if we could not unify heads
    if (unified == 0) return 0;

    // check for universal (star: `*`) selector
    bool is_universal = l_last_head->is_universal() ||
                        r_last_head->is_universal();

    if (is_universal)
    {
      // move the head
      l_last->head(0);
      r_last->head(unified);
    }

    // create nodes from both selectors
    Node lhsNode = complexSelectorToNode(this, ctx);
    Node rhsNode = complexSelectorToNode(other, ctx);

    // overwrite universal base
    if (!is_universal)
    {
      // create some temporaries to convert to node
      Sequence_Selector_Ptr fake = unified->to_complex(ctx.mem);
      Node unified_node = complexSelectorToNode(fake, ctx);
      // add to permutate the list?
      rhsNode.plus(unified_node);
    }

    // do some magic we inherit from node and extend
    Node node = Extend::subweave(lhsNode, rhsNode, ctx);
    CommaSequence_Selector_Ptr result = SASS_MEMORY_NEW(ctx.mem, CommaSequence_Selector, pstate());
    NodeDequePtr col = node.collection(); // move from collection to list
    for (NodeDeque::iterator it = col->begin(), end = col->end(); it != end; it++)
    { (*result) << nodeToComplexSelector(Node::naiveTrim(*it, ctx), ctx); }

    // only return if list has some entries
    return result->length() ? result : 0;

  }

  bool SimpleSequence_Selector_Ref::operator== (const SimpleSequence_Selector& rhs) const
  {
    // for array access
    size_t i = 0, n = 0;
    size_t iL = length();
    size_t nL = rhs.length();
    // create temporary vectors and sort them
    std::vector<Simple_Selector*> l_lst = this->elements();
    std::vector<Simple_Selector*> r_lst = rhs.elements();
    std::sort(l_lst.begin(), l_lst.end(), cmp_simple_selector());
    std::sort(r_lst.begin(), r_lst.end(), cmp_simple_selector());
    // process loop
    while (true)
    {
      // first check for valid index
      if (i == iL) return iL == nL;
      else if (n == nL) return iL == nL;
      // the access the vector items
      Simple_Selector* l = l_lst[i];
      Simple_Selector* r = r_lst[n];
      // skip nulls
      if (!l) ++i;
      if (!r) ++n;
      // do the check now
      else if (*l != *r)
      { return false; }
      // advance now
      ++i; ++n;
    }
    // no mismatch
    return true;
  }

  bool Sequence_Selector_Pointer_Compare::operator() (Sequence_Selector_Ptr_Const const pLeft, Sequence_Selector_Ptr_Const const pRight) const {
    return *pLeft < *pRight;
  }

  bool Sequence_Selector_Ref::is_superselector_of(SimpleSequence_Selector_Ptr rhs, std::string wrapping)
  {
    return last()->head() && last()->head()->is_superselector_of(rhs, wrapping);
  }

  bool Sequence_Selector_Ref::is_superselector_of(Sequence_Selector_Ptr rhs, std::string wrapping)
  {
    Sequence_Selector_Ptr lhs = this;
    // check for selectors with leading or trailing combinators
    if (!lhs->head() || !rhs->head())
    { return false; }
    Sequence_Selector_Ptr_Const l_innermost = lhs->innermost();
    if (l_innermost->combinator() != Sequence_Selector_Ref::ANCESTOR_OF)
    { return false; }
    Sequence_Selector_Ptr_Const r_innermost = rhs->innermost();
    if (r_innermost->combinator() != Sequence_Selector_Ref::ANCESTOR_OF)
    { return false; }
    // more complex (i.e., longer) selectors are always more specific
    size_t l_len = lhs->length(), r_len = rhs->length();
    if (l_len > r_len)
    { return false; }

    if (l_len == 1)
    { return lhs->head()->is_superselector_of(rhs->last()->head(), wrapping); }

    // we have to look one tail deeper, since we cary the
    // combinator around for it (which is important here)
    if (rhs->tail() && lhs->tail() && combinator() != Sequence_Selector_Ref::ANCESTOR_OF) {
      Sequence_Selector_Ptr lhs_tail = lhs->tail();
      Sequence_Selector_Ptr rhs_tail = rhs->tail();
      if (lhs_tail->combinator() != rhs_tail->combinator()) return false;
      if (lhs_tail->head() && !rhs_tail->head()) return false;
      if (!lhs_tail->head() && rhs_tail->head()) return false;
      if (lhs_tail->head() && rhs_tail->head()) {
        if (!lhs_tail->head()->is_superselector_of(rhs_tail->head())) return false;
      }
    }

    bool found = false;
    Sequence_Selector_Ptr marker = rhs;
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
    if (lhs->combinator() != Sequence_Selector_Ref::ANCESTOR_OF)
    {
      if (marker->combinator() == Sequence_Selector_Ref::ANCESTOR_OF)
      { return false; }
      if (!(lhs->combinator() == Sequence_Selector_Ref::PRECEDES ? marker->combinator() != Sequence_Selector_Ref::PARENT_OF : lhs->combinator() == marker->combinator()))
      { return false; }
      return lhs->tail()->is_superselector_of(marker->tail());
    }
    else if (marker->combinator() != Sequence_Selector_Ref::ANCESTOR_OF)
    {
      if (marker->combinator() != Sequence_Selector_Ref::PARENT_OF)
      { return false; }
      return lhs->tail()->is_superselector_of(marker->tail());
    }
    else
    {
      return lhs->tail()->is_superselector_of(marker->tail());
    }
    // catch-all
    return false;
  }

  size_t Sequence_Selector_Ref::length() const
  {
    // TODO: make this iterative
    if (!tail()) return 1;
    return 1 + tail()->length();
  }

  Sequence_Selector_Ptr Sequence_Selector_Ref::context(Context& ctx)
  {
    if (!tail()) return 0;
    if (!head()) return tail()->context(ctx);
    Sequence_Selector_Ptr cpy = SASS_MEMORY_NEW(ctx.mem, Sequence_Selector, pstate(), combinator(), head(), tail()->context(ctx));
    cpy->media_block(media_block());
    return cpy;
  }

  // append another complex selector at the end
  // check if we need to append some headers
  // then we need to check for the combinator
  // only then we can safely set the new tail
  void Sequence_Selector_Ref::append(Context& ctx, Sequence_Selector_Ptr ss)
  {

    Sequence_Selector_Ptr t = ss->tail();
    Combinator c = ss->combinator();
    String_Ptr r = ss->reference();
    SimpleSequence_Selector_Ptr h = ss->head();

    if (ss->has_line_feed()) has_line_feed(true);
    if (ss->has_line_break()) has_line_break(true);

    // append old headers
    if (h && h->length()) {
      if (last()->combinator() != ANCESTOR_OF && c != ANCESTOR_OF) {
        error("Invalid parent selector", pstate_);
      } else if (last()->head_ && last()->head_->length()) {
        SimpleSequence_Selector_Ptr rh = last()->head();
        size_t i = 0, L = h->length();
        if (dynamic_cast<Element_Selector_Ptr>(h->first())) {
          if (Class_Selector_Ptr sq = dynamic_cast<Class_Selector_Ptr>(rh->last())) {
            Class_Selector_Ptr sqs = SASS_MEMORY_NEW(ctx.mem, Class_Selector, *sq);
            sqs->name(sqs->name() + (*h)[0]->name());
            sqs->pstate((*h)[0]->pstate());
            (*rh)[rh->length()-1] = sqs;
            rh->pstate(h->pstate());
            for (i = 1; i < L; ++i) *rh << (*h)[i];
          } else if (Id_Selector_Ptr sq = dynamic_cast<Id_Selector_Ptr>(rh->last())) {
            Id_Selector_Ptr sqs = SASS_MEMORY_NEW(ctx.mem, Id_Selector, *sq);
            sqs->name(sqs->name() + (*h)[0]->name());
            sqs->pstate((*h)[0]->pstate());
            (*rh)[rh->length()-1] = sqs;
            rh->pstate(h->pstate());
            for (i = 1; i < L; ++i) *rh << (*h)[i];
          } else if (Element_Selector_Ptr ts = dynamic_cast<Element_Selector_Ptr>(rh->last())) {
            Element_Selector_Ptr tss = SASS_MEMORY_NEW(ctx.mem, Element_Selector, *ts);
            tss->name(tss->name() + (*h)[0]->name());
            tss->pstate((*h)[0]->pstate());
            (*rh)[rh->length()-1] = tss;
            rh->pstate(h->pstate());
            for (i = 1; i < L; ++i) *rh << (*h)[i];
          } else if (Placeholder_Selector_Ptr ps = dynamic_cast<Placeholder_Selector_Ptr>(rh->last())) {
            Placeholder_Selector_Ptr pss = SASS_MEMORY_NEW(ctx.mem, Placeholder_Selector, *ps);
            pss->name(pss->name() + (*h)[0]->name());
            pss->pstate((*h)[0]->pstate());
            (*rh)[rh->length()-1] = pss;
            rh->pstate(h->pstate());
            for (i = 1; i < L; ++i) *rh << (*h)[i];
          } else {
            *last()->head_ += h;
          }
        } else {
          *last()->head_ += h;
        }
      } else {
        *last()->head_ += h;
      }
    } else {
      // std::cerr << "has no or empty head\n";
    }

    if (last()) {
      if (last()->combinator() != ANCESTOR_OF && c != ANCESTOR_OF) {
        Sequence_Selector_Ptr inter = SASS_MEMORY_NEW(ctx.mem, Sequence_Selector, pstate());
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

  CommaSequence_Selector_Ptr CommaSequence_Selector_Ref::resolve_parent_refs(Context& ctx, CommaSequence_Selector_Ptr ps, bool implicit_parent)
  {
    if (!this->has_parent_ref()/* && !implicit_parent*/) return this;
    CommaSequence_Selector_Ptr ss = SASS_MEMORY_NEW(ctx.mem, CommaSequence_Selector, pstate());
    for (size_t pi = 0, pL = ps->length(); pi < pL; ++pi) {
      CommaSequence_Selector_Ptr list = SASS_MEMORY_NEW(ctx.mem, CommaSequence_Selector, pstate());
      *list << (*ps)[pi];
      for (size_t si = 0, sL = this->length(); si < sL; ++si) {
        *ss += (*this)[si]->resolve_parent_refs(ctx, list, implicit_parent);
      }
    }
    return ss;
  }

  CommaSequence_Selector_Ptr Sequence_Selector_Ref::resolve_parent_refs(Context& ctx, CommaSequence_Selector_Ptr parents, bool implicit_parent)
  {
    Sequence_Selector_Ptr tail = this->tail();
    SimpleSequence_Selector_Ptr head = this->head();

    if (!this->has_real_parent_ref() && !implicit_parent) {
      CommaSequence_Selector_Ptr retval = SASS_MEMORY_NEW(ctx.mem, CommaSequence_Selector, pstate());
      *retval << this;
      return retval;
    }

    // first resolve_parent_refs the tail (which may return an expanded list)
    CommaSequence_Selector_Ptr tails = tail ? tail->resolve_parent_refs(ctx, parents, implicit_parent) : 0;

    if (head && head->length() > 0) {

      CommaSequence_Selector_Ptr retval = 0;
      // we have a parent selector in a simple compound list
      // mix parent complex selector into the compound list
      if (dynamic_cast<Parent_Selector_Ptr>((*head)[0])) {
        retval = SASS_MEMORY_NEW(ctx.mem, CommaSequence_Selector, pstate());
        if (parents && parents->length()) {
          if (tails && tails->length() > 0) {
            for (size_t n = 0, nL = tails->length(); n < nL; ++n) {
              for (size_t i = 0, iL = parents->length(); i < iL; ++i) {
                Sequence_Selector_Ptr t = (*tails)[n];
                Sequence_Selector_Ptr parent = (*parents)[i];
                Sequence_Selector_Ptr s = parent->cloneFully(ctx);
                Sequence_Selector_Ptr ss = this->clone(ctx);
                ss->tail(t ? t->clone(ctx) : 0);
                SimpleSequence_Selector_Ptr h = head_->clone(ctx);
                // remove parent selector from sequence
                if (h->length()) h->erase(h->begin());
                ss->head(h->length() ? h : 0);
                // adjust for parent selector (1 char)
                if (h->length()) {
                  ParserState state((*h)[0]->pstate());
                  state.offset.column += 1;
                  state.column -= 1;
                  (*h)[0]->pstate(state);
                }
                // keep old parser state
                s->pstate(pstate());
                // append new tail
                s->append(ctx, ss);
                *retval << s;
              }
            }
          }
          // have no tails but parents
          // loop above is inside out
          else {
            for (size_t i = 0, iL = parents->length(); i < iL; ++i) {
              Sequence_Selector_Ptr parent = (*parents)[i];
              Sequence_Selector_Ptr s = parent->cloneFully(ctx);
              Sequence_Selector_Ptr ss = this->clone(ctx);
              // this is only if valid if the parent has no trailing op
              // otherwise we cannot append more simple selectors to head
              if (parent->last()->combinator() != ANCESTOR_OF) {
                throw Exception::InvalidParent(parent, ss);
              }
              ss->tail(tail ? tail->clone(ctx) : 0);
              SimpleSequence_Selector_Ptr h = head_->clone(ctx);
              // remove parent selector from sequence
              if (h->length()) h->erase(h->begin());
              ss->head(h->length() ? h : 0);
              // \/ IMO ruby sass bug \/
              ss->has_line_feed(false);
              // adjust for parent selector (1 char)
              if (h->length()) {
                ParserState state((*h)[0]->pstate());
                state.offset.column += 1;
                state.column -= 1;
                (*h)[0]->pstate(state);
              }
              // keep old parser state
              s->pstate(pstate());
              // append new tail
              s->append(ctx, ss);
              *retval << s;
            }
          }
        }
        // have no parent but some tails
        else {
          if (tails && tails->length() > 0) {
            for (size_t n = 0, nL = tails->length(); n < nL; ++n) {
              Sequence_Selector_Ptr cpy = this->clone(ctx);
              cpy->tail((*tails)[n]->cloneFully(ctx));
              cpy->head(SASS_MEMORY_NEW(ctx.mem, SimpleSequence_Selector, head->pstate()));
              for (size_t i = 1, L = this->head()->length(); i < L; ++i)
                *cpy->head() << (*this->head())[i];
              if (!cpy->head()->length()) cpy->head(0);
              *retval << cpy->skip_empty_reference();
            }
          }
          // have no parent nor tails
          else {
            Sequence_Selector_Ptr cpy = this->clone(ctx);
            cpy->head(SASS_MEMORY_NEW(ctx.mem, SimpleSequence_Selector, head->pstate()));
            for (size_t i = 1, L = this->head()->length(); i < L; ++i)
              *cpy->head() << (*this->head())[i];
            if (!cpy->head()->length()) cpy->head(0);
            *retval << cpy->skip_empty_reference();
          }
        }
      }
      // no parent selector in head
      else {
        retval = this->tails(ctx, tails);
      }

      for (Simple_Selector* ss : *head) {
        if (Wrapped_Selector_Ptr ws = dynamic_cast<Wrapped_Selector_Ptr>(ss)) {
          if (CommaSequence_Selector_Ptr sl = dynamic_cast<CommaSequence_Selector_Ptr>(ws->selector())) {
            if (parents) ws->selector(sl->resolve_parent_refs(ctx, parents, implicit_parent));
          }
        }
      }

      return retval;

    }
    // has no head
    else {
      return this->tails(ctx, tails);
    }

    // unreachable
    return 0;
  }

  CommaSequence_Selector_Ptr Sequence_Selector_Ref::tails(Context& ctx, CommaSequence_Selector_Ptr tails)
  {
    CommaSequence_Selector_Ptr rv = SASS_MEMORY_NEW(ctx.mem, CommaSequence_Selector, pstate_);
    if (tails && tails->length()) {
      for (size_t i = 0, iL = tails->length(); i < iL; ++i) {
        Sequence_Selector_Ptr pr = this->clone(ctx);
        pr->tail((*tails)[i]);
        *rv << pr;
      }
    }
    else {
      *rv << this;
    }
    return rv;
  }

  // return the last tail that is defined
  Sequence_Selector_Ptr Sequence_Selector_Ref::first()
  {
    // declare variables used in loop
    Sequence_Selector_Ptr cur = this;
    SimpleSequence_Selector_Ptr_Const head;
    // processing loop
    while (cur)
    {
      // get the head
      head = cur->head_;
      // abort (and return) if it is not a parent selector
      if (!head || head->length() != 1 || !dynamic_cast<Parent_Selector_Ptr>((*head)[0])) {
        break;
      }
      // advance to next
      cur = cur->tail_;
    }
    // result
    return cur;
  }

  // return the last tail that is defined
  Sequence_Selector_Ptr_Const Sequence_Selector_Ref::first() const
  {
    // declare variables used in loop
    Sequence_Selector_Ptr_Const cur = this->tail_;
    SimpleSequence_Selector_Ptr_Const head = head_;
    // processing loop
    while (cur)
    {
      // get the head
      head = cur->head_;
      // check for single parent ref
      if (head && head->length() == 1)
      {
        // abort (and return) if it is not a parent selector
        if (!dynamic_cast<Parent_Selector_Ptr>((*head)[0])) break;
      }
      // advance to next
      cur = cur->tail_;
    }
    // result
    return cur;
  }

  // return the last tail that is defined
  Sequence_Selector_Ptr Sequence_Selector_Ref::last()
  {
    // ToDo: implement with a while loop
    return tail_? tail_->last() : this;
  }

  // return the last tail that is defined
  Sequence_Selector_Ptr_Const Sequence_Selector_Ref::last() const
  {
    // ToDo: implement with a while loop
    return tail_? tail_->last() : this;
  }


  Sequence_Selector_Ref::Combinator Sequence_Selector_Ref::clear_innermost()
  {
    Combinator c;
    if (!tail() || tail()->tail() == 0)
    { c = combinator(); combinator(ANCESTOR_OF); tail(0); }
    else
    { c = tail()->clear_innermost(); }
    return c;
  }

  void Sequence_Selector_Ref::set_innermost(Sequence_Selector_Ptr val, Combinator c)
  {
    if (!tail())
    { tail(val); combinator(c); }
    else
    { tail()->set_innermost(val, c); }
  }

  Sequence_Selector_Ptr Sequence_Selector_Ref::clone(Context& ctx) const
  {
    Sequence_Selector_Ptr cpy = SASS_MEMORY_NEW(ctx.mem, Sequence_Selector, *this);
    cpy->is_optional(this->is_optional());
    cpy->media_block(this->media_block());
    if (tail()) cpy->tail(tail()->clone(ctx));
    return cpy;
  }

  Sequence_Selector_Ptr Sequence_Selector_Ref::cloneFully(Context& ctx) const
  {
    Sequence_Selector_Ptr cpy = SASS_MEMORY_NEW(ctx.mem, Sequence_Selector, *this);
    cpy->is_optional(this->is_optional());
    cpy->media_block(this->media_block());
    if (head()) {
      cpy->head(head()->clone(ctx));
    }

    if (tail()) {
      cpy->tail(tail()->cloneFully(ctx));
    }

    return cpy;
  }

  SimpleSequence_Selector_Ptr SimpleSequence_Selector_Ref::clone(Context& ctx) const
  {
    SimpleSequence_Selector_Ptr cpy = SASS_MEMORY_NEW(ctx.mem, SimpleSequence_Selector, *this);
    cpy->is_optional(this->is_optional());
    cpy->media_block(this->media_block());
    cpy->extended(this->extended());
    return cpy;
  }

  CommaSequence_Selector_Ptr CommaSequence_Selector_Ref::clone(Context& ctx) const
  {
    CommaSequence_Selector_Ptr cpy = SASS_MEMORY_NEW(ctx.mem, CommaSequence_Selector, *this);
    cpy->is_optional(this->is_optional());
    cpy->media_block(this->media_block());
    return cpy;
  }

  CommaSequence_Selector_Ptr CommaSequence_Selector_Ref::cloneFully(Context& ctx) const
  {
    CommaSequence_Selector_Ptr cpy = SASS_MEMORY_NEW(ctx.mem, CommaSequence_Selector, pstate());
    cpy->is_optional(this->is_optional());
    cpy->media_block(this->media_block());
    for (size_t i = 0, L = length(); i < L; ++i) {
      *cpy << (*this)[i]->cloneFully(ctx);
    }
    return cpy;
  }

  /* not used anymore - remove?
  Placeholder_Selector_Ptr Selector::find_placeholder()
  {
    return 0;
  }*/

  // remove parent selector references
  // basically unwraps parsed selectors
  void CommaSequence_Selector_Ref::remove_parent_selectors()
  {
    // Check every rhs selector against left hand list
    for(size_t i = 0, L = length(); i < L; ++i) {
      if (!(*this)[i]->head()) continue;
      if ((*this)[i]->head()->is_empty_reference()) {
        // simply move to the next tail if we have "no" combinator
        if ((*this)[i]->combinator() == Sequence_Selector_Ref::ANCESTOR_OF) {
          if ((*this)[i]->tail() != NULL) {
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

  bool CommaSequence_Selector_Ref::has_parent_ref()
  {
    for (Sequence_Selector_Ptr s : *this) {
      if (s && s->has_parent_ref()) return true;
    }
    return false;
  }

  bool CommaSequence_Selector_Ref::has_real_parent_ref()
  {
    for (Sequence_Selector_Ptr s : *this) {
      if (s && s->has_real_parent_ref()) return true;
    }
    return false;
  }

  bool Selector_Schema_Ref::has_parent_ref()
  {
    if (String_Schema_Ptr schema = dynamic_cast<String_Schema_Ptr>(contents())) {
      return schema->length() > 0 && dynamic_cast<Parent_Selector_Ptr>(schema->at(0)) != NULL;
    }
    return false;
  }

  bool Selector_Schema_Ref::has_real_parent_ref()
  {
    if (String_Schema_Ptr schema = dynamic_cast<String_Schema_Ptr>(contents())) {
      Parent_Selector_Ptr p = dynamic_cast<Parent_Selector_Ptr>(schema->at(0));
      return schema->length() > 0 && p != NULL && p->is_real_parent_ref();
    }
    return false;
  }

  void CommaSequence_Selector_Ref::adjust_after_pushing(Sequence_Selector_Ptr c)
  {
    // if (c->has_reference())   has_reference(true);
  }

  // it's a superselector if every selector of the right side
  // list is a superselector of the given left side selector
  bool Sequence_Selector_Ref::is_superselector_of(CommaSequence_Selector *sub, std::string wrapping)
  {
    // Check every rhs selector against left hand list
    for(size_t i = 0, L = sub->length(); i < L; ++i) {
      if (!is_superselector_of((*sub)[i], wrapping)) return false;
    }
    return true;
  }

  // it's a superselector if every selector of the right side
  // list is a superselector of the given left side selector
  bool CommaSequence_Selector_Ref::is_superselector_of(CommaSequence_Selector *sub, std::string wrapping)
  {
    // Check every rhs selector against left hand list
    for(size_t i = 0, L = sub->length(); i < L; ++i) {
      if (!is_superselector_of((*sub)[i], wrapping)) return false;
    }
    return true;
  }

  // it's a superselector if every selector on the right side
  // is a superselector of any one of the left side selectors
  bool CommaSequence_Selector_Ref::is_superselector_of(SimpleSequence_Selector *sub, std::string wrapping)
  {
    // Check every lhs selector against right hand
    for(size_t i = 0, L = length(); i < L; ++i) {
      if ((*this)[i]->is_superselector_of(sub, wrapping)) return true;
    }
    return false;
  }

  // it's a superselector if every selector on the right side
  // is a superselector of any one of the left side selectors
  bool CommaSequence_Selector_Ref::is_superselector_of(Sequence_Selector *sub, std::string wrapping)
  {
    // Check every lhs selector against right hand
    for(size_t i = 0, L = length(); i < L; ++i) {
      if ((*this)[i]->is_superselector_of(sub)) return true;
    }
    return false;
  }

  CommaSequence_Selector_Ptr CommaSequence_Selector_Ref::unify_with(CommaSequence_Selector_Ptr rhs, Context& ctx) {
    std::vector<Sequence_Selector_Ptr> unified_complex_selectors;
    // Unify all of children with RHS's children, storing the results in `unified_complex_selectors`
    for (size_t lhs_i = 0, lhs_L = length(); lhs_i < lhs_L; ++lhs_i) {
      Sequence_Selector_Ptr seq1 = (*this)[lhs_i];
      for(size_t rhs_i = 0, rhs_L = rhs->length(); rhs_i < rhs_L; ++rhs_i) {
        Sequence_Selector_Ptr seq2 = (*rhs)[rhs_i];

        CommaSequence_Selector_Ptr result = seq1->unify_with(seq2, ctx);
        if( result ) {
          for(size_t i = 0, L = result->length(); i < L; ++i) {
            unified_complex_selectors.push_back( (*result)[i] );
          }
        }
      }
    }

    // Creates the final CommaSequence_Selector by combining all the complex selectors
    CommaSequence_Selector_Ptr final_result = SASS_MEMORY_NEW(ctx.mem, CommaSequence_Selector, pstate());
    for (auto itr = unified_complex_selectors.begin(); itr != unified_complex_selectors.end(); ++itr) {
      *final_result << *itr;
    }
    return final_result;
  }

  void CommaSequence_Selector_Ref::populate_extends(CommaSequence_Selector_Ptr extendee, Context& ctx, ExtensionSubsetMap& extends)
  {

    CommaSequence_Selector_Ptr extender = this;
    for (auto complex_sel : extendee->elements()) {
      Sequence_Selector_Ptr c = complex_sel;


      // Ignore any parent selectors, until we find the first non Selector_Reference head
      SimpleSequence_Selector_Ptr compound_sel = c->head();
      Sequence_Selector_Ptr pIter = complex_sel;
      while (pIter) {
        SimpleSequence_Selector_Ptr pHead = pIter->head();
        if (pHead && dynamic_cast<Parent_Selector_Ptr>(pHead->elements()[0]) == NULL) {
          compound_sel = pHead;
          break;
        }

        pIter = pIter->tail();
      }

      if (!pIter->head() || pIter->tail()) {
        error("nested selectors may not be extended", c->pstate());
      }

      compound_sel->is_optional(extendee->is_optional());

      for (size_t i = 0, L = extender->length(); i < L; ++i) {
        extends.put(compound_sel->to_str_vec(), std::make_pair((*extender)[i], compound_sel));
      }
    }
  };

  std::vector<std::string> SimpleSequence_Selector_Ref::to_str_vec()
  {
    std::vector<std::string> result;
    result.reserve(length());
    for (size_t i = 0, L = length(); i < L; ++i)
    { result.push_back((*this)[i]->to_string()); }
    return result;
  }

  SimpleSequence_Selector& SimpleSequence_Selector_Ref::operator<<(Simple_Selector* element)
  {
    Vectorized<Simple_Selector*>::operator<<(element);
    pstate_.offset += element->pstate().offset;
    return *this;
  }

  SimpleSequence_Selector_Ptr SimpleSequence_Selector_Ref::minus(SimpleSequence_Selector_Ptr rhs, Context& ctx)
  {
    SimpleSequence_Selector_Ptr result = SASS_MEMORY_NEW(ctx.mem, SimpleSequence_Selector, pstate());
    // result->has_parent_reference(has_parent_reference());

    // not very efficient because it needs to preserve order
    for (size_t i = 0, L = length(); i < L; ++i)
    {
      bool found = false;
      std::string thisSelector((*this)[i]->to_string(ctx.c_options));
      for (size_t j = 0, M = rhs->length(); j < M; ++j)
      {
        if (thisSelector == (*rhs)[j]->to_string(ctx.c_options))
        {
          found = true;
          break;
        }
      }
      if (!found) (*result) << (*this)[i];
    }

    return result;
  }

  void SimpleSequence_Selector_Ref::mergeSources(SourcesSet& sources, Context& ctx)
  {
    for (SourcesSet::iterator iterator = sources.begin(), endIterator = sources.end(); iterator != endIterator; ++iterator) {
      this->sources_.insert((*iterator)->clone(ctx));
    }
  }

  Argument_Ptr Arguments_Ref::get_rest_argument()
  {
    Argument_Ptr arg = 0;
    if (this->has_rest_argument()) {
      for (auto a : this->elements()) {
        if (a->is_rest_argument()) {
          arg = a;
          break;
        }
      }
    }

    return arg;
  }

  Argument_Ptr Arguments_Ref::get_keyword_argument()
  {
    Argument_Ptr arg = 0;
    if (this->has_keyword_argument()) {
      for (auto a : this->elements()) {
        if (a->is_keyword_argument()) {
          arg = a;
          break;
        }
      }
    }

    return arg;
  }

  void Arguments_Ref::adjust_after_pushing(Argument_Ptr a)
  {
    if (!a->name().empty()) {
      if (/* has_rest_argument_ || */ has_keyword_argument_) {
        error("named arguments must precede variable-length argument", a->pstate());
      }
      has_named_arguments_ = true;
    }
    else if (a->is_rest_argument()) {
      if (has_rest_argument_) {
        error("functions and mixins may only be called with one variable-length argument", a->pstate());
      }
      if (has_keyword_argument_) {
        error("only keyword arguments may follow variable arguments", a->pstate());
      }
      has_rest_argument_ = true;
    }
    else if (a->is_keyword_argument()) {
      if (has_keyword_argument_) {
        error("functions and mixins may only be called with one keyword argument", a->pstate());
      }
      has_keyword_argument_ = true;
    }
    else {
      if (has_rest_argument_) {
        error("ordinal arguments must precede variable-length arguments", a->pstate());
      }
      if (has_named_arguments_) {
        error("ordinal arguments must precede named arguments", a->pstate());
      }
    }
  }

  bool Ruleset_Ref::is_invisible() const {
    if (CommaSequence_Selector_Ptr sl = dynamic_cast<CommaSequence_Selector_Ptr>(selector())) {
      for (size_t i = 0, L = sl->length(); i < L; ++i)
        if (!(*sl)[i]->has_placeholder()) return false;
    }
    return true;
  }

  bool Media_Block_Ref::is_invisible() const {
    for (size_t i = 0, L = block()->length(); i < L; ++i) {
      if (!(*block())[i]->is_invisible()) return false;
    }
    return true;
  }

  Number_Ref::Number_Ref(ParserState pstate, double val, std::string u, bool zero)
  : Value_Ref(pstate),
    value_(val),
    zero_(zero),
    numerator_units_(std::vector<std::string>()),
    denominator_units_(std::vector<std::string>()),
    hash_(0)
  {
    size_t l = 0, r = 0;
    if (!u.empty()) {
      bool nominator = true;
      while (true) {
        r = u.find_first_of("*/", l);
        std::string unit(u.substr(l, r == std::string::npos ? r : r - l));
        if (!unit.empty()) {
          if (nominator) numerator_units_.push_back(unit);
          else denominator_units_.push_back(unit);
        }
        if (r == std::string::npos) break;
        // ToDo: should error for multiple slashes
        // if (!nominator && u[r] == '/') error(...)
        if (u[r] == '/')
          nominator = false;
        l = r + 1;
      }
    }
    concrete_type(NUMBER);
  }

  std::string Number_Ref::unit() const
  {
    std::string u;
    for (size_t i = 0, S = numerator_units_.size(); i < S; ++i) {
      if (i) u += '*';
      u += numerator_units_[i];
    }
    if (!denominator_units_.empty()) u += '/';
    for (size_t i = 0, S = denominator_units_.size(); i < S; ++i) {
      if (i) u += '*';
      u += denominator_units_[i];
    }
    return u;
  }

  bool Number_Ref::is_valid_css_unit() const
  {
    return numerator_units().size() <= 1 &&
           denominator_units().size() == 0;
  }

  bool Number_Ref::is_unitless() const
  { return numerator_units_.empty() && denominator_units_.empty(); }

  void Number_Ref::normalize(const std::string& prefered, bool strict)
  {

    // first make sure same units cancel each other out
    // it seems that a map table will fit nicely to do this
    // we basically construct exponents for each unit
    // has the advantage that they will be pre-sorted
    std::map<std::string, int> exponents;

    // initialize by summing up occurences in unit vectors
    for (size_t i = 0, S = numerator_units_.size(); i < S; ++i) ++ exponents[numerator_units_[i]];
    for (size_t i = 0, S = denominator_units_.size(); i < S; ++i) -- exponents[denominator_units_[i]];

    // the final conversion factor
    double factor = 1;

    // get the first entry of numerators
    // forward it when entry is converted
    std::vector<std::string>::iterator nom_it = numerator_units_.begin();
    std::vector<std::string>::iterator nom_end = numerator_units_.end();
    std::vector<std::string>::iterator denom_it = denominator_units_.begin();
    std::vector<std::string>::iterator denom_end = denominator_units_.end();

    // main normalization loop
    // should be close to optimal
    while (denom_it != denom_end)
    {
      // get and increment afterwards
      const std::string denom = *(denom_it ++);
      // skip already canceled out unit
      if (exponents[denom] >= 0) continue;
      // skip all units we don't know how to convert
      if (string_to_unit(denom) == UNKNOWN) continue;
      // now search for nominator
      while (nom_it != nom_end)
      {
        // get and increment afterwards
        const std::string nom = *(nom_it ++);
        // skip already canceled out unit
        if (exponents[nom] <= 0) continue;
        // skip all units we don't know how to convert
        if (string_to_unit(nom) == UNKNOWN) continue;
        // we now have two convertable units
        // add factor for current conversion
        factor *= conversion_factor(nom, denom, strict);
        // update nominator/denominator exponent
        -- exponents[nom]; ++ exponents[denom];
        // inner loop done
        break;
      }
    }

    // now we can build up the new unit arrays
    numerator_units_.clear();
    denominator_units_.clear();

    // build them by iterating over the exponents
    for (auto exp : exponents)
    {
      // maybe there is more effecient way to push
      // the same item multiple times to a vector?
      for(size_t i = 0, S = abs(exp.second); i < S; ++i)
      {
        // opted to have these switches in the inner loop
        // makes it more readable and should not cost much
        if (!exp.first.empty()) {
          if (exp.second < 0) denominator_units_.push_back(exp.first);
          else if (exp.second > 0) numerator_units_.push_back(exp.first);
        }
      }
    }

    // apply factor to value_
    // best precision this way
    value_ *= factor;

    // maybe convert to other unit
    // easier implemented on its own
    try { convert(prefered, strict); }
    catch (incompatibleUnits& err)
    { error(err.what(), pstate()); }
    catch (...) { throw; }

  }

  // this does not cover all cases (multiple prefered units)
  double Number_Ref::convert_factor(const Number& n) const
  {

    // first make sure same units cancel each other out
    // it seems that a map table will fit nicely to do this
    // we basically construct exponents for each unit class
    // std::map<std::string, int> exponents;
    // initialize by summing up occurences in unit vectors
    // for (size_t i = 0, S = numerator_units_.size(); i < S; ++i) ++ exponents[unit_to_class(numerator_units_[i])];
    // for (size_t i = 0, S = denominator_units_.size(); i < S; ++i) -- exponents[unit_to_class(denominator_units_[i])];

    std::vector<std::string> l_miss_nums(0);
    std::vector<std::string> l_miss_dens(0);
    // create copy since we need these for state keeping
    std::vector<std::string> r_nums(n.numerator_units_);
    std::vector<std::string> r_dens(n.denominator_units_);

    std::vector<std::string>::const_iterator l_num_it = numerator_units_.begin();
    std::vector<std::string>::const_iterator l_num_end = numerator_units_.end();

    bool l_unitless = is_unitless();
    bool r_unitless = n.is_unitless();

    // overall conversion
    double factor = 1;

    // process all left numerators
    while (l_num_it != l_num_end)
    {
      // get and increment afterwards
      const std::string l_num = *(l_num_it ++);

      std::vector<std::string>::iterator r_num_it = r_nums.begin();
      std::vector<std::string>::iterator r_num_end = r_nums.end();

      bool found = false;
      // search for compatible numerator
      while (r_num_it != r_num_end)
      {
        // get and increment afterwards
        const std::string r_num = *(r_num_it);
        // get possible converstion factor for units
        double conversion = conversion_factor(l_num, r_num, false);
        // skip incompatible numerator
        if (conversion == 0) {
          ++ r_num_it;
          continue;
        }
        // apply to global factor
        factor *= conversion;
        // remove item from vector
        r_nums.erase(r_num_it);
        // found numerator
        found = true;
        break;
      }
      // maybe we did not find any
      // left numerator is leftover
      if (!found) l_miss_nums.push_back(l_num);
    }

    std::vector<std::string>::const_iterator l_den_it = denominator_units_.begin();
    std::vector<std::string>::const_iterator l_den_end = denominator_units_.end();

    // process all left denominators
    while (l_den_it != l_den_end)
    {
      // get and increment afterwards
      const std::string l_den = *(l_den_it ++);

      std::vector<std::string>::iterator r_den_it = r_dens.begin();
      std::vector<std::string>::iterator r_den_end = r_dens.end();

      bool found = false;
      // search for compatible denominator
      while (r_den_it != r_den_end)
      {
        // get and increment afterwards
        const std::string r_den = *(r_den_it);
        // get possible converstion factor for units
        double conversion = conversion_factor(l_den, r_den, false);
        // skip incompatible denominator
        if (conversion == 0) {
          ++ r_den_it;
          continue;
        }
        // apply to global factor
        factor *= conversion;
        // remove item from vector
        r_dens.erase(r_den_it);
        // found denominator
        found = true;
        break;
      }
      // maybe we did not find any
      // left denominator is leftover
      if (!found) l_miss_dens.push_back(l_den);
    }

    // check left-overs (ToDo: might cancel out)
    if (l_miss_nums.size() > 0 && !r_unitless) {
      throw Exception::IncompatibleUnits(n, *this);
    }
    if (l_miss_dens.size() > 0 && !r_unitless) {
      throw Exception::IncompatibleUnits(n, *this);
    }
    if (r_nums.size() > 0 && !l_unitless) {
      throw Exception::IncompatibleUnits(n, *this);
    }
    if (r_dens.size() > 0 && !l_unitless) {
      throw Exception::IncompatibleUnits(n, *this);
    }

    return factor;
  }

  // this does not cover all cases (multiple prefered units)
  bool Number_Ref::convert(const std::string& prefered, bool strict)
  {
    // no conversion if unit is empty
    if (prefered.empty()) return true;

    // first make sure same units cancel each other out
    // it seems that a map table will fit nicely to do this
    // we basically construct exponents for each unit
    // has the advantage that they will be pre-sorted
    std::map<std::string, int> exponents;

    // initialize by summing up occurences in unit vectors
    for (size_t i = 0, S = numerator_units_.size(); i < S; ++i) ++ exponents[numerator_units_[i]];
    for (size_t i = 0, S = denominator_units_.size(); i < S; ++i) -- exponents[denominator_units_[i]];

    // the final conversion factor
    double factor = 1;

    std::vector<std::string>::iterator denom_it = denominator_units_.begin();
    std::vector<std::string>::iterator denom_end = denominator_units_.end();

    // main normalization loop
    // should be close to optimal
    while (denom_it != denom_end)
    {
      // get and increment afterwards
      const std::string denom = *(denom_it ++);
      // check if conversion is needed
      if (denom == prefered) continue;
      // skip already canceled out unit
      if (exponents[denom] >= 0) continue;
      // skip all units we don't know how to convert
      if (string_to_unit(denom) == UNKNOWN) continue;
      // we now have two convertable units
      // add factor for current conversion
      factor *= conversion_factor(denom, prefered, strict);
      // update nominator/denominator exponent
      ++ exponents[denom]; -- exponents[prefered];
    }

    std::vector<std::string>::iterator nom_it = numerator_units_.begin();
    std::vector<std::string>::iterator nom_end = numerator_units_.end();

    // now search for nominator
    while (nom_it != nom_end)
    {
      // get and increment afterwards
      const std::string nom = *(nom_it ++);
      // check if conversion is needed
      if (nom == prefered) continue;
      // skip already canceled out unit
      if (exponents[nom] <= 0) continue;
      // skip all units we don't know how to convert
      if (string_to_unit(nom) == UNKNOWN) continue;
      // we now have two convertable units
      // add factor for current conversion
      factor *= conversion_factor(nom, prefered, strict);
      // update nominator/denominator exponent
      -- exponents[nom]; ++ exponents[prefered];
    }

    // now we can build up the new unit arrays
    numerator_units_.clear();
    denominator_units_.clear();

    // build them by iterating over the exponents
    for (auto exp : exponents)
    {
      // maybe there is more effecient way to push
      // the same item multiple times to a vector?
      for(size_t i = 0, S = abs(exp.second); i < S; ++i)
      {
        // opted to have these switches in the inner loop
        // makes it more readable and should not cost much
        if (!exp.first.empty()) {
          if (exp.second < 0) denominator_units_.push_back(exp.first);
          else if (exp.second > 0) numerator_units_.push_back(exp.first);
        }
      }
    }

    // apply factor to value_
    // best precision this way
    value_ *= factor;

    // success?
    return true;

  }

  // useful for making one number compatible with another
  std::string Number_Ref::find_convertible_unit() const
  {
    for (size_t i = 0, S = numerator_units_.size(); i < S; ++i) {
      std::string u(numerator_units_[i]);
      if (string_to_unit(u) != UNKNOWN) return u;
    }
    for (size_t i = 0, S = denominator_units_.size(); i < S; ++i) {
      std::string u(denominator_units_[i]);
      if (string_to_unit(u) != UNKNOWN) return u;
    }
    return std::string();
  }

  bool Custom_Warning_Ref::operator== (const Expression& rhs) const
  {
    if (Custom_Warning_Ptr_Const r = dynamic_cast<Custom_Warning_Ptr_Const>(&rhs)) {
      return message() == r->message();
    }
    return false;
  }

  bool Custom_Error_Ref::operator== (const Expression& rhs) const
  {
    if (Custom_Error_Ptr_Const r = dynamic_cast<Custom_Error_Ptr_Const>(&rhs)) {
      return message() == r->message();
    }
    return false;
  }

  bool Number_Ref::eq (const Expression& rhs) const
  {
    if (Number_Ptr_Const r = dynamic_cast<Number_Ptr_Const>(&rhs)) {
      size_t lhs_units = numerator_units_.size() + denominator_units_.size();
      size_t rhs_units = r->numerator_units_.size() + r->denominator_units_.size();
      if (!lhs_units && !rhs_units) {
        return std::fabs(value() - r->value()) < NUMBER_EPSILON;
      }
      return (numerator_units_ == r->numerator_units_) &&
             (denominator_units_ == r->denominator_units_) &&
             std::fabs(value() - r->value()) < NUMBER_EPSILON;
    }
    return false;
  }

  bool Number_Ref::operator== (const Expression& rhs) const
  {
    if (Number_Ptr_Const r = dynamic_cast<Number_Ptr_Const>(&rhs)) {
      size_t lhs_units = numerator_units_.size() + denominator_units_.size();
      size_t rhs_units = r->numerator_units_.size() + r->denominator_units_.size();
      // unitless and only having one unit seems equivalent (will change in future)
      if (!lhs_units || !rhs_units) {
        return std::fabs(value() - r->value()) < NUMBER_EPSILON;
      }
      return (numerator_units_ == r->numerator_units_) &&
             (denominator_units_ == r->denominator_units_) &&
             std::fabs(value() - r->value()) < NUMBER_EPSILON;
    }
    return false;
  }

  bool Number_Ref::operator< (const Number& rhs) const
  {
    size_t lhs_units = numerator_units_.size() + denominator_units_.size();
    size_t rhs_units = rhs.numerator_units_.size() + rhs.denominator_units_.size();
    // unitless and only having one unit seems equivalent (will change in future)
    if (!lhs_units || !rhs_units) {
      return value() < rhs.value();
    }

    Number tmp_r(rhs);
    tmp_r.normalize(find_convertible_unit());
    std::string l_unit(unit());
    std::string r_unit(tmp_r.unit());
    if (unit() != tmp_r.unit()) {
      error("cannot compare numbers with incompatible units", pstate());
    }
    return value() < tmp_r.value();
  }

  bool String_Quoted_Ref::operator== (const Expression& rhs) const
  {
    if (String_Quoted_Ptr_Const qstr = dynamic_cast<String_Quoted_Ptr_Const>(&rhs)) {
      return (value() == qstr->value());
    } else if (String_Constant_Ptr_Const cstr = dynamic_cast<String_Constant_Ptr_Const>(&rhs)) {
      return (value() == cstr->value());
    }
    return false;
  }

  bool String_Constant_Ref::is_invisible() const {
    return value_.empty() && quote_mark_ == 0;
  }

  bool String_Constant_Ref::operator== (const Expression& rhs) const
  {
    if (String_Quoted_Ptr_Const qstr = dynamic_cast<String_Quoted_Ptr_Const>(&rhs)) {
      return (value() == qstr->value());
    } else if (String_Constant_Ptr_Const cstr = dynamic_cast<String_Constant_Ptr_Const>(&rhs)) {
      return (value() == cstr->value());
    }
    return false;
  }

  bool String_Schema_Ref::is_left_interpolant(void) const
  {
    return length() && first()->is_left_interpolant();
  }
  bool String_Schema_Ref::is_right_interpolant(void) const
  {
    return length() && last()->is_right_interpolant();
  }

  bool String_Schema_Ref::operator== (const Expression& rhs) const
  {
    if (String_Schema_Ptr_Const r = dynamic_cast<String_Schema_Ptr_Const>(&rhs)) {
      if (length() != r->length()) return false;
      for (size_t i = 0, L = length(); i < L; ++i) {
        Expression_Ptr rv = (*r)[i];
        Expression_Ptr lv = (*this)[i];
        if (!lv || !rv) return false;
        if (!(*lv == *rv)) return false;
      }
      return true;
    }
    return false;
  }

  bool Boolean_Ref::operator== (const Expression& rhs) const
  {
    if (Boolean_Ptr_Const r = dynamic_cast<Boolean_Ptr_Const>(&rhs)) {
      return (value() == r->value());
    }
    return false;
  }

  bool Color_Ref::operator== (const Expression& rhs) const
  {
    if (Color_Ptr_Const r = dynamic_cast<Color_Ptr_Const>(&rhs)) {
      return r_ == r->r() &&
             g_ == r->g() &&
             b_ == r->b() &&
             a_ == r->a();
    }
    return false;
  }

  bool List_Ref::operator== (const Expression& rhs) const
  {
    if (List_Ptr_Const r = dynamic_cast<List_Ptr_Const>(&rhs)) {
      if (length() != r->length()) return false;
      if (separator() != r->separator()) return false;
      for (size_t i = 0, L = length(); i < L; ++i) {
        Expression_Ptr rv = (*r)[i];
        Expression_Ptr lv = (*this)[i];
        if (!lv || !rv) return false;
        if (!(*lv == *rv)) return false;
      }
      return true;
    }
    return false;
  }

  bool Map_Ref::operator== (const Expression& rhs) const
  {
    if (Map_Ptr_Const r = dynamic_cast<Map_Ptr_Const>(&rhs)) {
      if (length() != r->length()) return false;
      for (auto key : keys()) {
        Expression_Ptr lv = at(key);
        Expression_Ptr rv = r->at(key);
        if (!rv || !lv) return false;
        if (!(*lv == *rv)) return false;
      }
      return true;
    }
    return false;
  }

  bool Null::operator== (const Expression& rhs) const
  {
    return rhs.concrete_type() == NULL_VAL;
  }

  size_t List_Ref::size() const {
    if (!is_arglist_) return length();
    // arglist expects a list of arguments
    // so we need to break before keywords
    for (size_t i = 0, L = length(); i < L; ++i) {
      if (Argument_Ptr arg = dynamic_cast<Argument_Ptr>((*this)[i])) {
        if (!arg->name().empty()) return i;
      }
    }
    return length();
  }

  Expression_Ptr Hashed::at(Expression_Ptr k) const
  {
    if (elements_.count(k))
    { return elements_.at(k); }
    else { return &sass_null; }
  }

  bool Binary_Expression_Ref::is_left_interpolant(void) const
  {
    return is_interpolant() || (left() && left()->is_left_interpolant());
  }
  bool Binary_Expression_Ref::is_right_interpolant(void) const
  {
    return is_interpolant() || (right() && right()->is_right_interpolant());
  }

  std::string AST_Node_Ref::to_string(Sass_Inspect_Options opt) const
  {
    Sass_Output_Options out(opt);
    Emitter emitter(out);
    Inspect i(emitter);
    i.in_declaration = true;
    // ToDo: inspect should be const
    const_cast<AST_Node_Ptr>(this)->perform(&i);
    return i.get_buffer();
  }

  std::string AST_Node_Ref::to_string() const
  {
    return to_string({ NESTED, 5 });
  }

  std::string String_Quoted::inspect() const
  {
    return quote(value_, '*');
  }

  std::string String_Constant_Ref::inspect() const
  {
    return quote(value_, '*');
  }

  //////////////////////////////////////////////////////////////////////////////////////////
  // Additional method on Lists to retrieve values directly or from an encompassed Argument.
  //////////////////////////////////////////////////////////////////////////////////////////
  Expression_Ptr List_Ref::value_at_index(size_t i) {
    if (is_arglist_) {
      if (Argument_Ptr arg = dynamic_cast<Argument_Ptr>((*this)[i])) {
        return arg->value();
      } else {
        return (*this)[i];
      }
    } else {
      return (*this)[i];
    }
  }

  //////////////////////////////////////////////////////////////////////////////////////////
  // Convert map to (key, value) list.
  //////////////////////////////////////////////////////////////////////////////////////////
  List_Ptr Map_Ref::to_list(Context& ctx, ParserState& pstate) {
    List_Ptr ret = SASS_MEMORY_NEW(ctx.mem, List, pstate, length(), SASS_COMMA);

    for (auto key : keys()) {
      List_Ptr l = SASS_MEMORY_NEW(ctx.mem, List, pstate, 2);
      *l << key;
      *l << at(key);
      *ret << l;
    }

    return ret;
  }

}
