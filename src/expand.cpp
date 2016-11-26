#include "sass.hpp"
#include <iostream>
#include <typeinfo>

#include "expand.hpp"
#include "bind.hpp"
#include "eval.hpp"
#include "backtrace.hpp"
#include "context.hpp"
#include "parser.hpp"

namespace Sass {

  // simple endless recursion protection
  const unsigned int maxRecursion = 500;
  static unsigned int recursions = 0;

  Expand::Expand(Context& ctx, Env* env, Backtrace* bt, std::vector<Selector_List_Ptr>* stack)
  : ctx(ctx),
    eval(Eval(*this)),
    env_stack(std::vector<Env*>()),
    block_stack(std::vector<Block_Ptr>()),
    call_stack(std::vector<AST_Node_Ptr>()),
    property_stack(std::vector<String_Ptr>()),
    selector_stack(std::vector<Selector_List_Ptr>()),
    media_block_stack(std::vector<Media_Block_Ptr>()),
    backtrace_stack(std::vector<Backtrace*>()),
    in_keyframes(false),
    at_root_without_rule(false),
    old_at_root_without_rule(false)
  {
    env_stack.push_back(0);
    env_stack.push_back(env);
    block_stack.push_back(0);
    call_stack.push_back(0);
    // import_stack.push_back(0);
    property_stack.push_back(0);
    if (stack == NULL) { selector_stack.push_back(0); }
    else { selector_stack.insert(selector_stack.end(), stack->begin(), stack->end()); }
    media_block_stack.push_back(0);
    backtrace_stack.push_back(0);
    backtrace_stack.push_back(bt);
  }

  Context& Expand::context()
  {
    return ctx;
  }

  Env* Expand::environment()
  {
    if (env_stack.size() > 0)
      return env_stack.back();
    return 0;
  }

  Selector_List_Ptr Expand::selector()
  {
    if (selector_stack.size() > 0)
      return selector_stack.back();
    return 0;
  }

  Backtrace* Expand::backtrace()
  {
    if (backtrace_stack.size() > 0)
      return backtrace_stack.back();
    return 0;
  }

  // blocks create new variable scopes
  Statement_Ptr Expand::operator()(Block_Ptr b)
  {
    // create new local environment
    // set the current env as parent
    Env env(environment());
    // copy the block object (add items later)
    Block_Ptr bb = SASS_MEMORY_NEW(ctx.mem, Block,
                                b->pstate(),
                                b->length(),
                                b->is_root());
    // setup block and env stack
    this->block_stack.push_back(bb);
    this->env_stack.push_back(&env);
    // operate on block
    this->append_block(b);
    // revert block and env stack
    this->block_stack.pop_back();
    this->env_stack.pop_back();
    // return copy
    return bb;
  }

