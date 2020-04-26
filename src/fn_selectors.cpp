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
      if (TypeSelector * type = first->getTypeSelector()) {
        if (type->has_ns()) return nullptr;
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

        // Not enough parameters
        if (arguments[0]->lengthAsList() == 0) {
          throw Exception::SassRuntimeException2(
            "$selectors: At least one selector must be passed for `selector-nest'",
            logger642);
        }

        // Parse args into vector of selectors
        SelectorStack parsedSelectors;
        for (Value* exp : arguments[0]->iterator()) {
          if (exp->isNull()) {
            throw Exception::SassRuntimeException2( // "$selectors: "
              "null is not a valid selector: it must be a string,\n"
              "a list of strings, or a list of lists of strings.",
              logger642, exp->pstate());
          }
          if (StringObj str = exp->isString()) {
            str->hasQuotes(false);
          }
          sass::string exp_src = exp->to_string(ctx);

          SourceSpan state(exp->pstate());
          auto source = SASS_MEMORY_NEW(SourceItpl,
            std::move(exp_src), state);
          SelectorParser parser(ctx, source);
          parser._allowParent = true;
          SelectorListObj sel = parser.parse();
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
          BackTraces& traces = logger642;
          SelectorListObj rv = child->resolveParentSelectors(result, traces);
          result->elementsM(std::move(rv->elements()));
          // original_stack.pop_back();
        }

        return Listize::listize(result);

      }

      BUILT_IN_FN(append)
      {

        // Not enough parameters
        if (arguments[0]->lengthAsList() == 0) {
          throw Exception::SassRuntimeException2(
            "$selectors: At least one selector must be passed.",
            logger642);
        }

        sass::vector<SelectorListObj> selectors;
        for (Value* arg : arguments[0]->iterator()) {
          if (arg->isNull()) {
            throw Exception::SassRuntimeException2( // "$selectors: "
              "null is not a valid selector: it must be a string,\n"
              "a list of strings, or a list of lists of strings.",
              logger642, arg->pstate());
          }
          sass::string text(arg->to_css());
          SourceSpan state(arg->pstate());
          auto source = SASS_MEMORY_NEW(SourceItpl,
            std::move(text), state);
          SelectorParser parser(ctx, source, false);
          selectors.emplace_back(parser.parse());
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
                  logger642);
              }
              complex->at(0) = compound;
            }
            else {
              throw Exception::SassRuntimeException2(
                "Can't append " + child->to_css() + " to " +
                reduced->to_css() + ".",
                logger642);
            }
          }
          BackTraces& traces = logger642;
          reduced = cp->resolveParentSelectors(reduced, traces, false);
        }

        return Listize::listize(reduced);

      }

      BUILT_IN_FN(extend)
      {
        BackTraces& traces = logger642;
        // callStackFrame frame(traces, BackTrace(pstate, "selector-extend"));
        SelectorListObj selector = arguments[0]->assertSelector(ctx, "selector");
        SelectorListObj target = arguments[1]->assertSelector(ctx, "extendee");
        SelectorListObj source = arguments[2]->assertSelector(ctx, "extender");
        SelectorListObj result = Extender::extend(selector, source, target, traces);
        return Listize::listize(result);
      }

      BUILT_IN_FN(replace)
      {
        BackTraces& traces = logger642;
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
        List* l = SASS_MEMORY_NEW(List,
          selector->pstate(), sass::vector<ValueObj>(), SASS_COMMA);
        for (size_t i = 0, L = selector->length(); i < L; ++i) {
          const SimpleSelectorObj& ss = selector->get(i);
          sass::string ss_string = ss->to_string();
          l->append(SASS_MEMORY_NEW(String, ss->pstate(), ss_string));
        }
        return l;

      }

      BUILT_IN_FN(parse)
      {
        SelectorListObj selector = arguments[0]->assertSelector(ctx, "selector");
        return Listize::listize(selector);
      }

	    void registerFunctions(Context& ctx)
	    {

		    ctx.registerBuiltInFunction("selector-nest", "$selectors...", nest);
		    ctx.registerBuiltInFunction("selector-append", "$selectors...", append);
		    ctx.registerBuiltInFunction("selector-extend", "$selector, $extendee, $extender", extend);
		    ctx.registerBuiltInFunction("selector-replace", "$selector, $original, $replacement", replace);
		    ctx.registerBuiltInFunction("selector-unify", "$selector1, $selector2", unify);
		    ctx.registerBuiltInFunction("is-superselector", "$super, $sub", isSuper);
		    ctx.registerBuiltInFunction("simple-selectors", "$selector", simple);
		    ctx.registerBuiltInFunction("selector-parse", "$selector", parse);

	    }

    }

  }

}
