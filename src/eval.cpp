// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include <cstdlib>
#include <cmath>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <typeinfo>

#include "file.hpp"
#include "eval.hpp"
#include "ast.hpp"
#include "bind.hpp"
#include "util.hpp"
#include "inspect.hpp"
#include "operators.hpp"
#include "environment.hpp"
#include "position.hpp"
#include "sass/values.h"
#include "ast2c.hpp"
#include "c2ast.hpp"
#include "context.hpp"
#include "backtrace.hpp"
#include "lexer.hpp"
#include "debugger.hpp"
#include "expand.hpp"
#include "color_maps.hpp"
#include "sass_functions.hpp"
#include "error_handling.hpp"
#include "util_string.hpp"
#include "dart_helpers.hpp"

namespace Sass {

  Eval::Eval(Expand& exp)
  : exp(exp),
    ctx(exp.ctx),
    traces(exp.traces)
  {
    bool_true = SASS_MEMORY_NEW(Boolean, "[NA]", true);
    bool_false = SASS_MEMORY_NEW(Boolean, "[NA]", false);
  }
  Eval::~Eval() { }

  std::string Eval::serialize(AST_Node* node)
  {
    Sass_Inspect_Options serializeOpt(ctx.c_options);
    serializeOpt.output_style = TO_CSS;
    Sass_Output_Options out(serializeOpt);
    Emitter emitter(out);
    Inspect serialize(emitter);
    serialize.in_declaration = true;
    serialize.quotes = false;
    node->perform(&serialize);
    return serialize.get_buffer();
  }

  Env* Eval::environment()
  {
    return exp.environment();
  }

  const std::string Eval::cwd()
  {
    return ctx.cwd();
  }

  struct Sass_Inspect_Options& Eval::options()
  {
    return ctx.c_options;
  }

  struct Sass_Compiler* Eval::compiler()
  {
    return ctx.c_compiler;
  }

  EnvStack& Eval::env_stack()
  {
    return exp.env_stack;
  }

  std::vector<Sass_Callee>& Eval::callee_stack()
  {
    return ctx.callee_stack;
  }

  Value* Eval::operator()(Block* b)
  {
    Expression* val = 0;
    for (size_t i = 0, L = b->length(); i < L; ++i) {
      val = b->at(i)->perform(this);
      if (val) return Cast<Value>(val);
    }
    return Cast<Value>(val);
  }