  Statement_Ptr Expand::operator()(Ruleset_Ptr r)
  {
    LOCAL_FLAG(old_at_root_without_rule, at_root_without_rule);

    if (in_keyframes) {
      Keyframe_Rule_Ptr k = SASS_MEMORY_NEW(ctx.mem, Keyframe_Rule, r->pstate(), r->block()->perform(this)->block());
      if (r->selector()) {
        selector_stack.push_back(0);
        k->selector(static_cast<Selector_List_Ptr>(r->selector()->perform(&eval)));
        selector_stack.pop_back();
      }
      return k;
    }

    // reset when leaving scope
    LOCAL_FLAG(at_root_without_rule, false);

    // do some special checks for the base level rules
    if (r->is_root()) {
      if (Selector_List_Ptr selector_list = dynamic_cast<Selector_List_Ptr>(r->selector())) {
        for (Complex_Selector_Ptr complex_selector : selector_list->elements()) {
          Complex_Selector_Ptr tail = complex_selector;
          while (tail) {
            if (tail->head()) for (Simple_Selector* header : tail->head()->elements()) {
              if (dynamic_cast<Parent_Selector_Ptr>(header) == NULL) continue; // skip all others
              std::string sel_str(complex_selector->to_string(ctx.c_options));
              error("Base-level rules cannot contain the parent-selector-referencing character '&'.", header->pstate(), backtrace());
            }
            tail = tail->tail();
          }
        }
      }
    }

    Expression_Ptr ex = r->selector()->perform(&eval);
    Selector_List_Ptr sel = dynamic_cast<Selector_List_Ptr>(ex);
    if (sel == 0) throw std::runtime_error("Expanded null selector");

    if (sel->length() == 0 || sel->has_parent_ref()) {
      bool has_parent_selector = false;
      for (size_t i = 0, L = selector_stack.size(); i < L && !has_parent_selector; i++) {
        Selector_List_Ptr ll = selector_stack.at(i);
        has_parent_selector = ll != 0 && ll->length() > 0;
      }
      if (!has_parent_selector) {
        error("Base-level rules cannot contain the parent-selector-referencing character '&'.", sel->pstate(), backtrace());
      }
    }

    selector_stack.push_back(sel);
    Env env(environment());
    if (block_stack.back()->is_root()) {
      env_stack.push_back(&env);
    }
    sel->set_media_block(media_block_stack.back());
    Block_Ptr blk = r->block()->perform(this)->block();
    Ruleset_Ptr rr = SASS_MEMORY_NEW(ctx.mem, Ruleset,
                                  r->pstate(),
                                  sel,
                                  blk);
    selector_stack.pop_back();
    if (block_stack.back()->is_root()) {
      env_stack.pop_back();
    }

    rr->is_root(r->is_root());
    rr->tabs(r->tabs());

    return rr;
  }

  Statement_Ptr Expand::operator()(Supports_Block_Ptr f)
  {
    Expression_Ptr condition = f->condition()->perform(&eval);
    Supports_Block_Ptr ff = SASS_MEMORY_NEW(ctx.mem, Supports_Block,
                                       f->pstate(),
                                       static_cast<Supports_Condition_Ptr>(condition),
                                       f->block()->perform(this)->block());
    return ff;
  }

  Statement_Ptr Expand::operator()(Media_Block_Ptr m)
  {
    media_block_stack.push_back(m);
    Expression_Ptr mq = m->media_queries()->perform(&eval);
    std::string str_mq(mq->to_string(ctx.c_options));
    char* str = sass_copy_c_string(str_mq.c_str());
    ctx.strings.push_back(str);
    Parser p(Parser::from_c_str(str, ctx, mq->pstate()));
    mq = p.parse_media_queries();
    Media_Block_Ptr mm = SASS_MEMORY_NEW(ctx.mem, Media_Block,
                                      m->pstate(),
                                      static_cast<List_Ptr>(mq->perform(&eval)),
                                      m->block()->perform(this)->block(),
                                      0);
    media_block_stack.pop_back();
    mm->tabs(m->tabs());
    return mm;
  }

  Statement_Ptr Expand::operator()(At_Root_Block_Ptr a)
  {
    Block_Ptr ab = a->block();
    Expression_Ptr ae = a->expression();

    if (ae) ae = ae->perform(&eval);
    else ae = SASS_MEMORY_NEW(ctx.mem, At_Root_Query, a->pstate());

    LOCAL_FLAG(at_root_without_rule, true);
    LOCAL_FLAG(in_keyframes, false);

    Block_Ptr bb = ab ? ab->perform(this)->block() : 0;
    At_Root_Block_Ptr aa = SASS_MEMORY_NEW(ctx.mem, At_Root_Block,
                                        a->pstate(),
                                        bb,
                                        static_cast<At_Root_Query_Ptr>(ae));
    return aa;
  }

