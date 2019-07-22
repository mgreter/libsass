// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include <iostream>
#include <typeinfo>

#include "ast.hpp"
#include "expand.hpp"
#include "bind.hpp"
#include "eval.hpp"
#include "backtrace.hpp"
#include "context.hpp"
#include "sass_functions.hpp"
#include "error_handling.hpp"
#include "debugger.hpp"

#include "parser_scss.hpp"
#include "parser_selector.hpp"
#include "parser_media_query.hpp"

namespace Sass {

  Expand::Expand(Context& ctx, Env* env, SelectorStack* stack, SelectorStack* originals)
  : ctx(ctx),
    traces(ctx.traces),
    eval(Eval(*this)),
    recursions(0),
    in_keyframes(false),
    at_root_without_rule(false),
    old_at_root_without_rule(false),
    _styleRule(),
    _inFunction(false),
    _inUnknownAtRule(false),
    _atRootExcludingStyleRule(false),
    _inKeyframes(false),
    plainCss(false),
    // _stylesheet(),
    env_stack(),
    block_stack(),
    call_stack(),
    selector_stack(),
    originalStack(),
    mediaStack()
  {
    env_stack.push_back(nullptr);
    env_stack.push_back(env);
    block_stack.push_back(nullptr);
    call_stack.push_back({});
    if (stack == NULL) { pushToSelectorStack({}); }
    else {
      for (auto item : *stack) {
        if (item.isNull()) pushToSelectorStack({});
        else pushToSelectorStack(item);
      }
    }
    if (originals == NULL) { pushToOriginalStack({}); }
    else {
      for (auto item : *stack) {
        if (item.isNull()) pushToOriginalStack({});
        else pushToOriginalStack(item);
      }
    }
    mediaStack.push_back({});
  }

  Env* Expand::environment()
  {
    if (env_stack.size() > 0)
      return env_stack.back();
    return 0;
  }

  SelectorStack Expand::getSelectorStack()
  {
    return selector_stack;
  }

  SelectorListObj& Expand::selector()
  {
    if (selector_stack.size() > 0) {
      auto& sel = selector_stack.back();
      if (sel.isNull()) return sel;
      return sel;
    }
    // Avoid the need to return copies
    // We always want an empty first item
    selector_stack.push_back({});
    return selector_stack.back();;
  }

  SelectorListObj& Expand::original()
  {
    if (originalStack.size() > 0) {
      auto& sel = originalStack.back();
      if (sel.isNull()) return sel;
      return sel;
    }
    // Avoid the need to return copies
    // We always want an empty first item
    originalStack.push_back({});
    return originalStack.back();
  }

  SelectorListObj Expand::popFromSelectorStack()
  {
    SelectorListObj last = selector_stack.back();
    if (selector_stack.size() > 0)    
      selector_stack.pop_back();
    if (last.isNull()) return {};
    return last;
  }

  void Expand::pushToSelectorStack(SelectorListObj selector)
  {
    selector_stack.push_back(selector);
  }

  SelectorListObj Expand::popFromOriginalStack()
  {
    SelectorListObj last = originalStack.back();
    if (originalStack.size() > 0)
      originalStack.pop_back();
    if (last.isNull()) return {};
    return last;
  }

  void Expand::pushToOriginalStack(SelectorListObj selector)
  {
    originalStack.push_back(selector);
  }

  // blocks create new variable scopes
  Block* Expand::operator()(Block* b)
  {
    // create new local environment
    // set the current env as parent
    Env env(environment());
    // copy the block object (add items later)
    Block_Obj bb = SASS_MEMORY_NEW(Block,
                                b->pstate(),
                                b->length(),
                                b->is_root());
    // setup block and env stack
    this->block_stack.push_back(bb);
    this->env_stack.push_back(&env);
    // operate on block
    // this may throw up!
    this->append_block(b);
    // revert block and env stack
    this->block_stack.pop_back();
    this->env_stack.pop_back();
    // return copy
    return bb.detach();
  }