  Value* Eval::operator()(Assignment* a)
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
          Expression* e = Cast<Expression>(env->get_global(var));
          if (!e || e->concrete_type() == Expression::NULL_VAL) {
            env->set_global(var, a->value()->perform(this));
          }
        }
        else {
          env->set_global(var, a->value()->perform(this));
        }
      }
      else {
        env->set_global(var, a->value()->perform(this));
      }
    }
    else if (a->is_default()) {
      if (env->has_lexical(var)) {
        auto cur = env;
        while (cur && cur->is_lexical()) {
          if (cur->has_local(var)) {
            if (AST_Node_Obj node = cur->get_local(var)) {
              Expression* e = Cast<Expression>(node);
              if (!e || e->concrete_type() == Expression::NULL_VAL) {
                cur->set_local(var, a->value()->perform(this));
              }
            }
            else {
              throw std::runtime_error("Env not in sync");
            }
            return nullptr;
          }
          cur = cur->parent();
        }
        throw std::runtime_error("Env not in sync");
      }
      else if (env->has_global(var)) {
        if (AST_Node_Obj node = env->get_global(var)) {
          Expression* e = Cast<Expression>(node);
          if (!e || e->concrete_type() == Expression::NULL_VAL) {
            env->set_global(var, a->value()->perform(this));
          }
        }
      }
      else if (env->is_lexical()) {
        env->set_local(var, a->value()->perform(this));
      }
      else {
        env->set_local(var, a->value()->perform(this));
      }
    }
    else {
      env->set_lexical(var, a->value()->perform(this));
    }
    return nullptr;
  }

  Value* Eval::operator()(If* i)
  {
    Expression_Obj rv;
    Env env(environment());
    env_stack().push_back(&env);
    Expression_Obj cond = i->predicate()->perform(this);
    if (!cond->is_false()) {
      rv = i->block()->perform(this);
    }
    else {
      Block_Obj alt = i->alternative();
      if (alt) rv = alt->perform(this);
    }
    env_stack().pop_back();
    return Cast<Value>(rv.detach());
  }

  // For does not create a new env scope
  // But iteration vars are reset afterwards
  Value* Eval::operator()(For* f)
  {
    std::string variable(f->variable());
    Expression_Obj low = f->lower_bound()->perform(this);
    if (low->concrete_type() != Expression::NUMBER) {
      traces.push_back(Backtrace(low->pstate()));
      throw Exception::TypeMismatch(traces, *low, "a number");
    }
    Expression_Obj high = f->upper_bound()->perform(this);
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
    env_stack().push_back(&env);
    Block_Obj body = f->block();
    Expression* val = 0;
    if (start < end) {
      if (f->is_inclusive()) ++end;
      for (double i = start;
           i < end;
           ++i) {
        Number_Obj it = SASS_MEMORY_NEW(Number, low->pstate(), i, sass_end->unit());
        env.set_local(variable, it);
        val = body->perform(this);
        if (val) break;
      }
    } else {
      if (f->is_inclusive()) --end;
      for (double i = start;
           i > end;
           --i) {
        Number_Obj it = SASS_MEMORY_NEW(Number, low->pstate(), i, sass_end->unit());
        env.set_local(variable, it);
        val = body->perform(this);
        if (val) break;
      }
    }
    env_stack().pop_back();
    return Cast<Value>(val);
  }

  // Eval does not create a new env scope
  // But iteration vars are reset afterwards
  Value* Eval::operator()(Each* e)
  {
    std::vector<std::string> variables(e->variables());
    Expression_Obj expr = e->list()->perform(this);
    Env env(environment(), true);
    env_stack().push_back(&env);
    SassListObj list;
    Map* map = nullptr;
    if (expr->concrete_type() == Expression::MAP) {
      map = Cast<Map>(expr);
    }
    else if (expr->concrete_type() != Expression::LIST) {
      list = SASS_MEMORY_NEW(SassList, expr->pstate(), {}, SASS_COMMA);
      list->append(expr);
    }
    else if (SassList * slist = Cast<SassList>(expr)) {
      list = SASS_MEMORY_NEW(SassList, expr->pstate(), {}, slist->separator());
      list->hasBrackets(slist->hasBrackets());
      for (auto item : slist->elements()) {
        if (Argument * arg = Cast<Argument>(item)) {
          list->append(arg->value());
        }
        else {
          list->append(item);
        }
      }
    }
    else if (List * l = Cast<List>(expr)) {
      list = list_to_sass_list(l);
    }
    else {
      list = Cast<SassList>(expr);
    }

    Block_Obj body = e->block();
    Expression_Obj val;

    if (map) {
      for (Expression_Obj key : map->keys()) {
        Expression_Obj value = map->at(key);

        if (variables.size() == 1) {
          SassList* variable = SASS_MEMORY_NEW(SassList,
            map->pstate(), {}, SASS_SPACE);
          variable->append(key);
          variable->append(value);
          env.set_local(variables[0], variable);
        } else {
          env.set_local(variables[0], key);
          env.set_local(variables[1], value);
        }

        val = body->perform(this);
        if (val) break;
      }
    }
    else {
      for (size_t i = 0, L = list->length(); i < L; ++i) {
        Expression* item = list->at(i);
        // unwrap value if the expression is an argument
        if (Argument* arg = Cast<Argument>(item)) item = arg->value();
        // check if we got passed a list of args (investigate)
        /* if (List * scalars = Cast<List>(item)) {
          if (variables.size() == 1) {
            Expression* var = scalars;
            env.set_local(variables[0], var);
          }
          else {
            // XXX: this is never hit via spec tests
            for (size_t j = 0, K = variables.size(); j < K; ++j) {
              Expression* res = j >= scalars->length()
                ? SASS_MEMORY_NEW(Null, expr->pstate())
                : scalars->at(j);
              env.set_local(variables[j], res);
            }
          }
        }
        else */ if (SassList * scalars = Cast<SassList>(item)) {
          if (variables.size() == 1) {
            Expression* var = scalars;
            env.set_local(variables[0], var);
          }
          else {
            // XXX: this is never hit via spec tests
            for (size_t j = 0, K = variables.size(); j < K; ++j) {
              Expression* res = j >= scalars->length()
                ? SASS_MEMORY_NEW(Null, expr->pstate())
                : scalars->at(j);
              env.set_local(variables[j], res);
            }
          }
        }
        else {
          if (variables.size() > 0) {
            env.set_local(variables.at(0), item);
            for (size_t j = 1, K = variables.size(); j < K; ++j) {
              Expression* res = SASS_MEMORY_NEW(Null, expr->pstate());
              env.set_local(variables[j], res);
            }
          }
        }
        val = body->perform(this);
        if (val) break;
      }
    }
    env_stack().pop_back();
    return Cast<Value>(val.detach());
  }

  Value* Eval::operator()(While* w)
  {
    Expression_Obj pred = w->predicate();
    Block_Obj body = w->block();
    Env env(environment(), true);
    env_stack().push_back(&env);
    Expression_Obj cond = pred->perform(this);
    while (!cond->is_false()) {
      Expression_Obj val = body->perform(this);
      if (val) {
        env_stack().pop_back();
        return Cast<Value>(val.detach());
      }
      cond = pred->perform(this);
    }
    env_stack().pop_back();
    return nullptr;
  }

  Value* Eval::operator()(Return* r)
  {
    return Cast<Value>(r->value()->perform(this));
  }

  Value* Eval::operator()(Warning* w)
  {
    Sass_Output_Style outstyle = options().output_style;
    options().output_style = NESTED;
    Expression_Obj message = w->message()->perform(this);
    Env* env = environment();

    // try to use generic function
    if (env->has("@warn[f]")) {

      // add call stack entry
      callee_stack().push_back({
        "@warn",
        w->pstate().path,
        w->pstate().line + 1,
        w->pstate().column + 1,
        SASS_CALLEE_FUNCTION,
        { env }
      });

      Definition* def = Cast<Definition>((*env)["@warn[f]"]);
      // Block_Obj          body   = def->block();
      // Native_Function func   = def->native_function();
      Sass_Function_Entry c_function = def->c_function();
      Sass_Function_Fn c_func = sass_function_get_function(c_function);

      AST2C ast2c;
      union Sass_Value* c_args = sass_make_list(1, SASS_COMMA, false);
      sass_list_set_value(c_args, 0, message->perform(&ast2c));
      union Sass_Value* c_val = c_func(c_args, c_function, compiler());
      options().output_style = outstyle;
      callee_stack().pop_back();
      sass_delete_value(c_args);
      sass_delete_value(c_val);
      return nullptr;

    }

    std::string result(unquote(message->to_sass()));
    std::cerr << "WARNING: " << result << std::endl;
    traces.push_back(Backtrace(w->pstate()));
    std::cerr << traces_to_string(traces, "         ");
    std::cerr << std::endl;
    options().output_style = outstyle;
    traces.pop_back();
    return nullptr;
  }

  Value* Eval::operator()(Error* e)
  {
    Sass_Output_Style outstyle = options().output_style;
    options().output_style = NESTED;
    Expression_Obj message = e->message()->perform(this);
    Env* env = environment();

    // try to use generic function
    if (env->has("@error[f]")) {

      // add call stack entry
      callee_stack().push_back({
        "@error",
        e->pstate().path,
        e->pstate().line + 1,
        e->pstate().column + 1,
        SASS_CALLEE_FUNCTION,
        { env }
      });

      Definition* def = Cast<Definition>((*env)["@error[f]"]);
      // Block_Obj          body   = def->block();
      // Native_Function func   = def->native_function();
      Sass_Function_Entry c_function = def->c_function();
      Sass_Function_Fn c_func = sass_function_get_function(c_function);

      AST2C ast2c;
      union Sass_Value* c_args = sass_make_list(1, SASS_COMMA, false);
      sass_list_set_value(c_args, 0, message->perform(&ast2c));
      union Sass_Value* c_val = c_func(c_args, c_function, compiler());
      options().output_style = outstyle;
      callee_stack().pop_back();
      sass_delete_value(c_args);
      sass_delete_value(c_val);
      return nullptr;

    }

    std::string result(message->to_sass());
    options().output_style = outstyle;
    error(result, e->pstate(), traces);
    return nullptr;
  }

  Value* Eval::operator()(Debug* d)
  {
    Sass_Output_Style outstyle = options().output_style;
    options().output_style = NESTED;
    Expression_Obj message = d->value()->perform(this);
    Env* env = environment();

    // try to use generic function
    if (env->has("@debug[f]")) {

      // add call stack entry
      callee_stack().push_back({
        "@debug",
        d->pstate().path,
        d->pstate().line + 1,
        d->pstate().column + 1,
        SASS_CALLEE_FUNCTION,
        { env }
      });

      Definition* def = Cast<Definition>((*env)["@debug[f]"]);
      // Block_Obj          body   = def->block();
      // Native_Function func   = def->native_function();
      Sass_Function_Entry c_function = def->c_function();
      Sass_Function_Fn c_func = sass_function_get_function(c_function);

      AST2C ast2c;
      union Sass_Value* c_args = sass_make_list(1, SASS_COMMA, false);
      sass_list_set_value(c_args, 0, message->perform(&ast2c));
      union Sass_Value* c_val = c_func(c_args, c_function, compiler());
      options().output_style = outstyle;
      callee_stack().pop_back();
      sass_delete_value(c_args);
      sass_delete_value(c_val);
      return nullptr;

    }

    std::string result(unquote(message->to_sass()));
    std::string abs_path(Sass::File::rel2abs(d->pstate().path, cwd(), cwd()));
    std::string rel_path(Sass::File::abs2rel(d->pstate().path, cwd(), cwd()));
    std::string output_path(Sass::File::path_for_console(rel_path, abs_path, d->pstate().path));
    options().output_style = outstyle;

    std::cerr << output_path << ":" << d->pstate().line+1 << " DEBUG: " << result;
    std::cerr << std::endl;
    return nullptr;
  }

  SassList* Eval::operator()(ListExpression* l)
  {
    // debug_ast(l, "ListExp IN: ");
    // regular case for unevaluated lists
    SassListObj ll = SASS_MEMORY_NEW(SassList,
      l->pstate(),
      {},
      l->separator() // ,
      // l->is_arglist(),
      // l->is_bracketed()
    );
    ll->hasBrackets(l->hasBrackets());
    for (size_t i = 0, L = l->size(); i < L; ++i) {
      ll->append(l->get(i)->perform(this));
    }
    // debug_ast(ll, "ListExp OF: ");
    return ll.detach();
  }

  Map* Eval::operator()(MapExpression* m)
  {
    std::vector<ExpressionObj> kvlist = m->kvlist();
    MapObj map = SASS_MEMORY_NEW(Map,
      m->pstate(), kvlist.size() / 2);
    for (size_t i = 0, L = kvlist.size(); i < L; i += 2)
    {
      Expression_Obj key = kvlist[i + 0]->perform(this);
      Expression_Obj val = kvlist[i + 1]->perform(this);
      *map << std::make_pair(key, val);
    }
    if (map->has_duplicate_key()) {
      traces.push_back(Backtrace(map->pstate()));
      throw Exception::DuplicateKeyError(traces, *map, *m);
    }

    return map.detach(); //  ->perform(this);
  }

  Value* Eval::operator()(Map* m)
  {
    return m;
  }

  Value* Eval::operator()(ParenthesizedExpression* ex)
  {
    // return ex->expression();
    if (ex->expression()) {
      // ex->expression(ex->expression()->perform(this));
      return Cast<Value>(ex->expression()->perform(this));
    }
    return Cast<Value>(ex);
  }

  Value* Eval::operator()(Binary_Expression* b_in)
  {
    auto rv = evalBinOp(b_in);
    // if ()
    return rv;
  }

  Value* Eval::evalBinOp(Binary_Expression* b_in)
  {

    Sass_OP op_type = b_in->optype();
    ExpressionObj lhs = b_in->left();
    ExpressionObj rhs = b_in->right();

    switch (op_type) {
    case Sass_OP::AND:
      lhs = lhs->perform(this);
      if (!*lhs) return Cast<Value>(lhs.detach());
      return Cast<Value>(rhs->perform(this));
    case Sass_OP::OR:
      lhs = lhs->perform(this);
      if (*lhs) return Cast<Value>(lhs.detach());
      return Cast<Value>(rhs->perform(this));
    case Sass_OP::EQ:
      lhs = lhs->perform(this);
      rhs = rhs->perform(this);
      return *lhs == *rhs ?
        bool_true : bool_false;
    default:
      break;
    }

    /*
    // First evaluate left side
    lhs = lex->perform(this);

    switch (node->optype()) {

    // case Sass_OP::SEQ:

    case Sass_OP::EQ:
      rhs = rhs->perform(this);
      return *lhs == *rhs ?
        bool_true : bool_false;
    case Sass_OP::NEQ:
      rhs = rhs->perform(this);
      return *lhs != *rhs ?
        bool_true : bool_false;
    case Sass_OP::GT:
      rhs = rhs->perform(this);
      return *lhs > *rhs ?
        bool_true : bool_false;
    case Sass_OP::GTE:
      rhs = rhs->perform(this);
      return *lhs >= *rhs ?
        bool_true : bool_false;
      break;
    case Sass_OP::LT:
      rhs = rhs->perform(this);
      return *lhs < *rhs ?
        bool_true : bool_false;
    case Sass_OP::LTE:
      rhs = rhs->perform(this);
      return *lhs <= *rhs ?
        bool_true : bool_false;
    case Sass_OP::ADD:
    case Sass_OP::SUB:
    case Sass_OP::MUL:
    case Sass_OP::DIV:
    case Sass_OP::MOD:
      rhs = rhs->perform(this);
      return Operators::op_numbers(op_type, *lhs, *rhs, options(), b_in->pstate())

        break;
      break;
      break;
      break;
    case Sass_OP::OR:
      break;

    }
    */

    if (op_type == Sass_OP::IESEQ) {

      lhs = lhs->perform(this);
      rhs = rhs->perform(this);

      Value_Obj v_l = Cast<Value>(lhs);
      Value_Obj v_r = Cast<Value>(rhs);

      std::string str("");
      str += v_l->to_string(options());
      if (b_in->op().ws_before) str += " ";
      str += "="; //  b_in->separator();
      if (b_in->op().ws_after) str += " ";
      str += v_r->to_string(options());
      String_Constant* val = SASS_MEMORY_NEW(String_Constant, b_in->pstate(), str);
      return val;

    }

    Binary_Expression_Obj b = b_in;
    // lhs = lhs->perform(this);
    // rhs = rhs->perform(this);
    // debug_ast(b);

    if (Cast<FunctionExpression>(lhs)) {
      b->set_delayed(false);
    }
    if (Cast<FunctionExpression>(rhs)) {
      b->set_delayed(false);
    }

    while (Variable* l_v = Cast<Variable>(lhs)) {
      b->set_delayed(false);
      lhs = operator()(l_v);
    }
    while (Variable* r_v = Cast<Variable>(rhs)) {
      b->set_delayed(false);
      rhs = operator()(r_v);
    }

    // Evaluate sub-expressions early on
    while (Binary_Expression* l_b = Cast<Binary_Expression>(lhs)) {
      lhs = operator()(l_b);
    }
    while (Binary_Expression* r_b = Cast<Binary_Expression>(rhs)) {
      rhs = operator()(r_b);
    }

    // specific types we know are final
    // handle them early to avoid overhead
    if (Number* l_n = Cast<Number>(lhs)) {
      // lhs is number and rhs is number
      if (Number* r_n = Cast<Number>(rhs)) {
        try {
          Value* rv = nullptr;
          switch (op_type) {
            case Sass_OP::EQ: return *l_n == *r_n ? bool_true : bool_false;
            case Sass_OP::NEQ: return *l_n == *r_n ? bool_false : bool_true;
            case Sass_OP::LT: return *l_n < *r_n ? bool_true : bool_false;
            case Sass_OP::GTE: return *l_n < *r_n ? bool_false : bool_true;
            case Sass_OP::LTE: return *l_n < *r_n || *l_n == *r_n ? bool_true : bool_false;
            case Sass_OP::GT: return *l_n < *r_n || *l_n == *r_n ? bool_false : bool_true;
            case Sass_OP::ADD: case Sass_OP::SUB: case Sass_OP::MUL: case Sass_OP::DIV: case Sass_OP::MOD:
              rv = Operators::op_numbers(op_type, *l_n, *r_n, options(), b_in->pstate());
            default: break;
          }
          if (op_type == Sass_OP::DIV) {
            if (Number* nr = Cast<Number>(rv)) {
              if (b->allowsSlash()) {
                nr->lhsAsSlash(l_n);
                nr->rhsAsSlash(r_n);
              }
            }
          }
          return rv;
        }
        catch (Exception::OperationError& err)
        {
          traces.push_back(Backtrace(b_in->pstate()));
          throw Exception::SassValueError(traces, b_in->pstate(), err);
        }
      }
      // lhs is number and rhs is color
      // Todo: allow to work with HSLA colors
      else if (Color* r_col = Cast<Color>(rhs)) {
        Color_RGBA_Obj r_c = r_col->toRGBA();
        try {
          switch (op_type) {
            case Sass_OP::EQ: return *l_n == *r_c ? bool_true : bool_false;
            case Sass_OP::NEQ: return *l_n == *r_c ? bool_false : bool_true;
            case Sass_OP::ADD: case Sass_OP::SUB: case Sass_OP::MUL: case Sass_OP::DIV: case Sass_OP::MOD:
              return Operators::op_number_color(op_type, *l_n, *r_c, options(), b_in->pstate());
            default: break;
          }
        }
        catch (Exception::OperationError& err)
        {
          traces.push_back(Backtrace(b_in->pstate()));
          throw Exception::SassValueError(traces, b_in->pstate(), err);
        }
      }
    }
    else if (Color* l_col = Cast<Color>(lhs)) {
      Color_RGBA_Obj l_c = l_col->toRGBA();
      // lhs is color and rhs is color
      if (Color* r_col = Cast<Color>(rhs)) {
        Color_RGBA_Obj r_c = r_col->toRGBA();
        try {
          switch (op_type) {
            case Sass_OP::EQ: return *l_c == *r_c ? bool_true : bool_false;
            case Sass_OP::NEQ: return *l_c == *r_c ? bool_false : bool_true;
            case Sass_OP::LT: return *l_c < *r_c ? bool_true : bool_false;
            case Sass_OP::GTE: return *l_c < *r_c ? bool_false : bool_true;
            case Sass_OP::LTE: return *l_c < *r_c || *l_c == *r_c ? bool_true : bool_false;
            case Sass_OP::GT: return *l_c < *r_c || *l_c == *r_c ? bool_false : bool_true;
            case Sass_OP::ADD: case Sass_OP::SUB: case Sass_OP::MUL: case Sass_OP::DIV: case Sass_OP::MOD:
              return Operators::op_colors(op_type, *l_c, *r_c, options(), b_in->pstate());
            default: break;
          }
        }
        catch (Exception::OperationError& err)
        {
          traces.push_back(Backtrace(b_in->pstate()));
          throw Exception::SassValueError(traces, b_in->pstate(), err);
        }
      }
      // lhs is color and rhs is number
      else if (Number* r_n = Cast<Number>(rhs)) {
        try {
          switch (op_type) {
            case Sass_OP::EQ: return *l_c == *r_n ? bool_true : bool_false;
            case Sass_OP::NEQ: return *l_c == *r_n ? bool_false : bool_true;
            case Sass_OP::ADD: case Sass_OP::SUB: case Sass_OP::MUL: case Sass_OP::DIV: case Sass_OP::MOD:
              return Operators::op_color_number(op_type, *l_c, *r_n, options(), b_in->pstate());
            default: break;
          }
        }
        catch (Exception::OperationError& err)
        {
          traces.push_back(Backtrace(b_in->pstate()));
          throw Exception::SassValueError(traces, b_in->pstate(), err);
        }
      }
    }

    // fully evaluate their values
    if (op_type == Sass_OP::EQ ||
        op_type == Sass_OP::NEQ ||
        op_type == Sass_OP::GT ||
        op_type == Sass_OP::GTE ||
        op_type == Sass_OP::LT ||
        op_type == Sass_OP::LTE)
    {
      lhs = lhs->perform(this);
      rhs = rhs->perform(this);

      if (Number * nr = Cast<Number>(lhs)) {
        nr->lhsAsSlash({});
        nr->rhsAsSlash({});
      }

      if (Number * nr = Cast<Number>(rhs)) {
        nr->lhsAsSlash({});
        nr->rhsAsSlash({});
      }

    }
    else {
      lhs = lhs->perform(this);
    }

    // not a logical connective, so go ahead and eval the rhs
    rhs = rhs->perform(this);
    AST_Node_Obj lu = lhs;
    AST_Node_Obj ru = rhs;

    Expression::Type l_type;
    Expression::Type r_type;

    // see if it's a relational expression
    try {
      switch(op_type) {
        case Sass_OP::EQ:  return SASS_MEMORY_NEW(Boolean, b->pstate(), Operators::eq(lhs, rhs));
        case Sass_OP::NEQ: return SASS_MEMORY_NEW(Boolean, b->pstate(), Operators::neq(lhs, rhs));
        case Sass_OP::GT:  return SASS_MEMORY_NEW(Boolean, b->pstate(), Operators::gt(lhs, rhs));
        case Sass_OP::GTE: return SASS_MEMORY_NEW(Boolean, b->pstate(), Operators::gte(lhs, rhs));
        case Sass_OP::LT:  return SASS_MEMORY_NEW(Boolean, b->pstate(), Operators::lt(lhs, rhs));
        case Sass_OP::LTE: return SASS_MEMORY_NEW(Boolean, b->pstate(), Operators::lte(lhs, rhs));
        default: break;
      }
    }
    catch (Exception::OperationError& err)
    {
      traces.push_back(Backtrace(b->pstate()));
      throw Exception::SassValueError(traces, b->pstate(), err);
    }

    l_type = lhs->concrete_type();
    r_type = rhs->concrete_type();

    // ToDo: throw error in op functions
    // ToDo: then catch and re-throw them
    Value_Obj rv;
    try {
      ParserState pstate(b->pstate());
      if (l_type == Expression::NUMBER && r_type == Expression::NUMBER) {
        Number* l_n = Cast<Number>(lhs);
        Number* r_n = Cast<Number>(rhs);
        l_n->reduce(); r_n->reduce();
        rv = Operators::op_numbers(op_type, *l_n, *r_n, options(), pstate);
      }
      /* handled above 
      else if (l_type == Expression::NUMBER && r_type == Expression::COLOR) {
        Number* l_n = Cast<Number>(lhs);
        Color_RGBA_Obj r_c = Cast<Color>(rhs)->toRGBA();
        rv = Operators::op_number_color(op_type, *l_n, *r_c, options(), pstate);
      }
      else if (l_type == Expression::COLOR && r_type == Expression::NUMBER) {
        Color_RGBA_Obj l_c = Cast<Color>(lhs)->toRGBA();
        Number* r_n = Cast<Number>(rhs);
        rv = Operators::op_color_number(op_type, *l_c, *r_n, options(), pstate);
      }
      else if (l_type == Expression::COLOR && r_type == Expression::COLOR) {
        Color_RGBA_Obj l_c = Cast<Color>(lhs)->toRGBA();
        Color_RGBA_Obj r_c = Cast<Color>(rhs)->toRGBA();
        rv = Operators::op_colors(op_type, *l_c, *r_c, options(), pstate);
      }
      */
      else {
        // this will leak if perform does not return a value!
        Value_Obj v_l = Cast<Value>(lhs);
        Value_Obj v_r = Cast<Value>(rhs);
        bool interpolant = false;
        if (op_type == Sass_OP::SUB) interpolant = false;
        // if (op_type == Sass_OP::DIV) interpolant = true;
        // check for type violations
        if (l_type == Expression::MAP || l_type == Expression::FUNCTION_VAL) {
          traces.push_back(Backtrace(v_l->pstate()));
          throw Exception::InvalidValue(traces, *v_l);
        }
        if (r_type == Expression::MAP || l_type == Expression::FUNCTION_VAL) {
          traces.push_back(Backtrace(v_r->pstate()));
          throw Exception::InvalidValue(traces, *v_r);
        }
        Value* ex = Operators::op_strings(b->op(), *v_l, *v_r, options(), pstate, !interpolant); // pass true to compress
        if (String_Constant* str = Cast<String_Constant>(ex))
        {
          if (str->concrete_type() == Expression::STRING)
          {
            String_Constant* lstr = Cast<String_Constant>(lhs);
            String_Constant* rstr = Cast<String_Constant>(rhs);
            if (op_type != Sass_OP::SUB) {
              if (String_Constant* org = lstr ? lstr : rstr)
              { str->quote_mark(org->quote_mark()); }
            }
          }
        }
        rv = ex;
      }
    }
    catch (Exception::OperationError& err)
    {
      traces.push_back(Backtrace(b->pstate()));
      // throw Exception::Base(b->pstate(), err.what());
      throw Exception::SassValueError(traces, b->pstate(), err);
    }

    return rv.detach();

  }

  Value* Eval::operator()(Unary_Expression* u)
  {
    Expression_Obj operand = u->operand()->perform(this);
    if (u->optype() == Unary_Expression::NOT) {
      Boolean* result = SASS_MEMORY_NEW(Boolean, u->pstate(), (bool)*operand);
      result->value(!result->value());
      return result;
    }
    else if (Number_Obj nr = Cast<Number>(operand)) {
      // negate value for minus unary expression
      if (u->optype() == Unary_Expression::MINUS) {
        Number_Obj cpy = SASS_MEMORY_COPY(nr);
        cpy->value( - cpy->value() ); // negate value
        return cpy.detach(); // return the copy
      }
      else if (u->optype() == Unary_Expression::SLASH) {
        std::string str = '/' + nr->to_string(options());
        return SASS_MEMORY_NEW(String_Constant, u->pstate(), str);
      }
      // nothing for positive
      return nr.detach();
    }
    else {
      // Special cases: +/- variables which evaluate to null ouput just +/-,
      // but +/- null itself outputs the string
      if (operand->concrete_type() == Expression::NULL_VAL && Cast<Variable>(u->operand())) {
        u->operand(SASS_MEMORY_NEW(String_Quoted, u->pstate(), ""));
      }
      // Never apply unary opertions on colors @see #2140
      else if (Color* color = Cast<Color>(operand)) {
        // Use the color name if this was eval with one
        if (color->disp().length() > 0) {
          operand = SASS_MEMORY_NEW(String_Constant, operand->pstate(), color->disp());
          u->operand(operand);
        }
      }
      else {
        u->operand(operand);
      }

      return SASS_MEMORY_NEW(String_Quoted,
                             u->pstate(),
                             u->inspect());
    }
    // unreachable
    return Cast<Value>(u);
  }

  Expression* Eval::operator()(FunctionExpression* c)
  {

    // NESTING_GUARD(_recursion)

    if (traces.size() > Constants::MaxCallStack) {
        // XXX: this is never hit via spec tests
        std::ostringstream stm;
        stm << "Stack depth exceeded max of " << Constants::MaxCallStack;
        error(stm.str(), c->pstate(), traces);
    }

    if (Interpolation* itpl = Cast<Interpolation>(c->sname())) {
      std::string plain(itpl->getPlainString());
      if (plain.empty()) {
        std::string evaluated_name(interpolationToValue(itpl, true, true));
        if (!c->arguments()->empty()) {
          Argument* arg = c->arguments()->last();
          arg->is_rest_argument(false);
        }
        Expression_Obj evaluated_args = c->arguments()->perform(this);
        std::string str(evaluated_name);
        str += evaluated_args->to_string();
        return (SASS_MEMORY_NEW(String_Constant, c->pstate(), str));
        return Cast<Value>(c);
      }
     //  return ;
    }

    std::string name(c->name());
    std::string full_name(name + "[f]");

    // we make a clone here, need to implement that further
    Arguments_Obj args = c->arguments();

    Env* env = environment();
    if (!c->func() && !env->has(full_name) /* || (!c->via_call() && Prelexer::re_special_fun(name.c_str()))*/) {
      if (!env->has("*[f]")) {
        /* check now done earlier
        for (Argument_Obj arg : args->elements()) {
          if (List_Obj ls = Cast<List>(arg->value())) {
            if (ls->size() == 0) error("() isn't a valid CSS value.", c->pstate(), traces);
          }
          if (SassList_Obj ls = Cast<SassList>(arg->value())) {
            if (ls->length() == 0) error("() isn't a valid CSS value.", c->pstate(), traces);
          }
        }
        */
        args = Cast<Arguments>(args->perform(this));
        if (!args->empty()) args->last()->is_rest_argument(false);
        FunctionExpressionObj lit = SASS_MEMORY_NEW(FunctionExpression,
                                             c->pstate(),
                                             c->name(),
                                             args,
                                             c->ns());
        if (args->hasNamedArgument()) {
          // error("Plain CSS function " + c->name() + " doesn't support keyword arguments", c->pstate(), traces);
          error("Plain CSS functions don't support keyword arguments.", c->pstate(), traces);
        }
        String_Quoted* str = SASS_MEMORY_NEW(String_Quoted,
                                             c->pstate(),
                                             lit->to_string(options()));
        return str;
      } else {
        // call generic function
        full_name = "*[f]";
      }
    }

    // further delay for calls
    if (full_name != "call[f]") {
      if (Number * nr = Cast<Number>(args)) {
        nr->lhsAsSlash({});
        nr->rhsAsSlash({});
      }
    }
    if (full_name != "if[f]") {
      args = Cast<Arguments>(args->perform(this));
    }
    Definition* def = Cast<Definition>((*env)[full_name]);

    if (c->func()) def = c->func()->definition();

    if (def->is_overload_stub()) {

      std::stringstream ss;
      size_t L = args->length();
      // account for rest arguments
      if (args->hasRestArgument() && args->length() > 0) {
        // get the rest arguments list
        List* rest = Cast<List>(args->last()->value());
        // arguments before rest argument plus rest
        if (rest) L += rest->length() - 1;
      }
      ss << full_name << L;
      std::string resolved_name(ss.str());
      if (!env->has(resolved_name)) resolved_name = full_name + "*";
      // std::cerr << "CALLING " << resolved_name << "\n";
      if (!env->has(resolved_name)) {
        size_t LP = def->defaultParams();
        std::stringstream msg;
        msg << "Only " << LP << " ";
        msg << (LP == 1 ? "argument" : "arguments");
        msg << " allowed, but " << L << " ";
        msg << (L == 1 ? "was" : "were");
        msg << " passed.";
        error(msg.str(), c->pstate(), traces);
      }
      def = Cast<Definition>((*env)[resolved_name]);

    }

    Expression_Obj     result = c;
    Block_Obj          body   = def->block();
    Native_Function func   = def->native_function();
    Sass_Function_Entry c_function = def->c_function();

    if (c->is_css()) {
      return SASS_MEMORY_NEW(String_Constant, result->pstate(), result->to_css());
      std::cerr << "return css\n";
      return Cast<Value>(result.detach());
    }

    Parameters_Obj params = def->parameters();
    Env fn_env(def->environment());
    env_stack().push_back(&fn_env);

    if (func || body) {
      bind(std::string("Function"), c->name(), params, args, &fn_env, this, traces);
      std::string msg(", in function `" + c->name() + "`");
      traces.push_back(Backtrace(c->pstate(), msg));
      callee_stack().push_back({
        c->name().c_str(),
        c->pstate().path,
        c->pstate().line + 1,
        c->pstate().column + 1,
        SASS_CALLEE_FUNCTION,
        { env }
      });

      // eval the body if user-defined or special, invoke underlying CPP function if native
      if (body) {
        bool was_in_mixin = env->has_global("is_in_mixin");
        env->del_global("is_in_mixin");
        result = body->perform(this);
        if (was_in_mixin) env->set_global("is_in_mixin", bool_true);
      }
      else if (func) {
        result = func(fn_env, *env, ctx, def->signature(), c->pstate(), traces, exp.getSelectorStack(), exp.originalStack);
      }
      if (!result) {
        error(std::string("Function ") + c->name() + " finished without @return", c->pstate(), traces);
      }
      callee_stack().pop_back();
      traces.pop_back();
    }

    // else if it's a user-defined c function
    // convert call into C-API compatible form
    else if (c_function) {
      Sass_Function_Fn c_func = sass_function_get_function(c_function);
      if (full_name == "*[f]") {
        String_Quoted_Obj str = SASS_MEMORY_NEW(String_Quoted, c->pstate(), c->name());
        Arguments_Obj new_args = SASS_MEMORY_NEW(Arguments, c->pstate());
        new_args->append(SASS_MEMORY_NEW(Argument, c->pstate(), str));
        new_args->concat(args);
        args = new_args;
      }

      // populates env with default values for params
      std::string ff(c->name());
      bind(std::string("Function"), c->name(), params, args, &fn_env, this, traces);
      std::string msg(", in function `" + c->name() + "`");
      traces.push_back(Backtrace(c->pstate(), msg));
      callee_stack().push_back({
        c->name().c_str(),
        c->pstate().path,
        c->pstate().line + 1,
        c->pstate().column + 1,
        SASS_CALLEE_C_FUNCTION,
        { env }
      });

      AST2C ast2c;
      union Sass_Value* c_args = sass_make_list(params->length(), SASS_COMMA, false);
      for(size_t i = 0; i < params->length(); i++) {
        Parameter_Obj param = params->at(i);
        std::string key = param->name();
        AST_Node_Obj node = fn_env.get_local(key);
        Expression_Obj arg = Cast<Expression>(node);
        sass_list_set_value(c_args, i, arg->perform(&ast2c));
      }
      union Sass_Value* c_val = c_func(c_args, c_function, compiler());
      if (sass_value_get_tag(c_val) == SASS_ERROR) {
        std::string message("error in C function " + c->name() + ": " + sass_error_get_message(c_val));
        sass_delete_value(c_val);
        sass_delete_value(c_args);
        error(message, c->pstate(), traces);
      } else if (sass_value_get_tag(c_val) == SASS_WARNING) {
        std::string message("warning in C function " + c->name() + ": " + sass_warning_get_message(c_val));
        sass_delete_value(c_val);
        sass_delete_value(c_args);
        error(message, c->pstate(), traces);
      }
      result = c2ast(c_val, traces, c->pstate());

      callee_stack().pop_back();
      traces.pop_back();
      sass_delete_value(c_args);
      if (c_val != c_args)
        sass_delete_value(c_val);
    }
    else {
      std::cerr << "return a pure ref\n";
    }

    // link back to function definition
    // only do this for custom functions
    if (result->pstate().file == std::string::npos)
      result->pstate(c->pstate());

    if (Number* nr = Cast<Number>(result)) {
      nr->lhsAsSlash({});
      nr->rhsAsSlash({});
    }

    result = result->perform(this);
    env_stack().pop_back();
    return result.detach();
  }

  Value* Eval::operator()(Variable* v)
  {
    Expression_Obj value;
    Env* env = environment();
    std::string name(v->name());
    EnvResult rv(env->find(name));
    if (rv.found) value = rv.it->second.ptr();
    else error("Undefined variable.", v->pstate(), traces);
    if (Argument* arg = Cast<Argument>(value)) value = arg->value();
    // debug_ast(value, "RES: ");
    if (Number * nr = Cast<Number>(value)) {
      nr->lhsAsSlash({});
      nr->rhsAsSlash({});
    }

    value = value->perform(this);
    rv.it->second = value;
    // debug_ast(value);
    return Cast<Value>(value.detach());
  }

  Value* Eval::operator()(Color_RGBA* c)
  {
    return c;
  }

  Value* Eval::operator()(Color_HSLA* c)
  {
    return c;
  }

  Value* Eval::operator()(Number* n)
  {
    return n;
  }

  Value* Eval::operator()(Boolean* b)
  {
    return b;
  }

  Value* Eval::operator()(StringLiteral* s)
  {
    return s;
  }

  std::string Eval::joinStrings(
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
  std::string Eval::performInterpolation(InterpolationObj interpolation, bool warnForColor)
  {

    std::vector<std::string> results;
    for (auto value : interpolation->elements()) {

      if (StringLiteral * str = Cast<StringLiteral>(value)) {
        results.push_back(str->text());
        continue;
      }

      Expression* expression = value;
      ExpressionObj result = expression->perform(this);

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

      // return _serialize(result, expression, quote: false);
      // ToDo: quote = false

      results.push_back(result->to_css(options()));

    }

    return joinStrings(results, "");

  }

  /// Evaluates [interpolation] and wraps the result in a [CssValue].
  ///
  /// If [trim] is `true`, removes whitespace around the result. If
  /// [warnForColor] is `true`, this will emit a warning for any named color
  /// values passed into the interpolation.
  std::string Eval::interpolationToValue(InterpolationObj interpolation,
    bool trim, bool warnForColor)
  {
    std::string result = performInterpolation(interpolation, warnForColor);
    if (trim) { Util::ascii_str_trim(result); }
    return result;
    // return CssValue(trim ? trimAscii(result, excludeEscape: true) : result,
    //   interpolation.span);
  }


  String_Constant* Eval::operator()(Interpolation* s)
  {
    return SASS_MEMORY_NEW(String_Constant, "[pstate]",
      interpolationToValue(s, false, true), true);
  }

  Value* Eval::operator()(StringExpression* node)
  {

    // debug_ast(node, "STREX ");
   //  std::cerr << "IN visitStringExpression\n";
    // Don't use [performInterpolation] here because we need to get
    // the raw text from strings, rather than the semantic value.
    InterpolationObj itpl = node->text();
    std::vector<std::string> strings;
    for (ExpressionObj item : itpl->elements()) {
      if (StringLiteralObj lit = Cast<StringLiteral>(item)) {
        // std::cerr << "APPEND STRING1 [" << lit->text() << "]\n";
        strings.push_back(lit->text());
      }
      else if (String_Constant * lit = Cast<String_Constant>(item)) {
        // std::cerr << "APPEND STRING1 [" << lit->value() << "]\n";
        strings.push_back(lit->value());
      }
        // else if (StringExpression * ex = Cast<StringExpression>(item)) {
      //   auto qwe = ex->perform(this);
      //   strings.push_back(qwe->to_css(options()));
      // }
      else {
        // std::cerr << "perform item\n";
        ExpressionObj result = item->perform(this);
        // std::cerr << "performed item\n";
        if (StringLiteral * lit = Cast<StringLiteral>(result)) {
          // std::cerr << "APPEND STRING [" << lit->text() << "]\n";
          strings.push_back(lit->text());
        }
        else if (String_Constant * lit = Cast<String_Constant>(result)) {
          // std::cerr << "APPEND STRING [" << lit->value() << "]\n";
          strings.push_back(lit->value());
        }
        else if (Cast<Null>(result)) {
        }
        else {
          // debug_ast(result);
          // std::cerr << "buffer append5 [" << debug_vec(result) << "]\n";
          std::string css = serialize(result);
          // std::cerr << "CSSIZED [" << css << "]\n";
          strings.push_back(css);
        }
      }
    }

    // ToDo: quoting shit
    std::string joined(joinStrings(strings, ""));
    // Util::ascii_str_ltrim(joined);

    // std::cerr << "GOT [" << joined << "]\n";
    if (node->hasQuotes()) {
      auto qwe = SASS_MEMORY_NEW(String_Quoted, "[pstate]", joined, '*', false, true, true, true);
      // qwe->quote_mark('*');
      return qwe;
    }
    else {
      auto asd = SASS_MEMORY_NEW(String_Constant, "[pstate]", joined, true);
      return asd;
    }

  }

  Value* Eval::operator()(String_Constant* s)
  {
    return s;
  }

  Value* Eval::operator()(String_Quoted* s)
  {
    String_Quoted* str = SASS_MEMORY_NEW(String_Quoted, s->pstate(), "");
    str->value(s->value());
    str->quote_mark(s->quote_mark());
    return str;
  }

  /// Evaluates [expression] and calls `toCssString()` and wraps a
/// [SassScriptException] to associate it with [span].
  std::string Eval::_evaluateToCss(Expression* expression, bool quote)
  {
    Value* evaled = Cast<Value>(expression->perform(this));
    return _serialize(evaled, quote);
  }

  /// Calls `value.toCssString()` and wraps a [SassScriptException] to associate
/// it with [nodeWithSpan]'s source span.
///
/// This takes an [AstNode] rather than a [FileSpan] so it can avoid calling
/// [AstNode.span] if the span isn't required, since some nodes need to do
/// real work to manufacture a source span.
  std::string Eval::_serialize(Value* value, bool quote)
  {
    // _addExceptionSpan(nodeWithSpan, () = > value.toCssString(quote: quote));
    return value->to_css();
  }


  /// Evlauates [condition] and converts it to a plain CSS string, with
/// parentheses if necessary.
///
/// If [operator] is passed, it's the operator for the surrounding
/// [SupportsOperation], and is used to determine whether parentheses are
/// necessary if [condition] is also a [SupportsOperation].
  std::string Eval::_parenthesize(SupportsCondition* condition, SupportsOperation::Operand* operand) {
    SupportsNegation* negation = Cast<SupportsNegation>(condition);
    SupportsOperation* operation = Cast<SupportsOperation>(condition);
    if (negation ||
      (operation &&
      (operand == nullptr || *operand != operation->operand()))) {
      return "(" + _visitSupportsCondition(condition) + ")";
    }
    else {
      return _visitSupportsCondition(condition);
    }
  }

  std::string Eval::_visitSupportsCondition(SupportsCondition* condition)
  {
    if (auto operation = Cast<SupportsOperation>(condition)) {
      std::stringstream strm;
      SupportsOperation::Operand operand = operation->operand();
      strm << _parenthesize(operation->left(), &operand);
      strm << " " << (operand == SupportsOperation::AND ? "and " : "or ");
      strm << _parenthesize(operation->right(), &operand);
      return strm.str();
    }
    else if (auto negation = Cast<SupportsNegation>(condition)) {
      std::stringstream strm;
      strm << "not ";
      strm << _parenthesize(negation->condition(), nullptr);
      return strm.str();
    }
    else if (auto interpolation = Cast<SupportsInterpolation>(condition)) {
      return _evaluateToCss(interpolation->value(), false);
    }
    else if (auto declaration = Cast<SupportsDeclaration>(condition)) {
      std::stringstream strm;
      strm << "(" << _evaluateToCss(declaration->feature()) << ":";
      strm << " " << _evaluateToCss(declaration->value()) << ")";
      return strm.str();
    }
    else {
      return "";
    }

  }

  Value* Eval::operator()(SupportsRule* c)
  {

    // if (_declarationName != null) {
    //   throw _exception(
    //     "Supports rules may not be used within nested declarations.",
    //     node.span);
    // }

    operator()(c->condition());

    return nullptr;

  }

  String* Eval::operator()(SupportsCondition* condition)
  {

    return SASS_MEMORY_NEW(String_Constant,
      condition->pstate(), _visitSupportsCondition(condition));
    /*
        if (condition is SupportsOperation) {
      return "${_parenthesize(condition.left, condition.operator)} "
          "${condition.operator} "
          "${_parenthesize(condition.right, condition.operator)}";
    } else if (condition is SupportsNegation) {
      return "not ${_parenthesize(condition.condition)}";
    } else if (condition is SupportsInterpolation) {
      return _evaluateToCss(condition.expression, quote: false);
    } else if (condition is SupportsDeclaration) {
      return "(${_evaluateToCss(condition.name)}: "
          "${_evaluateToCss(condition.value)})";
    } else {
      return null;
    }

    */


  }

  // needs SupportsRule?
  /*
  Expression* Eval::operator()(SupportsOperation* c)
  {
    Expression* left = c->left()->perform(this);
    Expression* right = c->right()->perform(this);
    SupportsOperation* cc = SASS_MEMORY_NEW(SupportsOperation,
                                 c->pstate(),
                                 Cast<SupportsCondition>(left),
                                 Cast<SupportsCondition>(right),
                                 c->operand());
    return cc;
    // return SASS_MEMORY_NEW(String_Constant, cc->pstate(), cc->to_css());
  }

  Expression* Eval::operator()(SupportsNegation* c)
  {
    Expression* condition = c->condition()->perform(this);
    SupportsNegation* cc = SASS_MEMORY_NEW(SupportsNegation,
                                 c->pstate(),
                                 Cast<SupportsCondition>(condition));
    return cc;
  }
  
  Expression* Eval::operator()(SupportsDeclaration* c)
  {
    Expression* feature = c->feature()->perform(this);
    Expression* value = c->value()->perform(this);
    SupportsDeclaration* cc = SASS_MEMORY_NEW(SupportsDeclaration,
                              c->pstate(),
                              feature,
                              value);
    return cc;
  }

  Expression* Eval::operator()(SupportsInterpolation* c)
  {
    Expression* value = c->value()->perform(this);
    SupportsInterpolation* cc = SASS_MEMORY_NEW(SupportsInterpolation,
                            c->pstate(),
                            value);
    return cc;
  }
  */

  Expression* Eval::operator()(At_Root_Query* e)
  {
    Expression_Obj feature = e->feature();
    feature = (feature ? feature->perform(this) : 0);
    Expression_Obj value = e->value();
    value = (value ? value->perform(this) : 0);
    Expression* ee = SASS_MEMORY_NEW(At_Root_Query,
                                     e->pstate(),
                                     Cast<String>(feature),
                                     value);
    return ee;
  }

  Value* Eval::operator()(Null* n)
  {
    return n;
  }

  Expression* Eval::operator()(Argument* a)
  {
    Expression_Obj val = a->value()->perform(this);
    bool is_rest_argument = a->is_rest_argument();
    bool is_keyword_argument = a->is_keyword_argument();

    if (a->is_rest_argument()) {
      if (val->concrete_type() == Expression::MAP) {
        is_rest_argument = false;
        is_keyword_argument = true;
      }
      else if(val->concrete_type() != Expression::LIST) {
        SassList_Obj wrapper = SASS_MEMORY_NEW(SassList,
                                        val->pstate(),
          {},
                                        SASS_COMMA);
        wrapper->append(val);
        val = wrapper;
      }
    }
    return SASS_MEMORY_NEW(Argument,
                           a->pstate(),
                           val,
                           a->name(),
                           is_rest_argument,
                           is_keyword_argument);
  }

  Expression* Eval::operator()(Arguments* a)
  {
    Arguments_Obj aa = SASS_MEMORY_NEW(Arguments, a->pstate());
    if (a->length() == 0) return aa.detach();
    for (size_t i = 0, L = a->length(); i < L; ++i) {
      // std::cerr << "eval argument " << i << "\n";
      Expression_Obj rv = (*a)[i]->perform(this);
      Argument* arg = Cast<Argument>(rv);
      if (!(arg->is_rest_argument() || arg->is_keyword_argument())) {
        aa->append(arg);
      }
    }

    if (a->hasRestArgument()) {
      Expression_Obj rest = a->get_rest_argument()->perform(this);
      Expression_Obj splat = Cast<Argument>(rest)->value()->perform(this);

      Sass_Separator separator = SASS_COMMA;
      List* ls = Cast<List>(splat);
      SassList* lsl = Cast<SassList>(splat);
      Map* ms = Cast<Map>(splat);

      List_Obj arglist = SASS_MEMORY_NEW(List,
                                      splat->pstate(),
                                      0,
                                      ls ? ls->separator() : lsl ? lsl->separator() : separator,
                                      true);

      if (lsl) {
        for (size_t i = 0; i < lsl->length(); i++) {
          arglist->append(lsl->get(i));
        }
      }
      else if (ls && ls->is_arglist()) {
        arglist->concat(ls);
      } else if (ms) {
        aa->append(SASS_MEMORY_NEW(Argument, splat->pstate(), ms, "", false, true));
      } else if (ls) {
        arglist->concat(ls);
      } else {
        arglist->append(splat);
      }
      if (arglist->length()) {
        aa->append(SASS_MEMORY_NEW(Argument, splat->pstate(), arglist, "", true));
      }
    }

    if (a->hasKeywordArgument()) {
      Expression_Obj rv = a->get_keyword_argument()->perform(this);
      Argument* rvarg = Cast<Argument>(rv);
      Expression_Obj kwarg = rvarg->value()->perform(this);
      aa->append(SASS_MEMORY_NEW(Argument, kwarg->pstate(), kwarg, "", false, true));
    }
    // debug_ast(aa, "AAA: ");
    return aa.detach();
  }

  Value* Eval::operator()(LoudComment* c)
  {
    return nullptr;
  }

  Value* Eval::operator()(SilentComment* c)
  {
    return nullptr;
  }

  Value* Eval::operator()(Parent_Reference* p)
  {
    if (SelectorListObj parents = exp.original()) {
      return Listize::listize(parents);
    } else {
      return SASS_MEMORY_NEW(Null, p->pstate());
    }
  }

}