  Statement_Ptr Expand::operator()(Directive_Ptr a)
  {
    LOCAL_FLAG(in_keyframes, a->is_keyframes());
    Block_Ptr ab = a->block();
    Selector_Ptr as = a->selector();
    Expression_Ptr av = a->value();
    selector_stack.push_back(0);
    if (av) av = av->perform(&eval);
    if (as) as = dynamic_cast<Selector_Ptr>(as->perform(&eval));
    selector_stack.pop_back();
    Block_Ptr bb = ab ? ab->perform(this)->block() : 0;
    Directive_Ptr aa = SASS_MEMORY_NEW(ctx.mem, Directive,
                                  a->pstate(),
                                  a->keyword(),
                                  as,
                                  bb,
                                  av);
    return aa;
  }

  Statement_Ptr Expand::operator()(Declaration_Ptr d)
  {
    Block_Ptr ab = d->block();
    String_Ptr old_p = d->property();
    String_Ptr new_p = static_cast<String_Ptr>(old_p->perform(&eval));
    Expression_Ptr value = d->value()->perform(&eval);
    Block_Ptr bb = ab ? ab->perform(this)->block() : 0;
    if (!bb) {
      if (!value || (value->is_invisible() && !d->is_important())) return 0;
    }
    Declaration_Ptr decl = SASS_MEMORY_NEW(ctx.mem, Declaration,
                                        d->pstate(),
                                        new_p,
                                        value,
                                        d->is_important(),
                                        bb);
    decl->tabs(d->tabs());
    return decl;
  }

  Statement_Ptr Expand::operator()(Assignment_Ptr a)
  {
    Env* env = environment();
    std::string var(a->variable());
    if (a->is_global()) {
      if (a->is_default()) {
        if (env->has_global(var)) {
          Expression_Ptr e = dynamic_cast<Expression_Ptr>(env->get_global(var));
          if (!e || e->concrete_type() == Expression::NULL_VAL) {
            env->set_global(var, a->value()->perform(&eval));
          }
        }
        else {
          env->set_global(var, a->value()->perform(&eval));
        }
      }
      else {
        env->set_global(var, a->value()->perform(&eval));
      }
    }
    else if (a->is_default()) {
      if (env->has_lexical(var)) {
        auto cur = env;
        while (cur && cur->is_lexical()) {
          if (cur->has_local(var)) {
            if (AST_Node_Ptr node = cur->get_local(var)) {
              Expression_Ptr e = dynamic_cast<Expression_Ptr>(node);
              if (!e || e->concrete_type() == Expression::NULL_VAL) {
                cur->set_local(var, a->value()->perform(&eval));
              }
            }
            else {
              throw std::runtime_error("Env not in sync");
            }
            return 0;
          }
          cur = cur->parent();
        }
        throw std::runtime_error("Env not in sync");
      }
      else if (env->has_global(var)) {
        if (AST_Node_Ptr node = env->get_global(var)) {
          Expression_Ptr e = dynamic_cast<Expression_Ptr>(node);
          if (!e || e->concrete_type() == Expression::NULL_VAL) {
            env->set_global(var, a->value()->perform(&eval));
          }
        }
      }
      else if (env->is_lexical()) {
        env->set_local(var, a->value()->perform(&eval));
      }
      else {
        env->set_local(var, a->value()->perform(&eval));
      }
    }
    else {
      env->set_lexical(var, a->value()->perform(&eval));
    }
    return 0;
  }

  Statement_Ptr Expand::operator()(Import_Ptr imp)
  {
    Import_Ptr result = SASS_MEMORY_NEW(ctx.mem, Import, imp->pstate());
    if (imp->media_queries() && imp->media_queries()->size()) {
      Expression_Ptr ex = imp->media_queries()->perform(&eval);
      result->media_queries(dynamic_cast<List_Ptr>(ex));
    }
    for ( size_t i = 0, S = imp->urls().size(); i < S; ++i) {
      result->urls().push_back(imp->urls()[i]->perform(&eval));
    }
    // all resources have been dropped for Input_Stubs
    // for ( size_t i = 0, S = imp->incs().size(); i < S; ++i) {}
    return result;
  }

