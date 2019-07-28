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

      BUILT_IN_FN(unquote)
      {
        SassString* string = arguments[0]->assertString("string");
        if (!string->hasQuotes()) return string;
        return SASS_MEMORY_NEW(String_Quoted,
          pstate, string->value(), '\0');
      }

      BUILT_IN_FN(quote)
      {
        SassString* string = arguments[0]->assertString("string");
        if (string->hasQuotes()) return string;
        return SASS_MEMORY_NEW(String_Quoted,
          pstate, string->value(), '*');
      }

      BUILT_IN_FN(toUpperCase)
      {
        SassString* string = arguments[0]->assertString("string");
        return SASS_MEMORY_NEW(SassString, pstate,
          Util::ascii_str_toupper(string->value()));
      }

      BUILT_IN_FN(toLowerCase)
      {
        SassString* string = arguments[0]->assertString("string");
        return SASS_MEMORY_NEW(SassString, pstate,
          Util::ascii_str_tolower(string->value()));
      }

      BUILT_IN_FN(length)
      {
        SassString* string = arguments[0]->assertString("string");
        size_t len = UTF_8::code_point_count(string->value());
        return SASS_MEMORY_NEW(Number, pstate, (double)len);
      }

      BUILT_IN_FN(insert)
      {
        SassString* string = arguments[0]->assertString("string");
        SassString* insert = arguments[1]->assertString("insert");
        size_t len = UTF_8::code_point_count(string->value());
        long index = arguments[2]->assertNumber("index")
          ->assertNoUnits("index")->assertInt(epsilon, "index");

        // str-insert has unusual behavior for negative inputs. It guarantees that
        // the `$insert` string is at `$index` in the result, which means that we
        // want to insert before `$index` if it's positive and after if it's
        // negative.
        if (index < 0) {
          // +1 because negative indexes start counting from -1 rather than 0, and
          // another +1 because we want to insert *after* that index.
          index = len + index + 2;
        }

        std::string str(string->value());
        str.insert(UTF_8::offset_at_position(
          str, index - 1), insert->value());
        return SASS_MEMORY_NEW(String_Quoted, pstate,
          str, string->hasQuotes() ? '*' : '\0');
      }

      BUILT_IN_FN(index)
      {
        SassString* string = arguments[0]->assertString("string");
        SassString* substring = arguments[1]->assertString("substring");

        std::string str(string->value());
        std::string substr(substring->value());

        size_t c_index = str.find(substr);
        if (c_index == std::string::npos) {
          return SASS_MEMORY_NEW(Null, pstate);
        }

        return SASS_MEMORY_NEW(SassNumber, pstate,
          UTF_8::code_point_count(str, 0, c_index) + 1);
      }

      long _codepointForIndex(long index, long lengthInCodepoints, bool allowNegative = false) {
        if (index == 0) return 0;
        if (index > 0) return std::min(index - 1, lengthInCodepoints);
        long result = lengthInCodepoints + index;
        if (result < 0 && !allowNegative) return 0;
        return result;
      }

      BUILT_IN_FN(slice)
      {
        SassString* string = arguments[0]->assertString("string");
        SassNumber* beg = arguments[1]->assertNumber("start-at");
        SassNumber* end = arguments[2]->assertNumber("end-at");

        long len = UTF_8::code_point_count(string->value());
        beg = beg->assertNoUnits("start");
        end = end->assertNoUnits("end");

        // No matter what the start index is, an end
        // index of 0 will produce an empty string.
        long endInt = end->assertNoUnits("end")->assertInt(epsilon);
        if (endInt == 0) return SASS_MEMORY_NEW(String_Quoted,
          pstate, "", string->quote_mark());

        long begInt = beg->assertNoUnits("start")->assertInt(epsilon);
        begInt = _codepointForIndex(begInt, len, false);
        endInt = _codepointForIndex(endInt, len, true);

        if (endInt == len) endInt = len - 1;
        if (endInt < begInt) return SASS_MEMORY_NEW(
          String_Quoted, pstate,
          "", string->quote_mark());

        std::string value(string->value());
        std::string::iterator begIt = value.begin();
        std::string::iterator endIt = value.begin();
        utf8::advance(begIt, begInt + 0, value.end());
        utf8::advance(endIt, endInt + 1, value.end());

        return SASS_MEMORY_NEW(
          String_Constant, pstate,
          std::string(begIt, endIt),
          string->quote_mark());
      }

      BUILT_IN_FN(uniqueId)
      {
        std::stringstream ss;
        std::uniform_real_distribution<> distributor(0, 4294967296); // 16^8
        uint_fast32_t distributed = static_cast<uint_fast32_t>(distributor(rand));
        ss << "u" << std::setfill('0') << std::setw(8) << std::hex << distributed;
        return SASS_MEMORY_NEW(String_Quoted, pstate, ss.str());
      }

    }


    void handle_utf8_error (const ParserState& pstate, Backtraces traces)
    {
      try {
       throw;
      }
      catch (utf8::invalid_code_point&) {
        std::string msg("utf8::invalid_code_point");
        error(msg, pstate, traces);
      }
      catch (utf8::not_enough_room&) {
        std::string msg("utf8::not_enough_room");
        error(msg, pstate, traces);
      }
      catch (utf8::invalid_utf8&) {
        std::string msg("utf8::invalid_utf8");
        error(msg, pstate, traces);
      }
      catch (...) { throw; }
    }

    ///////////////////
    // STRING FUNCTIONS
    ///////////////////

    Signature unquote_sig = "unquote($string)";
    BUILT_IN(sass_unquote)
    {
      AST_Node_Obj arg = env["$string"];
      if (String_Quoted* string_quoted = Cast<String_Quoted>(arg)) {
        return SASS_MEMORY_NEW(String_Constant, pstate, string_quoted->value());
      }
      else if (String_Constant* str = Cast<String_Constant>(arg)) {
        return str;
      }
      std::stringstream msg;
      msg << "$string: ";
      msg << arg->to_string();
      msg << " is not a string.";
      error(msg.str(), arg->pstate(), traces);
      return nullptr;
    }

    Signature quote_sig = "quote($string)";
    BUILT_IN(sass_quote)
    {
      const String_Constant* s = ARGSTRC("$string");
      String_Quoted *result = SASS_MEMORY_NEW(
          String_Quoted, pstate, s->value(),
          /*q=*/'\0', /*keep_utf8_escapes=*/false, /*skip_unquoting=*/true);
      result->quote_mark('*');
      return result;
    }

    Signature str_length_sig = "str-length($string)";
    BUILT_IN(str_length)
    {
      size_t len = std::string::npos;
      try {
        String_Constant* s = ARGSTRC("$string");
        len = UTF_8::code_point_count(s->value(), 0, s->value().size());

      }
      // handle any invalid utf8 errors
      // other errors will be re-thrown
      catch (...) { handle_utf8_error(pstate, traces); }
      // return something even if we had an error (-1)
      return SASS_MEMORY_NEW(Number, pstate, (double)len);
    }

    Signature str_insert_sig = "str-insert($string, $insert, $index)";
    BUILT_IN(str_insert)
    {
      std::string str;
      try {
        String_Constant* s = ARGSTRC("$string");
        str = s->value();
        String_Constant* i = ARGSTRC("$insert");
        std::string ins = i->value();
        double index = ARGVAL("$index");
        assertInt("$index", index, pstate, traces);
        size_t len = UTF_8::code_point_count(str, 0, str.size());

        if (index > 0 && index <= len) {
          // positive and within string length
          str.insert(UTF_8::offset_at_position(str, static_cast<size_t>(index) - 1), ins);
        }
        else if (index > len) {
          // positive and past string length
          str += ins;
        }
        else if (index == 0) {
          str = ins + str;
        }
        else if (std::abs(index) <= len) {
          // negative and within string length
          index += len + 1;
          str.insert(UTF_8::offset_at_position(str, static_cast<size_t>(index)), ins);
        }
        else {
          // negative and past string length
          str = ins + str;
        }

        if (String_Quoted* ss = Cast<String_Quoted>(s)) {
          if (ss->quote_mark()) str = quote(str);
        }
      }
      // handle any invalid utf8 errors
      // other errors will be re-thrown
      catch (...) { handle_utf8_error(pstate, traces); }
      return SASS_MEMORY_NEW(String_Quoted, pstate, str);
    }

    Signature str_index_sig = "str-index($string, $substring)";
    BUILT_IN(str_index)
    {
      size_t index = std::string::npos;
      try {
        String_Constant* s = ARGSTRC("$string");
        String_Constant* t = ARGSTRC("$substring");
        std::string str = s->value();
        std::string substr = t->value();

        size_t c_index = str.find(substr);
        if(c_index == std::string::npos) {
          return SASS_MEMORY_NEW(Null, pstate);
        }
        index = UTF_8::code_point_count(str, 0, c_index) + 1;
      }
      // handle any invalid utf8 errors
      // other errors will be re-thrown
      catch (...) { handle_utf8_error(pstate, traces); }
      // return something even if we had an error (-1)
      return SASS_MEMORY_NEW(Number, pstate, (double)index);
    }

    Signature str_slice_sig = "str-slice($string, $start-at, $end-at: -1)";
    BUILT_IN(str_slice)
    {
      std::string newstr;
      try {
        String_Constant* s = ARGSTRC("$string");
        double start_at = ARGVAL("$start-at");
        double end_at = ARGVAL("$end-at");
        assertInt("", start_at, pstate, traces);
        assertInt("", end_at, pstate, traces);
        String_Quoted* ss = Cast<String_Quoted>(s);

        std::string str(s->value());

        size_t size = utf8::distance(str.begin(), str.end());

        if (!Cast<Number>(env["$end-at"])) {
          end_at = -1;
        }

        if (end_at == 0 || (end_at + size) < 0) {
          if (ss && ss->quote_mark()) newstr = quote("");
          return SASS_MEMORY_NEW(String_Quoted, pstate, newstr);
        }

        if (end_at < 0) {
          end_at += size + 1;
          if (end_at == 0) end_at = 1;
        }
        if (end_at > size) { end_at = (double)size; }
        if (start_at < 0) {
          start_at += size + 1;
          if (start_at <= 0) start_at = 1;
        }
        else if (start_at == 0) { ++ start_at; }

        if (start_at <= end_at)
        {
          std::string::iterator start = str.begin();
          utf8::advance(start, start_at - 1, str.end());
          std::string::iterator end = start;
          utf8::advance(end, end_at - start_at + 1, str.end());
          newstr = std::string(start, end);
        }
        if (ss) {
          if(ss->quote_mark()) newstr = quote(newstr);
        }
      }
      // handle any invalid utf8 errors
      // other errors will be re-thrown
      catch (...) { handle_utf8_error(pstate, traces); }
      return SASS_MEMORY_NEW(String_Quoted, pstate, newstr);
    }

    Signature to_upper_case_sig = "to-upper-case($string)";
    BUILT_IN(to_upper_case)
    {
      String_Constant* s = ARGSTRC("$string");
      std::string str = s->value();
      Util::ascii_str_toupper(&str);

      if (String_Quoted* ss = Cast<String_Quoted>(s)) {
        String_Quoted* cpy = SASS_MEMORY_COPY(ss);
        cpy->value(str);
        return cpy;
      } else {
        return SASS_MEMORY_NEW(String_Quoted, pstate, str);
      }
    }

    Signature to_lower_case_sig = "to-lower-case($string)";
    BUILT_IN(to_lower_case)
    {
      String_Constant* s = ARGSTRC("$string");
      std::string str = s->value();
      Util::ascii_str_tolower(&str);

      if (String_Quoted* ss = Cast<String_Quoted>(s)) {
        String_Quoted* cpy = SASS_MEMORY_COPY(ss);
        cpy->value(str);
        return cpy;
      } else {
        return SASS_MEMORY_NEW(String_Quoted, pstate, str);
      }
    }

  }

}
