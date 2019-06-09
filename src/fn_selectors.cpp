#include <numeric>

#include "parser.hpp"
#include "extend.hpp"
#include "extender.hpp"
#include "debugger.hpp"
#include "listize.hpp"
#include "fn_utils.hpp"
#include "fn_selectors.hpp"

namespace Sass {

  namespace Functions {
    /*
    std::string selectorString(std::vector<std::string> name) {
      std::string string = selectorStringOrNull();
      if (string != null) return string;

      throw _exception(
        "$this is not a valid selector: it must be a string,\n"
        "a list of strings, or a list of lists of strings.",
        name);
    }

    SelectorList_Obj assertSelector(std::string name, bool allowParent = false) {
      std::string string(selectorString(name));
      try {
        return SelectorList.parse(string, allowParent: allowParent);
      } on SassFormatException catch (error) {
        // TODO(nweiz): colorize this if we're running in an environment where
        // that works.
        throw _exception(error.toString());
      }
    }
    */


    Signature selector_nest_sig = "selector-nest($selectors...)";
    BUILT_IN(selector_nest)
    {
      List* arglist = ARG("$selectors", List);

      // Not enough parameters
      if( arglist->length() == 0 )
        error("$selectors: At least one selector must be passed for `selector-nest'", pstate, traces);

      // Parse args into vector of selectors
      SelectorStack2 parsedSelectors;
      for (size_t i = 0, L = arglist->length(); i < L; ++i) {
        Expression_Obj exp = Cast<Expression>(arglist->value_at_index(i));
        if (exp->concrete_type() == Expression::NULL_VAL) {
          std::stringstream msg;
          msg << "$selectors: null is not a valid selector: it must be a string,\n";
          msg << "a list of strings, or a list of lists of strings for 'selector-nest'";
          error(msg.str(), pstate, traces);
        }
        if (String_Constant_Obj str = Cast<String_Constant>(exp)) {
          str->quote_mark(0);
        }
        std::string exp_src = exp->to_string(ctx.c_options);
        Selector_List_Obj sel = Parser::parse_selector(exp_src.c_str(), ctx, traces);
        parsedSelectors.push_back(sel->toSelList());
      }

      // Nothing to do
      if( parsedSelectors.empty() ) {
        return SASS_MEMORY_NEW(Null, pstate);
      }

      // Set the first element as the `result`, keep appending to as we go down the parsedSelector vector.
      SelectorStack2::iterator itr = parsedSelectors.begin();
      SelectorList_Obj result = *itr;
      ++itr;

      for(;itr != parsedSelectors.end(); ++itr) {
        SelectorList_Obj child = *itr;
        std::vector<ComplexSelector_Obj> exploded;
        selector_stack.push_back(result);
        SelectorList_Obj rvs = child->resolve_parent_refs(selector_stack, traces);
        Selector_List_Obj rv = rvs->toSelectorList();
        selector_stack.pop_back();
        for (size_t m = 0, mLen = rv->length(); m < mLen; ++m) {
          exploded.push_back((*rv)[m]->toCplxSelector());
        }
        result->elements(exploded);
      }

      Listize listize;
      return Cast<Value>(result->toSelectorList()->perform(&listize));
    }
    /*
    CompoundSelector_Obj prependParent(CompoundSelector_Obj compound) {
      auto first = compound->first();
      // if (first is UniversalSelector) return null;
      if (Cast<Type_Selector>(first)) {
        if (first->ns() == "") return {};
        auto compound = SASS_MEMORY_NEW(CompoundSelector, "[tmp]");
        return CompoundSelector([
          ParentSelector(suffix:first.name.name),
            ...compound.components.skip(1)
        ]);
      }
      else {
        return CompoundSelector([ParentSelector(), ...compound.components]);
      }
    }

    SelectorList_Obj appendAndResolve(SelectorList_Obj parent, SelectorList_Obj child)
    {
      auto list = SASS_MEMORY_NEW(SelectorList, "[tmp]");
      for (auto complex : child->elements()) {
        CompoundOrCombinator_Obj component = complex->first();
        if (CompoundSelector_Obj compound = Cast<CompoundSelector>(component)) {
          auto newCompound = preapreParent(compound);
          if (newCompound.isNull()) {
            // error("Can0t append", pstate, traces);
          }
          auto cplx = SASS_MEMORY_NEW(ComplexSelector, "[tmp]");
          cplx->append(newCompound);
          for (size_t n = 1; n < complex->length(); n += 1) {
            cplx->append(complex->get(n));
          }
          list->append(cplx);
        }
        else {
          // error "Can appent"
        }
      }

      // return list->resolveParentSelectors(parent);
      return {};

    }
    */