  Statement_Ptr Expand::operator()(Import_Stub_Ptr i)
  {
    // get parent node from call stack
    AST_Node_Ptr parent = call_stack.back();
    if (parent && dynamic_cast<Block_Ptr>(parent) == NULL) {
      error("Import directives may not be used within control directives or mixins.", i->pstate());
    }
    // we don't seem to need that actually afterall
    Sass_Import_Entry import = sass_make_import(
      i->imp_path().c_str(),
      i->abs_path().c_str(),
      0, 0
    );
    ctx.import_stack.push_back(import);
    const std::string& abs_path(i->resource().abs_path);
    append_block(ctx.sheets.at(abs_path).root);
    sass_delete_import(ctx.import_stack.back());
    ctx.import_stack.pop_back();
    return 0;
  }

  Statement_Ptr Expand::operator()(Warning_Ptr w)
  {
    // eval handles this too, because warnings may occur in functions
    w->perform(&eval);
    return 0;
  }

  Statement_Ptr Expand::operator()(Error_Ptr e)
  {
    // eval handles this too, because errors may occur in functions
    e->perform(&eval);
    return 0;
  }

  Statement_Ptr Expand::operator()(Debug_Ptr d)
  {
    // eval handles this too, because warnings may occur in functions
    d->perform(&eval);
    return 0;
  }

  Statement_Ptr Expand::operator()(Comment_Ptr c)
  {
    eval.is_in_comment = true;
    auto rv = SASS_MEMORY_NEW(ctx.mem, Comment, c->pstate(), static_cast<String_Ptr>(c->text()->perform(&eval)), c->is_important());
    eval.is_in_comment = false;
    // TODO: eval the text, once we're parsing/storing it as a String_Schema
    return rv;
  }

  Statement_Ptr Expand::operator()(If_Ptr i)
  {
    Env env(environment(), true);
    env_stack.push_back(&env);
    call_stack.push_back(i);
    if (*i->predicate()->perform(&eval)) {
      append_block(i->block());
    }
    else {
      Block_Ptr alt = i->alternative();
      if (alt) append_block(alt);
    }
    call_stack.pop_back();
    env_stack.pop_back();
    return 0;
  }

  // For does not create a new env scope
  // But iteration vars are reset afterwards
  Statement_Ptr Expand::operator()(For_Ptr f)
  {
    std::string variable(f->variable());
    Expression_Ptr low = f->lower_bound()->perform(&eval);
    if (low->concrete_type() != Expression::NUMBER) {
      throw Exception::TypeMismatch(*low, "integer");
    }
    Expression_Ptr high = f->upper_bound()->perform(&eval);
    if (high->concrete_type() != Expression::NUMBER) {
      throw Exception::TypeMismatch(*high, "integer");
    }
    Number_Ptr sass_start = static_cast<Number_Ptr>(low);
    Number_Ptr sass_end = static_cast<Number_Ptr>(high);
    // check if units are valid for sequence
    if (sass_start->unit() != sass_end->unit()) {
      std::stringstream msg; msg << "Incompatible units: '"
        << sass_start->unit() << "' and '"
        << sass_end->unit() << "'.";
      error(msg.str(), low->pstate(), backtrace());
    }
    double start = sass_start->value();
    double end = sass_end->value();
    // only create iterator once in this environment
    Env env(environment(), true);
    env_stack.push_back(&env);
    call_stack.push_back(f);
    Number_Ptr it = SASS_MEMORY_NEW(env.mem, Number, low->pstate(), start, sass_end->unit());
    env.set_local(variable, it);
    Block_Ptr body = f->block();
    if (start < end) {
      if (f->is_inclusive()) ++end;
      for (double i = start;
           i < end;
           ++i) {
        it->value(i);
        env.set_local(variable, it);
        append_block(body);
      }
    } else {
      if (f->is_inclusive()) --end;
      for (double i = start;
           i > end;
           --i) {
        it->value(i);
        env.set_local(variable, it);
        append_block(body);
      }
    }
    call_stack.pop_back();
    env_stack.pop_back();
    return 0;
  }

