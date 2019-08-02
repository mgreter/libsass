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

  Cssize::Cssize(Context& ctx)
  : callStack(*ctx.logger),
    block_stack(BlockStack()),
    p_stack(sass::vector<Statement*>())
  { }

  Statement* Cssize::parent()
  {
    return p_stack.size() ? p_stack.back() : block_stack.front();
  }

  void
    Cssize::visitBlockStatements(
      const sass::vector<StatementObj>& children,
      sass::vector<StatementObj>& results)
  {

    Block_Obj cur = SASS_MEMORY_NEW(Block, SourceSpan::fake(""));
    // bb->tabs(b->tabs());
    block_stack.emplace_back(cur);

    // append_block(b, cur);

    for (size_t i = 0, L = children.size(); i < L; ++i) {
      Statement_Obj ith = children.at(i)->perform(this);
      if (Block_Obj bb = Cast<Block>(ith)) {
        for (size_t j = 0, K = bb->length(); j < K; ++j) {
          cur->append(bb->at(j));
        }
      }
      else if (ith) {
        cur->append(ith);
      }
    }


    block_stack.pop_back();

    results = std::move(cur->elements());

  }

  Block* Cssize::operator()(Block* b)
  {
    Block_Obj cur = SASS_MEMORY_NEW(Block, b->pstate(), b->length(), b->is_root());
    // bb->tabs(b->tabs());
    block_stack.emplace_back(cur);

    // append_block(b, cur);

    for (size_t i = 0, L = b->length(); i < L; ++i) {
      StatementObj ith = b->at(i)->perform(this);
      if (Block_Obj bb = Cast<Block>(ith)) {
        for (size_t j = 0, K = bb->length(); j < K; ++j) {
          cur->append(bb->at(j));
        }
      }
      else if (ith) {
        cur->append(ith);
      }
    }


    block_stack.pop_back();
    return cur.detach();
  }

  Statement* Cssize::operator()(Trace* trace)
  {
    callStackFrame frame(callStack,
      Backtrace(trace->pstate()));
    return trace->block()->perform(this);
  }

  Statement* Cssize::operator()(CssAtRule* r)
  {

    if (r->empty()) return r;

    if (Cast<CssStyleRule>(parent()))
    {
      return (r->is_keyframes()) ? SASS_MEMORY_NEW(Bubble, r->pstate(), r) : bubble(r);
    }

    p_stack.emplace_back(r);

    sass::vector<StatementObj> children;
    visitBlockStatements(r->elements(), children);

    CssAtRuleObj rr = SASS_MEMORY_NEW(CssAtRule,
      r->pstate(),
      r->name(), r->value(),
      children);
    rr->isChildless(r->isChildless());
    p_stack.pop_back();

    Block* result = SASS_MEMORY_NEW(Block, rr->pstate());
    if (rr->empty())
    {
      result->append(rr);
      return result;
    }

    Block_Obj db = SASS_MEMORY_NEW(Block, rr->pstate());
    db->concat(std::move(children));
    Block_Obj ss = debubble(db, rr);
    for (size_t i = 0, L = ss->length(); i < L; ++i) {
      result->append(ss->at(i));
    }

    return result;
  }

  Statement* Cssize::operator()(Keyframe_Rule* r)
  {
    if (!r->length()) return r;

    sass::vector<StatementObj> children;
    visitBlockStatements(r->elements(), children);

    Keyframe_Rule_Obj rr = SASS_MEMORY_NEW(Keyframe_Rule,
                                        r->pstate(),
                                        children);

    // if (!r->name().isNull()) rr->name(r->name());
    if (!r->name2().isNull()) rr->name2(r->name2());

    BlockObj blk = SASS_MEMORY_NEW(Block, SourceSpan::fake(""));
    blk->concat(std::move(children));

    return debubble(blk, rr);
  }

  Statement* Cssize::operator()(CssStyleRule* r)
  {
    p_stack.emplace_back(r);
    sass::vector<StatementObj> children;
    visitBlockStatements(r->elements(), children);
    // debug_ast(SASS_MEMORY_NEW(Block, "", children), "1: ");
    // debug_ast(SASS_MEMORY_NEW(Block, "", children2), "2: ");
    CssStyleRuleObj rr = SASS_MEMORY_NEW(CssStyleRule,
      r->pstate(),
      r->selector(),
      std::move(children));
    // rr->block(bb);

    // rr->is_root(r->is_root());
    p_stack.pop_back();


    Block_Obj props = SASS_MEMORY_NEW(Block, rr->pstate());
    Block* rules = SASS_MEMORY_NEW(Block, rr->pstate());
    for (size_t i = 0, L = rr->length(); i < L; i++)
    {
      Statement* s = rr->at(i);
      if (bubblable(s)) {
        rules->append(s);
      }
      else {
        props->append(s);
      }
    }

    if (props->length())
    {

      rr->elements(props->elements());

      for (Statement* stmt : rules->elements())
      {
        stmt->tabs(stmt->tabs() + 1);
      }

      rules->unshift(rr);
    }

    Block* ptr = rules;
    rules = debubble(rules);
    void* lp = ptr;
    void* rp = rules;
    if (lp != rp) {
      Block_Obj obj = ptr;
    }

    if (!(!rules->length() ||
      !bubblable(rules->last()) ||
      Cast<CssStyleRule>(parent())))
    {
      rules->last()->group_end(true);
    }
    return rules;
  }

  /* Not used anymore? Why do we not get nulls here?
  // they seem to be already catched earlier
  Statement* Cssize::operator()(Null* m)
  {
    return nullptr;
  }
  */

  Statement* Cssize::operator()(CssMediaRule* m)
  {
    if (Cast<CssStyleRule>(parent()))
    {
      return bubble(m);
    }

    if (Cast<CssMediaRule>(parent()))
    {
      return SASS_MEMORY_NEW(Bubble, m->pstate(), m);
    }

    p_stack.emplace_back(m);

    CssMediaRuleObj mm = SASS_MEMORY_NEW(CssMediaRule,
      m->pstate(), m->queries(), m->elements());
    sass::vector<StatementObj> children;
    visitBlockStatements(m->elements(), children);
    mm->elements(children);
    mm->tabs(m->tabs());

    p_stack.pop_back();

    BlockObj blk = SASS_MEMORY_NEW(Block, SourceSpan::fake(""));
    blk->elements(std::move(children));
    return debubble(blk, mm);
  }

  Statement* Cssize::operator()(CssSupportsRule* m)
  {
    if (!m->length())
    {
      return m;
    }

    if (Cast<CssStyleRule>(parent()))
    {
      return bubble(m);
    }

    p_stack.emplace_back(m);

    sass::vector<StatementObj> children;
    visitBlockStatements(m->elements(), children);
    CssSupportsRuleObj mm = SASS_MEMORY_NEW(CssSupportsRule,
      m->pstate(),
      m->condition(),
      children);
    // mm->block(operator()(m->block()));
    mm->tabs(m->tabs());

    p_stack.pop_back();

    BlockObj blk = SASS_MEMORY_NEW(Block, SourceSpan::fake(""));
    blk->concat(std::move(children));
    return debubble(blk, mm);
  }

  Statement* Cssize::operator()(CssAtRootRule* m)
  {
    bool tmp = false;
    for (size_t i = 0, L = p_stack.size(); i < L; ++i) {
      Statement* s = p_stack[i];
      tmp |= m->query()->excludes(s);
    }

    if (!tmp)
    {
      sass::vector<StatementObj> children;
      visitBlockStatements(m->elements(), children);
      for (size_t i = 0, L = children.size(); i < L; ++i) {
        // (bb->elements())[i]->tabs(m->tabs());
        Statement_Obj stm = children.at(i);
        if (bubblable(stm)) stm->tabs(stm->tabs() + m->tabs());
      }
      if (children.size() && bubblable(children.back())) children.back()->group_end(m->group_end());
      return SASS_MEMORY_NEW(Block, SourceSpan::fake(""), children);
    }

    if (m->query()->excludes(parent()))
    {
      return SASS_MEMORY_NEW(Bubble, m->pstate(), m);
    }

    return bubble(m);
  }

  Statement* Cssize::bubble(CssAtRule* m)
  {
    BlockObj bb = SASS_MEMORY_NEW(Block, this->parent()->pstate());
    StatementObj cp = SASS_MEMORY_COPY(this->parent());
    Block_Obj wrapper_block = SASS_MEMORY_NEW(Block, m->pstate());
    if (CssStyleRule * new_rule = Cast<CssStyleRule>(cp)) {
      new_rule->tabs(this->parent()->tabs());

      new_rule->elements(bb->elements());
      new_rule->concat(m->elements());

      wrapper_block->append(new_rule);

    }

    CssAtRuleObj mm = SASS_MEMORY_NEW(CssAtRule,
      m->pstate(),
      m->name(),
      m->value(),
      wrapper_block->elements());
    // mm->block(wrapper_block);

    Bubble* bubble = SASS_MEMORY_NEW(Bubble, mm->pstate(), mm);
    return bubble;
  }

  Statement* Cssize::bubble(CssAtRootRule* m)
  {
    if (!m) return NULL;
    BlockObj bb = SASS_MEMORY_NEW(Block, this->parent()->pstate());
    StatementObj cp = SASS_MEMORY_COPY(this->parent());
    BlockObj wrapper_block = SASS_MEMORY_NEW(Block, m->pstate());
    if (CssParentNode * new_rule = Cast<CssParentNode>(cp)) {
      new_rule->elements(bb->elements());
      new_rule->tabs(this->parent()->tabs());
      new_rule->concat(m->elements());
      wrapper_block->append(new_rule);
    }

    CssAtRootRule* mm = SASS_MEMORY_NEW(CssAtRootRule,
                                        m->pstate(),
                                        m->query(),
                                        wrapper_block->elements());
    Bubble* bubble = SASS_MEMORY_NEW(Bubble, mm->pstate(), mm);
    return bubble;
  }

  Statement* Cssize::bubble(CssSupportsRule* m)
  {
    if (CssStyleRuleObj parent = Cast<CssStyleRule>(SASS_MEMORY_COPY(this->parent()))) {

      BlockObj bb = SASS_MEMORY_NEW(Block, parent->pstate());
      CssStyleRule* new_rule = SASS_MEMORY_NEW(CssStyleRule,
        parent->pstate(),
        parent->selector(),
        bb->elements());

      new_rule->tabs(parent->tabs());

      // new_rule->elements(bb->elements());
      new_rule->concat(m->elements());

      // new_rule->concat(m->block()->elements());

      BlockObj wrapper_block = SASS_MEMORY_NEW(Block, m->pstate());
      wrapper_block->append(new_rule);
      CssSupportsRule* mm = SASS_MEMORY_NEW(CssSupportsRule,
        m->pstate(),
        m->condition(),
        wrapper_block->elements());
      // mm->block(wrapper_block);
      mm->tabs(m->tabs());

      Bubble* bubble = SASS_MEMORY_NEW(Bubble, mm->pstate(), mm);
      return bubble;

    }


    std::cerr << "return nully2\n";
    return nullptr;

  }
  Statement* Cssize::bubble(CssMediaRule* m)
  {
    if (CssStyleRuleObj parent = Cast<CssStyleRule>(SASS_MEMORY_COPY(this->parent()))) {

      CssStyleRule* new_rule = SASS_MEMORY_NEW(CssStyleRule,
        parent->pstate(), parent->selector(), m->elements());
      new_rule->tabs(parent->tabs());

      CssMediaRuleObj mm = SASS_MEMORY_NEW(CssMediaRule,
        m->pstate(), m->queries(), sass::vector<StatementObj>({ new_rule }));
      mm->tabs(m->tabs());

      return SASS_MEMORY_NEW(Bubble, mm->pstate(), mm);

    }

    std::cerr << "return nully3\n";
    return nullptr;
  }

  bool Cssize::bubblable(Statement* s) const
  {
    return Cast<CssStyleRule>(s) || Cast<CssSupportsRule>(s) || s->bubbles();
  }

  sass::vector<StatementObj> Cssize::flatten2(Statement* s)
  {
    sass::vector<StatementObj> result;
    if (const Block * bb = Cast<Block>(s)) {
      Block_Obj bs = flatten(bb);
      for (size_t j = 0, K = bs->length(); j < K; ++j) {
        result.emplace_back(bs->at(j));
      }
    }
    else {
      result.emplace_back(s);
    }
    return result;
  }

  Block* Cssize::flatten(const Block* b)
  {
    Block* result = SASS_MEMORY_NEW(Block, b->pstate(), 0, b->is_root());
    for (size_t i = 0, L = b->length(); i < L; ++i) {
      result->append(b->at(i));
    }
    return result;
  }

  void Cssize::slice_by_bubble(Block* b, std::vector<std::pair<bool, Block_Obj>>& results)
  {
    for (size_t i = 0, L = b->length(); i < L; ++i) {
      Statement_Obj value = b->at(i);
      bool key = Cast<Bubble>(value) != NULL;

      if (!results.empty() && results.back().first == key)
      {
        Block_Obj wrapper_block = results.back().second;
        wrapper_block->append(value);
      }
      else
      {
        Block* wrapper_block = SASS_MEMORY_NEW(Block, value->pstate());
        wrapper_block->append(value);
        results.emplace_back(std::make_pair(key, wrapper_block));
      }
    }
  }

  Block* Cssize::debubble(Block* children, Statement* parent)
  {
    sass::vector<StatementObj>* previousBlock = nullptr;
    std::vector<std::pair<bool, Block_Obj>> baz;
    slice_by_bubble(children, baz);
    Block_Obj result = SASS_MEMORY_NEW(Block, children->pstate());

    for (size_t i = 0, L = baz.size(); i < L; ++i) {
      bool is_bubble = baz[i].first;
      Block_Obj slice = baz[i].second;

      if (!is_bubble) {
        if (!parent) {
          std::copy(slice->begin(), slice->end(),
            std::back_inserter(result->elements()));
        }
        else if (previousBlock) {
           std::copy(slice->begin(), slice->end(),
             std::back_inserter(*previousBlock));
        }
        else if (ParentStatementObj prev = Cast<ParentStatement>(parent)) {
          previousBlock = &slice->elements();
          prev = SASS_MEMORY_COPY(prev);
          prev->block(slice);
          result->append(prev);

        }
        else if (CssParentNode * prev = Cast<CssParentNode>(parent)) {
          previousBlock = &slice->elements();
          prev = SASS_MEMORY_COPY(prev);
          prev->elements(slice->elements());
          previousBlock = &prev->elements();
          result->append(prev);
        }
        continue;
      }

      for (size_t j = 0, K = slice->length(); j < K; ++j)
      {
        Bubble* bubble = Cast<Bubble>(slice->get(j));
        StatementObj bubbled = bubble->node()->perform(this);
        sass::vector<StatementObj> results = flatten2(bubbled);
        result->concat(results);

        if (!results.empty()) {
          previousBlock = {};
        }
      }
    }

    return flatten(result);
  }

  void Cssize::append_block(Block* b, Block* cur)
  {
    for (size_t i = 0, L = b->length(); i < L; ++i) {
      Statement_Obj ith = b->at(i)->perform(this);
      if (Block_Obj bb = Cast<Block>(ith)) {
        for (size_t j = 0, K = bb->length(); j < K; ++j) {
          cur->append(bb->at(j));
        }
      }
      else if (ith) {
        cur->append(ith);
      }
    }
  }

}