    Signature selector_append_sig = "selector-append($selectors...)";
    BUILT_IN(selector_append)
    {
      List* arglist = ARG("$selectors", List);

      // Not enough parameters
      if (arglist->empty()) {
        error("$selectors: At least one selector must be "
          "passed for `selector-append'", pstate, traces);
      }

      // Parse args into vector of selectors
      SelectorStack2 parsedSelectors;
      parsedSelectors.push_back({});
      for (size_t i = 0, L = arglist->length(); i < L; ++i) {
        Expression_Obj exp = Cast<Expression>(arglist->value_at_index(i));
        if (exp->concrete_type() == Expression::NULL_VAL) {
          std::stringstream msg;
          msg << "$selectors: null is not a valid selector: it must be a string,\n";
          msg << "a list of strings, or a list of lists of strings for 'selector-append'";
          error(msg.str(), pstate, traces);
        }
        if (String_Constant* str = Cast<String_Constant>(exp)) {
          str->quote_mark(0);
        }
        std::string exp_src = exp->to_string();
        SelectorList_Obj sel = Parser::parse_selector2(exp_src.c_str(), ctx, traces,
                                                       exp->pstate(), pstate.src,
                                                       /*allow_parent=*/true);

        for (auto asd : sel->elements()) {
          // asd->chroots(false);
          if (asd->empty()) {
            std::cerr << "EMPTY\n";
            auto qwe = SASS_MEMORY_NEW(CompoundSelector, "[tmp]");
            // qwe->hasRealParent(true);
            asd->append(qwe);
          }
          if (CompoundSelector_Obj comp = Cast<CompoundSelector>(asd->first())) {
            comp->hasRealParent(true);
            asd->chroots(true);
          }
        }

        if (parsedSelectors.size() > 1) {

          if (!sel->canHaveRealParent()) {
            auto parent = parsedSelectors.back();
            for (auto asd : parent->elements()) {
              if (CompoundSelector_Obj comp = Cast<CompoundSelector>(asd->first())) {
                comp->hasRealParent(false);
              }
            }
            error("Can't append \"" + sel->to_string() + "\" to \"" +
            parent->to_string() + "\" for `selector-append'", pstate, traces);
          }

          // Build the resolved stack from the left. It's cheaper to directly
          // calculate and update each resolved selcted from the left, than to
          // recursively calculate them from the right side, as we would need
          // to go through the whole stack depth to build the final results.
          // E.g. 'a', 'b', 'x, y' => 'a' => 'a b' => 'a b x, a b y'
          // vs 'a', 'b', 'x, y' => 'x' => 'b x' => 'a b x', 'y' ...
          parsedSelectors.push_back(sel->resolve_parent_refs(parsedSelectors, traces, true));
        }
        else {
          parsedSelectors.push_back(sel);
        }
      }

      // Nothing to do
      if( parsedSelectors.empty() ) {
        return SASS_MEMORY_NEW(Null, pstate);
      }

      return Cast<Value>(Listize::perform(parsedSelectors.back()));



      /*

      // Set the first element as the `result`, keep appending to as we go down the parsedSelector vector.
      SelectorStack::iterator itr = parsedSelectors.begin();
      Selector_List_Obj result = *itr;
      ++itr;

      for(;itr != parsedSelectors.end(); ++itr) {
        Selector_List_Obj child = *itr;
        std::vector<Complex_Selector_Obj> newElements;

        // For every COMPLEX_SELECTOR in `result`
        // For every COMPLEX_SELECTOR in `child`
          // let parentSeqClone equal a copy of result->elements[i]
          // let childSeq equal child->elements[j]
          // Append all of childSeq head elements into parentSeqClone
          // Set the innermost tail of parentSeqClone, to childSeq's tail
        // Replace result->elements with newElements
        for (size_t i = 0, resultLen = result->length(); i < resultLen; ++i) {
          for (size_t j = 0, childLen = child->length(); j < childLen; ++j) {
            Complex_Selector_Obj parentSeqClone = (*result)[i]->clone();
            Complex_Selector_Obj childSeq = (*child)[j];
            Complex_Selector_Obj base = childSeq->tail();

            // Must be a simple sequence
            if( childSeq->combinator() != Complex_Selector::Combinator::ANCESTOR_OF ) {
              error("Can't append \"" + childSeq->to_string() + "\" to \"" +
                parentSeqClone->to_string() + "\" for `selector-append'", pstate, traces);
            }

            // Cannot be a Universal selector
            Type_Selector_Obj pType = Cast<Type_Selector>(childSeq->head()->first());
            if(pType && pType->name() == "*") {
              error("Can't append \"" + childSeq->to_string() + "\" to \"" +
                parentSeqClone->to_string() + "\" for `selector-append'", pstate, traces);
            }

            // TODO: Add check for namespace stuff

            Complex_Selector* lastComponent = parentSeqClone->mutable_last();
            if (lastComponent->head() == nullptr) {
              std::string msg = "Parent \"" + parentSeqClone->to_string() + "\" is incompatible with \"" + base->to_string() + "\"";
              error(msg, pstate, traces);
            }
            lastComponent->head()->concat(base->head());
            lastComponent->tail(base->tail());

            newElements.push_back(parentSeqClone);
          }
        }

        result->elements(newElements);
      }

      Listize listize;
      return Cast<Value>(result->perform(&listize));
      */
    }