  Statement* Expand::operator()(StyleRule* r)
  {
    LOCAL_FLAG(old_at_root_without_rule, at_root_without_rule);

    LocalOption<StyleRuleObj> oldStyleRule(_styleRule, r);

    if (in_keyframes) {
      Block* bb = operator()(r->block());
      Keyframe_Rule_Obj k = SASS_MEMORY_NEW(Keyframe_Rule, r->pstate(), bb);
      if (r->interpolation()) {
        pushToSelectorStack({});
        auto val = eval.interpolationToValue(r->interpolation(), true, false);
        k->name2(SASS_MEMORY_NEW(StringLiteral, "[pstate]", val));
        popFromSelectorStack();
      }

      return k.detach();
    }

    SelectorListObj slist;
    if (r->interpolation()) {
      Sass_Import_Entry imp = ctx.import_stack.back();
      bool plainCss = imp->type == SASS_IMPORT_CSS;
      slist = itplToSelector(r->interpolation(), plainCss);
    }

    // reset when leaving scope
    LOCAL_FLAG(at_root_without_rule, false);
    SASS_ASSERT(slist, "must have selectors");

    SelectorListObj evaled = slist->resolveParentSelectors(
      getSelectorStack().back(), traces, !old_at_root_without_rule);
    // do not connect parent again
    Env env(environment());
    if (block_stack.back()->is_root()) {
      env_stack.push_back(&env);
    }
    Block_Obj blk;
    pushToSelectorStack(evaled);
    // The copy is needed for parent reference evaluation
    // dart-sass stores it as `originalSelector` member
    pushToOriginalStack(SASS_MEMORY_COPY(evaled));
    ctx.extender.addSelector(evaled, mediaStack.back());
    if (r->block()) blk = operator()(r->block());
    popFromOriginalStack();
    popFromSelectorStack();

    CssStyleRule* rr = SASS_MEMORY_NEW(CssStyleRule,
      r->pstate(),
      evaled);

    if (block_stack.back()->is_root()) {
      env_stack.pop_back();
    }

    rr->concat(blk->elements());
    rr->block(blk);

    rr->tabs(r->tabs());

    return rr;
  }

  Statement* Expand::operator()(SupportsRule* f)
  {
    Expression_Obj condition = f->condition()->perform(&eval);
    CssSupportsRuleObj ff = SASS_MEMORY_NEW(CssSupportsRule,
                                       f->pstate(),
                                       condition);
    Block* bb = operator()(f->block());
    ff->block(bb);
    ff->tabs(f->tabs());
    return ff.detach();
  }

  bool Expand::isInMixin()
  {
    for (Sass_Callee& callee : ctx.callee_stack) {
      if (callee.type == SASS_CALLEE_MIXIN) return true;
    }
    return false;
  }

  bool Expand::isInContentBlock()
  {
    return false;
  }

  std::vector<CssMediaQueryObj> Expand::mergeMediaQueries(
    const std::vector<CssMediaQueryObj>& lhs,
    const std::vector<CssMediaQueryObj>& rhs)
  {
    std::vector<CssMediaQueryObj> queries;
    for (CssMediaQueryObj query1 : lhs) {
      for (CssMediaQueryObj query2 : rhs) {
        CssMediaQueryObj result = query1->merge(query2);
        if (result && !result->empty()) {
          queries.push_back(result);
        }
      }
    }
    return queries;
  }

  Statement* Expand::operator()(MediaRule* m)
  {
    Expression_Obj mq;
    std::string str_mq;
    ParserState state(m->pstate());
    if (m->query()) {
      state = m->query()->pstate();
      str_mq = performInterpolation(m->query());
    }
    char* str = sass_copy_c_string(str_mq.c_str());
    ctx.strings.push_back(str);
    MediaQueryParser p2(ctx, str, state.path, state.file);
    // Create a new CSS only representation of the media rule
    CssMediaRuleObj css = SASS_MEMORY_NEW(CssMediaRule, m->pstate(), m->block());
    std::vector<CssMediaQueryObj> parsed = p2.parse();
    if (mediaStack.size() && mediaStack.back()) {
      auto& parent = mediaStack.back()->elements();
      css->Vectorized::concat(mergeMediaQueries(parent, parsed));
    }
    else {
      css->Vectorized::concat(parsed);
    }
    mediaStack.push_back(css);
    css->block(operator()(m->block()));
    mediaStack.pop_back();
    return css.detach();

  }

