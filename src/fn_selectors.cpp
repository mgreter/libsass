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
        return SASS_MEMORY_NEW(String_Constant, "[pstate]", "selector-nest");
      }

      BUILT_IN_FN(append)
      {

        std::vector<ValueObj> arglist = arguments[0]->asVector();

        // Not enough parameters
        if (arglist.empty()) {
          throw Exception::SassRuntimeException(
            "$selectors: At least one selector must be passed.",
            pstate);
        }

        std::vector<SelectorListObj> selectors;
        for (ValueObj& arg : arglist) {
          if (Cast<Null>(arg)) {
            throw Exception::SassRuntimeException( // "$selectors: "
              "null is not a valid selector: it must be a string,\n"
              "a list of strings, or a list of lists of strings for 'selector-append'",
              pstate);
          }
          std::string text = arg->to_css();
          ParserState state(arg->pstate());
          char* str = sass_copy_c_string(text.c_str());
          ctx.strings.push_back(str);
          SelectorParser p2(ctx, str, state.path, state.file);
          selectors.push_back(p2.parse());
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
                throw Exception::SassRuntimeException(
                  "Can't append " + child->to_css() + " to " +
                  reduced->to_css() + ".", pstate);
              }
              complex->at(0) = compound;
            }
            else {
              throw Exception::SassRuntimeException(
                "Can't append " + child->to_css() + " to " +
                reduced->to_css() + ".", pstate);
            }
          }
          Backtraces traces;
          reduced = cp->resolveParentSelectors(reduced, traces, false);
        }

        return Listize::listize(reduced);

      }

      BUILT_IN_FN(extend)
      {
        Backtraces traces;
        SelectorListObj selector = arguments[0]->assertSelector(ctx, "selector");
        SelectorListObj target = arguments[1]->assertSelector(ctx, "extendee");
        SelectorListObj source = arguments[2]->assertSelector(ctx, "extender");
        SelectorListObj result = Extender::extend(selector, source, target, traces);
        return Listize::listize(result);
      }

      BUILT_IN_FN(replace)
      {
        Backtraces traces;
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
          selector->pstate(), {}, SASS_COMMA);
        for (size_t i = 0, L = selector->length(); i < L; ++i) {
          const SimpleSelectorObj& ss = selector->get(i);
          std::string ss_string = ss->to_string();
          l->append(SASS_MEMORY_NEW(String_Quoted, ss->pstate(), ss_string));
        }
        return l;

      }

      BUILT_IN_FN(parse)
      {
        SelectorListObj selector = arguments[0]->assertSelector(ctx, "selector");
        return Listize::listize(selector);
      }

    }

    Signature selector_nest_sig = "selector-nest($selectors...)";
    BUILT_IN(selector_nest)
    {
      List* arglist = ARGLIST("$selectors");

      // Not enough parameters
      if (arglist->length() == 0) {
        error(
          "$selectors: At least one selector must be passed for `selector-nest'",
          pstate, traces);
      }

      // Parse args into vector of selectors
      SelectorStack parsedSelectors;
      for (size_t i = 0, L = arglist->length(); i < L; ++i) {
        Expression_Obj exp = Cast<Expression>(arglist->value_at_index(i));
        if (exp->concrete_type() == Expression::NULL_VAL) {
          error( // "$selectors: "
            "null is not a valid selector: it must be a string,\n"
            "a list of strings, or a list of lists of strings for 'selector-nest'",
            pstate, traces);
        }
        if (String_Constant_Obj str = Cast<String_Constant>(exp)) {
          str->quote_mark(0);
        }
        std::string exp_src = exp->to_string(ctx.c_options);

        ParserState state(exp->pstate());
        char* str = sass_copy_c_string(exp_src.c_str());
        ctx.strings.push_back(str);
        SelectorParser p2(ctx, str, state.path, state.file);
        p2._allowParent = true;
        SelectorListObj sel = p2.parse();
        parsedSelectors.push_back(sel);
      }

      // Nothing to do
      if( parsedSelectors.empty() ) {
        return SASS_MEMORY_NEW(Null, pstate);
      }

      // Set the first element as the `result`, keep
      // appending to as we go down the parsedSelector vector.
      SelectorStack::iterator itr = parsedSelectors.begin();
      SelectorListObj& result = *itr;
      ++itr;

      for(;itr != parsedSelectors.end(); ++itr) {
        SelectorListObj& child = *itr;
        original_stack.push_back(result);
        SelectorListObj rv = child->resolveParentSelectors(result, traces);
        result->elements(rv->elements());
        original_stack.pop_back();
      }

      return Listize::listize(result);
    }

    Signature selector_append_sig = "selector-append($selectors...)";
    BUILT_IN(selector_append)
    {
      List* arglist = ARGLIST("$selectors");

      // Not enough parameters
      if (arglist->empty()) {
        error(
          "$selectors: At least one selector must be passed.",
          pstate, traces);
      }

      std::vector<SelectorListObj> selectors;
      for (ExpressionObj& arg : arglist->values()) {
        if (Cast<Null>(arg)) {
          error( // "$selectors: "
            "null is not a valid selector: it must be a string,\n"
            "a list of strings, or a list of lists of strings for 'selector-append'",
            pstate, traces);
        }
        std::string text = arg->to_css();
        ParserState state(arg->pstate());
        char* str = sass_copy_c_string(text.c_str());
        ctx.strings.push_back(str);
        SelectorParser p2(ctx, str, state.path, state.file);
        selectors.push_back(p2.parse());
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
          if (CompoundSelector* compound = component->getCompound()) {
            compound = _prependParent(compound);
            if (compound == nullptr) {
              error("Can't append " + child->to_css() + " to " +
                reduced->to_css() + ".", pstate, traces);
            }
            complex->at(0) = compound;
          }
          else {
            error("Can't append " + child->to_css() + " to " +
              reduced->to_css() + ".", pstate, traces);
          }
        }
        reduced = cp->resolveParentSelectors(reduced, traces, false);
      }

      return Listize::listize(reduced);
    }

    Signature selector_unify_sig = "selector-unify($selector1, $selector2)";
    BUILT_IN(selector_unify)
    {
      SelectorListObj selector1 = ARGSELS("$selector1");
      SelectorListObj selector2 = ARGSELS("$selector2");
      SelectorListObj result = selector1->unifyWith(selector2);
      return Listize::listize(result);
    }

    Signature simple_selectors_sig = "simple-selectors($selector)";
    BUILT_IN(simple_selectors)
    {
      CompoundSelectorObj sel = ARGSEL("$selector");

      SassList* l = SASS_MEMORY_NEW(SassList, sel->pstate(), {}, SASS_COMMA);

      for (size_t i = 0, L = sel->length(); i < L; ++i) {
        const SimpleSelectorObj& ss = sel->get(i);
        std::string ss_string = ss->to_string() ;
        l->append(SASS_MEMORY_NEW(String_Quoted, ss->pstate(), ss_string));
      }

      return l;
    }

    Signature selector_extend_sig = "selector-extend($selector, $extendee, $extender)";
    BUILT_IN(selector_extend)
    {
      SelectorListObj selector = ARGSELS("$selector");
      SelectorListObj target = ARGSELS("$extendee");
      SelectorListObj source = ARGSELS("$extender");
      SelectorListObj result = Extender::extend(selector, source, target, traces);
      return Listize::listize(result);
    }

    Signature selector_replace_sig = "selector-replace($selector, $original, $replacement)";
    BUILT_IN(selector_replace)
    {
      SelectorListObj selector = ARGSELS("$selector");
      SelectorListObj target = ARGSELS("$original");
      SelectorListObj source = ARGSELS("$replacement");
      SelectorListObj result = Extender::replace(selector, source, target, traces);
      return Listize::listize(result);
    }

    Signature selector_parse_sig = "selector-parse($selector)";
    BUILT_IN(selector_parse)
    {
      SelectorListObj selector = ARGSELS("$selector");
      return Listize::listize(selector);
    }

    Signature is_superselector_sig = "is-superselector($super, $sub)";
    BUILT_IN(is_superselector)
    {
      SelectorListObj sel_sup = ARGSELS("$super");
      SelectorListObj sel_sub = ARGSELS("$sub");
      bool result = sel_sup->isSuperselectorOf(sel_sub);
      return SASS_MEMORY_NEW(Boolean, pstate, result);
    }

  }

}
