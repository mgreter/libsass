#include "sass.hpp"
#include <iostream>
#include <typeinfo>
#include <vector>

#include "cssize.hpp"
#include "context.hpp"
#include "backtrace.hpp"

namespace Sass {

  Cssize::Cssize(Context& ctx, Backtrace* bt)
  : ctx(ctx),
    block_stack(std::vector<Block_Ptr>()),
    p_stack(std::vector<Statement_Ptr>()),
    backtrace(bt)
  { }

  Statement_Ptr Cssize::parent()
  {
    return p_stack.size() ? p_stack.back() : block_stack.front();
  }

  Block_Ptr Cssize::operator()(Block_Ptr b)
  {
    Block_Ptr bb = SASS_MEMORY_NEW(ctx.mem, Block, b->pstate(), b->length(), b->is_root());
    // bb->tabs(b->tabs());
    block_stack.push_back(bb);
    append_block(b);
    block_stack.pop_back();
    return bb;
  }

  Statement_Ptr Cssize::operator()(Trace_Ptr t)
  {
    return t->block()->perform(this);
  }

  Statement_Ptr Cssize::operator()(Declaration_Ptr d)
  {
    String_Ptr property = dynamic_cast<String_Ptr>(d->property());

    if (Declaration_Ptr dd = dynamic_cast<Declaration_Ptr>(parent())) {
      String_Ptr parent_property = dynamic_cast<String_Ptr>(dd->property());
      property = SASS_MEMORY_NEW(ctx.mem, String_Constant,
                                 d->property()->pstate(),
                                 parent_property->to_string() + "-" + property->to_string());
      if (!dd->value()) {
        d->tabs(dd->tabs() + 1);
      }
    }

    Declaration_Ptr dd = SASS_MEMORY_NEW(ctx.mem, Declaration,
                                      d->pstate(),
                                      property,
                                      d->value(),
                                      d->is_important());
    dd->is_indented(d->is_indented());
    dd->tabs(d->tabs());

    p_stack.push_back(dd);
    Block_Obj bb = d->block() ? operator()(d->block()) : NULL;
    p_stack.pop_back();

    if (bb && bb->length()) {
      if (dd->value() && !dd->value()->is_invisible()) {
        bb->unshift(dd);
      }
      return &bb;
    }
    else if (dd->value() && !dd->value()->is_invisible()) {
      return dd;
    }

    return 0;
  }

  Statement_Ptr Cssize::operator()(Directive_Ptr r)
  {
    if (!r->block() || !r->block()->length()) return r;

    if (parent()->statement_type() == Statement_Ref::RULESET)
    {
      return (r->is_keyframes()) ? SASS_MEMORY_NEW(ctx.mem, Bubble, r->pstate(), r) : bubble(r);
    }

    p_stack.push_back(r);
    Directive_Ptr rr = SASS_MEMORY_NEW(ctx.mem, Directive,
                                  r->pstate(),
                                  r->keyword(),
                                  r->selector(),
                                  r->block() ? operator()(r->block()) : 0);
    if (r->value()) rr->value(r->value());
    p_stack.pop_back();

    bool directive_exists = false;
    size_t L = rr->block() ? rr->block()->length() : 0;
    for (size_t i = 0; i < L && !directive_exists; ++i) {
      Statement_Obj s = (*r->block())[i];
      if (s->statement_type() != Statement_Ref::BUBBLE) directive_exists = true;
      else {
        Bubble_Obj s_obj = SASS_MEMORY_CAST(Bubble, s);
        s = s_obj->node();
        if (s->statement_type() != Statement_Ref::DIRECTIVE) directive_exists = false;
        else directive_exists = (static_cast<Directive_Ptr>(&s)->keyword() == rr->keyword());
      }

    }

    Block_Ptr result = SASS_MEMORY_NEW(ctx.mem, Block, rr->pstate());
    if (!(directive_exists || rr->is_keyframes()))
    {
      Directive_Ptr empty_node = static_cast<Directive_Ptr>(rr);
      empty_node->block(SASS_MEMORY_NEW(ctx.mem, Block, rr->block() ? rr->block()->pstate() : rr->pstate()));
      *result << empty_node;
    }

    Block_Obj ss = debubble(rr->block() ? rr->block() : SASS_MEMORY_NEW(ctx.mem, Block, rr->pstate()), rr);
    for (size_t i = 0, L = ss->length(); i < L; ++i) {
      result->append(ss->at(i));
    }

    return result;
  }