  Statement* Expand::operator()(At_Root_Block* a)
  {
    Block_Obj ab = a->block();
    At_Root_Query_Obj ae = a->expression();
    if (ae) ae = eval(ae);
    else ae = SASS_MEMORY_NEW(At_Root_Query, a->pstate());
    /*
    if (false && _inKeyframes && ae->exclude("keyframes")) {
      LOCAL_FLAG(_inKeyframes, false);
      Block_Obj bb = ab ? operator()(ab) : NULL;
      At_Root_Block_Obj aa = SASS_MEMORY_NEW(At_Root_Block,
        a->pstate(),
        Cast<At_Root_Query>(ae),
        bb);
      return aa.detach();

    }

    else if (false && _inUnknownAtRule) {
      LOCAL_FLAG(_inUnknownAtRule, false);
      Block_Obj bb = ab ? operator()(ab) : NULL;
      At_Root_Block_Obj aa = SASS_MEMORY_NEW(At_Root_Block,
        a->pstate(),
        Cast<At_Root_Query>(ae),
        bb);

      return aa.detach();

    }
    else {*/
      LOCAL_FLAG(in_keyframes, false);
      LOCAL_FLAG(_inUnknownAtRule, false);
      LOCAL_FLAG(at_root_without_rule, ae->exclude("rule"));
      Block_Obj bb = ab ? operator()(ab) : NULL;
      At_Root_Block_Obj aa = SASS_MEMORY_NEW(At_Root_Block,
        a->pstate(),
        Cast<At_Root_Query>(ae),
        bb);

      return aa.detach();

    // }

  }

  SelectorListObj Expand::itplToSelector(Interpolation* itpl, bool plainCss)
  {
    auto text = interpolationToValue(itpl, true);
    char* cstr = sass_copy_c_string(text.c_str());
    ctx.strings.push_back(cstr);
    SelectorParser p2(ctx, cstr, itpl->pstate().path, itpl->pstate().file);
    p2._allowPlaceholder = plainCss == false;
    p2._allowParent = plainCss == false;
    return p2.parse();
  }

  Statement* Expand::operator()(AtRule* a)
  {
    Block* ab = a->block();

    Expression* av = a->value();
    std::string name(a->keyword());

    if (a->value2()) {
      av = SASS_MEMORY_NEW(StringLiteral, "[pstate]", interpolationToValue(a->value2(), true));
      // if (as) as->clear();
    }

    if (name.empty() && a->interpolation()) {
      name = interpolationToValue(a->interpolation(), true, true);
    }


    std::string normalized(Util::unvendor(name));
    LOCAL_FLAG(in_keyframes, normalized == "keyframes");
    LOCAL_FLAG(_inKeyframes, normalized == "keyframes");
    LOCAL_FLAG(_inUnknownAtRule, normalized != "keyframes");
    if (name[0] != 0 && name[0] != '@') {
      name = "@" + name;
    }

    Block* bb = ab ? operator()(ab) : NULL;
    AtRule* aa = SASS_MEMORY_NEW(AtRule,
                                  a->pstate(),
                                  name,
                                  bb,
                                  av);
    return aa;
  }

  Statement* Expand::operator()(Declaration* d)
  {


    if (!isInStyleRule() && !_inUnknownAtRule && !_inKeyframes) {
      error(
        "Declarations may only be used within style rules.",
        d->pstate(), traces);
    }

    Block_Obj ab = d->block();
    String_Obj old_p = d->property();
    Expression_Obj prop = old_p->perform(&eval);

    if (auto itpl = Cast<Interpolation>(old_p)) {
      auto text = interpolationToValue(itpl, true);
      prop = SASS_MEMORY_NEW(String_Constant, old_p->pstate(), text, true);
    }

    String_Obj new_p = Cast<String>(prop);
    // we might get a color back
    if (!new_p) {
      std::string str(prop->to_string(ctx.c_options));
      new_p = SASS_MEMORY_NEW(StringLiteral, old_p->pstate(), str);
    }
    Expression_Obj value = d->value();
    if (value) value = value->perform(&eval);

    bool is_custom_prop = false;

    if (String_Constant * str = Cast<String_Constant>(value)) {
      if (!d->is_custom_property()) {
        std::string text(str->value());
        Util::ascii_str_ltrim(text);
        str->value(text);
      }
    }


    Block_Obj bb = ab ? operator()(ab) : NULL;
    if (!bb) {
      if (!value || (value->is_invisible())) {
        if (d->is_custom_property() || is_custom_prop) {
          error("Custom property values may not be empty.", d->value()->pstate(), traces);
        } else {
          return nullptr;
        }
      }
    }


    Declaration* decl = SASS_MEMORY_NEW(Declaration,
                                        d->pstate(),
                                        new_p,
                                        value,
                                        d->is_custom_property(),
                                        bb);
    decl->tabs(d->tabs());
    return decl;
  }

