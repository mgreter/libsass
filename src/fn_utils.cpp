// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"
#include "ast.hpp"

#include "fn_utils.hpp"
#include "util_string.hpp"
#include "parser_scss.hpp"
#include "parser_selector.hpp"

namespace Sass {

  // Returns whether [number1] and [number2] are equal within [epsilon].
  bool fuzzyEquals(double number1, double number2, double epsilon) {
    return abs(number1 - number2) < epsilon;
  }

  // Returns whether [number1] is less than [number2], and not [fuzzyEquals].
  bool fuzzyLessThan(double number1, double number2, double epsilon) {
    return number1 < number2 && !fuzzyEquals(number1, number2, epsilon);
  }

  // Returns whether [number1] is less than [number2], or [fuzzyEquals].
  bool fuzzyLessThanOrEquals(double number1, double number2, double epsilon) {
    return number1 < number2 || fuzzyEquals(number1, number2, epsilon);
  }

  // Returns whether [number1] is greater than [number2], and not [fuzzyEquals].
  bool fuzzyGreaterThan(double number1, double number2, double epsilon) {
    return number1 > number2 && !fuzzyEquals(number1, number2, epsilon);
  }

  // Returns whether [number1] is greater than [number2], or [fuzzyEquals].
  bool fuzzyGreaterThanOrEquals(double number1, double number2, double epsilon) {
    return number1 > number2 || fuzzyEquals(number1, number2, epsilon);
  }

  // Returns whether [number] is [fuzzyEquals] to an integer.
  bool fuzzyIsInt(double number, double epsilon) {
    // if (number is int) return true;

    // Check against 0.5 rather than 0.0 so that we catch numbers that
    // are both very slightly above an integer, and very slightly below.
    return fuzzyEquals(fmod(abs(number - 0.5), 1.0), 0.5, epsilon);
  }

  /// If [number] is an integer according to [fuzzyIsInt], returns it as an
  /// [int].
  ///
  /// Otherwise, returns `NAN`.
  long fuzzyAsInt(double number, double epsilon) {
    return lround(number);
    // return fuzzyIsInt(number, epsilon) ?
    //   lround(number) : NAN;
  }

  /// Rounds [number] to the nearest integer.
  ///
  /// This rounds up numbers that are [fuzzyEquals] to `X.5`.
  long fuzzyRound(double number, double epsilon) {
    // If the number is within epsilon of X.5,
    // round up (or down for negative numbers).
    if (number > 0) {
      return lround(fuzzyLessThan(
        fmod(number, 1.0), 0.5, epsilon)
        ? floor(number) : ceill(number));
    }
    return lround(fuzzyLessThanOrEquals(
      fmod(number, 1.0), 0.5, epsilon)
      ? floorl(number) : ceill(number));
  }

  /// Returns [number] if it's within [min] and [max], or `NAN` if it's not.
  ///
  /// If [number] is [fuzzyEquals] to [min] or [max], it's clamped to the
  /// appropriate value.
  double fuzzyCheckRange(double number, double min, double max, double epsilon) {
    if (fuzzyEquals(number, min, epsilon)) return min;
    if (fuzzyEquals(number, max, epsilon)) return max;
    if (number > min && number < max) return number;
    return NAN;
  }

  /// Throws a [RangeError] if [number] isn't within [min] and [max].
  ///
  /// If [number] is [fuzzyEquals] to [min] or [max], it's clamped to the
  /// appropriate value. [name] is used in error reporting.
  double fuzzyAssertRange(double number, double min, double max, double epsilon, std::string name) {
    double result = fuzzyCheckRange(number, min, max, epsilon);
    if (result != NAN) return result;
    throw std::range_error("must be between $min and $max.");
    // throw RangeError.value(number, name, "must be between $min and $max.");
  }


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

  UserDefinedCallable::UserDefinedCallable(
    ParserState pstate) :
    Callable(pstate) {}

  PlainCssCallable::PlainCssCallable(
    ParserState pstate) :
    Callable(pstate) {}

  BuiltInCallable::BuiltInCallable(
    std::string name,
    ArgumentDeclaration* parameters,
    SassFnSig callback) :
    Callable("[BUILTIN]"),
    name_(name),
    parameters_(parameters),
    callback_(callback)
  {
  }

}
