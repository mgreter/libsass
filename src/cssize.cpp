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

  Statement_Ptr Cssize::operator()(Block_Ptr b)
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
    Block_Ptr bb = d->block() ? d->block()->perform(this)->block() : 0;
    p_stack.pop_back();

    if (bb && bb->length()) {
      if (dd->value() && !dd->value()->is_invisible()) {
        bb->unshift(dd);
      }
      return bb;
    }
    else if (dd->value() && !dd->value()->is_invisible()) {
      return dd;
    }

    return 0;
  }

  Statement_Ptr Cssize::operator()(Directive_Ptr r)
  {
    if (!r->block() || !r->block()->length()) return r;

    if (parent()->statement_type() == Statement::RULESET)
    {
      return (r->is_keyframes()) ? SASS_MEMORY_NEW(ctx.mem, Bubble, r->pstate(), r) : bubble(r);
    }

    p_stack.push_back(r);
    Directive_Ptr rr = SASS_MEMORY_NEW(ctx.mem, Directive,
                                  r->pstate(),
                                  r->keyword(),
                                  r->selector(),
                                  r->block() ? r->block()->perform(this)->block() : 0);
    if (r->value()) rr->value(r->value());
    p_stack.pop_back();

    bool directive_exists = false;
    size_t L = rr->block() ? rr->block()->length() : 0;
    for (size_t i = 0; i < L && !directive_exists; ++i) {
      Statement_Ptr s = r->block()->get(i);
      if (s->statement_type() != Statement::BUBBLE) directive_exists = true;
      else {
        s = static_cast<Bubble_Ptr>(s)->node();
        if (s->statement_type() != Statement::DIRECTIVE) directive_exists = false;
        else directive_exists = (static_cast<Directive_Ptr>(s)->keyword() == rr->keyword());
      }

    }

    Block_Ptr result = SASS_MEMORY_NEW(ctx.mem, Block, rr->pstate());
    if (!(directive_exists || rr->is_keyframes()))
    {
      Directive_Ptr empty_node = static_cast<Directive_Ptr>(rr);
      empty_node->block(SASS_MEMORY_NEW(ctx.mem, Block, rr->block() ? rr->block()->pstate() : rr->pstate()));
      result->push2(empty_node);
    }

    Statement_Ptr ss = debubble(rr->block() ? rr->block() : SASS_MEMORY_NEW(ctx.mem, Block, rr->pstate()), rr);
    for (size_t i = 0, L = ss->block()->length(); i < L; ++i) {
      result->push2(ss->block()->get(i));
    }

    return result;
  }

  Statement_Ptr Cssize::operator()(Keyframe_Rule_Ptr r)
  {
    if (!r->block() || !r->block()->length()) return r;

    Keyframe_Rule_Ptr rr = SASS_MEMORY_NEW(ctx.mem, Keyframe_Rule,
                                        r->pstate(),
                                        r->block()->perform(this)->block());
    if (r->selector()) rr->selector(r->selector());

    return debubble(rr->block(), rr)->block();
  }

  Statement_Ptr Cssize::operator()(Ruleset_Ptr r)
  {
    p_stack.push_back(r);
    // this can return a string schema
    // string schema is not a statement!
    // r->block() is already a string schema
    // and that is comming from propset expand
    Statement_Ptr stmt = r->block()->perform(this);
    // this should protect us (at least a bit) from our mess
    // fixing this properly is harder that it should be ...
    if (dynamic_cast<Statement_Ptr>((AST_Node_Ptr)stmt) == NULL) {
      error("Illegal nesting: Only properties may be nested beneath properties.", r->block()->pstate());
    }
    Ruleset_Ptr rr = SASS_MEMORY_NEW(ctx.mem, Ruleset,
                                  r->pstate(),
                                  r->selector(),
                                  stmt->block());
    rr->is_root(r->is_root());
    // rr->tabs(r->block()->tabs());
    p_stack.pop_back();

    if (!rr->block()) {
      error("Illegal nesting: Only properties may be nested beneath properties.", r->block()->pstate());
    }

    Block_Ptr props = SASS_MEMORY_NEW(ctx.mem, Block, rr->block()->pstate());
    Block_Ptr rules = SASS_MEMORY_NEW(ctx.mem, Block, rr->block()->pstate());
    for (size_t i = 0, L = rr->block()->length(); i < L; i++)
    {
      Statement_Ptr s = rr->block()->get(i);
      if (bubblable(s)) rules->push2(s);
      if (!bubblable(s)) props->push2(s);
    }

    if (props->length())
    {
      Block_Ptr bb = SASS_MEMORY_NEW(ctx.mem, Block, rr->block()->pstate());
      bb->concat(props);
      rr->block(bb);

      for (size_t i = 0, L = rules->length(); i < L; i++)
      {
        rules->get(i)->tabs(rules->get(i)->tabs() + 1);
      }

      rules->unshift(rr);
    }

    rules = debubble(rules)->block();

    if (!(!rules->length() ||
          !bubblable(rules->last()) ||
          parent()->statement_type() == Statement::RULESET))
    {
      rules->last()->group_end(true);
    }

    return rules;
  }

  Statement_Ptr Cssize::operator()(Null_Ptr m)
  {
    return 0;
  }

  Statement_Ptr Cssize::operator()(Media_Block_Ptr m)
  {
    if (parent()->statement_type() == Statement::RULESET)
    { return bubble(m); }

    if (parent()->statement_type() == Statement::MEDIA)
    { return SASS_MEMORY_NEW(ctx.mem, Bubble, m->pstate(), m); }

    p_stack.push_back(m);

    Media_Block_Ptr mm = SASS_MEMORY_NEW(ctx.mem, Media_Block,
                                      m->pstate(),
                                      m->media_queries(),
                                      m->block()->perform(this)->block());
    mm->tabs(m->tabs());

    p_stack.pop_back();

    return debubble(mm->block(), mm)->block();
  }

  Statement_Ptr Cssize::operator()(Supports_Block_Ptr m)
  {
    if (!m->block()->length())
    { return m; }

    if (parent()->statement_type() == Statement::RULESET)
    { return bubble(m); }

    p_stack.push_back(m);

    Supports_Block_Ptr mm = SASS_MEMORY_NEW(ctx.mem, Supports_Block,
                                       m->pstate(),
                                       m->condition(),
                                       m->block()->perform(this)->block());
    mm->tabs(m->tabs());

    p_stack.pop_back();

    return debubble(mm->block(), mm)->block();
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
      Block_Ptr bb = m->block()->perform(this)->block();
      for (size_t i = 0, L = bb->length(); i < L; ++i) {
        // (bb->elements())[i]->tabs(m->tabs());
        if (bubblable(bb->get(i))) bb->get(i)->tabs(bb->get(i)->tabs() + m->tabs());
      }
      if (bb->length() && bubblable(bb->last())) bb->last()->group_end(m->group_end());
      return bb;
    }

    if (m->exclude_node(parent()))
    {
      return SASS_MEMORY_NEW(ctx.mem, Bubble, m->pstate(), m);
    }

    return bubble(m);
  }

  Statement_Ptr Cssize::bubble(Directive_Ptr m)
  {
    Block_Ptr bb = SASS_MEMORY_NEW(ctx.mem, Block, this->parent()->pstate());
    Has_Block_Ptr new_rule = static_cast<Has_Block_Ptr>(shallow_copy(this->parent()));
    new_rule->block(bb);
    new_rule->tabs(this->parent()->tabs());

    size_t L = m->block() ? m->block()->length() : 0;
    for (size_t i = 0; i < L; ++i) {
      new_rule->block()->push2(m->block()->get(i));
    }

    Block_Ptr wrapper_block = SASS_MEMORY_NEW(ctx.mem, Block, m->block() ? m->block()->pstate() : m->pstate());
    wrapper_block->push2(new_rule);
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
    Block_Ptr bb = SASS_MEMORY_NEW(ctx.mem, Block, this->parent()->pstate());
    Has_Block_Ptr new_rule = static_cast<Has_Block_Ptr>(shallow_copy(this->parent()));
    new_rule->block(bb);
    new_rule->tabs(this->parent()->tabs());

    for (size_t i = 0, L = m->block()->length(); i < L; ++i) {
      new_rule->block()->push2(m->block()->get(i));
    }

    Block_Ptr wrapper_block = SASS_MEMORY_NEW(ctx.mem, Block, m->block()->pstate());
    wrapper_block->push2(new_rule);
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

    Block_Ptr bb = SASS_MEMORY_NEW(ctx.mem, Block, parent->block()->pstate());
    Ruleset_Ptr new_rule = SASS_MEMORY_NEW(ctx.mem, Ruleset,
                                        parent->pstate(),
                                        parent->selector(),
                                        bb);
    new_rule->tabs(parent->tabs());

    for (size_t i = 0, L = m->block()->length(); i < L; ++i) {
      new_rule->block()->push2(m->block()->get(i));
    }

    Block_Ptr wrapper_block = SASS_MEMORY_NEW(ctx.mem, Block, m->block()->pstate());
    wrapper_block->push2(new_rule);
    Supports_Block_Ptr mm = SASS_MEMORY_NEW(ctx.mem, Supports_Block,
                                       m->pstate(),
                                       m->condition(),
                                       wrapper_block);

    mm->tabs(m->tabs());

    Bubble_Ptr bubble = SASS_MEMORY_NEW(ctx.mem, Bubble, mm->pstate(), mm);
    return bubble;
  }

  Statement_Ptr Cssize::bubble(Media_Block_Ptr m)
  {
    Ruleset_Ptr parent = static_cast<Ruleset_Ptr>(shallow_copy(this->parent()));

    Block_Ptr bb = SASS_MEMORY_NEW(ctx.mem, Block, parent->block()->pstate());
    Ruleset_Ptr new_rule = SASS_MEMORY_NEW(ctx.mem, Ruleset,
                                        parent->pstate(),
                                        parent->selector(),
                                        bb);
    new_rule->tabs(parent->tabs());

    for (size_t i = 0, L = m->block()->length(); i < L; ++i) {
      new_rule->block()->push2(m->block()->get(i));
    }

    Block_Ptr wrapper_block = SASS_MEMORY_NEW(ctx.mem, Block, m->block()->pstate());
    wrapper_block->push2(new_rule);
    Media_Block_Ptr mm = SASS_MEMORY_NEW(ctx.mem, Media_Block,
                                      m->pstate(),
                                      m->media_queries(),
                                      wrapper_block,
                                      0);

    mm->tabs(m->tabs());

    Bubble_Ptr bubble = SASS_MEMORY_NEW(ctx.mem, Bubble, mm->pstate(), mm);

    return bubble;
  }

  bool Cssize::bubblable(Statement_Ptr s)
  {
    return s->statement_type() == Statement::RULESET || s->bubbles();
  }

  Statement_Ptr Cssize::flatten(Statement_Ptr s)
  {
    Block_Ptr bb = s->block();
    Block_Ptr result = SASS_MEMORY_NEW(ctx.mem, Block, bb->pstate(), 0, bb->is_root());
    for (size_t i = 0, L = bb->length(); i < L; ++i) {
      Statement_Ptr ss = bb->get(i);
      if (ss->block()) {
        ss = flatten(ss);
        for (size_t j = 0, K = ss->block()->length(); j < K; ++j) {
          result->push2(ss->block()->get(j));
        }
      }
      else {
        result->push2(ss);
      }
    }
    return result;
  }

  std::vector<std::pair<bool, Block_Ptr>> Cssize::slice_by_bubble(Statement_Ptr b)
  {
    std::vector<std::pair<bool, Block_Ptr>> results;
    for (size_t i = 0, L = b->block()->length(); i < L; ++i) {
      Statement_Ptr value = b->block()->get(i);
      bool key = value->statement_type() == Statement::BUBBLE;

      if (!results.empty() && results.back().first == key)
      {
        Block_Ptr wrapper_block = results.back().second;
        wrapper_block->push2(value);
      }
      else
      {
        Block_Ptr wrapper_block = SASS_MEMORY_NEW(ctx.mem, Block, value->pstate());
        wrapper_block->push2(value);
        results.push_back(std::make_pair(key, wrapper_block));
      }
    }
    return results;
  }

  Statement_Ptr Cssize::shallow_copy(Statement_Ptr s)
  {
    switch (s->statement_type())
    {
      case Statement::RULESET:
        return SASS_MEMORY_NEW(ctx.mem, Ruleset, *static_cast<Ruleset_Ptr>(s));
      case Statement::MEDIA:
        return SASS_MEMORY_NEW(ctx.mem, Media_Block, *static_cast<Media_Block_Ptr>(s));
      case Statement::BUBBLE:
        return SASS_MEMORY_NEW(ctx.mem, Bubble, *static_cast<Bubble_Ptr>(s));
      case Statement::DIRECTIVE:
        return SASS_MEMORY_NEW(ctx.mem, Directive, *static_cast<Directive_Ptr>(s));
      case Statement::SUPPORTS:
        return SASS_MEMORY_NEW(ctx.mem, Supports_Block, *static_cast<Supports_Block_Ptr>(s));
      case Statement::ATROOT:
        return SASS_MEMORY_NEW(ctx.mem, At_Root_Block, *static_cast<At_Root_Block_Ptr>(s));
      case Statement::KEYFRAMERULE:
        return SASS_MEMORY_NEW(ctx.mem, Keyframe_Rule, *static_cast<Keyframe_Rule_Ptr>(s));
      case Statement::NONE:
      default:
        error("unknown internal error; please contact the LibSass maintainers", s->pstate(), backtrace);
        String_Quoted_Ptr msg = SASS_MEMORY_NEW(ctx.mem, String_Quoted, ParserState("[WARN]"), std::string("`CSSize` can't clone ") + typeid(*s).name());
        return SASS_MEMORY_NEW(ctx.mem, Warning, ParserState("[WARN]"), msg);
    }
  }

  Statement_Ptr Cssize::debubble(Block_Ptr children, Statement_Ptr parent)
  {
    Has_Block_Ptr previous_parent = 0;
    std::vector<std::pair<bool, Block_Ptr>> baz = slice_by_bubble(children);
    Block_Ptr result = SASS_MEMORY_NEW(ctx.mem, Block, children->pstate());

    for (size_t i = 0, L = baz.size(); i < L; ++i) {
      bool is_bubble = baz[i].first;
      Block_Ptr slice = baz[i].second;

      if (!is_bubble) {
        if (!parent) {
          result->push2(slice);
        }
        else if (previous_parent) {
          previous_parent->block()->concat(slice);
        }
        else {
          previous_parent = static_cast<Has_Block_Ptr>(shallow_copy(parent));
          previous_parent->block(slice);
          previous_parent->tabs(parent->tabs());

          Has_Block_Ptr new_parent = previous_parent;

          result->push2(new_parent);
        }
        continue;
      }

      for (size_t j = 0, K = slice->length(); j < K; ++j)
      {
        Statement_Ptr ss = 0;
        Bubble_Ptr node = static_cast<Bubble_Ptr>(slice->get(j));

        if (!parent ||
            parent->statement_type() != Statement::MEDIA ||
            node->node()->statement_type() != Statement::MEDIA ||
            static_cast<Media_Block_Ptr>(node->node())->media_queries() == static_cast<Media_Block_Ptr>(parent)->media_queries())
        {
          ss = node->node();
        }
        else
        {
          List_Ptr mq = merge_media_queries(static_cast<Media_Block_Ptr>(node->node()), static_cast<Media_Block_Ptr>(parent));
          if (!mq->length()) continue;
          static_cast<Media_Block_Ptr>(node->node())->media_queries(mq);
          ss = node->node();
        }

        if (!ss) continue;

        ss->tabs(ss->tabs() + node->tabs());
        ss->group_end(node->group_end());

        if (!ss) continue;

        Block_Ptr bb = SASS_MEMORY_NEW(ctx.mem, Block,
                                    children->block()->pstate(),
                                    children->block()->length(),
                                    children->block()->is_root());
        bb->push2(ss->perform(this));

        Block_Ptr wrapper_block = SASS_MEMORY_NEW(ctx.mem, Block,
                                              children->block()->pstate(),
                                              children->block()->length(),
                                              children->block()->is_root());

        Statement_Ptr wrapper = flatten(bb);
        wrapper_block->push2(wrapper);

        if (wrapper->block()->length()) {
          previous_parent = 0;
        }

        if (wrapper_block) {
          result->push2(wrapper_block);
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
      Statement_Ptr ith = b->get(i)->perform(this);
      if (ith && ith->block()) {
        for (size_t j = 0, K = ith->block()->length(); j < K; ++j) {
          current_block->push2(ith->block()->get(j));
        }
      }
      else if (ith) {
        current_block->push2(ith);
      }
    }
  }

  List_Ptr Cssize::merge_media_queries(Media_Block_Ptr m1, Media_Block_Ptr m2)
  {
    List_Ptr qq = SASS_MEMORY_NEW(ctx.mem, List,
                               m1->media_queries()->pstate(),
                               m1->media_queries()->length(),
                               SASS_COMMA);

    for (size_t i = 0, L = m1->media_queries()->length(); i < L; i++) {
      for (size_t j = 0, K = m2->media_queries()->length(); j < K; j++) {
        Media_Query_Ptr mq1 = static_cast<Media_Query_Ptr>(m1->media_queries()->get(i));
        Media_Query_Ptr mq2 = static_cast<Media_Query_Ptr>(m2->media_queries()->get(j));
        Media_Query_Ptr mq = merge_media_query(mq1, mq2);

        if (mq) qq->push2(mq);
      }
    }

    return qq;
  }


  Media_Query_Ptr Cssize::merge_media_query(Media_Query_Ptr mq1, Media_Query_Ptr mq2)
  {

    std::string type;
    std::string mod;

    std::string m1 = std::string(mq1->is_restricted() ? "only" : mq1->is_negated() ? "not" : "");
    std::string t1 = mq1->media_type() ? mq1->media_type()->to_string(ctx.c_options) : "";
    std::string m2 = std::string(mq2->is_restricted() ? "only" : mq1->is_negated() ? "not" : "");
    std::string t2 = mq2->media_type() ? mq2->media_type()->to_string(ctx.c_options) : "";


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
      mq1->length() + mq2->length(),
      mod == "not",
      mod == "only"
    );

    if (!type.empty()) {
      mm->media_type(SASS_MEMORY_NEW(ctx.mem, String_Quoted, mq1->pstate(), type));
    }

    mm->concat(mq2);
    mm->concat(mq1);
    return mm;
  }
}
