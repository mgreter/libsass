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
        return list->getValueAt(index, *ctx.logger123);
      }

      BUILT_IN_FN(setNth)
      {
        Value* input = arguments[0];
        Value* index = arguments[1];

        if (selfAssign && input->refcount < AssignableRefCount) {
          if (SassList* lst = Cast<SassList>(input)) {
            size_t idx = input->sassIndexToListIndex(index, *ctx.logger123, "n");
            lst->at(idx) = arguments[2];
            return lst;
          }
        }

        // I create a copy anyway!?
        sass::vector<ValueObj> values;
        auto it = input->iterator();
        values.reserve(input->lengthAsList());
        std::copy(it.begin(), it.end(),
          std::back_inserter(values));
        size_t idx = input->sassIndexToListIndex(index, *ctx.logger123, "n");
        values[idx] = arguments[2];
        return SASS_MEMORY_NEW(SassList,
          input->pstate(), std::move(values),
          input->separator(), input->hasBrackets());
      }

      BUILT_IN_FN(join)
      {
        Value* list1 = arguments[0];
        Value* list2 = arguments[1];
        SassString* separatorParam = arguments[2]->assertString(*ctx.logger123, pstate, "separator");
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
            *ctx.logger123, pstate);
        }

        bool bracketed = bracketedParam->isTruthy();
        if (SassString * str = bracketedParam->isString()) {
          if (str->value() == "auto") {
            bracketed = list1->hasBrackets();
          }
        }

        if (selfAssign && list2->refcount < AssignableRefCount) {
          if (SassList* lst = Cast<SassList>(list2)) {
            lst->separator(separator);
            lst->hasBrackets(bracketed);
            auto it2 = list2->iterator();
            std::copy(it2.begin(), it2.end(),
              std::back_inserter(lst->elements()));
            return lst;
          }
        }

        auto it1 = list1->iterator();
        auto it2 = list2->iterator();
        size_t L1 = list1->lengthAsList();
        size_t L2 = list2->lengthAsList();
        sass::vector<ValueObj> values;
        values.reserve(L1 + L2);
        std::copy(it1.begin(), it1.end(),
          std::back_inserter(values));
        std::copy(it2.begin(), it2.end(),
          std::back_inserter(values));
        return SASS_MEMORY_NEW(SassList, pstate,
          std::move(values), separator, bracketed);
      }

      BUILT_IN_FN(append)
      {
        Value* list = arguments[0]->assertValue(*ctx.logger123, "list");
        Value* value = arguments[1]->assertValue(*ctx.logger123, "val");
        SassString* separatorParam = arguments[2]->assertString(*ctx.logger123, pstate, "separator");
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
            *ctx.logger123, pstate);
        }

        if (selfAssign && list->refcount < AssignableRefCount) {
          if (SassList* lst = Cast<SassList>(list)) {
            lst->separator(separator);
            lst->append(value);
            return lst;
          }
        }

        sass::vector<ValueObj> values;
        auto it = list->iterator();
        std::copy(it.begin(), it.end(),
          std::back_inserter(values));
        values.emplace_back(value);
        return SASS_MEMORY_NEW(SassList,
          list->pstate(), std::move(values),
          separator, list->hasBrackets());
      }

      BUILT_IN_FN(zip)
      {
        size_t shortest = sass::string::npos;
        sass::vector<sass::vector<ValueObj>> lists;
        for (Value* arg : arguments[0]->iterator()) {
          Values it = arg->iterator();
          sass::vector<ValueObj> inner;
          inner.reserve(arg->lengthAsList());
          std::copy(it.begin(), it.end(),
            std::back_inserter(inner));
          shortest = std::min(shortest, inner.size());
          lists.emplace_back(inner);
        }
        if (lists.empty()) {
          return SASS_MEMORY_NEW(SassList,
            pstate, {}, SASS_COMMA);
        }
        sass::vector<ValueObj> result;
        if (!lists.empty()) {
          for (size_t i = 0; i < shortest; i++) {
            sass::vector<ValueObj> inner;
            inner.reserve(lists.size());
            for (sass::vector<ValueObj>& arg : lists) {
              inner.push_back(arg[i]);
            }
            result.emplace_back(SASS_MEMORY_NEW(
              SassList, pstate, inner, SASS_SPACE));
          }
        }
        return SASS_MEMORY_NEW(
          SassList, pstate, result, SASS_COMMA);
      }

      BUILT_IN_FN(index)
      {
        Value* value = arguments[1];
        size_t index = arguments[0]->indexOf(value);
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
        return SASS_MEMORY_NEW(SassString, arguments[0]->pstate(),
          std::move(arguments[0]->separator() == SASS_COMMA ? "comma" : "space"));
      }

      BUILT_IN_FN(isBracketed)
      {
        return SASS_MEMORY_NEW(Boolean, pstate,
          arguments[0]->hasBrackets());
      }

	    void registerFunctions(Context& ctx)
	    {

		    ctx.registerBuiltInFunction("length", "$list", length);
		    ctx.registerBuiltInFunction("nth", "$list, $n", nth);
		    ctx.registerBuiltInFunction("set-nth", "$list, $n, $value", setNth);
		    ctx.registerBuiltInFunction("join", "$list1, $list2, $separator: auto, $bracketed: auto", join);
		    ctx.registerBuiltInFunction("append", "$list, $val, $separator: auto", append);
		    ctx.registerBuiltInFunction("zip", "$lists...", zip);
		    ctx.registerBuiltInFunction("index", "$list, $value", index);
		    ctx.registerBuiltInFunction("list-separator", "$list", separator);
		    ctx.registerBuiltInFunction("is-bracketed", "$list", isBracketed);

	    }

    }
  }

}