  Statement_Ptr Cssize::operator()(Keyframe_Rule_Ptr r)
  {
    if (!r->block() || !r->block()->length()) return r;

    Keyframe_Rule_Ptr rr = SASS_MEMORY_NEW(ctx.mem, Keyframe_Rule,
                                        r->pstate(),
                                        operator()(r->block()));
    if (&r->selector2()) rr->selector2(r->selector2());

    return &debubble(rr->block(), rr);
  }

  Statement_Ptr Cssize::operator()(Ruleset_Ptr r)
  {
    p_stack.push_back(r);
    // this can return a string schema
    // string schema is not a statement!
    // r->block() is already a string schema
    // and that is comming from propset expand
    Block_Obj bb = operator()(r->block());
    // this should protect us (at least a bit) from our mess
    // fixing this properly is harder that it should be ...
    if (dynamic_cast<Statement_Ptr>(&bb) == NULL) {
      error("Illegal nesting: Only properties may be nested beneath properties.", r->block()->pstate());
    }
    Ruleset_Ptr rr = SASS_MEMORY_NEW(ctx.mem, Ruleset,
                                  r->pstate(),
                                  r->selector(),
                                  bb);
    rr->is_root(r->is_root());
    // rr->tabs(r->block()->tabs());
    p_stack.pop_back();

    if (!rr->block()) {
      error("Illegal nesting: Only properties may be nested beneath properties.", r->block()->pstate());
    }

    Block_Obj props = SASS_MEMORY_NEW(ctx.mem, Block, rr->block()->pstate());
    Block_Obj rules = SASS_MEMORY_NEW(ctx.mem, Block, rr->block()->pstate());
    for (size_t i = 0, L = rr->block()->length(); i < L; i++)
    {
      Statement_Ptr s = rr->block()->at(i);
      if (bubblable(s)) rules->append(s);
      if (!bubblable(s)) props->append(s);
    }

    if (props->length())
    {
      Block_Obj bb = SASS_MEMORY_NEW(ctx.mem, Block, rr->block()->pstate());
      bb->concat(&props);
      rr->block(&bb);

      for (size_t i = 0, L = rules->length(); i < L; i++)
      {
        rules->at(i)->tabs(rules->at(i)->tabs() + 1);
      }

      rules->unshift(rr);
    }

    rules = &debubble(rules);

    if (!(!rules->length() ||
          !bubblable(rules->last()) ||
          parent()->statement_type() == Statement_Ref::RULESET))
    {
      rules->last()->group_end(true);
    }

    return &rules;
  }

  Statement_Ptr Cssize::operator()(Null_Ptr m)
  {
    return 0;
  }

  Statement_Ptr Cssize::operator()(Media_Block_Ptr m)
  {
    if (parent()->statement_type() == Statement_Ref::RULESET)
    { return bubble(m); }

    if (parent()->statement_type() == Statement_Ref::MEDIA)
    { return SASS_MEMORY_NEW(ctx.mem, Bubble, m->pstate(), m); }

    p_stack.push_back(m);

    Media_Block_Obj mm = SASS_MEMORY_NEW(ctx.mem, Media_Block,
                                      m->pstate(),
                                      &m->media_queries(),
                                      operator()(m->block()));
    mm->tabs(m->tabs());

    p_stack.pop_back();

    return &debubble(mm->block(), &mm);
  }