  // Eval does not create a new env scope
  // But iteration vars are reset afterwards
  Statement_Ptr Expand::operator()(Each_Ptr e)
  {
    std::vector<std::string> variables(e->variables());
    Expression_Ptr expr = e->list()->perform(&eval);
    Vectorized<Expression_Ptr>* list = 0;
    Map_Ptr map = 0;
    if (expr->concrete_type() == Expression::MAP) {
      map = static_cast<Map_Ptr>(expr);
    }
    else if (Selector_List_Ptr ls = dynamic_cast<Selector_List_Ptr>(expr)) {
      Listize listize(ctx.mem);
      list = dynamic_cast<List_Ptr>(ls->perform(&listize));
    }
    else if (expr->concrete_type() != Expression::LIST) {
      list = SASS_MEMORY_NEW(ctx.mem, List, expr->pstate(), 1, SASS_COMMA);
      *list << expr;
    }
    else {
      list = static_cast<List_Ptr>(expr);
    }
    // remember variables and then reset them
    Env env(environment(), true);
    env_stack.push_back(&env);
    call_stack.push_back(e);
    Block_Ptr body = e->block();

    if (map) {
      for (auto key : map->keys()) {
        Expression_Ptr k = key->perform(&eval);
        Expression_Ptr v = map->at(key)->perform(&eval);

        if (variables.size() == 1) {
          List_Ptr variable = SASS_MEMORY_NEW(ctx.mem, List, map->pstate(), 2, SASS_SPACE);
          *variable << k;
          *variable << v;
          env.set_local(variables[0], variable);
        } else {
          env.set_local(variables[0], k);
          env.set_local(variables[1], v);
        }
        append_block(body);
      }
    }
    else {
      // bool arglist = list->is_arglist();
      if (list->length() == 1 && dynamic_cast<Selector_List_Ptr>(list)) {
        list = dynamic_cast<Vectorized<Expression_Ptr>*>(list);
      }
      for (size_t i = 0, L = list->length(); i < L; ++i) {
        Expression_Ptr e = list->get(i);
        // unwrap value if the expression is an argument
        if (Argument_Ptr arg = dynamic_cast<Argument_Ptr>(e)) e = arg->value();
        // check if we got passed a list of args (investigate)
        if (List_Ptr scalars = dynamic_cast<List_Ptr>(e)) {
          if (variables.size() == 1) {
            Expression_Ptr var = scalars;
            // if (arglist) var = (*scalars)[0];
            env.set_local(variables[0], var);
          } else {
            for (size_t j = 0, K = variables.size(); j < K; ++j) {
              Expression_Ptr res = j >= scalars->length()
                ? SASS_MEMORY_NEW(ctx.mem, Null, expr->pstate())
                : scalars->get(j)->perform(&eval);
              env.set_local(variables[j], res);
            }
          }
        } else {
          if (variables.size() > 0) {
            env.set_local(variables[0], e);
            for (size_t j = 1, K = variables.size(); j < K; ++j) {
              Expression_Ptr res = SASS_MEMORY_NEW(ctx.mem, Null, expr->pstate());
              env.set_local(variables[j], res);
            }
          }
        }
        append_block(body);
      }
    }
    call_stack.pop_back();
    env_stack.pop_back();
    return 0;
  }

  Statement_Ptr Expand::operator()(While_Ptr w)
  {
    Expression_Ptr pred = w->predicate();
    Block_Ptr body = w->block();
    Env env(environment(), true);
    env_stack.push_back(&env);
    call_stack.push_back(w);
    while (*pred->perform(&eval)) {
      append_block(body);
    }
    call_stack.pop_back();
    env_stack.pop_back();
    return 0;
  }

  Statement_Ptr Expand::operator()(Return_Ptr r)
  {
    error("@return may only be used within a function", r->pstate(), backtrace());
    return 0;
  }


