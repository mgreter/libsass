#include <numeric>

#include "extender.hpp"
#include "listize.hpp"
#include "fn_utils.hpp"
#include "fn_selectors.hpp"
#include "parser_selector.hpp"

namespace Sass {

  namespace Functions {

    // Adds a [ParentSelector] to the beginning of [compound],
    // or returns `null` if that wouldn't produce a valid selector.
    CompoundSelector* _prependParent(CompoundSelector* compound) {
      SimpleSelector* first = compound->first();
      if (first->is_universal()) return nullptr;
      if (TypeSelector * type = Cast<TypeSelector>(first)) {
        if (!type->ns().empty()) return nullptr;
        CompoundSelector* cp = SASS_MEMORY_COPY(compound);
        cp->hasRealParent(true);
        return cp;
      }
      else {
        CompoundSelector* cp = SASS_MEMORY_COPY(compound);
        cp->hasRealParent(true);
        return cp;
      }
    }

    namespace Selectors {

      BUILT_IN_FN(nest)
      {
        sass::vector<ValueObj> arglist = arguments[0]->asVector();

        // Not enough parameters
        if (arglist.size() == 0) {
          throw Exception::SassRuntimeException(
            "$selectors: At least one selector must be passed for `selector-nest'",
            pstate);
        }

        // Parse args into vector of selectors
        SelectorStack parsedSelectors;
        for (size_t i = 0, L = arglist.size(); i < L; ++i) {
          ValueObj exp = arglist[i];
          if (exp->concrete_type() == Expression::NULL_VAL) {
            callStackFrame frame(*ctx.logger, Backtrace(exp->pstate()));
            throw Exception::SassRuntimeException2( // "$selectors: "
              "null is not a valid selector: it must be a string,\n"
              "a list of strings, or a list of lists of strings.",
              *ctx.logger, pstate);
          }
          if (String_Constant_Obj str = Cast<String_Constant>(exp)) {
            str->hasQuotes(false);
          }
          sass::string exp_src = exp->to_string(ctx.c_options);

          SourceSpan state(exp->pstate());
          char* str = sass_copy_c_string(exp_src.c_str());
          ctx.strings.emplace_back(str);
          auto qwe = SASS_MEMORY_NEW(SourceFile,
            state.getPath(), str, state.getSrcId());
          SelectorParser p2(ctx, qwe);
          p2._allowParent = true;
          SelectorListObj sel = p2.parse();
          parsedSelectors.emplace_back(sel);
        }

        // Nothing to do
        if (parsedSelectors.empty()) {
          return SASS_MEMORY_NEW(Null, pstate);
        }

        // Set the first element as the `result`, keep
        // appending to as we go down the parsedSelector vector.
        SelectorStack::iterator itr = parsedSelectors.begin();
        SelectorListObj& result = *itr;
        ++itr;

        for (; itr != parsedSelectors.end(); ++itr) {
          SelectorListObj& child = *itr;
          // original_stack.emplace_back(result);
          sass::vector<Backtrace>& traces = *ctx.logger;
          SelectorListObj rv = child->resolveParentSelectors(result, traces);
          result->elements(rv->elements());
          // original_stack.pop_back();
        }

        return Listize::listize(result);

      }