  Statement_Ptr Cssize::operator()(Supports_Block_Ptr m)
  {
    if (!m->block()->length())
    { return m; }

    if (parent()->statement_type() == Statement_Ref::RULESET)
    { return bubble(m); }

    p_stack.push_back(m);

    Supports_Block_Ptr mm = SASS_MEMORY_NEW(ctx.mem, Supports_Block,
                                       m->pstate(),
                                       &m->condition(),
                                       operator()(m->block()));
    mm->tabs(m->tabs());

    p_stack.pop_back();

    return &debubble(mm->block(), mm);
  }

  Statement_Ptr Cssize::operator()(At_Root_Block_Ptr m)
  {
    bool tmp = false;
    for (size_t i = 0, L = p_stack.size(); i < L; ++i) {
      Statement_Ptr s = p_stack[i];
      tmp |= m->exclude_node(s);
    }

    if (!tmp)
    {
      Block_Obj bb = operator()(m->block());
      for (size_t i = 0, L = bb->length(); i < L; ++i) {
        // (bb->elements())[i]->tabs(m->tabs());
        if (bubblable(bb->at(i))) bb->at(i)->tabs(bb->at(i)->tabs() + m->tabs());
      }
      if (bb->length() && bubblable(bb->last())) bb->last()->group_end(m->group_end());
      return &bb;
    }

    if (m->exclude_node(parent()))
    {
      return SASS_MEMORY_NEW(ctx.mem, Bubble, m->pstate(), m);
    }

    return bubble(m);
  }

  Statement_Ptr Cssize::bubble(Directive_Ptr m)
  {
    Block_Obj bb = SASS_MEMORY_NEW(ctx.mem, Block, this->parent()->pstate());
    Has_Block_Ptr new_rule = static_cast<Has_Block_Ptr>(shallow_copy(this->parent()));
    new_rule->block(&bb);
    new_rule->tabs(this->parent()->tabs());

    size_t L = m->block() ? m->block()->length() : 0;
    for (size_t i = 0; i < L; ++i) {
      *new_rule->block() << (*m->block())[i];
    }

    Block_Obj wrapper_block = SASS_MEMORY_NEW(ctx.mem, Block, m->block() ? m->block()->pstate() : m->pstate());
    wrapper_block->append(new_rule);
    Directive_Ptr mm = SASS_MEMORY_NEW(ctx.mem, Directive,
                                  m->pstate(),
                                  m->keyword(),
                                  m->selector(),
                                  wrapper_block);
    if (m->value()) mm->value(m->value());

    Bubble_Ptr bubble = SASS_MEMORY_NEW(ctx.mem, Bubble, mm->pstate(), mm);
    return bubble;
  }

  Statement_Ptr Cssize::bubble(At_Root_Block_Ptr m)
  {
    Block_Obj bb = SASS_MEMORY_NEW(ctx.mem, Block, this->parent()->pstate());
    Has_Block_Ptr new_rule = static_cast<Has_Block_Ptr>(shallow_copy(this->parent()));
    new_rule->block(&bb);
    new_rule->tabs(this->parent()->tabs());

    for (size_t i = 0, L = m->block()->length(); i < L; ++i) {
      *new_rule->block() << (*m->block())[i];
    }

    Block_Obj wrapper_block = SASS_MEMORY_NEW(ctx.mem, Block, m->block()->pstate());
    wrapper_block->append(new_rule);
    At_Root_Block_Ptr mm = SASS_MEMORY_NEW(ctx.mem, At_Root_Block,
                                        m->pstate(),
                                        wrapper_block,
                                        m->expression());
    Bubble_Ptr bubble = SASS_MEMORY_NEW(ctx.mem, Bubble, mm->pstate(), mm);
    return bubble;
  }

