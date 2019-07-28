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
  bool fuzzyEquals(double number1, double number2, double epsilon);

  /// `1 / epsilon`, cached since [math.pow] may not be computed at compile-time
  /// and thus this probably won't be constant-folded.
  // final _inverseEpsilon = 1 / epsilon;

  /// Returns a hash code for [number] that matches [fuzzyEquals].
  // int fuzzyHashCode(num number) = > (number * _inverseEpsilon).round().hashCode;

  // Returns whether [number1] is less than [number2], and not [fuzzyEquals].
  bool fuzzyLessThan(double number1, double number2, double epsilon);

  // Returns whether [number1] is less than [number2], or [fuzzyEquals].
  bool fuzzyLessThanOrEquals(double number1, double number2, double epsilon);

  // Returns whether [number1] is greater than [number2], and not [fuzzyEquals].
  bool fuzzyGreaterThan(double number1, double number2, double epsilon);

  // Returns whether [number1] is greater than [number2], or [fuzzyEquals].
  bool fuzzyGreaterThanOrEquals(double number1, double number2, double epsilon);

  // Returns whether [number] is [fuzzyEquals] to an integer.
  bool fuzzyIsInt(double number, double epsilon);


  /// If [number] is an integer according to [fuzzyIsInt], returns it as an
/// [int].
///
/// Otherwise, returns `NAN`.
  long fuzzyAsInt(double number, double epsilon);

  /// Rounds [number] to the nearest integer.
  ///
  /// This rounds up numbers that are [fuzzyEquals] to `X.5`.
  long fuzzyRound(double number, double epsilon);

  /// Returns [number] if it's within [min] and [max], or `NAN` if it's not.
  ///
  /// If [number] is [fuzzyEquals] to [min] or [max], it's clamped to the
  /// appropriate value.
  double fuzzyCheckRange(double number, double min, double max, double epsilon);

  /// Throws a [RangeError] if [number] isn't within [min] and [max].
  ///
  /// If [number] is [fuzzyEquals] to [min] or [max], it's clamped to the
  /// appropriate value. [name] is used in error reporting.
  double fuzzyAssertRange(double number, double min, double max, double epsilon, std::string name = "");


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