  Statement* Expand::operator()(Assignment* a)
  {
    Env* env = environment();
    std::string var(a->variable());
    if (a->is_global()) {
      if (env->is_global()) {
        if (!env->has_global(var)) {
          deprecatedDart(
            "As of Dart Sass 2.0.0, !global assignments won't be able to\n"
            "declare new variables.Since this assignment is at the root of the stylesheet,\n"
            "the !global flag is unnecessary and can safely be removed.",
            true, a->pstate());
        }
      }
      else {
        if (!env->has_global(var)) {
          deprecatedDart(
            "As of Dart Sass 2.0.0, !global assignments won't be able to\n"
            "declare new variables. Consider adding `" + var + ": null` at the root of the\n"
            "stylesheet.",
            true, a->pstate());
        }
      }
      if (a->is_default()) {
        if (env->has_global(var)) {
          Expression_Obj e = Cast<Expression>(env->get_global(var));
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
              Expression_Obj e = Cast<Expression>(node);
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
          Expression_Obj e = Cast<Expression>(node);
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

  Statement* Expand::operator()(Import* imp)
  {
    // std::cerr << "visit an import\n";
    Import_Obj result = SASS_MEMORY_NEW(Import, imp->pstate());
    if (!imp->queries2().empty()) {
      std::vector<std::string> results;
      std::vector<ExpressionObj> queries = imp->queries2();
      for (auto& query : queries) {
        results.push_back(query->perform(&eval)->to_string());
      }
      std::string reparse(joinStrings(results, ", "));
      ParserState state(imp->pstate());
      char* str = sass_copy_c_string(reparse.c_str());
      ctx.strings.push_back(str);
      MediaQueryParser p2(ctx, str, state.path, state.file);
      auto queries2 = p2.parse();
      result->queries(queries2);
      result->import_queries({});
    }
    for ( size_t i = 0, S = imp->urls().size(); i < S; ++i) {
      result->urls().push_back(imp->urls()[i]->perform(&eval));
    }
    // all resources have been dropped for Input_Stubs

    for ( size_t i = 0, S = imp->incs().size(); i < S; ++i) {
      Include inc = imp->incs()[i];
      // std::cerr << "convert incs to import stubs " << inc.type << "\n";
      Import_StubObj stub = SASS_MEMORY_NEW(Import_Stub, "[pstate]", inc);
      stub->perform(this);
    }

    return result.detach();
  }

  Statement* Expand::operator()(ImportRule* rule)
  {
    for (ImportBaseObj import : rule->elements()) {
      if (import) import->perform(this);
    }
    return rule;
  }

  // Adds a CSS import for [import].
  Statement* Expand::operator()(StaticImport* rule)
  {

    if (rule->url()) rule->url(rule->url()->perform(&eval));
    if (rule->media()) {
      std::string str(eval.interpolationToValue(rule->media(), true, true));
      rule->media(SASS_MEMORY_NEW(Interpolation, "[pstate]"));
      rule->media()->append(SASS_MEMORY_NEW(StringLiteral, "[pstate]", str));
    }
    if (rule->supports()) { rule->supports(rule->supports()->perform(&eval)); }
    return rule;
  }

  // Adds the stylesheet imported by [import] to the current document.
  Statement* Expand::operator()(DynamicImport* rule)
  {
    std::cerr << "visit dynamicImport" << "\n";
    return rule;
  }

  Statement* Expand::operator()(Import_Stub* i)
  {
    // std::cerr << "visit an import stub (dynamicImport)" << "\n";
    traces.push_back(Backtrace(i->pstate()));
    // get parent node from call stack
    AST_Node_Obj parent = call_stack.back();
    // we don't seem to need that actually afterall
    Sass_Import_Entry import = sass_make_import(
      i->imp_path().c_str(),
      i->abs_path().c_str(),
      0, 0
    );

    Block_Obj trace_block = SASS_MEMORY_NEW(Block, i->pstate());
    Trace_Obj trace = SASS_MEMORY_NEW(Trace, i->pstate(), i->imp_path(), trace_block, 'i');
    block_stack.back()->append(trace);
    block_stack.push_back(trace_block);

    const std::string& abs_path(i->resource().abs_path);
    StyleSheet sheet = ctx.sheets.at(abs_path);
    import->type = sheet.syntax;
    ctx.import_stack.push_back(import);
    append_block(sheet.root);

    sass_delete_import(ctx.import_stack.back());
    ctx.import_stack.pop_back();
    block_stack.pop_back();
    traces.pop_back();
    return 0;
  }

  Statement* Expand::operator()(Warning* w)
  {
    // eval handles this too, because warnings may occur in functions
    w->perform(&eval);
    return 0;
  }

  Statement* Expand::operator()(Error* e)
  {
    // eval handles this too, because errors may occur in functions
    e->perform(&eval);
    return 0;
  }

  Statement* Expand::operator()(Debug* d)
  {
    // eval handles this too, because warnings may occur in functions
    d->perform(&eval);
    return 0;
  }

  Statement* Expand::operator()(LoudComment* c)
  {
    std::string text(performInterpolation(c->text()));
    bool preserve = text[2] == '!';
    return SASS_MEMORY_NEW(CssComment, c->pstate(), text, preserve);
  }

  Statement* Expand::operator()(SilentComment* c)
  {
    return c;
  }

  Statement* Expand::operator()(If* i)
  {
    Env env(environment(), true);
    env_stack.push_back(&env);
    call_stack.push_back(i);
    Expression_Obj rv = i->predicate()->perform(&eval);
    if (*rv) {
      append_block(i->block());
    }
    else {
      Block* alt = i->alternative();
      if (alt) append_block(alt);
    }
    call_stack.pop_back();
    env_stack.pop_back();
    return 0;
  }

  // For does not create a new env scope
  // But iteration vars are reset afterwards
  Statement* Expand::operator()(For* f)
  {
    std::string variable(f->variable());
    Expression_Obj low = f->lower_bound()->perform(&eval);
    if (low->concrete_type() != Expression::NUMBER) {
      traces.push_back(Backtrace(low->pstate()));
      throw Exception::TypeMismatch(traces, *low, "a number");
    }
    Expression_Obj high = f->upper_bound()->perform(&eval);
    if (high->concrete_type() != Expression::NUMBER) {
      traces.push_back(Backtrace(high->pstate()));
      throw Exception::TypeMismatch(traces, *high, "a number");
    }
    Number_Obj sass_start = Cast<Number>(low);
    Number_Obj sass_end = Cast<Number>(high);
    // check if units are valid for sequence
    if (sass_start->unit() != sass_end->unit()) {
      std::stringstream msg; msg << "Incompatible units "
        << sass_start->unit() << " and "
        << sass_end->unit() << ".";
      error(msg.str(), low->pstate(), traces);
    }
    double start = sass_start->value();
    double end = sass_end->value();
    // only create iterator once in this environment
    Env env(environment(), true);
    env_stack.push_back(&env);
    call_stack.push_back(f);
    Block* body = f->block();
    if (start < end) {
      if (f->is_inclusive()) ++end;
      for (double i = start;
           i < end;
           ++i) {
        Number_Obj it = SASS_MEMORY_NEW(Number, low->pstate(), i, sass_end->unit());
        env.set_local(variable, it);
        append_block(body);
      }
    } else {
      if (f->is_inclusive()) --end;
      for (double i = start;
           i > end;
           --i) {
        Number_Obj it = SASS_MEMORY_NEW(Number, low->pstate(), i, sass_end->unit());
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
  Statement* Expand::operator()(Each* e)
  {
    std::vector<std::string> variables(e->variables());
    Expression_Obj expr = e->list()->perform(&eval);
    SassList_Obj list;
    Map_Obj map;
    if (expr->concrete_type() == Expression::MAP) {
      map = Cast<Map>(expr);
    }
    else if (expr->concrete_type() != Expression::LIST) {
      list = SASS_MEMORY_NEW(SassList, expr->pstate(), SASS_COMMA);
      list->append(expr);
    }
    else if (SassList * slist = Cast<SassList>(expr)) {
      list = SASS_MEMORY_NEW(SassList, expr->pstate(), slist->separator());
      list->hasBrackets(slist->hasBrackets());
      for (auto item : slist->elements()) {
        list->append(item);
      }
    }
    else if (List * ls = Cast<List>(expr)) {
      list = list_to_sass_list(ls);
    }
    else {
      list = Cast<SassList>(expr);
    }
    // remember variables and then reset them
    Env env(environment(), true);
    env_stack.push_back(&env);
    call_stack.push_back(e);
    Block* body = e->block();

    if (map) {
      for (auto key : map->keys()) {
        Expression_Obj k = key->perform(&eval);
        Expression_Obj v = map->at(key)->perform(&eval);

        if (variables.size() == 1) {
          SassList_Obj variable = SASS_MEMORY_NEW(SassList, map->pstate(), SASS_SPACE);
          variable->append(k);
          variable->append(v);
          env.set_local(variables[0], variable);
        } else {
          env.set_local(variables[0], k);
          env.set_local(variables[1], v);
        }
        append_block(body);
      }
    }
    else if (SassList * sasslist = Cast<SassList>(expr)) {
      // bool arglist = list->is_arglist();
      for (size_t i = 0, L = sasslist->length(); i < L; ++i) {
        Expression_Obj item = sasslist->at(i);
        // unwrap value if the expression is an argument
        if (Argument_Obj arg = Cast<Argument>(item)) item = arg->value();
        // check if we got passed a list of args (investigate)
        if (List_Obj scalars = Cast<List>(item)) {
          if (variables.size() == 1) {
            List_Obj var = scalars;
            // if (arglist) var = (*scalars)[0];
            env.set_local(variables[0], var);
          }
          else {
            for (size_t j = 0, K = variables.size(); j < K; ++j) {
              Expression_Obj res = j >= scalars->length()
                ? SASS_MEMORY_NEW(Null, expr->pstate())
                : (*scalars)[j]->perform(&eval);
              env.set_local(variables[j], res);
            }
          }
        }
        else if (SassList_Obj scalars = Cast<SassList>(item)) {
          if (variables.size() == 1) {
            SassList_Obj var = scalars;
            // if (arglist) var = (*scalars)[0];
            env.set_local(variables[0], var);
          }
          else {
            for (size_t j = 0, K = variables.size(); j < K; ++j) {
              Expression_Obj res = j >= scalars->length()
                ? SASS_MEMORY_NEW(Null, expr->pstate())
                : (*scalars)[j]->perform(&eval);
              env.set_local(variables[j], res);
            }
          }
        }
        else {
          if (variables.size() > 0) {
            env.set_local(variables.at(0), item);
            for (size_t j = 1, K = variables.size(); j < K; ++j) {
              Expression_Obj res = SASS_MEMORY_NEW(Null, expr->pstate());
              env.set_local(variables[j], res);
            }
          }
        }
        append_block(body);
      }
    }
    else {
      for (size_t i = 0, L = list->length(); i < L; ++i) {
        Expression_Obj item = list->at(i);
        // unwrap value if the expression is an argument
        if (Argument_Obj arg = Cast<Argument>(item)) item = arg->value();
        // check if we got passed a list of args (investigate)
        if (List_Obj scalars = Cast<List>(item)) {
          if (variables.size() == 1) {
            List_Obj var = scalars;
            // if (arglist) var = (*scalars)[0];
            env.set_local(variables[0], var);
          }
          else {
            for (size_t j = 0, K = variables.size(); j < K; ++j) {
              Expression_Obj res = j >= scalars->length()
                ? SASS_MEMORY_NEW(Null, expr->pstate())
                : (*scalars)[j]->perform(&eval);
              env.set_local(variables[j], res);
            }
          }
        }
        else if (SassList_Obj scalars = Cast<SassList>(item)) {
          if (variables.size() == 1) {
            SassList_Obj var = scalars;
            // if (arglist) var = (*scalars)[0];
            env.set_local(variables[0], var);
          }
          else {
            for (size_t j = 0, K = variables.size(); j < K; ++j) {
              Expression_Obj res = j >= scalars->length()
                ? SASS_MEMORY_NEW(Null, expr->pstate())
                : (*scalars)[j]->perform(&eval);
              env.set_local(variables[j], res);
            }
          }
        }
        else {
          if (variables.size() > 0) {
            env.set_local(variables.at(0), item);
            for (size_t j = 1, K = variables.size(); j < K; ++j) {
              Expression_Obj res = SASS_MEMORY_NEW(Null, expr->pstate());
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

  Statement* Expand::operator()(While* w)
  {
    Expression_Obj pred = w->predicate();
    Block* body = w->block();
    Env env(environment(), true);
    env_stack.push_back(&env);
    call_stack.push_back(w);
    Expression_Obj cond = pred->perform(&eval);
    while (!cond->is_false()) {
      append_block(body);
      cond = pred->perform(&eval);
    }
    call_stack.pop_back();
    env_stack.pop_back();
    return 0;
  }

  std::string Expand::joinStrings(
    std::vector<std::string>& strings,
    const char* const separator)
  {
    switch (strings.size())
    {
    case 0:
      return "";
    case 1:
      return strings[0];
    default:
      std::ostringstream os;
      std::move(strings.begin(), strings.end() - 1,
        std::ostream_iterator<std::string>(os, separator));
      os << *strings.rbegin();
      return os.str();
    }
  }

  /// Evaluates [interpolation].
  ///
  /// If [warnForColor] is `true`, this will emit a warning for any named color
  /// values passed into the interpolation.
  std::string Expand::performInterpolation(InterpolationObj interpolation, bool warnForColor)
  {
    // return eval(interpolation)->value();
    std::vector<std::string> results;

    // debug_ast(interpolation);

    for (auto value : interpolation->elements()) {

      if (StringLiteral * str = Cast<StringLiteral>(value)) {
        results.push_back(str->text());
        continue;
      }

      Expression* expression = value;

      ExpressionObj result = expression->perform(&eval);

      if (Cast<Null>(result)) continue;
      if (result == nullptr) continue;

      /*
      if (warnForColor &&
        result is SassColor &&
        namesByColor.containsKey(result)) {
        var alternative = BinaryOperationExpression(
          BinaryOperator.plus,
          StringExpression(Interpolation([""], null), quotes: true),
          expression);
        _warn(
          "You probably don't mean to use the color value "
          "${namesByColor[result]} in interpolation here.\n"
          "It may end up represented as $result, which will likely produce "
          "invalid CSS.\n"
          "Always quote color names when using them as strings or map keys "
          '(for example, "${namesByColor[result]}").\n'
          "If you really want to use the color value here, use '$alternative'.",
          expression.span);
      }
      */

      if (String_Constant* str = Cast<String_Constant>(result)) {
        String_Constant_Obj sobj = SASS_MEMORY_COPY(str);
        sobj->quote_mark(0);
        // return _serialize(result, expression, quote: false);
        // ToDo: quote = false
        results.push_back(sobj->to_css());

      }
      else {
        // return _serialize(result, expression, quote: false);

        // ToDo: quote = false
        results.push_back(result->to_css());

      }

    }

    return joinStrings(results);

  }

  /// Evaluates [interpolation] and wraps the result in a [CssValue].
  ///
  /// If [trim] is `true`, removes whitespace around the result. If
  /// [warnForColor] is `true`, this will emit a warning for any named color
  /// values passed into the interpolation.
  std::string Expand::interpolationToValue(InterpolationObj interpolation,
    bool trim, bool warnForColor)
  {
    std::string result = performInterpolation(interpolation, warnForColor);
    if (trim) { Util::ascii_str_trim(result); } // ToDo: excludeEscape: true
    return result;
    // return CssValue(trim ? trimAscii(result, excludeEscape: true) : result,
    //   interpolation.span);
  }


  Statement* Expand::operator()(ExtendRule* e)
  {

    if (!isInStyleRule() && !isInMixin() && !isInContentBlock()) {
      error(
        "@extend may only be used within style rules.",
        e->pstate(), traces);
    }

    InterpolationObj itpl = e->selector();

    std::string text = interpolationToValue(itpl, true);

    /*
    var list = _adjustParseError(
      targetText,
      () = > SelectorList.parse(
        trimAscii(targetText.value, excludeEscape: true),
        logger: _logger,
        allowParent : false));
        */

    // Sass_Import_Entry parent = ctx.import_stack.back();
    char* cstr = sass_copy_c_string(text.c_str());
    ctx.strings.push_back(cstr);
    SelectorParser p2(ctx, cstr, "foobar", 0);
    if (block_stack.size() > 2) {
      Block* b = block_stack.at(block_stack.size() - 2);
      if (b->is_root()) p2._allowParent = false;
    }

    SelectorListObj slist = p2.parse();

    if (slist) {

      for (auto complex : slist->elements()) {

        if (complex->length() != 1) {
          error("complex selectors may not be extended.", complex->pstate(), traces);
        }

        if (const CompoundSelector* compound = complex->first()->getCompound()) {

          if (compound->length() != 1) {

            std::cerr <<
              "compound selectors may no longer be extended.\n"
              "Consider `@extend ${compound.components.join(', ')}` instead.\n"
              "See http://bit.ly/ExtendCompound for details.\n";

            // Make this an error once deprecation is over
            for (SimpleSelectorObj simple : compound->elements()) {
              // Pass every selector we ever see to extender (to make them findable for extend)
              ctx.extender.addExtension(selector(), simple, mediaStack.back(), e->isOptional());
            }

          }
          else {
            // Pass every selector we ever see to extender (to make them findable for extend)
            ctx.extender.addExtension(selector(), compound->first(), mediaStack.back(), e->isOptional());
          }

        }
        else {
          error("complex selectors may not be extended.", complex->pstate(), traces);
        }
      }
    }

    return nullptr;

  }

  Statement* Expand::operator()(Definition* d)
  {
    Env* env = environment();
    Definition_Obj dd = SASS_MEMORY_COPY(d);

    env->local_frame()[d->name() +
                        (d->type() == Definition::MIXIN ? "[m]" : "[f]")] = dd;

    // set the static link so we can have lexical scoping
    dd->environment(env);
    return 0;
  }

  Statement* Expand::operator()(Mixin_Call* c)
  {

    RECURSION_GUARD(recursions, c->pstate());

    Env* env = environment();
    std::string full_name(c->name() + "[m]");
    if (!env->has(full_name)) {
      error("no mixin named " + c->name(), c->pstate(), traces);
    }
    Definition_Obj def = Cast<Definition>((*env)[full_name]);
    Block_Obj body = def->block();
    Parameters_Obj params = def->parameters();

    if (c->block() && c->name() != "@content" && !body->has_content()) {
      // error("Mixin \"" + c->name() + "\" does not accept a content block.", c->pstate(), traces);
      error("Mixin doesn't accept a content block.", c->pstate(), traces);
    }
    // debug_ast(c->arguments(), "IN: ");
    Expression_Obj rv = c->arguments()->perform(&eval);
    // debug_ast(rv, "OUT: ");
    Arguments_Obj args = Cast<Arguments>(rv);
    std::string msg(", in mixin `" + c->name() + "`");
    traces.push_back(Backtrace(c->pstate(), msg));
    ctx.callee_stack.push_back({
      c->name().c_str(),
      c->pstate().path,
      c->pstate().line + 1,
      c->pstate().column + 1,
      SASS_CALLEE_MIXIN,
      { env }
    });

    Env new_env(def->environment());
    env_stack.push_back(&new_env);
    if (c->block()) {
      Parameters_Obj params = c->block_parameters();
      if (!params) params = SASS_MEMORY_NEW(Parameters, c->pstate());
      // represent mixin content blocks as thunks/closures
      Definition_Obj thunk = SASS_MEMORY_NEW(Definition,
                                          c->pstate(),
                                          "@content",
                                          params,
                                          c->block(),
                                          Definition::MIXIN);
      thunk->environment(env);
      new_env.local_frame()["@content[m]"] = thunk;
    }

    bind(std::string("Mixin"), c->name(), params, args, &new_env, &eval, traces);

    Block_Obj trace_block = SASS_MEMORY_NEW(Block, c->pstate());
    Trace_Obj trace = SASS_MEMORY_NEW(Trace, c->pstate(), c->name(), trace_block);

    env->set_global("is_in_mixin", bool_true);
    if (Block* pr = block_stack.back()) {
      trace_block->is_root(pr->is_root());
    }
    block_stack.push_back(trace_block);
    for (auto bb : body->elements()) {
      Statement_Obj ith = bb->perform(this);
      if (ith) trace->block()->append(ith);
    }
    block_stack.pop_back();
    env->del_global("is_in_mixin");

    ctx.callee_stack.pop_back();
    env_stack.pop_back();
    traces.pop_back();

    return trace.detach();
  }

  Statement* Expand::operator()(Content* c)
  {
    Env* env = environment();
    // convert @content directives into mixin calls to the underlying thunk
    if (!env->has("@content[m]")) return 0;
    Arguments_Obj args = c->arguments();
    if (!args) args = SASS_MEMORY_NEW(Arguments, c->pstate());

    Mixin_Call_Obj call = SASS_MEMORY_NEW(Mixin_Call,
                                       c->pstate(),
                                       "@content",
                                       args);

    Trace_Obj trace = Cast<Trace>(call->perform(this));
    return trace.detach();
  }

  // process and add to last block on stack
  inline void Expand::append_block(Block* b)
  {
    if (b->is_root()) call_stack.push_back(b);
    for (size_t i = 0, L = b->length(); i < L; ++i) {
      Statement* stm = b->at(i);
      Statement_Obj ith = stm->perform(this);
      if (ith) block_stack.back()->append(ith);
    }
    if (b->is_root()) call_stack.pop_back();
  }

}