  Statement_Ptr Cssize::bubble(Supports_Block_Ptr m)
  {
    Ruleset_Ptr parent = static_cast<Ruleset_Ptr>(shallow_copy(this->parent()));

    Block_Obj bb = SASS_MEMORY_NEW(ctx.mem, Block, parent->block()->pstate());
    Ruleset_Ptr new_rule = SASS_MEMORY_NEW(ctx.mem, Ruleset,
                                        parent->pstate(),
                                        parent->selector(),
                                        bb);
    new_rule->tabs(parent->tabs());

    for (size_t i = 0, L = m->block()->length(); i < L; ++i) {
      *new_rule->block() << (*m->block())[i];
    }

    Block_Obj wrapper_block = SASS_MEMORY_NEW(ctx.mem, Block, m->block()->pstate());
    wrapper_block->append(new_rule);
    Supports_Block_Ptr mm = SASS_MEMORY_NEW(ctx.mem, Supports_Block,
                                       m->pstate(),
                                       &m->condition(),
                                       wrapper_block);

    mm->tabs(m->tabs());

    Bubble_Ptr bubble = SASS_MEMORY_NEW(ctx.mem, Bubble, mm->pstate(), mm);
    return bubble;
  }

  Statement_Ptr Cssize::bubble(Media_Block_Obj m)
  {
    Ruleset_Ptr parent = static_cast<Ruleset_Ptr>(shallow_copy(this->parent()));

    Block_Obj bb = SASS_MEMORY_NEW(ctx.mem, Block, parent->block()->pstate());
    Ruleset_Ptr new_rule = SASS_MEMORY_NEW(ctx.mem, Ruleset,
                                        parent->pstate(),
                                        parent->selector(),
                                        bb);
    new_rule->tabs(parent->tabs());

    for (size_t i = 0, L = m->block()->length(); i < L; ++i) {
      *new_rule->block() << (*m->block())[i];
    }

    Block_Obj wrapper_block = SASS_MEMORY_NEW(ctx.mem, Block, m->block()->pstate());
    wrapper_block->append(new_rule);
    Media_Block_Obj mm = SASS_MEMORY_NEW(ctx.mem, Media_Block,
                                      m->pstate(),
                                      &m->media_queries(),
                                      wrapper_block,
                                      0);

    mm->tabs(m->tabs());

    return SASS_MEMORY_NEW(ctx.mem, Bubble, mm->pstate(), &mm);
  }

  bool Cssize::bubblable(Statement_Ptr s)
  {
    return s->statement_type() == Statement_Ref::RULESET || s->bubbles();
  }

