// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include <iostream>
#include <typeinfo>
#include <vector>

#include "cssize.hpp"
#include "context.hpp"

namespace Sass {

  Cssize::Cssize(Context& ctx)
  : traces(ctx.traces),
    block_stack(BlockStack()),
    p_stack(std::vector<Statement*>())
  { }

  Statement* Cssize::parent()
  {
    return p_stack.size() ? p_stack.back() : block_stack.front();
  }

  Block* Cssize::operator()(Block* b)
  {
    Block_Obj bb = SASS_MEMORY_NEW(Block, b->pstate(), b->length(), b->is_root());
    // bb->tabs(b->tabs());
    block_stack.push_back(bb);
    append_block(b, bb);
    block_stack.pop_back();
    return bb.detach();
  }

  Statement* Cssize::operator()(Trace* t)
  {
    traces.push_back(Backtrace(t->pstate()));
    auto result = t->block()->perform(this);
    traces.pop_back();
    return result;
  }

  Statement* Cssize::operator()(Declaration* d)
  {
    String_Obj property = Cast<String>(d->property());

    if (Declaration* dd = Cast<Declaration>(parent())) {
      String_Obj parent_property = Cast<String>(dd->property());
      property = SASS_MEMORY_NEW(String_Constant,
                                 d->property()->pstate(),
                                 parent_property->to_string() + "-" + property->to_string());
      if (!dd->value()) {
        d->tabs(dd->tabs() + 1);
      }
    }

    Declaration_Obj dd = SASS_MEMORY_NEW(Declaration,
                                      d->pstate(),
                                      property,
                                      d->value(),
                                      d->is_custom_property());
    // dd->is_indented(d->is_indented());
    dd->tabs(d->tabs());

    p_stack.push_back(dd);
    Block_Obj bb = d->block() ? operator()(d->block()) : NULL;
    p_stack.pop_back();

    if (bb && bb->length()) {
      if (dd->value() && !dd->value()->is_invisible()) {
        bb->unshift(dd);
      }
      return bb.detach();
    }
    else if (dd->value() && !dd->value()->is_invisible()) {
      return dd.detach();
    }

    std::cerr << "return nully4\n";
    return nullptr;
  }

  Statement* Cssize::operator()(AtRule* r)
  {
    if (!r->block() || !r->block()->length()) return r;

    if (parent()->statement_type() == Statement::RULESET)
    {
      return (r->is_keyframes()) ? SASS_MEMORY_NEW(Bubble, r->pstate(), r) : bubble(r);
    }

    p_stack.push_back(r);
    AtRuleObj rr = SASS_MEMORY_NEW(AtRule,
                                  r->pstate(),
                                  r->keyword(),
                                  r->block() ? operator()(r->block()) : 0);
    if (r->value()) rr->value(r->value());
    p_stack.pop_back();

    bool directive_exists = false;
    size_t L = rr->block() ? rr->block()->length() : 0;
    for (size_t i = 0; i < L && !directive_exists; ++i) {
      Statement_Obj s = r->block()->at(i);
      if (s->statement_type() != Statement::BUBBLE) directive_exists = true;
      else {
        // not hit via specs, seems legit!?
        Bubble_Obj s_obj = Cast<Bubble>(s);
        s = s_obj->node();
        if (s->statement_type() != Statement::DIRECTIVE) directive_exists = false;
        else directive_exists = (Cast<AtRule>(s)->keyword() == rr->keyword());
      }

    }

    Block* result = SASS_MEMORY_NEW(Block, rr->pstate());
    if (!(directive_exists || rr->is_keyframes()))
    {
      AtRule* empty_node = Cast<AtRule>(rr);
      empty_node->block(SASS_MEMORY_NEW(Block, rr->block() ? rr->block()->pstate() : rr->pstate()));
      result->append(empty_node);
    }

    Block_Obj db = rr->block();
    if (db.isNull()) db = SASS_MEMORY_NEW(Block, rr->pstate());
    Block_Obj ss = debubble(db, rr);
    for (size_t i = 0, L = ss->length(); i < L; ++i) {
      result->append(ss->at(i));
    }

    return result;
  }