    Signature selector_unify_sig = "selector-unify($selector1, $selector2)";
    BUILT_IN(selector_unify)
    {
      SelectorList_Obj selector1 = ARGSELS2("$selector1");
      SelectorList_Obj selector2 = ARGSELS2("$selector2");
      SelectorList_Obj result = selector1->unify_with(selector2);
      return Cast<Value>(Listize::perform(result));
    }

    Signature simple_selectors_sig = "simple-selectors($selector)";
    BUILT_IN(simple_selectors)
    {
      CompoundSelector_Obj sel = ARGSEL2("$selector");

      List* l = SASS_MEMORY_NEW(List, sel->pstate(), sel->length(), SASS_COMMA);

      for (size_t i = 0, L = sel->length(); i < L; ++i) {
        Simple_Selector_Obj ss = (*sel)[i];
        std::string ss_string = ss->to_string() ;
        l->append(SASS_MEMORY_NEW(String_Quoted, ss->pstate(), ss_string));
      }

      return l;
    }

    bool isOriginal(ComplexSelector_Obj complex)
    {
      return false;
    }

    Signature selector_trim_sig = "selector-trim($selectors)";
    BUILT_IN(selector_trim)
    {
      SelectorList_Obj  selector = ARGSELS2("$selectors");
      Extender extender(Extender::NORMAL, traces);
      std::vector<ComplexSelector_Obj> list;
      list.insert(list.begin(), selector->begin(), selector->end());
      SelectorList_Obj result = SASS_MEMORY_NEW(SelectorList, ParserState("{tmp}"));
      result->concat(extender.trim(list, isOriginal));
      return Cast<Value>(Listize::perform(result));

    }

    Signature selector_extend_sig = "selector-extend($selector, $extendee, $extender)";
    BUILT_IN(selector_extend)
    {
      SelectorList_Obj selector = ARGSELS2("$selector");
      SelectorList_Obj target = ARGSELS2("$extendee");
      SelectorList_Obj source = ARGSELS2("$extender");
      SelectorList_Obj result = Extender::extend(selector, source, target, traces);
      return Cast<Value>(Listize::perform(result));
    }

    Signature selector_replace_sig = "selector-replace($selector, $original, $replacement)";
    BUILT_IN(selector_replace)
    {
      SelectorList_Obj selector = ARGSELS2("$selector");
      SelectorList_Obj target = ARGSELS2("$original");
      SelectorList_Obj source = ARGSELS2("$replacement");
      SelectorList_Obj result = Extender::replace(selector, source, target, traces);
      return Cast<Value>(Listize::perform(result));
    }

    Signature selector_parse_sig = "selector-parse($selector)";
    BUILT_IN(selector_parse)
    {
      SelectorList_Obj selector = ARGSELS2("$selector");
      return Cast<Value>(Listize::perform(selector));
    }

    Signature is_superselector_sig = "is-superselector($super, $sub)";
    BUILT_IN(is_superselector)
    {
      SelectorList_Obj sel_sup = ARGSELS2("$super");
      SelectorList_Obj sel_sub = ARGSELS2("$sub");
      bool result = sel_sup->isSuperselectorOf(sel_sub);
      return SASS_MEMORY_NEW(Boolean, pstate, result);
    }

  }

}