  Block_Obj Cssize::flatten(Block_Obj b)
  {
    Block_Obj result = SASS_MEMORY_OBJ(ctx.mem, Block, b->pstate(), 0, b->is_root());
    for (size_t i = 0, L = b->length(); i < L; ++i) {
      Statement_Ptr ss = b->at(i);
      if (Block_Obj bb = SASS_MEMORY_CAST_PTR(Block, ss)) {
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

  std::vector<std::pair<bool, Block_Obj>> Cssize::slice_by_bubble(Block_Obj b)
  {
    std::vector<std::pair<bool, Block_Obj>> results;
    for (size_t i = 0, L = b->length(); i < L; ++i) {
      Statement_Ptr value = b->at(i);
      bool key = value->statement_type() == Statement_Ref::BUBBLE;

      if (!results.empty() && results.back().first == key)
      {
        Block_Obj wrapper_block = results.back().second;
        wrapper_block->append(value);
      }
      else
      {
        Block_Obj wrapper_block = SASS_MEMORY_NEW(ctx.mem, Block, value->pstate());
        wrapper_block->append(value);
        results.push_back(std::make_pair(key, wrapper_block));
      }
    }
    return results;
  }

  Statement_Ptr Cssize::shallow_copy(Statement_Ptr s)
  {
    switch (s->statement_type())
    {
      case Statement_Ref::RULESET:
        return SASS_MEMORY_NEW(ctx.mem, Ruleset, *static_cast<Ruleset_Ptr>(s));
      case Statement_Ref::MEDIA:
        return SASS_MEMORY_NEW(ctx.mem, Media_Block, *static_cast<Media_Block_Ptr>(s));
      case Statement_Ref::BUBBLE:
        return SASS_MEMORY_NEW(ctx.mem, Bubble, *static_cast<Bubble_Ptr>(s));
      case Statement_Ref::DIRECTIVE:
        return SASS_MEMORY_NEW(ctx.mem, Directive, *static_cast<Directive_Ptr>(s));
      case Statement_Ref::SUPPORTS:
        return SASS_MEMORY_NEW(ctx.mem, Supports_Block, *static_cast<Supports_Block_Ptr>(s));
      case Statement_Ref::ATROOT:
        return SASS_MEMORY_NEW(ctx.mem, At_Root_Block, *static_cast<At_Root_Block_Ptr>(s));
      case Statement_Ref::KEYFRAMERULE:
        return SASS_MEMORY_NEW(ctx.mem, Keyframe_Rule, *static_cast<Keyframe_Rule_Ptr>(s));
      case Statement_Ref::NONE:
      default:
        error("unknown internal error; please contact the LibSass maintainers", s->pstate(), backtrace);
        String_Quoted_Ptr msg = SASS_MEMORY_NEW(ctx.mem, String_Quoted, ParserState("[WARN]"), std::string("`CSSize` can't clone ") + typeid(*s).name());
        return SASS_MEMORY_NEW(ctx.mem, Warning, ParserState("[WARN]"), msg);
    }
  }

  Block_Obj Cssize::debubble(Block_Obj children, Statement_Ptr parent)
  {
    Has_Block_Ptr previous_parent = 0;
    std::vector<std::pair<bool, Block_Obj>> baz = slice_by_bubble(children);
    Block_Obj result = SASS_MEMORY_NEW(ctx.mem, Block, children->pstate());

    for (size_t i = 0, L = baz.size(); i < L; ++i) {
      bool is_bubble = baz[i].first;
      Block_Obj slice = baz[i].second;

      if (!is_bubble) {
        if (!parent) {
          result->append(&slice);
        }
        else if (previous_parent) {
          previous_parent->block()->concat(&slice);
        }
        else {
          previous_parent = static_cast<Has_Block_Ptr>(shallow_copy(parent));
          previous_parent->block(&slice);
          previous_parent->tabs(parent->tabs());

          Has_Block_Ptr new_parent = previous_parent;

          result->append(new_parent);
        }
        continue;
      }

      for (size_t j = 0, K = slice->length(); j < K; ++j)
      {
        Statement_Ptr ss = 0;
        Bubble_Obj node = SASS_MEMORY_CAST(Bubble, *slice->at(j));
        Media_Block_Obj m1;
        Media_Block_Obj m2;
        if (parent) m1 = SASS_MEMORY_CAST(Media_Block, *parent);
        if (node) m2 = SASS_MEMORY_CAST(Media_Block, node->node());
        if (!parent ||
            parent->statement_type() != Statement_Ref::MEDIA ||
            node->node()->statement_type() != Statement_Ref::MEDIA ||
            (m1 && m2 && &m1->media_queries() == &m2->media_queries())
          )
        {
          ss = &node->node();
        }
        else
        {
          List_Obj mq = merge_media_queries(static_cast<Media_Block_Ptr>(&node->node()), static_cast<Media_Block_Ptr>(parent));
          if (!mq->length()) continue;
          static_cast<Media_Block_Ptr>(&node->node())->media_queries(mq);
          ss = &node->node();
        }

        if (!ss) continue;

        ss->tabs(ss->tabs() + node->tabs());
        ss->group_end(node->group_end());

        if (!ss) continue;

        Block_Obj bb = SASS_MEMORY_NEW(ctx.mem, Block,
                                    children->pstate(),
                                    children->length(),
                                    children->is_root());
        bb->append(ss->perform(this));

        Block_Obj wrapper_block = SASS_MEMORY_NEW(ctx.mem, Block,
                                              children->pstate(),
                                              children->length(),
                                              children->is_root());

        Block_Obj wrapper = flatten(bb);
        wrapper_block->append(&wrapper);

        if (wrapper->length()) {
          previous_parent = 0;
        }

        if (wrapper_block) {
          result->append(&wrapper_block);
        }
      }
    }

    return flatten(result);
  }

  Statement_Ptr Cssize::fallback_impl(AST_Node_Ptr n)
  {
    return static_cast<Statement_Ptr>(n);
  }

  void Cssize::append_block(Block_Ptr b)
  {
    Block_Ptr current_block = block_stack.back();

    for (size_t i = 0, L = b->length(); i < L; ++i) {
      Statement_Ptr ith = b->at(i)->perform(this);
      if (Block_Obj bb = SASS_MEMORY_CAST_PTR(Block, ith)) {
        for (size_t j = 0, K = bb->length(); j < K; ++j) {
          current_block->append(bb->at(j));
        }
      }
      else if (ith) {
        *current_block << ith;
      }
    }
  }

  List_Obj Cssize::merge_media_queries(Media_Block_Obj m1, Media_Block_Obj m2)
  {
    List_Obj qq = SASS_MEMORY_OBJ(ctx.mem, List,
                               m1->media_queries()->pstate(),
                               m1->media_queries()->length(),
                               SASS_COMMA);

    for (size_t i = 0, L = m1->media_queries()->length(); i < L; i++) {
      for (size_t j = 0, K = m2->media_queries()->length(); j < K; j++) {
        Expression_Obj l1 = m1->media_queries()->at(i);
        Expression_Obj l2 = m2->media_queries()->at(j);
        Media_Query_Obj mq1 = SASS_MEMORY_CAST(Media_Query, l1);
        Media_Query_Obj mq2 = SASS_MEMORY_CAST(Media_Query, l2);;
        Media_Query_Obj mq = merge_media_query(mq1, mq2);
        if (mq) qq->append(&mq);
      }
    }

    return qq;
  }


  Media_Query_Obj Cssize::merge_media_query(Media_Query_Obj mq1, Media_Query_Obj mq2)
  {

    std::string type;
    std::string mod;

    std::string m1 = std::string(mq1->is_restricted() ? "only" : mq1->is_negated() ? "not" : "");
    std::string t1 = &mq1->media_type() ? mq1->media_type()->to_string(ctx.c_options) : "";
    std::string m2 = std::string(mq2->is_restricted() ? "only" : mq1->is_negated() ? "not" : "");
    std::string t2 = &mq2->media_type() ? mq2->media_type()->to_string(ctx.c_options) : "";


    if (t1.empty()) t1 = t2;
    if (t2.empty()) t2 = t1;

    if ((m1 == "not") ^ (m2 == "not")) {
      if (t1 == t2) {
        return 0;
      }
      type = m1 == "not" ? t2 : t1;
      mod = m1 == "not" ? m2 : m1;
    }
    else if (m1 == "not" && m2 == "not") {
      if (t1 != t2) {
        return 0;
      }
      type = t1;
      mod = "not";
    }
    else if (t1 != t2) {
      return 0;
    } else {
      type = t1;
      mod = m1.empty() ? m2 : m1;
    }

    Media_Query_Ptr mm = SASS_MEMORY_NEW(ctx.mem, Media_Query,

mq1->pstate(), 0,
mq1->length() + mq2->length(), mod == "not", mod == "only"
);

    if (!type.empty()) {
      mm->media_type(SASS_MEMORY_NEW(ctx.mem, String_Quoted, mq1->pstate(), type));
    }

    mm->concat(&mq2);
    mm->concat(&mq1);

    return mm;
  }
}
