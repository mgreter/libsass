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
#include "randomize.hpp"

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
        Number* number = arguments[0]->assertNumber(*ctx.logger, "number");
        return SASS_MEMORY_NEW(Number, pstate,
          fuzzyRound(number->value(), ctx.logger->epsilon),
          number->unit());
      }

      BUILT_IN_FN(ceil)
      {
        Number* number = arguments[0]->assertNumber(*ctx.logger, "number");
        return SASS_MEMORY_NEW(Number, pstate,
          std::ceil(number->value()),
          number->unit());
      }

      BUILT_IN_FN(floor)
      {
        Number* number = arguments[0]->assertNumber(*ctx.logger, "number");
        return SASS_MEMORY_NEW(Number, pstate,
          std::floor(number->value()),
          number->unit());
      }

      BUILT_IN_FN(abs)
      {
        Number* number = arguments[0]->assertNumber(*ctx.logger, "number");
        return SASS_MEMORY_NEW(Number, pstate,
          std::abs(number->value()),
          number->unit());
      }

      BUILT_IN_FN(max)
      {
        SassNumber* max = nullptr;
        for (Value* value : arguments[0]->asVector()) {
          SassNumber* number = value->assertNumber(*ctx.logger);
          if (max == nullptr || max->lessThan(number, *ctx.logger, pstate)) {
            max = number;
          }
        }
        if (max != nullptr) return max;
        // Report invalid arguments error
        throw Exception::SassScriptException2(
          "At least one argument must be passed.",
          *ctx.logger, pstate);
      }

      BUILT_IN_FN(min)
      {
        SassNumber* min = nullptr;
        for (Value* value : arguments[0]->asVector()) {
          SassNumber* number = value->assertNumber(*ctx.logger);
          if (min == nullptr || min->greaterThan(number, *ctx.logger, pstate)) {
            min = number;
          }
        }
        if (min != nullptr) return min;
        // Report invalid arguments error
        throw Exception::SassScriptException2(
          "At least one argument must be passed.",
          *ctx.logger, pstate);
      }

      BUILT_IN_FN(random)
      {
        if (arguments[0]->isNull()) {
          return SASS_MEMORY_NEW(Number, pstate,
            getRandomDouble(0, 1));
        }
        Number* nr = arguments[0]->assertNumber(*ctx.logger, "limit");
        long limit = nr->assertInt(*ctx.logger, pstate, "limit");
        if (limit >= 1) {
          return SASS_MEMORY_NEW(Number, pstate,
            (long) getRandomDouble(1, limit + 1));
        }
        // Report invalid arguments error
        sass::sstream strm;
        strm << "$limit: Must be greater than 0, was " << limit << ".";
        throw Exception::SassScriptException2(strm.str(), *ctx.logger, pstate);
      }

      BUILT_IN_FN(unit)
      {
        Number* number = arguments[0]->assertNumber(*ctx.logger, "number");
        return SASS_MEMORY_NEW(String_Constant, pstate, number->unit(), true);
      }

      BUILT_IN_FN(isUnitless)
      {
        Number* number = arguments[0]->assertNumber(*ctx.logger, "number");
        return SASS_MEMORY_NEW(Boolean, pstate, !number->hasUnits());
      }

      BUILT_IN_FN(percentage)
      {
        Number* number = arguments[0]->assertNumber(*ctx.logger, "number");
        number->assertNoUnits(*ctx.logger, pstate, "number");
        return SASS_MEMORY_NEW(Number, pstate,
          number->value() * 100, "%");
      }

      BUILT_IN_FN(compatible)
      {
        Number* n1 = arguments[0]->assertNumber(*ctx.logger, "number1");
        Number* n2 = arguments[1]->assertNumber(*ctx.logger, "number2");
        if (n1->is_unitless() || n2->is_unitless()) {
          return SASS_MEMORY_NEW(Boolean, pstate, true);
        }
        // normalize into main units
        n1->normalize(); n2->normalize();
        Units& lhs_unit = *n1, & rhs_unit = *n2;
        bool is_comparable = (lhs_unit == rhs_unit);
        return SASS_MEMORY_NEW(Boolean, pstate, is_comparable);
      }

    }

  }

}
