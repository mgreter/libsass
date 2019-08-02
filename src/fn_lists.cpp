// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include "listize.hpp"
#include "operators.hpp"
#include "fn_utils.hpp"
#include "fn_lists.hpp"
#include "debugger.hpp"
#include "dart_helpers.hpp"

namespace Sass {

  namespace Functions {

    namespace Lists {

      BUILT_IN_FN(length)
      {
        return SASS_MEMORY_NEW(Number,
          arguments[0]->pstate(),
          (double) arguments[0]->lengthAsList());
      }

      BUILT_IN_FN(nth)
      {
        Value* list = arguments[0];
        Value* index = arguments[1];
        const sass::vector<ValueObj>& values = list->asVector();
        size_t idx = list->sassIndexToListIndex(index, *ctx.logger, "n");
        return ((ValueObj)values[idx]).detach();
      }

      BUILT_IN_FN(setNth)
      {
        Value* list = arguments[0];
        Value* index = arguments[1];
        Value* value = arguments[2];
        sass::vector<ValueObj> newList = list->asVector();
        size_t idx = list->sassIndexToListIndex(index, *ctx.logger, "n");
        newList[idx] = value;
        return arguments[0]->changeListContents(newList);
      }

      BUILT_IN_FN(join)
      {
        Value* list1 = arguments[0];
        Value* list2 = arguments[1];
        String_Constant* separatorParam = arguments[2]->assertString(*ctx.logger, pstate, "separator");
        Value* bracketedParam = arguments[3];

        Sass_Separator separator = SASS_UNDEF;
        if (separatorParam->value() == "auto") {
          if (list1->separator() != SASS_UNDEF) {
            separator = list1->separator();
          }
          else if (list2->separator() != SASS_UNDEF) {
            separator = list2->separator();
          }
          else {
            separator = SASS_SPACE;
          }
        }
        else if (separatorParam->value() == "space") {
          separator = SASS_SPACE;
        }
        else if (separatorParam->value() == "comma") {
          separator = SASS_COMMA;
        }
        else {
          throw Exception::SassScriptException2(
            "$separator: Must be \"space\", \"comma\", or \"auto\".",
            *ctx.logger, pstate);
        }

        bool bracketed = bracketedParam->isTruthy();
        if (String_Constant * str = Cast<String_Constant>(bracketedParam)) {
          if (str->value() == "auto") {
            bracketed = list1->hasBrackets();
          }
        }

        sass::vector<ValueObj> values;
        sass::vector<ValueObj> l1vals = list1->asVector();
        sass::vector<ValueObj> l2vals = list2->asVector();
        std::move(l1vals.begin(), l1vals.end(), std::back_inserter(values));
        std::move(l2vals.begin(), l2vals.end(), std::back_inserter(values));
        return SASS_MEMORY_NEW(SassList, SourceSpan::fake("[pstateXY]"),
          std::move(values), separator, bracketed);
      }

      BUILT_IN_FN(append)
      {
        Value* list = arguments[0]->assertValue(*ctx.logger, "list");
        Value* value = arguments[1]->assertValue(*ctx.logger, "val");
        String_Constant* separatorParam = arguments[2]->assertString(*ctx.logger, pstate, "separator");
        Sass_Separator separator = SASS_UNDEF;
        if (separatorParam->value() == "auto") {
          separator = list->separator() == SASS_UNDEF
            ? SASS_SPACE : list->separator();
        }
        else if (separatorParam->value() == "space") {
          separator = SASS_SPACE;
        }
        else if (separatorParam->value() == "comma") {
          separator = SASS_COMMA;
        }
        else {
          throw Exception::SassScriptException2(
            "$separator: Must be \"space\", \"comma\", or \"auto\".",
            *ctx.logger, pstate);
        }
        sass::vector<ValueObj> newList(list->asVector());
        newList.emplace_back(value); // append the new value
        return list->changeListContents(newList, separator);
      }

      BUILT_IN_FN(zip)
      {
        size_t shortest = sass::string::npos;
        sass::vector<sass::vector<ValueObj>> lists;
        for (Value* arg : arguments[0]->asVector()) {
          sass::vector<ValueObj> inner = arg->asVector();
          shortest = std::min(shortest, inner.size());
          lists.emplace_back(inner);
        }
        SassListObj result = SASS_MEMORY_NEW(
          SassList, pstate, sass::vector<ValueObj>(), SASS_COMMA);
        if (lists.empty()) { return result.detach(); }
        for (size_t i = 0; i < shortest; i++) {
          SassList* inner = SASS_MEMORY_NEW(
            SassList, pstate, sass::vector<ValueObj>(), SASS_SPACE);
          for (sass::vector<ValueObj>& arg : lists) {
            inner->append(arg[i]);
          }
          result->append(inner);
        }
        return result.detach();
      }

      BUILT_IN_FN(index)
      {
        sass::vector<ValueObj> list =
          arguments[0]->asVector();
        Value* value = arguments[1];
        size_t index = indexOf(list, value);
        if (index == sass::string::npos) {
          return SASS_MEMORY_NEW(Null,
            arguments[0]->pstate());
        }
        return SASS_MEMORY_NEW(Number,
          arguments[0]->pstate(),
          (double) index + 1);
      }

      BUILT_IN_FN(separator)
      {
        return SASS_MEMORY_NEW(String_Constant, arguments[0]->pstate(),
          std::move(arguments[0]->separator() == SASS_COMMA ? "comma" : "space"));
      }

      BUILT_IN_FN(isBracketed)
      {
        return SASS_MEMORY_NEW(Boolean, pstate,
          arguments[0]->hasBrackets());
      }

    }
  }

}