  void Expand::expand_selector_list(Selector_Ptr s, Selector_List_Ptr extender) {

    if (Selector_List_Ptr sl = dynamic_cast<Selector_List_Ptr>(s)) {
      for (Complex_Selector_Ptr complex_selector : sl->elements()) {
        Complex_Selector_Ptr tail = complex_selector;
        while (tail) {
          if (tail->head()) for (Simple_Selector* header : tail->head()->elements()) {
            if (dynamic_cast<Parent_Selector_Ptr>(header) == NULL) continue; // skip all others
            std::string sel_str(complex_selector->to_string(ctx.c_options));
            error("Can't extend " + sel_str + ": can't extend parent selectors", header->pstate(), backtrace());
          }
          tail = tail->tail();
        }
      }
    }


    Selector_List_Ptr contextualized = dynamic_cast<Selector_List_Ptr>(s->perform(&eval));
    if (contextualized == NULL) return;
    for (auto complex_sel : contextualized->elements()) {
      Complex_Selector_Ptr c = complex_sel;
      if (!c->head() || c->tail()) {
        std::string sel_str(contextualized->to_string(ctx.c_options));
        error("Can't extend " + sel_str + ": can't extend nested selectors", c->pstate(), backtrace());
      }
      Compound_Selector_Ptr placeholder = c->head();
      if (contextualized->is_optional()) placeholder->is_optional(true);
      for (size_t i = 0, L = extender->length(); i < L; ++i) {
        Complex_Selector_Ptr sel = extender->get(i);
        if (!(sel->head() && sel->head()->length() > 0 &&
            dynamic_cast<Parent_Selector_Ptr>(sel->head()->get(0))))
        {
          Compound_Selector_Ptr hh = SASS_MEMORY_NEW(ctx.mem, Compound_Selector, extender->get(i)->pstate());
          hh->media_block(extender->get(i)->media_block());
          Complex_Selector_Ptr ssel = SASS_MEMORY_NEW(ctx.mem, Complex_Selector, extender->get(i)->pstate());
          ssel->media_block(extender->get(i)->media_block());
          if (sel->has_line_feed()) ssel->has_line_feed(true);
          Parent_Selector_Ptr ps = SASS_MEMORY_NEW(ctx.mem, Parent_Selector, extender->get(i)->pstate());
          ps->media_block(extender->get(i)->media_block());
          *hh << ps;
          ssel->tail(sel);
          ssel->head(hh);
          sel = ssel;
        }
        // if (c->has_line_feed()) sel->has_line_feed(true);
        ctx.subset_map.put(placeholder->to_str_vec(), std::make_pair(sel, placeholder));
      }
    }

  }

  Statement_Ptr Expand::operator()(Extension_Ptr e)
  {
    if (Selector_List_Ptr extender = dynamic_cast<Selector_List_Ptr>(selector())) {
      Selector_Ptr s = e->selector();
      if (Selector_Schema_Ptr schema = dynamic_cast<Selector_Schema_Ptr>(s)) {
        if (schema->has_parent_ref()) s = eval(schema);
      }
      if (Selector_List_Ptr sl = dynamic_cast<Selector_List_Ptr>(s)) {
        for (Complex_Selector_Ptr cs : *sl) {
          if (cs != NULL && cs->head() != NULL) {
            cs->head()->media_block(media_block_stack.back());
          }
        }
      }
      selector_stack.push_back(0);
      expand_selector_list(s, extender);
      selector_stack.pop_back();
    }
    return 0;
  }

  Statement_Ptr Expand::operator()(Definition_Ptr d)
  {
    Env* env = environment();
    Definition_Ptr dd = SASS_MEMORY_NEW(ctx.mem, Definition, *d);
    env->local_frame()[d->name() +
                        (d->type() == Definition::MIXIN ? "[m]" : "[f]")] = dd;

    if (d->type() == Definition::FUNCTION && (
      Prelexer::calc_fn_call(d->name().c_str()) ||
      d->name() == "element"    ||
      d->name() == "expression" ||
      d->name() == "url"
    )) {
      deprecated(
        "Naming a function \"" + d->name() + "\" is disallowed",
        "This name conflicts with an existing CSS function with special parse rules.",
         d->pstate()
      );
    }


    // set the static link so we can have lexical scoping
    dd->environment(env);
    return 0;
  }

