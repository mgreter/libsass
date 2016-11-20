#include "sass.hpp"
#include <iostream>
#include <typeinfo>

#include "ast.hpp"
#include "expand.hpp"
#include "debugger.hpp"
#include "bind.hpp"
#include "eval.hpp"
#include "backtrace.hpp"
#include "context.hpp"
#include "parser.hpp"

namespace Sass {

  // simple endless recursion protection
  const unsigned int maxRecursion = 500;
  static unsigned int recursions = 0;

  Expand::Expand(Context& ctx, Env* env, Backtrace* bt, std::vector<Selector_List_Obj>* stack)
  : ctx(ctx),
    eval(Eval(*this)),
    env_stack(std::vector<Env*>()),
    block_stack(std::vector<Block_Ptr>()),
    call_stack(std::vector<AST_Node_Obj>()),
    // property_stack(std::vector<String_Ptr>()),
    selector_stack(std::vector<Selector_List_Obj>()),
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
    // property_stack.push_back(0);
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

  Selector_List_Obj Expand::selector()
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
  Block_Ptr Expand::operator()(Block_Ptr b)
  {
    // create new local environment
    // set the current env as parent
    Env env(environment());
    // copy the block object (add items later)
    Block_Obj bb = SASS_MEMORY_NEW(ctx.mem, Block,
                                b->pstate(),
                                b->length(),
                                b->is_root());
    // setup block and env stack
    this->block_stack.push_back(&bb);
    this->env_stack.push_back(&env);
    // operate on block
    // this may throw up!
    this->append_block(b);
    // revert block and env stack
    this->block_stack.pop_back();
    this->env_stack.pop_back();
    // return copy
    return bb ? bb->copy2(ctx.mem, __FILE__, __LINE__) : 0;
  }

