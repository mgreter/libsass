// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>

#include "ast.hpp"
#include "units.hpp"
#include "fn_utils.hpp"
#include "fn_numbers.hpp"

#ifdef __MINGW32__
#include "windows.h"
#include "wincrypt.h"
#endif
#include "debugger.hpp"

namespace Sass {

  namespace Functions {

    #ifdef __MINGW32__
      uint64_t GetSeed()
      {
        HCRYPTPROV hp = 0;
        BYTE rb[8];
        CryptAcquireContext(&hp, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
        CryptGenRandom(hp, sizeof(rb), rb);
        CryptReleaseContext(hp, 0);

        uint64_t seed;
        memcpy(&seed, &rb[0], sizeof(seed));

        return seed;
      }
    #else
      uint64_t GetSeed()
      {
        std::random_device rd;
        return rd();
      }
    #endif

    // note: the performance of many implementations of
    // random_device degrades sharply once the entropy pool
    // is exhausted. For practical use, random_device is
    // generally only used to seed a PRNG such as mt19937.
    static std::mt19937 rand(static_cast<unsigned int>(GetSeed()));



    namespace Math {

      BUILT_IN_FN(round)
      {
        Number* number = arguments[0]->assertNumber("number");
        return SASS_MEMORY_NEW(Number, pstate,
          fuzzyRound(number->value(), epsilon),
          number->unit());
      }

      BUILT_IN_FN(ceil)
      {
        Number* number = arguments[0]->assertNumber("number");
        return SASS_MEMORY_NEW(Number, pstate,
          std::ceil(number->value()),
          number->unit());
      }

      BUILT_IN_FN(floor)
      {
        Number* number = arguments[0]->assertNumber("number");
        return SASS_MEMORY_NEW(Number, pstate,
          std::floor(number->value()),
          number->unit());
      }

      BUILT_IN_FN(abs)
      {
        Number* number = arguments[0]->assertNumber("number");
        return SASS_MEMORY_NEW(Number, pstate,
          std::abs(number->value()),
          number->unit());
      }

      BUILT_IN_FN(max)
      {
        SassNumber* max = nullptr;
        for (Value* value : arguments[0]->asVector()) {
          SassNumber* number = value->assertNumber();
          if (max == nullptr || *max < *number) {
            max = number;
          }
        }
        if (max != nullptr) return max;
        // Report invalid arguments error
        throw Exception::SassScriptException(
          "At least one argument must be passed.");
      }

      BUILT_IN_FN(min)
      {
        SassNumber* min = nullptr;
        for (Value* value : arguments[0]->asVector()) {
          SassNumber* number = value->assertNumber();
          if (min == nullptr || *min > *number) {
            min = number;
          }
        }
        if (min != nullptr) return min;
        // Report invalid arguments error
        throw Exception::SassScriptException(
          "At least one argument must be passed.");
      }

      BUILT_IN_FN(random)
      {
        if (arguments[0]->isNull()) {
          std::uniform_real_distribution<> distributor(0, 1);
          return SASS_MEMORY_NEW(Number, pstate, distributor(rand));
        }
        Number* nr = arguments[0]->assertNumber("limit");
        long limit = nr->assertInt("limit");
        if (limit >= 1) {
          std::uniform_real_distribution<> distributor(1, limit + 1);
          return SASS_MEMORY_NEW(Number, pstate, distributor(rand));
        }
        // Report invalid arguments error
        std::stringstream strm;
        strm << "$limit: Must be greater than 0, was " << limit << ".";
        throw Exception::SassScriptException(strm.str());
      }

      BUILT_IN_FN(unit)
      {
        Number* number = arguments[0]->assertNumber("number");
        std::string str(quote(number->unit(), '"'));
        return SASS_MEMORY_NEW(String_Quoted, pstate, str);
      }

      BUILT_IN_FN(isUnitless)
      {
        Number* number = arguments[0]->assertNumber("number");
        return SASS_MEMORY_NEW(Boolean, pstate, !number->hasUnits());
      }

      BUILT_IN_FN(percentage)
      {
        Number* number = arguments[0]->assertNumber("number");
        number->assertNoUnits("number");
        return SASS_MEMORY_NEW(Number, pstate,
          number->value() * 100, "%");
      }

      BUILT_IN_FN(compatible)
      {
        Number* number1 = arguments[0]->assertNumber("number1");
        Number* number2 = arguments[1]->assertNumber("number2");
        return SASS_MEMORY_NEW(Boolean, pstate,
          false /* number1->isComparableTo(number2) */);
      }

    }



    ///////////////////
    // NUMBER FUNCTIONS
    ///////////////////

    Signature percentage_sig = "percentage($number)";
    BUILT_IN(percentage)
    {
      Number_Obj n = ARGN("$number");
      if (!n->is_unitless()) error("$number: Expected " + n->to_string() + " to have no units.", pstate, traces);
      return SASS_MEMORY_NEW(Number, pstate, n->value() * 100, "%");
    }

    Signature round_sig = "round($number)";
    BUILT_IN(round)
    {
      Number_Obj r = ARGN("$number");
      r->value(Sass::round(r->value(), ctx.c_options.precision));
      r->pstate(pstate);
      return r.detach();
    }

    Signature ceil_sig = "ceil($number)";
    BUILT_IN(ceil)
    {
      Number_Obj r = ARGN("$number");
      r->value(std::ceil(r->value()));
      r->pstate(pstate);
      return r.detach();
    }

    Signature floor_sig = "floor($number)";
    BUILT_IN(floor)
    {
      auto val = env["$number"];
      // debug_ast(val);
      Number_Obj r = ARGN("$number");
      r->value(std::floor(r->value()));
      r->pstate(pstate);
      return r.detach();
    }

    Signature abs_sig = "abs($number)";
    BUILT_IN(abs)
    {
      Number_Obj r = ARGN("$number");
      r->value(std::abs(r->value()));
      r->pstate(pstate);
      return r.detach();
    }

    Signature min_sig = "min($numbers...)";
    BUILT_IN(min)
    {
      List* arglist = ARGLIST("$numbers");
      Number_Obj least;
      size_t L = arglist->length();
      if (L == 0) {
        error("At least one argument must be passed.", pstate, traces);
      }
      for (size_t i = 0; i < L; ++i) {
        Expression_Obj val = arglist->value_at_index(i);
        Number_Obj xi = Cast<Number>(val);
        if (!xi) {
          error(val->to_string(ctx.c_options) + " is not a number.", val->pstate(), traces);
        }
        if (least) {
          if (*xi < *least) least = xi;
        } else least = xi;
      }
      return least.detach();
    }

    Signature max_sig = "max($numbers...)";
    BUILT_IN(max)
    {
      List* arglist = ARGLIST("$numbers");
      Number_Obj greatest;
      size_t L = arglist->length();
      if (L == 0) {
        error("At least one argument must be passed.", pstate, traces);
      }
      for (size_t i = 0; i < L; ++i) {
        Expression_Obj val = arglist->value_at_index(i);
        Number_Obj xi = Cast<Number>(val);
        if (!xi) {
          error(val->to_string(ctx.c_options) + " is not a number.", val->pstate(), traces);
        }
        if (greatest) {
          if (*xi > *greatest) greatest = xi;
        } else greatest = xi;
      }
      return greatest.detach();
    }

    Signature random_sig = "random($limit: null)";
    BUILT_IN(random)
    {
      AST_Node_Obj arg = env["$limit"];
      Null* n = Cast<Null>(arg);
      Value* v = Cast<Value>(arg);
      Number* l = Cast<Number>(arg);
      Boolean* b = Cast<Boolean>(arg);
      if (n) {
        std::uniform_real_distribution<> distributor(0, 1);
        return SASS_MEMORY_NEW(Number, pstate, distributor(rand));
      }
      else if (l) {
        double lv = l->value();
        // bool eq_int = std::fabs(trunc(lv) - lv) < NUMBER_EPSILON;
        assertInt("$limit", lv, pstate, traces);
        if (lv < 1) {
          std::stringstream err;
          err << "$limit: Must be greater than 0, was " << lv << ".";
          error(err.str(), pstate, traces);
        }
        std::uniform_real_distribution<> distributor(1, lv + 1);
        uint_fast32_t distributed = static_cast<uint_fast32_t>(distributor(rand));
        return SASS_MEMORY_NEW(Number, pstate, (double)distributed);
      }
      else if (b) {
        std::uniform_real_distribution<> distributor(0, 1);
        double distributed = static_cast<double>(distributor(rand));
        return SASS_MEMORY_NEW(Number, pstate, distributed);
      } else if (v) {
        traces.push_back(Backtrace(pstate));
        throw Exception::InvalidArgumentType(pstate, traces, "random", "$limit", "number", v);
      } else {
        traces.push_back(Backtrace(pstate));
        throw Exception::InvalidArgumentType(pstate, traces, "random", "$limit", "number");
      }
    }

    Signature unique_id_sig = "unique-id()";
    BUILT_IN(unique_id)
    {
      std::stringstream ss;
      std::uniform_real_distribution<> distributor(0, 4294967296); // 16^8
      uint_fast32_t distributed = static_cast<uint_fast32_t>(distributor(rand));
      ss << "u" << std::setfill('0') << std::setw(8) << std::hex << distributed;
      return SASS_MEMORY_NEW(String_Quoted, pstate, ss.str());
    }

    Signature unit_sig = "unit($number)";
    BUILT_IN(unit)
    {
      Number_Obj arg = ARGN("$number");
      std::string str(quote(arg->unit(), '"'));
      return SASS_MEMORY_NEW(String_Quoted, pstate, str);
    }

    Signature unitless_sig = "unitless($number)";
    BUILT_IN(unitless)
    {
      Number_Obj arg = ARGN("$number");
      bool unitless = arg->is_unitless();
      return SASS_MEMORY_NEW(Boolean, pstate, unitless);
    }

    Signature comparable_sig = "comparable($number1, $number2)";
    BUILT_IN(comparable)
    {
      Number_Obj n1 = ARGN("$number1");
      Number_Obj n2 = ARGN("$number2");
      if (n1->is_unitless() || n2->is_unitless()) {
        return SASS_MEMORY_NEW(Boolean, pstate, true);
      }
      // normalize into main units
      n1->normalize(); n2->normalize();
      Units &lhs_unit = *n1, &rhs_unit = *n2;
      bool is_comparable = (lhs_unit == rhs_unit);
      return SASS_MEMORY_NEW(Boolean, pstate, is_comparable);
    }

  }

}