  Statement_Ptr Expand::operator()(Mixin_Call_Ptr c)
  {
    recursions ++;

    if (recursions > maxRecursion) {
      throw Exception::StackError(*c);
    }

    Env* env = environment();
    std::string full_name(c->name() + "[m]");
    if (!env->has(full_name)) {
      error("no mixin named " + c->name(), c->pstate(), backtrace());
    }
    Definition_Ptr def = static_cast<Definition_Ptr>((*env)[full_name]);
    Block_Ptr body = def->block();
    Parameters_Ptr params = def->parameters();

    if (c->block() && c->name() != "@content" && !body->has_content()) {
      error("Mixin \"" + c->name() + "\" does not accept a content block.", c->pstate(), backtrace());
    }
    Arguments_Ptr args = static_cast<Arguments_Ptr>(c->arguments()
                                               ->perform(&eval));
    Backtrace new_bt(backtrace(), c->pstate(), ", in mixin `" + c->name() + "`");
    backtrace_stack.push_back(&new_bt);
    Env new_env(def->environment());
    env_stack.push_back(&new_env);
    if (c->block()) {
      // represent mixin content blocks as thunks/closures
      Definition_Ptr thunk = SASS_MEMORY_NEW(ctx.mem, Definition,
                                          c->pstate(),
                                          "@content",
                                          SASS_MEMORY_NEW(ctx.mem, Parameters, c->pstate()),
                                          c->block(),
                                          Definition::MIXIN);
      thunk->environment(env);
      new_env.local_frame()["@content[m]"] = thunk;
    }

    bind(std::string("Mixin"), c->name(), params, args, &ctx, &new_env, &eval);

    Block_Ptr trace_block = SASS_MEMORY_NEW(ctx.mem, Block, c->pstate());
    Trace_Ptr trace = SASS_MEMORY_NEW(ctx.mem, Trace, c->pstate(), c->name(), trace_block);


    block_stack.push_back(trace_block);
    for (auto bb : *body) {
      Statement_Ptr ith = bb->perform(this);
      if (ith) *trace->block() << ith;
    }
    block_stack.pop_back();

    env_stack.pop_back();
    backtrace_stack.pop_back();

    recursions --;
    return trace;
  }

  Statement_Ptr Expand::operator()(Content_Ptr c)
  {
    Env* env = environment();
    // convert @content directives into mixin calls to the underlying thunk
    if (!env->has("@content[m]")) return 0;

    if (block_stack.back()->is_root()) {
      selector_stack.push_back(0);
    }

    Mixin_Call_Ptr call = SASS_MEMORY_NEW(ctx.mem, Mixin_Call,
                                       c->pstate(),
                                       "@content",
                                       SASS_MEMORY_NEW(ctx.mem, Arguments, c->pstate()));

    Trace_Ptr trace = dynamic_cast<Trace_Ptr>(call->perform(this));

    if (block_stack.back()->is_root()) {
      selector_stack.pop_back();
    }

    return trace;
  }

  // produce an error if something is not implemented
  inline Statement_Ptr Expand::fallback_impl(AST_Node_Ptr n)
  {
    std::string err =std:: string("`Expand` doesn't handle ") + typeid(*n).name();
    String_Quoted_Ptr msg = SASS_MEMORY_NEW(ctx.mem, String_Quoted, ParserState("[WARN]"), err);
    error("unknown internal error; please contact the LibSass maintainers", n->pstate(), backtrace());
    return SASS_MEMORY_NEW(ctx.mem, Warning, ParserState("[WARN]"), msg);
  }

  // process and add to last block on stack
  inline void Expand::append_block(Block_Ptr b)
  {
    if (b->is_root()) call_stack.push_back(b);
    for (size_t i = 0, L = b->length(); i < L; ++i) {
      Statement_Ptr ith = b->get(i)->perform(this);
      if (ith) *block_stack.back() << ith;
    }
    if (b->is_root()) call_stack.pop_back();
  }

}