      BUILT_IN_FN(append)
      {

        sass::vector<ValueObj> arglist = arguments[0]->asVector();

        // Not enough parameters
        if (arglist.empty()) {
          throw Exception::SassRuntimeException2(
            "$selectors: At least one selector must be passed.",
            *ctx.logger, pstate);
        }

        sass::vector<SelectorListObj> selectors;
        for (ValueObj& arg : arglist) {
          if (Cast<Null>(arg)) {
            callStackFrame frame(*ctx.logger, Backtrace(arg->pstate()));
            throw Exception::SassRuntimeException2( // "$selectors: "
              "null is not a valid selector: it must be a string,\n"
              "a list of strings, or a list of lists of strings.",
              *ctx.logger, pstate);
          }
          sass::string text = arg->to_css();
          SourceSpan state(arg->pstate());
          char* str = sass_copy_c_string(text.c_str());
          ctx.strings.emplace_back(str);
          auto qwe = SASS_MEMORY_NEW(SourceFile,
            state.getPath(), str, state.getSrcId());
          SelectorParser p2(ctx, qwe);
          selectors.emplace_back(p2.parse());
        }

        SelectorListObj reduced;
        // Implement reduce/accumulate
        for (SelectorList* child : selectors) {
          // The first iteration
          if (reduced.isNull()) {
            reduced = child;
            continue;
          }
          // Combine child with parent
          SelectorListObj cp = SASS_MEMORY_COPY(child);
          for (ComplexSelector* complex : child->elements()) {
            SelectorComponent* component = complex->first();
            if (CompoundSelector * compound = component->getCompound()) {
              compound = _prependParent(compound);
              if (compound == nullptr) {
                throw Exception::SassRuntimeException2(
                  "Can't append " + child->to_css() + " to " +
                  reduced->to_css() + ".",
                  *ctx.logger, pstate);
              }
              complex->at(0) = compound;
            }
            else {
              throw Exception::SassRuntimeException2(
                "Can't append " + child->to_css() + " to " +
                reduced->to_css() + ".",
                *ctx.logger, pstate);
            }
          }
          sass::vector<Backtrace>& traces = *ctx.logger;
          reduced = cp->resolveParentSelectors(reduced, traces, false);
        }

        return Listize::listize(reduced);

      }

      BUILT_IN_FN(extend)
      {
        sass::vector<Backtrace>& traces = *ctx.logger;
        // callStackFrame frame(traces, Backtrace(pstate, "selector-extend"));
        SelectorListObj selector = arguments[0]->assertSelector(ctx, "selector");
        SelectorListObj target = arguments[1]->assertSelector(ctx, "extendee");
        SelectorListObj source = arguments[2]->assertSelector(ctx, "extender");
        SelectorListObj result = Extender::extend(selector, source, target, traces);
        return Listize::listize(result);
      }

      BUILT_IN_FN(replace)
      {
        sass::vector<Backtrace>& traces = *ctx.logger;
        SelectorListObj selector = arguments[0]->assertSelector(ctx, "selector");
        SelectorListObj target = arguments[1]->assertSelector(ctx, "original");
        SelectorListObj source = arguments[2]->assertSelector(ctx, "replacement");
        SelectorListObj result = Extender::replace(selector, source, target, traces);
        return Listize::listize(result);
      }

      BUILT_IN_FN(unify)
      {
        SelectorListObj selector1 = arguments[0]->assertSelector(ctx, "selector1");
        SelectorListObj selector2 = arguments[1]->assertSelector(ctx, "selector2");
        SelectorListObj result = selector1->unifyWith(selector2);
        return Listize::listize(result);
      }

      BUILT_IN_FN(isSuper)
      {
        SelectorListObj sel_sup = arguments[0]->assertSelector(ctx, "super");
        SelectorListObj sel_sub = arguments[1]->assertSelector(ctx, "sub");
        bool result = sel_sup->isSuperselectorOf(sel_sub);
        return SASS_MEMORY_NEW(Boolean, pstate, result);
      }

      BUILT_IN_FN(simple)
      {
        CompoundSelectorObj selector = arguments[0]->assertCompoundSelector(ctx, "selector");
        SassList* l = SASS_MEMORY_NEW(SassList,
          selector->pstate(), sass::vector<ValueObj>(), SASS_COMMA);
        for (size_t i = 0, L = selector->length(); i < L; ++i) {
          const SimpleSelectorObj& ss = selector->get(i);
          sass::string ss_string = ss->to_string();
          l->append(SASS_MEMORY_NEW(String_Constant, ss->pstate(), ss_string));
        }
        return l;

      }

      BUILT_IN_FN(parse)
      {
        SelectorListObj selector = arguments[0]->assertSelector(ctx, "selector");
        return Listize::listize(selector);
      }

    }

  }

}
