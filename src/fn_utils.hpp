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
    // epsilon = std::numeric_limits<double>::epsilon();
    return fabs(number1 - number2) < epsilon;
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
    double _fabs_ = fabs(number - 0.5);
    double _fmod_ = fmod(_fabs_, 1.0);
    return fuzzyEquals(_fmod_, 0.5, epsilon);
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
    // epsilon = std::numeric_limits<double>::epsilon();
    // If the number is within epsilon of X.5,
    // round up (or down for negative numbers).
    if (number > 0) {
      return lround(fuzzyLessThan(
        fmod(number, 1.0), 0.5, epsilon)
          ? floor(number) : ceill(number));
    }
    return lround(fuzzyLessThanOrEquals(
      fmod(number, 1.0), - 0.5, epsilon)
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

  inline bool fuzzyCheckRange(double number, double min, double max, double epsilon, double& value) {
    if (fuzzyEquals(number, min, epsilon)) { value = min; return true; }
    if (fuzzyEquals(number, max, epsilon)) { value = max; return true; }
    if (number > min && number < max) { value = number;  return true; }
    return false;
  }

  /// Throws a [RangeError] if [number] isn't within [min] and [max].
  ///
  /// If [number] is [fuzzyEquals] to [min] or [max], it's clamped to the
  /// appropriate value. [name] is used in error reporting.
  inline double fuzzyAssertRange(double number, double min, double max, double epsilon, double& value, sass::string name = "") {
    double result = 0.0;
    if (!fuzzyCheckRange(number, min, max, epsilon, result)) {
      throw std::range_error("must be between $min and $max.");
    }
    /* if (!std::isnan(result)) */ return result;
    // throw RangeError.value(number, name, "must be between $min and $max.");
  }



  typedef const char* Signature;

  #define BUILT_IN_FN(name) Value* name(FN_PROTOTYPE2)

  ExternalCallable* make_c_function2(Sass_Function_Entry c_func, Context& ctx);

}

#endif