  Statement_Ptr Expand::operator()(Ruleset_Ptr r)
  {
    LOCAL_FLAG(old_at_root_without_rule, at_root_without_rule);

    if (in_keyframes) {
      Block_Ptr bb = operator()(&r->oblock());
      Keyframe_Rule_Obj k = SASS_MEMORY_NEW(ctx.mem, Keyframe_Rule, r->pstate(), bb);
      if (r->selector()) {
        selector_stack.push_back(0);
        k->selector2(SASS_MEMORY_CAST_PTR(Selector_List, r->selector()->perform(&eval)));
        selector_stack.pop_back();
      }
      return k->copy2(ctx.mem, __FILE__, __LINE__);
    }

    // reset when leaving scope
    LOCAL_FLAG(at_root_without_rule, false);

    // do some special checks for the base level rules
    if (r->is_root()) {
      if (Selector_List_Ptr selector_list = SASS_MEMORY_CAST(Selector_List, r->selector())) {
        for (Complex_Selector_Obj complex_selector : selector_list->elements()) {
          Complex_Selector_Ptr tail = &complex_selector;
          while (tail) {
            if (tail->head()) for (Simple_Selector_Obj header : tail->head()->elements()) {
              if (SASS_MEMORY_CAST(Parent_Selector, header) == NULL) continue; // skip all others
              std::string sel_str(complex_selector->to_string(ctx.c_options));
              error("Base-level rules cannot contain the parent-selector-referencing character '&'.", header->pstate(), backtrace());
            }
            tail = &tail->tail();
          }
        }
      }
    }

    Expression_Obj ex = 0;
    if (r->selector()) ex = r->selector()->perform(&eval);
    Selector_List_Obj sel = SASS_MEMORY_CAST(Selector_List, ex);
    if (sel == 0) throw std::runtime_error("Expanded null selector");

    if (sel->length() == 0 || sel->has_parent_ref()) {
      bool has_parent_selector = false;
      for (size_t i = 0, L = selector_stack.size(); i < L && !has_parent_selector; i++) {
        Selector_List_Obj ll = selector_stack.at(i);
        has_parent_selector = ll != 0 && ll->length() > 0;
      }
      if (!has_parent_selector) {
        error("Base-level rules cannot contain the parent-selector-referencing character '&'.", sel->pstate(), backtrace());
      }
    }

    selector_stack.push_back(&sel);
    Env env(environment());
    if (block_stack.back()->is_root()) {
      env_stack.push_back(&env);
    }
    sel->set_media_block(media_block_stack.back());
    Block_Obj blk = 0;
    if (r->oblock()) blk = operator()(&r->oblock());
    Ruleset_Ptr rr = SASS_MEMORY_NEW(ctx.mem, Ruleset,
                                  r->pstate(),
                                  &sel,
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
    Expression_Obj condition = f->condition()->perform(&eval);
    Supports_Block_Obj ff = SASS_MEMORY_NEW(ctx.mem, Supports_Block,
                                       f->pstate(),
                                       SASS_MEMORY_CAST(Supports_Condition, condition),
                                       operator()(&f->oblock()));
    return ff->copy2(ctx.mem, __FILE__, __LINE__);
  }

  Statement_Ptr Expand::operator()(Media_Block_Ptr m)
  {
    media_block_stack.push_back(m);
    Expression_Obj mq = m->media_queries()->perform(&eval);
    std::string str_mq(mq->to_string(ctx.c_options));
    char* str = sass_copy_c_string(str_mq.c_str());
    ctx.strings.push_back(str);
    Parser p(Parser::from_c_str(str, ctx, mq->pstate()));
    mq = &p.parse_media_queries(); // re-assign now
    List_Obj ls = SASS_MEMORY_CAST_PTR(List, mq->perform(&eval));
    Block_Obj blk = operator()(&m->oblock());
    Media_Block_Ptr mm = SASS_MEMORY_NEW(ctx.mem, Media_Block,
                                      m->pstate(),
                                      ls,
                                      blk,
                                      0);
    media_block_stack.pop_back();
    mm->tabs(m->tabs());
    return mm;
  }

  Statement_Ptr Expand::operator()(At_Root_Block_Ptr a)
  {
    Block_Obj ab = a->oblock();
    Expression_Obj ae = &a->expression();

    if (ae) ae = ae->perform(&eval);
    else ae = SASS_MEMORY_NEW(ctx.mem, At_Root_Query, a->pstate());

    LOCAL_FLAG(at_root_without_rule, true);
    LOCAL_FLAG(in_keyframes, false);

                                       ;

    Block_Obj bb = ab ? operator()(&ab) : NULL;
    At_Root_Block_Obj aa = SASS_MEMORY_NEW(ctx.mem, At_Root_Block,
                                        a->pstate(),
                                        bb,
                                        SASS_MEMORY_CAST(At_Root_Query, ae));
    return aa ? aa->copy2(ctx.mem, __FILE__, __LINE__) : 0;
  }

  Statement_Ptr Expand::operator()(Directive_Ptr a)
  {
    LOCAL_FLAG(in_keyframes, a->is_keyframes());
    Block_Ptr ab = &a->oblock();
    Selector_Ptr as = &a->selector();
    Expression_Ptr av = &a->value();
    selector_stack.push_back(0);
    if (av) av = av->perform(&eval);
    if (as) as = SASS_MEMORY_CAST_PTR(Selector, as->perform(&eval));
    selector_stack.pop_back();
    Block_Ptr bb = ab ? operator()(ab) : NULL;
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
    Block_Obj ab = d->oblock();
    String_Obj old_p = d->property();
    Expression_Obj prop = old_p->perform(&eval);
    String_Obj new_p = SASS_MEMORY_CAST(String, prop);
    // we might get a color back
    if (!new_p) {
      std::string str(prop->to_string(ctx.c_options));
      new_p = SASS_MEMORY_NEW(ctx.mem, String_Constant, old_p->pstate(), str);
    }
    Expression_Obj value = d->value()->perform(&eval);
    Block_Obj bb = ab ? operator()(&ab) : NULL;
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
          Expression_Obj e = SASS_MEMORY_CAST(Expression, env->get_global(var));
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
            if (AST_Node_Obj node = cur->get_local(var)) {
              Expression_Obj e = SASS_MEMORY_CAST(Expression, node);
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
        if (AST_Node_Obj node = env->get_global(var)) {
          Expression_Obj e = SASS_MEMORY_CAST(Expression, node);
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
    Import_Obj result = SASS_MEMORY_NEW(ctx.mem, Import, imp->pstate());
    if (imp->import_queries() && imp->import_queries()->size()) {
      Expression_Obj ex = imp->import_queries()->perform(&eval);
      result->import_queries(SASS_MEMORY_CAST(List, ex));
    }
    for ( size_t i = 0, S = imp->urls().size(); i < S; ++i) {
      result->urls().push_back(imp->urls()[i]->perform(&eval));
    }
    // all resources have been dropped for Input_Stubs
    // for ( size_t i = 0, S = imp->incs().size(); i < S; ++i) {}
    return result->copy2(ctx.mem, __FILE__, __LINE__);
  }

  Statement_Ptr Expand::operator()(Import_Stub_Ptr i)
  {
    // get parent node from call stack
    AST_Node_Obj parent = call_stack.back();
    if (SASS_MEMORY_CAST(Block, parent) == NULL) {
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
    append_block(&ctx.sheets.at(abs_path).root);
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
    Comment_Ptr rv = SASS_MEMORY_NEW(ctx.mem, Comment, c->pstate(), SASS_MEMORY_CAST_PTR(String, c->text()->perform(&eval)), c->is_important());
    eval.is_in_comment = false;
    // TODO: eval the text, once we're parsing/storing it as a String_Schema
    return rv;
  }

  Statement_Ptr Expand::operator()(If_Ptr i)
  {
    Env env(environment(), true);
    env_stack.push_back(&env);
    call_stack.push_back(i);
    Expression_Obj rv = i->predicate()->perform(&eval);
    if (*rv) {
      append_block(&i->oblock());
    }
    else {
      Block_Ptr alt = &i->alternative();
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
    Expression_Obj low = f->lower_bound()->perform(&eval);
    if (low->concrete_type() != Expression::NUMBER) {
      throw Exception::TypeMismatch(*low, "integer");
    }
    Expression_Obj high = f->upper_bound()->perform(&eval);
    if (high->concrete_type() != Expression::NUMBER) {
      throw Exception::TypeMismatch(*high, "integer");
    }
    Number_Obj sass_start = SASS_MEMORY_CAST(Number, low);
    Number_Obj sass_end = SASS_MEMORY_CAST(Number, high);
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
    Number_Obj it = SASS_MEMORY_NEW(env.mem, Number, low->pstate(), start, sass_end->unit());
    env.set_local(variable, &it);
    Block_Ptr body = &f->oblock();
    if (start < end) {
      if (f->is_inclusive()) ++end;
      for (double i = start;
           i < end;
           ++i) {
        it->value(i);
        env.set_local(variable, &it);
        append_block(body);
      }
    } else {
      if (f->is_inclusive()) --end;
      for (double i = start;
           i > end;
           --i) {
        it->value(i);
        env.set_local(variable, &it);
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
    Expression_Obj expr = e->list()->perform(&eval);
    List_Obj list = 0;
    Map_Obj map;
    if (expr->concrete_type() == Expression::MAP) {
      map = SASS_MEMORY_CAST(Map, expr);
    }
    else if (Selector_List_Ptr ls = SASS_MEMORY_CAST(Selector_List, expr)) {
      Listize listize(ctx.mem);
      Expression_Obj rv = ls->perform(&listize);
      list = SASS_MEMORY_CAST(List, rv);
    }
    else if (expr->concrete_type() != Expression::LIST) {
      list = SASS_MEMORY_NEW(ctx.mem, List, expr->pstate(), 1, SASS_COMMA);
      list->append(expr);
    }
    else {
      list = SASS_MEMORY_CAST(List, expr);
    }
    // remember variables and then reset them
    Env env(environment(), true);
    env_stack.push_back(&env);
    call_stack.push_back(e);
    Block_Ptr body = &e->oblock();

    if (map) {
      for (auto key : map->keys()) {
        Expression_Obj k = key->perform(&eval);
        Expression_Obj v = map->at(key)->perform(&eval);

        if (variables.size() == 1) {
          List_Obj variable = SASS_MEMORY_NEW(ctx.mem, List, map->pstate(), 2, SASS_SPACE);
          *variable << k;
          *variable << v;
          env.set_local(variables[0], &variable);
        } else {
          env.set_local(variables[0], &k);
          env.set_local(variables[1], &v);
        }
        append_block(body);
      }
    }
    else {
      // bool arglist = list->is_arglist();
      if (list->length() == 1 && SASS_MEMORY_CAST(Selector_List, list)) {
        list = SASS_MEMORY_CAST(List, list);
      }
      for (size_t i = 0, L = list->length(); i < L; ++i) {
        Expression_Obj e = list->at(i);
        // unwrap value if the expression is an argument
        if (Argument_Obj arg = SASS_MEMORY_CAST(Argument, e)) e = arg->value();
        // check if we got passed a list of args (investigate)
        if (List_Obj scalars = SASS_MEMORY_CAST(List, e)) {
          if (variables.size() == 1) {
            List_Obj var = scalars;
            // if (arglist) var = (*scalars)[0];
            env.set_local(variables[0], &var);
          } else {
            for (size_t j = 0, K = variables.size(); j < K; ++j) {
              Expression_Obj res = j >= scalars->length()
                ? SASS_MEMORY_NEW(ctx.mem, Null, expr->pstate())
                : (*scalars)[j]->perform(&eval);
              env.set_local(variables[j], &res);
            }
          }
        } else {
          if (variables.size() > 0) {
            env.set_local(variables.at(0), &e);
            for (size_t j = 1, K = variables.size(); j < K; ++j) {
              Expression_Obj res = SASS_MEMORY_NEW(ctx.mem, Null, expr->pstate());
              env.set_local(variables[j], &res);
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
    Expression_Obj pred = w->predicate();
    Block_Ptr body = &w->oblock();
    Env env(environment(), true);
    env_stack.push_back(&env);
    call_stack.push_back(w);
    Expression_Obj cond = pred->perform(&eval);
    while (*&cond) {
      append_block(body);
      cond = pred->perform(&eval);
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


  void Expand::expand_selector_list(Selector_Obj s, Selector_List_Obj extender) {

    if (Selector_List_Obj sl = SASS_MEMORY_CAST(Selector_List, s)) {
      for (Complex_Selector_Obj complex_selector : sl->elements()) {
        Complex_Selector_Obj tail = complex_selector;
        while (tail) {
          if (tail->head()) for (Simple_Selector_Obj header : tail->head()->elements()) {
            if (SASS_MEMORY_CAST(Parent_Selector, header) == NULL) continue; // skip all others
            std::string sel_str(complex_selector->to_string(ctx.c_options));
            error("Can't extend " + sel_str + ": can't extend parent selectors", header->pstate(), backtrace());
          }
          tail = tail->tail();
        }
      }
    }


    Selector_List_Obj contextualized = SASS_MEMORY_CAST_PTR(Selector_List, s->perform(&eval));
    if (contextualized == false) return;
    for (auto complex_sel : contextualized->elements()) {
      Complex_Selector_Obj c = complex_sel;
      if (!c->head() || c->tail()) {
        std::string sel_str(contextualized->to_string(ctx.c_options));
        error("Can't extend " + sel_str + ": can't extend nested selectors", c->pstate(), backtrace());
      }
      Compound_Selector_Obj placeholder = c->head();
      if (contextualized->is_optional()) placeholder->is_optional(true);
      for (size_t i = 0, L = extender->length(); i < L; ++i) {
        Complex_Selector_Obj sel = (*extender)[i];
        if (!(sel->head() && sel->head()->length() > 0 &&
            SASS_MEMORY_CAST(Parent_Selector, (*sel->head())[0])))
        {
          Compound_Selector_Obj hh = SASS_MEMORY_NEW(ctx.mem, Compound_Selector, (*extender)[i]->pstate());
          hh->media_block((*extender)[i]->media_block());
          Complex_Selector_Obj ssel = SASS_MEMORY_NEW(ctx.mem, Complex_Selector, (*extender)[i]->pstate());
          ssel->media_block((*extender)[i]->media_block());
          if (sel->has_line_feed()) ssel->has_line_feed(true);
          Parent_Selector_Obj ps = SASS_MEMORY_NEW(ctx.mem, Parent_Selector, (*extender)[i]->pstate());
          ps->media_block((*extender)[i]->media_block());
          hh->append(&ps);
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
    if (Selector_List_Obj extender = SASS_MEMORY_CAST(Selector_List, selector())) {
      Selector_Obj s = e->selector();
      if (Selector_Schema_Obj schema = SASS_MEMORY_CAST(Selector_Schema, s)) {
        if (schema->has_parent_ref()) s = eval(&schema);
      }
      if (Selector_List_Obj sl = SASS_MEMORY_CAST(Selector_List, s)) {
        for (Complex_Selector_Obj cs : sl->elements()) {
          if (cs && cs->head()) {
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
    Definition_Obj dd = SASS_MEMORY_NEW(ctx.mem, Definition, *d);
    env->local_frame()[d->name() +
                        (d->type() == Definition::MIXIN ? "[m]" : "[f]")] = &dd;
/*
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
*/

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
    Definition_Obj def = SASS_MEMORY_CAST(Definition, (*env)[full_name]);
    Block_Obj body = def->oblock();
    Parameters_Obj params = def->parameters();

    if (c->oblock() && c->name() != "@content" && !body->has_content()) {
      error("Mixin \"" + c->name() + "\" does not accept a content block.", c->pstate(), backtrace());
    }
    Expression_Obj rv = c->arguments()->perform(&eval);
    Arguments_Obj args = SASS_MEMORY_CAST(Arguments, rv);
    Backtrace new_bt(backtrace(), c->pstate(), ", in mixin `" + c->name() + "`");
    backtrace_stack.push_back(&new_bt);
    Env new_env(def->environment());
    env_stack.push_back(&new_env);
    if (c->oblock()) {
      // represent mixin content blocks as thunks/closures
      Definition_Obj thunk = SASS_MEMORY_NEW(ctx.mem, Definition,
                                          c->pstate(),
                                          "@content",
                                          SASS_MEMORY_NEW(ctx.mem, Parameters, c->pstate()),
                                          c->oblock(),
                                          Definition::MIXIN);
      thunk->environment(env);
      new_env.local_frame()["@content[m]"] = &thunk;
    }

    bind(std::string("Mixin"), c->name(), params, args, &ctx, &new_env, &eval);

    Block_Obj trace_block = SASS_MEMORY_OBJ(ctx.mem, Block, c->pstate());
    Trace_Obj trace = SASS_MEMORY_NEW(ctx.mem, Trace, c->pstate(), c->name(), trace_block);


    block_stack.push_back(&trace_block);
    for (auto bb : body->elements()) {
      Statement_Obj ith = bb->perform(this);
      if (ith) trace->oblock()->append(ith);
    }
    block_stack.pop_back();

    env_stack.pop_back();
    backtrace_stack.pop_back();

    recursions --;
    return trace->copy2(ctx.mem, __FILE__, __LINE__);
  }

  Statement_Ptr Expand::operator()(Content_Ptr c)
  {
    Env* env = environment();
    // convert @content directives into mixin calls to the underlying thunk
    if (!env->has("@content[m]")) return 0;

    if (block_stack.back()->is_root()) {
      selector_stack.push_back(0);
    }

    Mixin_Call_Obj call = SASS_MEMORY_NEW(ctx.mem, Mixin_Call,
                                       c->pstate(),
                                       "@content",
                                       SASS_MEMORY_NEW(ctx.mem, Arguments, c->pstate()));

    Trace_Obj trace = SASS_MEMORY_CAST_PTR(Trace, call->perform(this));

    if (block_stack.back()->is_root()) {
      selector_stack.pop_back();
    }

    return trace->copy2(ctx.mem, __FILE__, __LINE__);
  }

  // produce an error if something is not implemented
  inline Statement_Ptr Expand::fallback_impl(AST_Node_Ptr n)
  {
    std::string err =std:: string("`Expand` doesn't handle ") + typeid(*n).name();
    String_Quoted_Obj msg = SASS_MEMORY_NEW(ctx.mem, String_Quoted, ParserState("[WARN]"), err);
    error("unknown internal error; please contact the LibSass maintainers", n->pstate(), backtrace());
    return SASS_MEMORY_NEW(ctx.mem, Warning, ParserState("[WARN]"), &msg);
  }

  // process and add to last block on stack
  inline void Expand::append_block(Block_Ptr b)
  {
    if (b->is_root()) call_stack.push_back(b);
    for (size_t i = 0, L = b->length(); i < L; ++i) {
      Statement_Ptr stm = &b->at(i);
      Statement_Obj ith = stm->perform(this);
      if (ith) block_stack.back()->append(ith);
    }
    if (b->is_root()) call_stack.pop_back();
  }

}
