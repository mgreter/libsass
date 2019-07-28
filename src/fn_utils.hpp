#ifndef SASS_FN_UTILS_H
#define SASS_FN_UTILS_H

// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include "units.hpp"
#include "backtrace.hpp"
#include "environment.hpp"
#include "ast_fwd_decl.hpp"
#include "error_handling.hpp"

namespace Sass {

  // Returns whether [number1] and [number2] are equal within [epsilon].
  inline bool fuzzyEquals(double number1, double number2, double epsilon) {
    return abs(number1 - number2) < epsilon;
  }

  /// `1 / epsilon`, cached since [math.pow] may not be computed at compile-time
  /// and thus this probably won't be constant-folded.
  // final _inverseEpsilon = 1 / epsilon;

  /// Returns a hash code for [number] that matches [fuzzyEquals].
  // int fuzzyHashCode(num number) = > (number * _inverseEpsilon).round().hashCode;

  // Returns whether [number1] is less than [number2], and not [fuzzyEquals].
  inline bool fuzzyLessThan(double number1, double number2, double epsilon) {
    return number1 < number2 && !fuzzyEquals(number1, number2, epsilon);
  }

  // Returns whether [number1] is less than [number2], or [fuzzyEquals].
  inline bool fuzzyLessThanOrEquals(double number1, double number2, double epsilon) {
    return number1 < number2 || fuzzyEquals(number1, number2, epsilon);
  }

  // Returns whether [number1] is greater than [number2], and not [fuzzyEquals].
  inline bool fuzzyGreaterThan(double number1, double number2, double epsilon) {
    return number1 > number2 && !fuzzyEquals(number1, number2, epsilon);
  }

  // Returns whether [number1] is greater than [number2], or [fuzzyEquals].
  inline bool fuzzyGreaterThanOrEquals(double number1, double number2, double epsilon) {
    return number1 > number2 || fuzzyEquals(number1, number2, epsilon);
  }

  // Returns whether [number] is [fuzzyEquals] to an integer.
  inline bool fuzzyIsInt(double number, double epsilon) {
    // if (number is int) return true;

    // Check against 0.5 rather than 0.0 so that we catch numbers that
    // are both very slightly above an integer, and very slightly below.
    return fuzzyEquals(fmod(abs(number - 0.5), 1.0), 0.5, epsilon);
  }


  /// If [number] is an integer according to [fuzzyIsInt], returns it as an
/// [int].
///
/// Otherwise, returns `NAN`.
  inline long fuzzyAsInt(double number, double epsilon) {
    return lround(number);
    // return fuzzyIsInt(number, epsilon) ?
    //   lround(number) : NAN;
  }

  /// Rounds [number] to the nearest integer.
  ///
  /// This rounds up numbers that are [fuzzyEquals] to `X.5`.
  inline long fuzzyRound(double number, double epsilon) {
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
  inline double fuzzyCheckRange(double number, double min, double max, double epsilon) {
    if (fuzzyEquals(number, min, epsilon)) return min;
    if (fuzzyEquals(number, max, epsilon)) return max;
    if (number > min && number < max) return number;
    return NAN;
  }

  /// Throws a [RangeError] if [number] isn't within [min] and [max].
  ///
  /// If [number] is [fuzzyEquals] to [min] or [max], it's clamped to the
  /// appropriate value. [name] is used in error reporting.
  inline double fuzzyAssertRange(double number, double min, double max, double epsilon, std::string name = "") {
    double result = fuzzyCheckRange(number, min, max, epsilon);
    if (result != NAN) return result;
    throw std::range_error("must be between $min and $max.");
    // throw RangeError.value(number, name, "must be between $min and $max.");
  }


  #define FN_PROTOTYPE \
    Env& env, \
    Env& d_env, \
    Context& ctx, \
    Signature sig, \
    ParserState pstate, \
    Backtraces& traces, \
    SelectorStack selector_stack, \
    SelectorStack original_stack \

  typedef const char* Signature;
  typedef Value* (*Native_Function)(FN_PROTOTYPE);
  #define BUILT_IN(name) Value* name(FN_PROTOTYPE)

  #define ARG(argname, argtype, type) get_arg<argtype>(argname, env, sig, pstate, traces, type)
  #define ARGCOL(argname) get_arg<Color>(argname, env, sig, pstate, traces, "a color")
  #define ARGNUM(argname) get_arg<Number>(argname, env, sig, pstate, traces, "a number")
  #define ARGLIST(argname) get_arg<List>(argname, env, sig, pstate, traces, "a list")
  #define ARGSTRC(argname) get_arg<String_Constant>(argname, env, sig, pstate, traces, "a string")
  // special function for weird hsla percent (10px == 10% == 10 != 0.1)
  #define ARGVAL(argname) get_arg_val(argname, env, sig, pstate, traces) // double

  Definition* make_native_function(Signature, Native_Function, Context& ctx);
  Definition* make_c_function(Sass_Function_Entry c_func, Context& ctx);

  namespace Functions {

    std::string envValueToString(Env& env, const std::string& name);

    template <typename T>
    T* get_arg(const std::string& argname, Env& env, Signature sig, ParserState pstate, Backtraces traces, std::string type)
    {
      AST_Node* node = env.get_local(argname);
      T* val = Cast<T>(node);
      if (node && !val) {
        error(argname + ": " + envValueToString(env, argname) + " is not " + type + ".", pstate, traces);
      }
      return val;
    }

    Map* get_arg_m(const std::string& argname, Env& env, Signature sig, ParserState pstate, Backtraces traces); // maps only
    Number* get_arg_n(const std::string& argname, Env& env, Signature sig, ParserState pstate, Backtraces traces); // numbers only
    double get_arg_r(const std::string& argname, Env& env, Signature sig, ParserState pstate, Backtraces traces, double lo, double hi, std::string unit = ""); // colors only
    double get_arg_val(const std::string& argname, Env& env, Signature sig, ParserState pstate, Backtraces traces); // shared
    SelectorListObj get_arg_sels(const std::string& argname, Env& env, Signature sig, ParserState pstate, Backtraces traces, Context& ctx); // selectors only
    CompoundSelectorObj get_arg_sel(const std::string& argname, Env& env, Signature sig, ParserState pstate, Backtraces traces, Context& ctx); // selectors only

    int assertInt(const std::string& name, double nr, ParserState pstate, Backtraces traces);

  }

}

#endif