  Statement* Cssize::operator()(Keyframe_Rule* r)
  {
    if (!r->block() || !r->block()->length()) return r;

    Keyframe_Rule_Obj rr = SASS_MEMORY_NEW(Keyframe_Rule,
                                        r->pstate(),
                                        operator()(r->block()));

    if (!r->name().isNull()) rr->name(r->name());
    if (!r->name2().isNull()) rr->name2(r->name2());

    return debubble(rr->block(), rr);
  }

  Statement* Cssize::operator()(CssStyleRule* r)
  {
    p_stack.push_back(r);
    Block* bb = operator()(r->block());
    CssStyleRuleObj rr = SASS_MEMORY_NEW(CssStyleRule,
      r->pstate(),
      r->selector());
    rr->block(bb);

    // rr->is_root(r->is_root());
    p_stack.pop_back();


    Block_Obj props = SASS_MEMORY_NEW(Block, rr->block()->pstate());
    Block* rules = SASS_MEMORY_NEW(Block, rr->block()->pstate());
    for (size_t i = 0, L = rr->block()->length(); i < L; i++)
    {
      Statement* s = rr->block()->at(i);
      if (bubblable(s)) {
        rules->append(s);
      }
      else {
        props->append(s);
      }
    }

    if (props->length())
    {

      rr->block(SASS_MEMORY_NEW(Block,
        rr->block()->pstate(),
        props->elements()));

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
      parent()->statement_type() == Statement::RULESET))
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
    if (parent()->statement_type() == Statement::RULESET)
    {
      return bubble(m);
    }

    if (parent()->statement_type() == Statement::MEDIA)
    {
      return SASS_MEMORY_NEW(Bubble, m->pstate(), m);
    }

    p_stack.push_back(m);

    CssMediaRuleObj mm = SASS_MEMORY_NEW(CssMediaRule, m->pstate(), m->block());
    mm->Vectorized::concat(m->elements());
    mm->block(operator()(m->block()));
    mm->tabs(m->tabs());

    p_stack.pop_back();

