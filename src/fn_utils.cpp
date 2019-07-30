// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"
#include "ast.hpp"

#include "fn_utils.hpp"
#include "util_string.hpp"
#include "parser_scss.hpp"
#include "parser_selector.hpp"

namespace Sass {

  Definition* make_native_function(Signature sig, Native_Function func, Context& ctx)
  {
    ScssParser p2(ctx, sig, 0, 0);
    std::string name(p2.identifier());
    ParametersObj params = p2.parseArgumentDeclaration();
    return SASS_MEMORY_NEW(Definition,
                          ParserState("[built-in function]"),
                          sig, name, params, func, false);
  }

  Definition* make_c_function(Sass_Function_Entry c_func, Context& ctx)
  {
    const char* sig = sass_function_get_signature(c_func);
    ScssParser p2(ctx, sig, 0, 0);
    std::string name;
    if (p2.scanner.scanChar(Character::$at)) {
      name = "@"; // allow @warn etc.
    }
    name += p2.identifier();
    Parameters_Obj params = p2.parseArgumentDeclaration();
    return SASS_MEMORY_NEW(Definition,
                          ParserState("[c function]"),
                          sig, name, params, c_func);
  }

  namespace Functions {

    std::string function_name(Signature sig)
    {
      std::string str(sig);
      return str.substr(0, str.find('('));
    }

    std::string envValueToString(Env& env, const std::string& name)
    {
      return env[name]->to_string();
    }

    Map* get_arg_m(const std::string& argname, Env& env, Signature sig, ParserState pstate, Backtraces traces)
    {
      AST_Node* value = env[argname];
      if (Map* map = Cast<Map>(value)) return map;
      List* list = Cast<List>(value);
      SassList* slist = Cast<SassList>(value);
      if (list && list->length() == 0) {
        return SASS_MEMORY_NEW(Map, pstate, 0);
      }
      if (slist && slist->empty()) {
        return SASS_MEMORY_NEW(Map, pstate, 0);
      }
      return get_arg<Map>(argname, env, sig, pstate, traces, "a map");
    }

    double get_arg_r(const std::string& argname, Env& env, Signature sig, ParserState pstate, Backtraces traces, double lo, double hi, std::string unit)
    {
      Number* val = get_arg<Number>(argname, env, sig, pstate, traces, "a number");
      Number tmpnr(val);
      tmpnr.reduce();
      double v = tmpnr.value();
      if (!(lo <= v && v <= hi)) {
        std::stringstream msg;
        if (!argname.empty()) msg << argname << ": ";
        msg << "Expected " << v << unit << " to be within ";
        msg << lo << unit << " and " << hi << unit << ".";
        error(msg.str(), pstate, traces);
      }
      return v;
    }

    Number* get_arg_n(const std::string& argname, Env& env, Signature sig, ParserState pstate, Backtraces traces)
    {
      Number* val = get_arg<Number>(argname, env, sig, pstate, traces, "a number");
      val = SASS_MEMORY_COPY(val);
      val->reduce();
      return val;
    }

    double get_arg_val(const std::string& argname, Env& env, Signature sig, ParserState pstate, Backtraces traces)
    {
      Number* val = get_arg<Number>(argname, env, sig, pstate, traces, "a number");
      Number tmpnr(val);
      tmpnr.reduce();
      return tmpnr.value();
    }

    SelectorListObj get_arg_sels(const std::string& argname, Env& env, Signature sig, ParserState pstate, Backtraces traces, Context& ctx) {
      Expression_Obj exp = ARG(argname, Expression, "an expression");
      if (exp->concrete_type() == Expression::NULL_VAL) {
        std::stringstream msg;
        msg << argname << ": null is not a valid selector: it must be a string,\n";
        msg << "a list of strings, or a list of lists of strings for `" << function_name(sig) << "'";
        error(msg.str(), exp->pstate(), traces);
      }
      if (String_Constant* str = Cast<String_Constant>(exp)) {
        str->quote_mark(0);
      }
      std::string exp_src = exp->to_string(ctx.c_options);
      // std::cerr << "Hey " << exp_src << "\n";
      ParserState exstate(exp->pstate());
      char* str = sass_copy_c_string(exp_src.c_str());
      ctx.strings.push_back(str);
      SelectorParser p2(ctx, str, exstate.path, exstate.file);
      p2._allowParent = false;
      return p2.parse();
    }

    CompoundSelectorObj get_arg_sel(const std::string& argname, Env& env, Signature sig, ParserState pstate, Backtraces traces, Context& ctx) {
      Expression_Obj exp = ARG(argname, Expression, "an expression");
      if (exp->concrete_type() == Expression::NULL_VAL) {
        std::stringstream msg;
        if (!argname.empty()) {
          msg << argname << ": ";
        }
        msg << "null is not a valid selector: it must be a string,\n" 
          << "a list of strings, or a list of lists of strings.";
        error(msg.str(), exp->pstate(), traces);
      }
      if (String_Constant* str = Cast<String_Constant>(exp)) {
        str->quote_mark(0);
      }
      std::string exp_src = exp->to_string(ctx.c_options);
      ParserState exstate(exp->pstate());
      char* str = sass_copy_c_string(exp_src.c_str());
      ctx.strings.push_back(str);
      SelectorParser p2(ctx, str, exstate.path, exstate.file);
      p2._allowParent = false;
      SelectorListObj sel_list = p2.parse();
      if (sel_list->length() == 0) return {};
      return sel_list->first()->first();
    }

    int assertInt(const std::string& name, double value, ParserState pstate, Backtraces traces)
    {
      if (std::fabs(trunc(value) - value) < NUMBER_EPSILON) {
        return (int) value;
      }
      std::stringstream err;
      if (!name.empty()) err << name << ": ";
      err << value << " is not an int.";
      error(err.str(), pstate, traces);
      return 0;
    }


  }

  Callable::Callable(
    ParserState pstate) :
    SassNode(pstate) {}

  PlainCssCallable::PlainCssCallable(
    ParserState pstate, std::string name) :
    Callable(pstate), name_(name) {}

  bool PlainCssCallable::operator==(const Callable& rhs) const
  {
    if (const PlainCssCallable * plain = Cast<PlainCssCallable>(&rhs)) {
      return this == plain;
    }
    return false;
  }

  BuiltInCallable::BuiltInCallable(
    std::string name,
    ArgumentDeclaration* parameters,
    SassFnSig callback) :
    Callable("[BUILTIN]"),
    name_(name),
    // Create a single entry in overloaded function
    overloads_(1, std::make_pair(parameters, callback))
  {
  }

  BuiltInCallable::BuiltInCallable(
    std::string name,
    SassFnPairs overloads) :
    Callable("[BUILTIN]"),
    name_(name),
    overloads_(overloads)
  {
  }

  SassFnPair BuiltInCallable::callbackFor(
    size_t positional, NormalizedMap<ValueObj> names)
  {
    for (SassFnPair& pair : overloads_) {
      if (pair.first->matches(positional, names)) {
        return pair;
      }
    }
    return overloads_.back();
  }

  bool BuiltInCallable::operator==(const Callable& rhs) const
  {
    if (const BuiltInCallable* builtin = Cast<BuiltInCallable>(&rhs)) {
      return this == builtin;
    }
    return false;
  }


}
