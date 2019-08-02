// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include "utf8.h"
#include "ast.hpp"
#include "fn_utils.hpp"
#include "fn_strings.hpp"
#include "fn_utils.hpp"
#include "fn_numbers.hpp"
#include "util_string.hpp"
#include "randomize.hpp"
#include "debugger.hpp"

#include "utf8.h"
#include <random>
#include <iomanip>
#include <algorithm>

#ifdef __MINGW32__
#include "windows.h"
#include "wincrypt.h"
#endif

namespace Sass {

  namespace Functions {

    namespace Strings {

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

      long _codepointForIndex(long index, long lengthInCodepoints, bool allowNegative = false) {
        if (index == 0) return 0;
        if (index > 0) return std::min(index - 1, lengthInCodepoints);
        long result = lengthInCodepoints + index;
        if (result < 0 && !allowNegative) return 0;
        return result;
      }

      BUILT_IN_FN(unquote)
      {
        String_Constant* string = arguments[0]->assertString(*ctx.logger, pstate, "string");
        if (!string->hasQuotes()) return string;
        return SASS_MEMORY_NEW(String_Constant,
          string->pstate(), string->value(), false);
      }

      BUILT_IN_FN(quote)
      {
        String_Constant* string = arguments[0]->assertString(*ctx.logger, pstate, "string");
        if (string->hasQuotes()) return string;
        return SASS_MEMORY_NEW(String_Constant,
          string->pstate(), string->value(), true);
      }

      BUILT_IN_FN(toUpperCase)
      {
        String_Constant* string = arguments[0]->assertString(*ctx.logger, pstate, "string");
        return SASS_MEMORY_NEW(String_Constant, pstate,
          Util::ascii_str_toupper(string->value()),
          string->hasQuotes());
      }

      BUILT_IN_FN(toLowerCase)
      {
        String_Constant* string = arguments[0]->assertString(*ctx.logger, pstate, "string");
        return SASS_MEMORY_NEW(String_Constant, pstate,
          Util::ascii_str_tolower(string->value()),
          string->hasQuotes());
      }

      BUILT_IN_FN(length)
      {
        String_Constant* string = arguments[0]->assertString(*ctx.logger, pstate, "string");
        size_t len = UTF_8::code_point_count(string->value());
        return SASS_MEMORY_NEW(Number, pstate, (double)len);
      }

      BUILT_IN_FN(insert)
      {
        String_Constant* string = arguments[0]->assertString(*ctx.logger, pstate, "string");
        String_Constant* insert = arguments[1]->assertString(*ctx.logger, pstate, "insert");
        size_t len = UTF_8::code_point_count(string->value());
        long index = arguments[2]->assertNumber(*ctx.logger, "index")
          ->assertNoUnits(*ctx.logger, pstate, "index")
          ->assertInt(*ctx.logger, pstate, "index");

        // str-insert has unusual behavior for negative inputs. It guarantees that
        // the `$insert` string is at `$index` in the result, which means that we
        // want to insert before `$index` if it's positive and after if it's
        // negative.
        if (index < 0) {
          // +1 because negative indexes start counting from -1 rather than 0, and
          // another +1 because we want to insert *after* that index.
          index = (long)len + index + 2;
        }

        index = (long) _codepointForIndex(index, (long)len);

        sass::string str(string->value());
        // std::cerr << "Insert at " << index << "\n";
        str.insert(UTF_8::offset_at_position(
          str, index), insert->value());

        return SASS_MEMORY_NEW(String_Constant,
          pstate, str, string->hasQuotes());
      }

      BUILT_IN_FN(index)
      {
        String_Constant* string = arguments[0]->assertString(*ctx.logger, pstate, "string");
        String_Constant* substring = arguments[1]->assertString(*ctx.logger, pstate, "substring");

        sass::string str(string->value());
        sass::string substr(substring->value());

        size_t c_index = str.find(substr);
        if (c_index == sass::string::npos) {
          return SASS_MEMORY_NEW(Null, pstate);
        }

        return SASS_MEMORY_NEW(SassNumber, pstate,
          (double) UTF_8::code_point_count(str, 0, c_index) + 1);
      }

      BUILT_IN_FN(slice)
      {
        String_Constant* string = arguments[0]->assertString(*ctx.logger, pstate, "string");
        SassNumber* beg = arguments[1]->assertNumber(*ctx.logger, "start-at");
        SassNumber* end = arguments[2]->assertNumber(*ctx.logger, "end-at");

        size_t len = UTF_8::code_point_count(string->value());
        beg = beg->assertNoUnits(*ctx.logger, pstate, "start");
        end = end->assertNoUnits(*ctx.logger, pstate, "end");

        // No matter what the start index is, an end
        // index of 0 will produce an empty string.
        long endInt = end->assertNoUnits(*ctx.logger, pstate, "end")
          ->assertInt(*ctx.logger, pstate);
        if (endInt == 0) {
          return SASS_MEMORY_NEW(String_Constant,
            pstate, "", string->hasQuotes());
        }

        long begInt = beg->assertNoUnits(*ctx.logger, pstate, "start")
          ->assertInt(*ctx.logger, pstate);
        begInt = (long)_codepointForIndex(begInt, (long)len, false);
        endInt = (long)_codepointForIndex(endInt, (long)len, true);

        if (endInt == (long)len) endInt = (long)len - 1;
        if (endInt < begInt) {
          return SASS_MEMORY_NEW(String_Constant,
            pstate, "", string->hasQuotes());
        }

        sass::string value(string->value());
        sass::string::iterator begIt = value.begin();
        sass::string::iterator endIt = value.begin();
        utf8::advance(begIt, begInt + 0, value.end());
        utf8::advance(endIt, endInt + 1, value.end());

        sass::string sliced(begIt, endIt);

        return SASS_MEMORY_NEW(
          String_Constant, pstate,
          sliced, string->hasQuotes());

      }

      BUILT_IN_FN(uniqueId)
      {
        sass::sstream ss; ss << "u"
          << std::setfill('0') << std::setw(8)
          << std::hex << getRandomUint32();
        return SASS_MEMORY_NEW(String_Constant,
          pstate, ss.str(), true);
      }

    }


    void handle_utf8_error (const SourceSpan& pstate, Backtraces traces)
    {
      try {
       throw;
      }
      catch (utf8::invalid_code_point&) {
        sass::string msg("utf8::invalid_code_point");
        error(msg, pstate, traces);
      }
      catch (utf8::not_enough_room&) {
        sass::string msg("utf8::not_enough_room");
        error(msg, pstate, traces);
      }
      catch (utf8::invalid_utf8&) {
        sass::string msg("utf8::invalid_utf8");
        error(msg, pstate, traces);
      }
      catch (...) { throw; }
    }

  }

}
