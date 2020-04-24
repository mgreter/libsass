// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include <iostream>
#include <typeinfo>
#include <vector>

#include "cssize.hpp"
#include "context.hpp"
#include "debugger.hpp"

namespace Sass {



  ////////////////////////
  // Blocks of statements.
  ////////////////////////

  Block::Block(const SourceSpan& pstate, sass::vector<CssNodeObj>&& vec)
    : CssNode(pstate), VectorizedNopsi<CssNode>(std::move(vec)) {}

  Cssize::Cssize(Logger& logger)
  : callStack(logger)
  { }

  CssParentNode* Cssize::parent()
  {
    if (cssStack.empty()) {
      return nullptr;
    }
    return cssStack.back();
  }

  void
    Cssize::visitBlockStatements(
      const sass::vector<CssNodeObj>& children,
      sass::vector<CssNodeObj>& results)
  {
    for (size_t i = 0, L = children.size(); i < L; ++i) {
      debug_ast(children.at(i), "IN: ");
      CssNodeObj ith = children.at(i)->perform(this);
      debug_ast(ith);
      if (Block* bb = Cast<Block>(ith)) {
        // Not sure if move is safe here!?
        std::move(bb->begin(), bb->end(),
          std::back_inserter(results));
      }
      else if (ith) {
        results.push_back(ith);
      }
    }
  }

  CssRoot* Cssize::doit(CssRootObj b)
  {
    sass::vector<CssNodeObj> children;
    children.reserve(b->length());
    visitBlockStatements(b->elements(), children);
    return SASS_MEMORY_NEW(CssRoot, b->pstate(),
      std::move(children));
  }

  
  CssNode* Cssize::operator()(CssParentNode* b)
  {
    sass::vector<CssNodeObj> children;
    children.reserve(b->length());
    visitBlockStatements(b->elements(), children);
    return SASS_MEMORY_NEW(Block, b->pstate(),
      std::move(children));
  }

  CssNode* Cssize::operator()(Trace* trace)
  {
    SourceSpan span(trace->pstate());
    callStackFrame frame(callStack,
      BackTrace(span));
    return trace->CssParentNode::perform(this);
  }

  CssNode* Cssize::operator()(CssAtRule* r)
  {

    if (r->empty()) return r;

    if (Cast<CssStyleRule>(parent()))
    {
      return (r->bubbles()) ? SASS_MEMORY_NEW(Bubble, r->pstate(), r) : bubble(r);
    }

    cssStack.emplace_back(r);
    sass::vector<CssNodeObj> children;
    children.reserve(r->length());
    visitBlockStatements(r->elements(), children);
    cssStack.pop_back();

    if (children.empty())
    {
      CssAtRuleObj rr = SASS_MEMORY_NEW(CssAtRule,
        r->pstate(), r->name(), r->value());
      rr->isChildless(r->isChildless());
      return rr.detach();
    }

    return SASS_MEMORY_NEW(Block, r->pstate(),
      std::move(debubble(children, r)));
  }

  CssNode* Cssize::operator()(Keyframe_Rule* r)
  {
    if (r == nullptr) return nullptr;
    if (r->empty()) return nullptr;
    sass::vector<CssNodeObj> children;
    children.reserve(r->length());
    visitBlockStatements(r->elements(), children);
    return SASS_MEMORY_NEW(Block, r->pstate(),
      std::move(debubble(children, r)));
  }

  CssNode* Cssize::operator()(CssStyleRule* r)
  {
    cssStack.emplace_back(r);
    sass::vector<CssNodeObj> children;
    children.reserve(r->length());
    visitBlockStatements(r->elements(), children);
    cssStack.pop_back();

    // Split statements and bubblers
    sass::vector<CssNodeObj> statemts;
    sass::vector<CssNodeObj> bubblers;
    for (size_t i = 0, L = children.size(); i < L; i++)
    {
      CssNode* s = children[i];
      if (s->bubbles()) {
        bubblers.push_back(s);
      }
      else {
        statemts.push_back(s);
      }
    }

    if (!statemts.empty()) {
      for (CssNode* stmt : bubblers) {
        stmt->tabs(stmt->tabs() + 1);
      }
      bubblers.insert(bubblers.begin(),
        SASS_MEMORY_NEW(CssStyleRule, r->pstate(),
          r->selector(), std::move(statemts)));
    }

    return SASS_MEMORY_NEW(Block,
      r->pstate(), std::move(debubble(bubblers)));
  }

  CssNode* Cssize::operator()(CssMediaRule* m)
  {
    if (Cast<CssStyleRule>(parent()))
    {
      return bubble(m);
    }

    if (Cast<CssMediaRule>(parent()))
    {
      return SASS_MEMORY_NEW(Bubble, m->pstate(), m);
    }

    cssStack.emplace_back(m);
    sass::vector<CssNodeObj> children;
    children.reserve(m->length());
    visitBlockStatements(m->elements(), children);
    cssStack.pop_back();

    return SASS_MEMORY_NEW(Block, m->pstate(),
      std::move(debubble(children, m)));
  }

  CssNode* Cssize::operator()(CssSupportsRule* m)
  {
    if (m == nullptr) return nullptr;
    if (m->empty()) return nullptr;

    if (Cast<CssStyleRule>(parent()))
    {
      return bubble(m);
    }

    cssStack.emplace_back(m);
    sass::vector<CssNodeObj> children;
    children.reserve(m->length());
    visitBlockStatements(m->elements(), children);
    cssStack.pop_back();

    return SASS_MEMORY_NEW(Block, m->pstate(),
      std::move(debubble(children, m)));

  }