    return debubble(mm->block(), mm);
  }

  Statement* Cssize::operator()(SupportsRule* m)
  {
    if (!m->block()->length())
    {
      return m;
    }

    if (parent()->statement_type() == Statement::RULESET)
    {
      return bubble(m);
    }

    p_stack.push_back(m);

    SupportsRule_Obj mm = SASS_MEMORY_NEW(SupportsRule,
      m->pstate(),
      m->condition(),
      operator()(m->block()));
    mm->tabs(m->tabs());

    p_stack.pop_back();

    return debubble(mm->block(), mm);
  }

  Statement* Cssize::operator()(CssSupportsRule* m)
  {
    if (!m->block()) {
      return m;
    }
    if (!m->block()->length())
    {
      return m;
    }

    if (parent()->statement_type() == Statement::RULESET)
    {
      return bubble(m);
    }

    p_stack.push_back(m);

    CssSupportsRuleObj mm = SASS_MEMORY_NEW(CssSupportsRule,
      m->pstate(),
      m->condition());
    mm->block(operator()(m->block()));
    mm->tabs(m->tabs());

    p_stack.pop_back();

    return debubble(mm->block(), mm);
  }

  Statement* Cssize::operator()(At_Root_Block* m)
  {
    bool tmp = false;
    for (size_t i = 0, L = p_stack.size(); i < L; ++i) {
      Statement* s = p_stack[i];
      tmp |= m->exclude_node(s);
    }

    if (!tmp && m->block())
    {
      Block* bb = operator()(m->block());
      for (size_t i = 0, L = bb->length(); i < L; ++i) {
        // (bb->elements())[i]->tabs(m->tabs());
        Statement_Obj stm = bb->at(i);
        if (bubblable(stm)) stm->tabs(stm->tabs() + m->tabs());
      }
      if (bb->length() && bubblable(bb->last())) bb->last()->group_end(m->group_end());
      return bb;
    }

    if (m->exclude_node(parent()))
    {
      return SASS_MEMORY_NEW(Bubble, m->pstate(), m);
    }

    return bubble(m);
  }

  Statement* Cssize::bubble(AtRule* m)
  {
    Block* bb = SASS_MEMORY_NEW(Block, this->parent()->pstate());
    StatementObj cp = SASS_MEMORY_COPY(this->parent());
    Block_Obj wrapper_block = SASS_MEMORY_NEW(Block, m->block() ? m->block()->pstate() : m->pstate());
    if (CssStyleRule* new_rule = Cast<CssStyleRule>(cp)) {
      new_rule->block(bb);
      new_rule->tabs(this->parent()->tabs());
      new_rule->block()->concat(m->block());
      wrapper_block->append(new_rule);
    }

    AtRuleObj mm = SASS_MEMORY_NEW(AtRule,
                                  m->pstate(),
                                  m->keyword(),
                                  wrapper_block);
    if (m->value()) mm->value(m->value());

    Bubble* bubble = SASS_MEMORY_NEW(Bubble, mm->pstate(), mm);
    return bubble;
  }

  Statement* Cssize::bubble(At_Root_Block* m)
  {
    if (!m || !m->block()) return NULL;
    Block* bb = SASS_MEMORY_NEW(Block, this->parent()->pstate());
    StatementObj cp = SASS_MEMORY_COPY(this->parent());
    Block* wrapper_block = SASS_MEMORY_NEW(Block, m->block()->pstate());
    if (CssStyleRule * new_rule = Cast<CssStyleRule>(cp)) {
      new_rule->block(bb);
      new_rule->tabs(this->parent()->tabs());
      new_rule->block()->concat(m->block());
      wrapper_block->append(new_rule);
    }
    if (CssSupportsRule * new_rule = Cast<CssSupportsRule>(cp)) {
      new_rule->block(bb);
      new_rule->tabs(this->parent()->tabs());
      new_rule->block()->concat(m->block());
      wrapper_block->append(new_rule);
    }
    if (Has_Block * new_rule = Cast<Has_Block>(cp)) {
      new_rule->block(bb);
      new_rule->tabs(this->parent()->tabs());
      new_rule->block()->concat(m->block());
      wrapper_block->append(new_rule);
    }

    At_Root_Block* mm = SASS_MEMORY_NEW(At_Root_Block,
                                        m->pstate(),
                                        m->expression(),
                                        wrapper_block);
    Bubble* bubble = SASS_MEMORY_NEW(Bubble, mm->pstate(), mm);
    return bubble;
  }

  Statement* Cssize::bubble(SupportsRule* m)
  {
    if (CssStyleRuleObj parent = Cast<CssStyleRule>(SASS_MEMORY_COPY(this->parent()))) {

      Block* bb = SASS_MEMORY_NEW(Block, parent->block()->pstate());
      CssStyleRule* new_rule = SASS_MEMORY_NEW(CssStyleRule,
        parent->pstate(),
        parent->selector());
      new_rule->block(bb);
      new_rule->tabs(parent->tabs());
      new_rule->block()->concat(m->block());
      new_rule->concat(m->block()->elements());

      Block* wrapper_block = SASS_MEMORY_NEW(Block, m->block()->pstate());
      wrapper_block->append(new_rule);
      SupportsRule* mm = SASS_MEMORY_NEW(SupportsRule,
        m->pstate(),
        m->condition(),
        wrapper_block);

      mm->tabs(m->tabs());

      Bubble* bubble = SASS_MEMORY_NEW(Bubble, mm->pstate(), mm);
      return bubble;

    }

    std::cerr << "return nully\n";
    return nullptr;

  }

  Statement* Cssize::bubble(CssSupportsRule* m)
  {
    if (CssStyleRuleObj parent = Cast<CssStyleRule>(SASS_MEMORY_COPY(this->parent()))) {

      Block* bb = SASS_MEMORY_NEW(Block, parent->block()->pstate());
      CssStyleRule* new_rule = SASS_MEMORY_NEW(CssStyleRule,
        parent->pstate(),
        parent->selector());
      new_rule->block(bb);
      new_rule->tabs(parent->tabs());
      new_rule->block()->concat(m->block());
      new_rule->concat(m->block()->elements());

      Block* wrapper_block = SASS_MEMORY_NEW(Block, m->block()->pstate());
      wrapper_block->append(new_rule);
      CssSupportsRule* mm = SASS_MEMORY_NEW(CssSupportsRule,
        m->pstate(),
        m->condition());
      mm->block(wrapper_block);
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

      Block* bb = SASS_MEMORY_NEW(Block, parent->block()->pstate());
      CssStyleRule* new_rule = SASS_MEMORY_NEW(CssStyleRule,
        parent->pstate(),
        parent->selector());
      new_rule->block(bb);
      new_rule->tabs(parent->tabs());
      new_rule->block()->concat(m->block());
      new_rule->concat(m->block()->elements());

      Block* wrapper_block = SASS_MEMORY_NEW(Block, m->block()->pstate());
      wrapper_block->append(new_rule);
      CssMediaRuleObj mm = SASS_MEMORY_NEW(CssMediaRule,
        m->pstate(),
        wrapper_block);
      mm->Vectorized::concat(m->elements());

      mm->tabs(m->tabs());

      return SASS_MEMORY_NEW(Bubble, mm->pstate(), mm);

    }

    std::cerr << "return nully3\n";
    return nullptr;
  }

  bool Cssize::bubblable(Statement* s)
  {
    return Cast<CssStyleRule>(s) || Cast<CssSupportsRule>(s) || s->bubbles();
  }

  Block* Cssize::flatten(const Block* b)
  {
    Block* result = SASS_MEMORY_NEW(Block, b->pstate(), 0, b->is_root());
    for (size_t i = 0, L = b->length(); i < L; ++i) {
      Statement* ss = b->at(i);
      if (const Block* bb = Cast<Block>(ss)) {
        Block_Obj bs = flatten(bb);
        for (size_t j = 0, K = bs->length(); j < K; ++j) {
          result->append(bs->at(j));
        }
      }
      else {
        result->append(ss);
      }
    }
    return result;
  }

  std::vector<std::pair<bool, Block_Obj>> Cssize::slice_by_bubble(Block* b)
  {
    std::vector<std::pair<bool, Block_Obj>> results;

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
        results.push_back(std::make_pair(key, wrapper_block));
      }
    }
    return results;
  }

  Block* Cssize::debubble(Block* children, Statement* parent)
  {
    BlockObj previousBlock;
    std::vector<std::pair<bool, Block_Obj>> baz = slice_by_bubble(children);
    Block_Obj result = SASS_MEMORY_NEW(Block, children->pstate());

    for (size_t i = 0, L = baz.size(); i < L; ++i) {
      bool is_bubble = baz[i].first;
      Block_Obj slice = baz[i].second;

      if (!is_bubble) {
        if (!parent) {
          result->append(slice);
        }
        else if (previousBlock) {
          previousBlock->concat(slice);
        }
        else if (Has_Block_Obj prev = Cast<Has_Block>(parent)) {
          previousBlock = slice;
          prev = SASS_MEMORY_COPY(prev);
          prev->block(slice);
          prev->tabs(parent->tabs());
          result->append(prev);
        }
        else if (CssSupportsRuleObj prev = Cast<CssSupportsRule>(parent)) {
          previousBlock = slice;
          prev = SASS_MEMORY_COPY(prev);
          prev->block(slice);
          prev->tabs(parent->tabs());
          result->append(prev);
        }
        /* we dont see any style rule objects here yet
        else if (CssStyleRuleObj prev = Cast<CssStyleRule>(parent)) {
          previousBlock = slice;
          prev = SASS_MEMORY_COPY(prev);
          prev->block(slice);
          prev->tabs(parent->tabs());
          result->append(prev);
        }
        */
        continue;
      }

      for (size_t j = 0, K = slice->length(); j < K; ++j)
      {
        Statement_Obj ss;
        Statement_Obj stm = slice->at(j);
        // this has to go now here (too bad)
        Bubble_Obj node = Cast<Bubble>(stm);

        CssMediaRule* rule1 = NULL;
        CssMediaRule* rule2 = NULL;
        if (parent) rule1 = Cast<CssMediaRule>(parent);
        if (node) rule2 = Cast<CssMediaRule>(node->node());
        if (rule1 || rule2) {
          ss = node->node();
        }

        ss = node->node();

        if (!ss) {
          continue;
        }

        ss->tabs(ss->tabs() + node->tabs());
        ss->group_end(node->group_end());

        Block_Obj bb = SASS_MEMORY_NEW(Block,
                                    children->pstate(),
                                    children->length(),
                                    children->is_root());
        bb->append(ss->perform(this));

        Block_Obj wrapper_block = SASS_MEMORY_NEW(Block,
                                              children->pstate(),
                                              children->length(),
                                              children->is_root());

        Block* wrapper = flatten(bb);
        wrapper_block->append(wrapper);

        if (wrapper->length()) {
          previousBlock = {};
        }

        if (wrapper_block) {
          result->append(wrapper_block);
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