  CssNode* Cssize::operator()(CssAtRootRule* m)
  {
    bool excludes = false;
    for (CssParentNode* stacked : cssStack) {
      if (m->query()->excludes(stacked)) {
        excludes = true;
        break;
      }
    }

    if (!excludes)
    {
      sass::vector<CssNodeObj> children;
      children.reserve(m->length());
      visitBlockStatements(m->elements(), children);
      for (CssNodeObj& stm : children) {
        if (!stm->bubbles()) continue;
        stm->tabs(stm->tabs() + m->tabs());
      }
      return SASS_MEMORY_NEW(Block,
        m->pstate(), std::move(children));
    }

    if (m->query()->excludes(parent()))
    {
      return SASS_MEMORY_NEW(Bubble, m->pstate(), m);
    }

    return bubble(m);
  }

  Bubble* Cssize::bubble(CssAtRule* m)
  {
    if (m == nullptr) return nullptr;
    if (m->empty()) return nullptr;
    sass::vector<CssNodeObj> children;
    children.reserve(m->length());
    if (auto rule = Cast<CssStyleRule>(parent())) {
      rule = SASS_MEMORY_COPY(rule);
      rule->elementsC(m->elements());
      children.emplace_back(rule);
    }
    CssAtRuleObj mm = SASS_MEMORY_NEW(CssAtRule, m->pstate(),
      m->name(), m->value(), std::move(children));
    // mm->tabs(m->tabs());
    return SASS_MEMORY_NEW(Bubble, mm->pstate(), mm);
  }

  Bubble* Cssize::bubble(CssAtRootRule* m)
  {
    if (m == nullptr) return nullptr;
    if (m->empty()) return nullptr;
    sass::vector<CssNodeObj> children;
    children.reserve(m->length());
    if (auto rule = Cast<CssParentNode>(parent())) {
      rule = SASS_MEMORY_COPY(rule);
      rule->elementsC(m->elements());
      children.emplace_back(rule);
    }
    CssAtRootRule* mm = SASS_MEMORY_NEW(CssAtRootRule,
      m->pstate(), m->query(), std::move(children));
    // mm->tabs(m->tabs());
    return SASS_MEMORY_NEW(Bubble, mm->pstate(), mm);
  }

  Bubble* Cssize::bubble(CssSupportsRule* m)
  {
    if (m == nullptr) return nullptr;
    if (m->empty()) return nullptr;
    sass::vector<CssNodeObj> children;
    children.reserve(m->length());
    if (auto rule = Cast<CssStyleRule>(parent())) {
      rule = SASS_MEMORY_COPY(rule);
      rule->elementsC(m->elements());
      children.emplace_back(rule);
    }
    CssSupportsRule* mm = SASS_MEMORY_NEW(CssSupportsRule,
      m->pstate(), m->condition(), std::move(children));
    mm->tabs(m->tabs());
    return SASS_MEMORY_NEW(Bubble, mm->pstate(), mm);
  }

  Bubble* Cssize::bubble(CssMediaRule* m)
  {
    if (m == nullptr) return nullptr;
    if (m->empty()) return nullptr;
    sass::vector<CssNodeObj> children;
    children.reserve(m->length());
    if (auto rule = Cast<CssStyleRule>(parent())) {
      rule = SASS_MEMORY_COPY(rule);
      rule->elementsC(m->elements());
      children.emplace_back(rule);
    }
    CssMediaRuleObj mm = SASS_MEMORY_NEW(CssMediaRule,
      m->pstate(), m->queries(), std::move(children));
    mm->tabs(m->tabs());
    return SASS_MEMORY_NEW(Bubble, mm->pstate(), mm);
  }

  void Cssize::slice_by_bubble(const sass::vector<CssNodeObj>& children,
    sass::vector<std::pair<bool, sass::vector<CssNodeObj>>>& results)
  {
    results.reserve(results.size() + children.size());
    for (const CssNodeObj& value : children) {
      bool isBubble = Cast<Bubble>(value) != nullptr;
      if (!results.empty() && results.back().first == isBubble)
      {
        results.back().second.push_back(value);
      }
      else
      {
        results.emplace_back(std::make_pair(isBubble,
          sass::vector<CssNodeObj>{ value }));
      }
    }
  }



  sass::vector<CssNodeObj> Cssize::debubble(const sass::vector<CssNodeObj>& children, CssNode* parent)
  {
    sass::vector<CssNodeObj>* previousBlock = nullptr;
    sass::vector<std::pair<bool, sass::vector<CssNodeObj>>> baz;
    slice_by_bubble(children, baz); // ToDo: make obsolete
    sass::vector<CssNodeObj> items;

    for (size_t i = 0, L = baz.size(); i < L; ++i) {
      bool is_bubble = baz[i].first;
      sass::vector<CssNodeObj>& slice = baz[i].second;

      if (!is_bubble) {

        if (!parent) {
          std::move(slice.begin(), slice.end(),
            std::back_inserter(items));
        }
        else if (previousBlock) {
          std::move(slice.begin(), slice.end(),
            std::back_inserter(*previousBlock));
        }
        else if (CssParentNode* prev = Cast<CssParentNode>(parent)) {
          prev = SASS_MEMORY_COPY(prev);
          prev->elementsM(std::move(slice));
          previousBlock = &prev->elements();
          items.push_back(prev);
        }

      }
      else {

        for (size_t j = 0, K = slice.size(); j < K; ++j)
        {
          Bubble* bubble = static_cast<Bubble*>(slice[j].ptr());
          CssNodeObj bubbled = bubble->node()->perform(this);
          if (const Block* bb = Cast<Block>(bubbled)) {
            std::move(bb->begin(), bb->end(),
              std::back_inserter(items));
            if (!bb->empty()) {
              previousBlock = {};
            }
          }
          else {
            items.push_back(bubbled);
          }
        }

      }

    }

    return items;
  }

}
