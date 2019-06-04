// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include "tsl/ordered_map.h"
#include "tsl/ordered_set.h"

#include "ast_fwd_decl.hpp"
#include "extension.hpp"
#include "extender.hpp"
#include "extend.hpp"
#include "context.hpp"
#include "backtrace.hpp"
#include "paths.hpp"
#include "debugger.hpp"
#include "parser.hpp"
#include "expand.hpp"
#include "node.hpp"
#include "sass_util.hpp"
#include "dart_helpers.hpp"
#include "remove_placeholders.hpp"
#include "debugger.hpp"
#include "debug.hpp"
#include <iostream>
#include <deque>
#include <set>

namespace Sass {

  SelectorList_Obj Extender::extend(SelectorList_Obj selector, SelectorList_Obj source, SelectorList_Obj targets)
  {
    return _extendOrReplace(selector, source, targets, ExtendMode::TARGETS);
  }

  SelectorList_Obj Extender::replace(SelectorList_Obj selector, SelectorList_Obj source, SelectorList_Obj targets)
  {
    return _extendOrReplace(selector, source, targets, ExtendMode::REPLACE);
  }

  SelectorList_Obj Extender::_extendOrReplace(SelectorList_Obj selector, SelectorList_Obj source, SelectorList_Obj targets, ExtendMode mode)
  {
    ExtSelExtMapEntry extenders;

    for (auto complex : source->elements()) {
      // std::cerr << "REGISTER CPLX " << debug_vec(complex) << "\n";
      extenders[complex] = Extension2(complex);
    }

    // std::cerr << "MAP: " << debug_vec(extenders) << "\n";

    for (auto complex : targets->elements()) {

      if (complex->length() != 1) {
        // throw "can't extend complex selector $complex."
        // std::cerr << "MUST THROW\n";
      }

      if (auto compound = Cast<CompoundSelector>(complex->first())) {

        ExtSelExtMap extensions;

        for (auto simple : compound->elements()) {
          extensions[simple] = extenders;
        }

        // std::cerr << "EXTS: " << debug_vec(extensions) << "\n";

        Extender extender(mode);

        if (!selector->is_invisible()) {
          for (auto sel : selector->elements()) {
            extender.originals.insert(sel);
          }
        }

        // std::cerr << "ORIGINALS: " << debug_vec(extender.originals) << "\n";

        selector = extender.extendList(selector, extensions);

        // std::cerr << "GOT BACK2 " << debug_vec(selector) << "\n";

      }

    }

    // std::cerr << "BEFORE END\n";

    return selector;

    exit(1);

    /*

    var extenders = Map<ComplexSelector, Extension>.fromIterable(
      source.components,
      value: (complex) = > Extension.oneOff(complex as ComplexSelector));
      */

    // return _extendOrReplace(selector, source, targets, ExtendMode.REPLACE);
  }

  template <class T, class U>
  bool setContains(T key, std::set<T, U> set)
  {
    return set.find(key) != set.end();
  }

  SelectorList_Obj Extender::extendList(SelectorList_Obj list, ExtSelExtMap extensions)
  {

    // std::cerr << "in extend list " << debug_vec(list) << "\n";

    // This could be written more simply using [List.map], but we want to
    // avoid any allocations in the common case where no extends apply.
    std::vector<ComplexSelector_Obj> extended;
    for (size_t i = 0; i < list->length(); i++) {
      ComplexSelector_Obj complex = list->get(i);
      // std::cerr << "CPLX IN " << std::string(complex) << "\n";
      std::vector<ComplexSelector_Obj> result =
        extendComplex(complex, extensions);
      // std::cerr << "CPLX RV " << debug_vec(result) << "\n";
      if (result.empty()) {
        if (!extended.empty()) {
          // std::cerr << "CPLX ADD " << debug_vec(complex) << "\n";
          extended.push_back(complex);
        }
      }
      else {
        if (extended.empty()) {
          for (size_t n = 0; n < i; n += 1) {
            extended.push_back(list->get(n));
          }
        }
        for (auto sel : result) {
          extended.push_back(sel);
        }
      }
    }

    if (extended.empty()) {
      return list;
    }

    // std::cerr << "EXTENDED " << debug_vec(extended) << "\n";

    auto tt = trim(extended, originals);

    // std::cerr << "TRIMMED " << debug_vec(tt) << "\n";

    SelectorList_Obj rv = SASS_MEMORY_NEW(SelectorList, list->pstate());
    rv->concat(tt);
    return rv;
  }

  std::vector<ComplexSelector_Obj> Extender::extendComplex(ComplexSelector_Obj complex, ExtSelExtMap extensions)
  {

    // The complex selectors that each compound selector in [complex.components]
    // can expand to.
    //
    // For example, given
    //
    //     .a .b {...}
    //     .x .y {@extend .b}
    //
    // this will contain
    //
    //     [
    //       [.a],
    //       [.b, .x .y]
    //     ]
    //
    // This could be written more simply using [List.map], but we want to avoid
    // any allocations in the common case where no extends apply.

    std::vector<ComplexSelector_Obj> wwrv;

    std::vector<std::vector<ComplexSelector_Obj>> extendedNotExpanded;
    bool isOriginal = originals.find(complex) != originals.end();
    for (size_t i = 0; i < complex->length(); i += 1) {
      CompoundOrCombinator_Obj component = complex->get(i);
      if (CompoundSelector_Obj compound = Cast<CompoundSelector>(component)) {
        // std::cerr << "COMP IN " << debug_vec(compound) << "\n";
        std::vector<ComplexSelector_Obj> extended = extendCompound(compound, extensions, isOriginal);
        // std::cerr << "COMP RV " << debug_vec(extended) << "\n";
        if (extended.empty()) {
          // std::cerr << "ADD AS EXT IS EMPTY\n";
          // Note: dart-sass checks for null!?
          // if (!extendedNotExpanded.empty()) {
            extendedNotExpanded.push_back({
              compound->wrapInComplex()
            });
          // }
        }
        else {

          // Note: dart-sass checks for null!?
          if (extendedNotExpanded.empty()) {
            for (size_t n = 0; n < i; n++) {
              if (CompoundSelector_Obj compound = Cast<CompoundSelector>(complex->at(n))) {
                // std::cerr << "ADD ITEM " << n << "\n";
                extendedNotExpanded.push_back({
                  compound->wrapInComplex()
                });
              }
            }
          }
          extendedNotExpanded.push_back(extended);
          // std::cerr << "EXT NOT EMPTY " << debug_vec(extendedNotExpanded) << "\n";
        }
      }
      else {
        // Note: dart-sass checks for null!?
        if (!extendedNotExpanded.empty()) {
          extendedNotExpanded.push_back({
            compound->wrapInComplex()
          });
        }
      }
    }

    // Note: dart-sass checks for null!?
    if (extendedNotExpanded.empty()) return {};

    bool first = true;

    // std::cerr << "EXT WEAVE " << debug_vec(extendedNotExpanded) << "\n";

    std::vector<std::vector<ComplexSelector_Obj>>
      sels = paths(extendedNotExpanded);
    
    // std::cerr << "EXT PATHS " << debug_vec(sels) << "\n";

    for (std::vector<ComplexSelector_Obj> path : sels) {
      std::vector<std::vector<CompoundOrCombinator_Obj>> qwe;
      for (ComplexSelector_Obj sel : path) {
        qwe.insert(qwe.end(), sel->elements());
      }
      // std::cerr << "EXT WIN " << debug_vec(qwe) << "\n";
      std::vector<std::vector<CompoundOrCombinator_Obj>> ww = weave(qwe);
      // std::cerr << "EXT WWWW " << debug_vec(ww) << "\n";
      // qwe.insert(qwe.begin(), path)

      // path.any((inputComplex) = > inputComplex.lineBreak)

      for (std::vector<CompoundOrCombinator_Obj> components : ww) {

        ComplexSelector_Obj cplx = SASS_MEMORY_NEW(ComplexSelector, "[ext]");
        cplx->hasPreLineFeed(complex->hasPreLineFeed());
        cplx->elements(components);

        // Make sure that copies of [complex] retain their status
        // as "original" selectors. This includes selectors that
        // are modified because a :not() was extended into.
        if (first && originals.find(complex) != originals.end()) {
          // std::cerr << "ADD " << debug_vec(cplx) << "\n";
          originals.insert(cplx);
        }
        first = false;

        wwrv.push_back(cplx);

      }

    }

    // std::cerr << "RV CPLX " << debug_vec(wwrv) << "\n";

    return wwrv;
  }

  Extension2 extensionForSimple(Simple_Selector_Obj simple)
  {
    Extension2 extension(simple->wrapInComplex());
    // specificity: _sourceSpecificity[simple]
    extension.isOriginal = true;
    return extension;
  }

  Extension2 extensionForCompound(std::vector<Simple_Selector_Obj> simples)
  {
    CompoundSelector_Obj compound = SASS_MEMORY_NEW(CompoundSelector, ParserState("[ext]"));
    Extension2 extension(compound->concat(simples)->wrapInComplex());
    // specificity: _sourceSpecificity[simple]
    extension.isOriginal = true;
    return extension;
  }

  std::vector<ComplexSelector_Obj> Extender::extendCompound(CompoundSelector_Obj compound, ExtSelExtMap extensions, bool inOriginal)
  {

    // If there's more than one target and they all need to
    // match, we track which targets are actually extended.
    // bool targetsUsed = mode == ExtendMode::NORMAL || extensions.length < 2
    //   ? null
    //   : Set<SimpleSelector>();
    ExtSmplSelSet targetsUsed;

    // std::cerr << "COMP IN " << std::string(compound) << "\n";
    // std::cerr << "COMP EXT " << debug_vec(extensions) << "\n";

    std::vector<ComplexSelector_Obj> result;

    // The complex selectors produced from each component of [compound].
    std::vector<std::vector<Extension2>> options;

    for (size_t i = 0; i < compound->length(); i++) {
      Simple_Selector_Obj simple = compound->get(i);
      auto extended = extendSimple(simple, extensions, targetsUsed);
      // std::cerr << "targetUsed after " << debug_vec(targetsUsed) << "\n";
      // std::cerr << "  " << debug_vec(extended) << "\n";
      if (extended.empty()/* == null */) {
        // std::cerr << "Add option 1\n";
        if (!options.empty()) {
          options.push_back({ extensionForSimple(simple) });
        }
      }
      else {
        if (options.empty() /* == null */) {
          // options = [];
          if (i != 0) {
            std::vector<Simple_Selector_Obj> in;
            for (size_t n = 0; n < i; n += 1) {
              in.push_back(compound->get(n));
            }
            // std::cerr << "Add option 2 " << debug_vec(in) << "\n";
            options.push_back({ extensionForCompound(in) });
          }
        }
        // std::cerr << "Add option 3\n";
        options.insert(options.end(), extended.begin(), extended.end());
      }
    }

    if (options.empty()) {
      // std::cerr << "FAIL: OPTIONS IS NULL\n";
      return {};
    }

    // std::cerr << "OPTIONS " << debug_vec(options) << "\n";

    // std::cerr << "CHECK HERE " << targetsUsed.size() << " vs " << extensions.size() << "\n";

    // If [_mode] isn't [ExtendMode.normal] and we didn't use all
    // the targets in [extensions], extension fails for [compound].
    if (targetsUsed.size() != extensions.size()) {
      if (!targetsUsed.empty()) {
        // std::cerr << "FAIL: SIZE MISMATCH\n";
        return {};
      }
    }

    // Optimize for the simple case of a single simple
    // selector that doesn't need any unification.
    if (options.size() == 1) {
      std::vector<Extension2> exts = options[0];
      for (size_t n = 0; n < exts.size(); n += 1) {
        // state.assertCompatibleMediaContext(mediaQueryContext);
        result.push_back(exts[n].extender);
      }
      // std::cerr << "EASY CASE\n";
      return result;
    }

    // Find all paths through [options]. In this case, each path represents a
    // different unification of the base selector. For example, if we have:
    //
    //     .a.b {...}
    //     .w .x {@extend .a}
    //     .y .z {@extend .b}
    //
    // then [options] is `[[.a, .w .x], [.b, .y .z]]` and `paths(options)` is
    //
    //     [
    //       [.a, .b],
    //       [.a, .y .z],
    //       [.w .x, .b],
    //       [.w .x, .y .z]
    //     ]
    //
    // We then unify each path to get a list of complex selectors:
    //
    //     [
    //       [.a.b],
    //       [.y .a.z],
    //       [.w .x.b],
    //       [.w .y .x.z, .y .w .x.z]
    //     ]

    bool first = mode != ExtendMode::REPLACE;
    std::vector<ComplexSelector_Obj> unifiedPaths;
    // std::cerr << "from opts " << debug_vec(options) << "\n";
    std::vector<std::vector<Extension2>> prePaths = paths(options);
    // std::cerr << "prePaths " << debug_vec(prePaths) << "\n";
    for (size_t i = 0; i < prePaths.size(); i += 1) {
      std::vector<std::vector<CompoundOrCombinator_Obj>> complexes;
      std::vector<Extension2> path = prePaths.at(i);
      if (first) {
        // The first path is always the original selector. We can't just
        // return [compound] directly because pseudo selectors may be
        // modified, but we don't have to do any unification.
        first = false;
        CompoundSelector_Obj mergedSelector =
          SASS_MEMORY_NEW(CompoundSelector, "[ext]");
        for (size_t n = 0; n < path.size(); n += 1) {
          ComplexSelector_Obj sel = path[n].extender;
          // std::cerr << "BEFORE " << debug_vec(sel) << "\n";
          if (CompoundSelector_Obj compound = Cast<CompoundSelector>(sel->last())) {
            mergedSelector->concat(compound->elements());
          }
        }
        complexes.push_back({ mergedSelector });
        // std::cerr << "FIRST " << debug_vec(complexes) << "\n";
      }
      else {
        std::vector<Simple_Selector_Obj> originals;
        std::vector<std::vector<CompoundOrCombinator_Obj>> toUnify;

        for (auto state : path) {
          if (state.isOriginal) {
            ComplexSelector_Obj sel = state.extender;
            if (CompoundSelector_Obj compound = Cast<CompoundSelector>(sel->last())) {
              originals.insert(originals.begin(), compound->begin(), compound->end());
            }
          }
          else {
            toUnify.push_back(state.extender->elements());
          }
        }

        if (!originals.empty()) {

          CompoundSelector_Obj merged =
            SASS_MEMORY_NEW(CompoundSelector, "[ext]");
          merged->concat(originals);
          toUnify.insert(toUnify.begin(), { merged });
        }

        complexes = unifyComplex(toUnify);
        if (complexes.empty()) return {};

      }

      bool lineBreak = false;
      // var specificity = _sourceSpecificityFor(compound);
      for (auto state : path) {
        // state.assertCompatibleMediaContext(mediaQueryContext);
        lineBreak = lineBreak || state.extender->hasPreLineFeed();
        // specificity = math.max(specificity, state.specificity);
      }

      // std::vector<ComplexSelector_Obj> unifiedPath;
      for (std::vector<CompoundOrCombinator_Obj> components : complexes) {
        auto sel = SASS_MEMORY_NEW(ComplexSelector, "[ext]");
        sel->hasPreLineFeed(lineBreak);
        sel->elements(components);
        unifiedPaths.push_back(sel);
      }
      // unifiedPaths.push_back(unifiedPath);

    }

    // std::cerr << "----- " << debug_vec(unifiedPaths) << "\n";

    return unifiedPaths;
  }

  std::vector<Extension2> Extender::extendWithoutPseudo(Simple_Selector_Obj simple, ExtSelExtMap extensions, ExtSmplSelSet& targetsUsed) {

    auto ext = extensions.find(simple);
    if (ext == extensions.end()) return {};
    auto extenders = ext->second;
    targetsUsed.insert(simple);
    if (mode == ExtendMode::REPLACE) {
      auto rv = mapValues(extenders);
      // std::cerr << "HAS REPLACE MODE\n";
      return rv;
    }

    std::vector<Extension2> result;
    result.push_back(extensionForSimple(simple));
    for (const auto& extender : extenders) {
      result.push_back(extender.second);
    }
    //    if (_mode == ExtendMode.replace) return extenders.values.toList();

    return result;
  }

  std::vector<std::vector<Extension2>> Extender::extendSimple(Simple_Selector_Obj simple, ExtSelExtMap extensions, ExtSmplSelSet& targetsUsed) {

    if (Pseudo_Selector_Obj pseudo = Cast<Pseudo_Selector>(simple)) {
      // if (simple.selector != null) // Implement/Checks what this does?
      auto extended = extendPseudo(pseudo, extensions);
      // std::cerr << "extended pseudo " << debug_vec(extended) << "\n";

      std::vector<std::vector<Extension2>> rv;
      for (auto extend : extended) {
        auto vec = extendWithoutPseudo(extend, extensions, targetsUsed);
        if (vec.empty()) { vec = { extensionForSimple(extend) }; }
        rv.insert(rv.end(), { vec });
      }
      // std::cerr << "EARLY RET " << debug_vec(rv) << "\n";
      return rv;
    }

    std::vector<Extension2> result =
      extendWithoutPseudo(simple, extensions, targetsUsed);


    if (result.empty()) return {};
    return { result };
  }

  std::vector<ComplexSelector_Obj> extendPseudoComplex(ComplexSelector_Obj complex, Pseudo_Selector_Obj pseudo) {

    if (complex->length() != 1) return { complex };
    auto compound = Cast<CompoundSelector>(complex->get(0));
    if (compound == NULL) return { complex };

    if (compound->length() != 1) return { complex };
    auto innerPseudo = Cast<Pseudo_Selector>(compound->get(0));
    if (innerPseudo == NULL) return { complex };
    if (innerPseudo->selector()) return { complex };

    std::string name(pseudo->normalized());

    if (name == "not") {
      // In theory, if there's a `:not` nested within another `:not`, the
      // inner `:not`'s contents should be unified with the return value.
      // For example, if `:not(.foo)` extends `.bar`, `:not(.bar)` should
      // become `.foo:not(.bar)`. However, this is a narrow edge case and
      // supporting it properly would make this code and the code calling it
      // a lot more complicated, so it's not supported for now.
      if (innerPseudo->normalized() != "matches") return {};
      return toSelectorList(innerPseudo->selector())->elements();
    }
    else if (name == "matches" && name == "any" && name == "current" && name == "nth-child" && name == "nth-last-child") {
      // As above, we could theoretically support :not within :matches, but
      // doing so would require this method and its callers to handle much
      // more complex cases that likely aren't worth the pain.
      if (innerPseudo->name() != pseudo->name()) return {};
      if (*innerPseudo->expression() != *pseudo->expression()) return {};
      return toSelectorList(innerPseudo->selector())->elements();
    }
    else if (name == "has" && name == "host" && name == "host-context" && name == "slotted") {
      // We can't expand nested selectors here, because each layer adds an
      // additional layer of semantics. For example, `:has(:has(img))`
      // doesn't match `<div><img></div>` but `:has(img)` does.
      return { complex };
    }

    return {};

  }

  bool hasExactlyOne(ComplexSelector_Obj vec)
  {
    return vec->length() == 1;
  }

  bool hasMoreThanOne(Complex_Selector_Obj vec)
  {
    return vec->length() > 1;
  }


  std::vector<Pseudo_Selector_Obj> Extender::extendPseudo(Pseudo_Selector_Obj pseudo, ExtSelExtMap extensions)
  {
    auto sel = toSelectorList(pseudo->selector());
    // std::cerr << "CALL extend list\n";
    SelectorList_Obj extended = extendList(sel, extensions);
    // std::cerr << "CALLED extend list\n";


    if (!extended || !pseudo || !pseudo->selector()) {
      // std::cerr << "STUFF IS NULL\n";
      return {};
    }

    // debug_ast(extended);
    // debug_ast(pseudo->selector());
    // doDebug = true;

    if (*(pseudo->selector()) == *extended) {
      // std::cerr << "STUFF IS IDENTICAL\n";
      return {}; // null
    }

    doDebug = false;

    // For `:not()`, we usually want to get rid of any complex selectors because
    // that will cause the selector to fail to parse on all browsers at time of
    // writing. We can keep them if either the original selector had a complex
    // selector, or the result of extending has only complex selectors, because
    // either way we aren't breaking anything that isn't already broken.
    std::vector<ComplexSelector_Obj> complexes = extended->elements();

    if (pseudo->normalized() == "not") {
      bool b2 = hasAny(extended->elements(), hasExactlyOne);
      bool b1 = hasAny(pseudo->selector()->elements(), hasMoreThanOne);
      // std::cerr << "HAS PSEUDO NOT " << (b1?"true":"false") << ", " << (b2 ? "true" : "false") << "\n";
      if (!(b1 && b2)) {
        complexes.clear();
        for (auto complex : extended->elements()) {
          if (complex->length() <= 1) {
            complexes.push_back(complex);
          }
        }
        // std::cerr << "HAS PSEUDO NOT\n";
      }
    }

    // std::cerr << "AFTER WHERE " << debug_vec(complexes) << "\n";
    // std::cerr << "++++++++++++++++++++++++++++++++++++++++++++++\n";

    auto rv = expandListFn(complexes, extendPseudoComplex, pseudo);

    // std::cerr << "AFTER EXPAND " << debug_vec(rv) << "\n";

    std::vector<Pseudo_Selector_Obj> rvv;

    if (pseudo->normalized() == "not") {
      if (pseudo->selector()->length() == 1) {
        // std::cerr << "ADD WITH SELECTOR\n";
        for (size_t i = 0; i < complexes.size(); i += 1) {
          rvv.push_back(pseudo->withSelector(
            complexes[i]->wrapInList()
          ));
        }
        return rvv;
      }
    }

    SelectorList_Obj list = SASS_MEMORY_NEW(SelectorList, "[ext]");
    return { pseudo->withSelector(list->concat(complexes)) };

    // auto extended = extendList(pseudo.selector, extensions);
    // std::cerr << "IMPLEMENT extendPseudo\n";
    return {};
  }

  bool dontTrimComplex(ComplexSelector_Obj complex2, ComplexSelector_Obj complex1, size_t maxSpecificity)
  {
    Complex_Selector_Obj c1 = complex1->toComplexSelector();
    Complex_Selector_Obj c2 = complex2->toComplexSelector();
    return complex2->isSuperselectorOf(complex1);
  }


  // Rotates the element in list from [start] (inclusive) to [end] (exclusive)
  // one index higher, looping the final element back to [start].
  void rotateSlice(std::vector<ComplexSelector_Obj>& list, size_t start, size_t end) {
    auto element = list[end - 1];
    for (size_t i = start; i < end; i++) {
      auto next = list[i];
      list[i] = element;
      element = next;
    }
  }

  std::vector<ComplexSelector_Obj> Extender::trim(std::vector<ComplexSelector_Obj> selectors, ExtCplxSelSet set)
  {


    // Avoid truly horrific quadratic behavior.
    // TODO(nweiz): I think there may be a way to get perfect trimming 
    // without going quadratic by building some sort of trie-like
    // data structure that can be used to look up superselectors.
    // TODO(mgreter): Check how this perfoms in C++ (up the limit)
    if (selectors.size() > 100) return selectors;

    size_t cnt = 0;

    std::vector<ComplexSelector_Obj> result; size_t numOriginals = 0;
    // This is n² on the sequences, but only comparing between separate sequences
    // should limit the quadratic behavior. We iterate from last to first and reverse
    // the result so that, if two selectors are identical, we keep the first one.
  redo:

    if (cnt++ > 50) exit(0);

    for (size_t i = selectors.size() - 1; i != std::string::npos; i--) {

      // std::cerr << "LOOP " << i << "\n";
    // if (cnt++ > 10) break;
    ComplexSelector_Obj complex1 = selectors.at(i);
    if (set.find(complex1) != set.end()) {
      // std::cerr << "HAS ORG " << i << "\n";
      // Make sure we don't include duplicate originals, which could
      // happen if a style rule extends a component of its own selector.

      for (size_t j = 0; j < numOriginals; j++) {
        if (*result[j] == *complex1) {
          rotateSlice(result, 0, j + 1);
          // std::cerr << "REDO\n";
          goto redo;
        }
      }

      numOriginals++;
      // std::cerr << "ADD ORIGINAL " << i << "\n";
      // ToDo: convert to a dequeue?
      result.insert(result.begin(), complex1);
      // Do next iteration
      continue;
    }

    // The maximum specificity of the sources that caused [complex1]
    // to be generated. In order for [complex1] to be removed, there
    // must be another selector that's a superselector of it *and*
    // that has specificity greater or equal to this.
    size_t maxSpecificity = 0;
    for (CompoundOrCombinator_Obj component : complex1->elements()) {
      if (CompoundSelector_Obj compound = Cast<CompoundSelector>(component)) {
        // maxSpecificity = std::max(maxSpecificity, sourceSpecificityFor(compound));
      }
    }

    // Look in [result] rather than [selectors] for selectors after [i]. This
    // ensures we aren't comparing against a selector that's already been trimmed,
    // and thus that if there are two identical selectors only one is trimmed.
    // std::cerr << "CHECK ANY1\n";
    if (hasAny(result, dontTrimComplex, complex1, maxSpecificity)) {
      // std::cerr << "HAS RESULT " << i << "\n";
      continue;
    }
    // std::cerr << "CHECK ANY2\n";
    if (hasSubAny(selectors, i, dontTrimComplex, complex1, maxSpecificity)) {
      // std::cerr << "HAS SELECTOR " << i << "\n";
      continue;
    }

    // std::cerr << "ADD FOREIGN " << debug_vec(complex1) << "\n";
    result.insert(result.begin(), complex1);
    // std::cerr << "------------------------------\n";

  }

        return result;

  }

  // Removes elements from [selectors] if they're subselectors of other
  // elements. The [isOriginal] callback indicates which selectors are
  // original to the document, and thus should never be trimmed.
  std::vector<ComplexSelector_Obj> Extender::trim(std::vector<ComplexSelector_Obj> selectors, bool (*isOriginal)(ComplexSelector_Obj complex))
  {
    
    // Avoid truly horrific quadratic behavior.
    // TODO(nweiz): I think there may be a way to get perfect trimming 
    // without going quadratic by building some sort of trie-like
    // data structure that can be used to look up superselectors.
    // TODO(mgreter): Check how this perfoms in C++ (up the limit)
    if (selectors.size() > 100) return selectors;

    size_t cnt = 0;

    std::vector<ComplexSelector_Obj> result; size_t numOriginals = 0;
    // This is n² on the sequences, but only comparing between separate sequences
    // should limit the quadratic behavior. We iterate from last to first and reverse
    // the result so that, if two selectors are identical, we keep the first one.
    redo: for (size_t i = selectors.size() - 1; i != std::string::npos; i--) {

    // std::cerr << "LOOP " << i << "\n";
      if (cnt++ > 10) break;
      ComplexSelector_Obj complex1 = selectors.at(i);
      if (isOriginal(complex1)) {
        // std::cerr << "HAS ORG " << i << "\n";
        // Make sure we don't include duplicate originals, which could
        // happen if a style rule extends a component of its own selector.

        for (size_t j = 0; j < numOriginals; j++) {
          if (*result[j] == *complex1) {
            rotateSlice(result, 0, j + 1);
            goto redo;
          }
        }

        numOriginals++;
        // std::cerr << "ADD ORIGINAL " << i << "\n";
        // ToDo: convert to a dequeue?
        result.insert(result.begin(), complex1);
        // Do next iteration
        continue;
      }

      // The maximum specificity of the sources that caused [complex1]
      // to be generated. In order for [complex1] to be removed, there
      // must be another selector that's a superselector of it *and*
      // that has specificity greater or equal to this.
      size_t maxSpecificity = 0;
      for (CompoundOrCombinator_Obj component : complex1->elements()) {
        if (CompoundSelector_Obj compound = Cast<CompoundSelector>(component)) {
          // maxSpecificity = std::max(maxSpecificity, sourceSpecificityFor(compound));
        }
      }

      // Look in [result] rather than [selectors] for selectors after [i]. This
      // ensures we aren't comparing against a selector that's already been trimmed,
      // and thus that if there are two identical selectors only one is trimmed.
      if (hasAny(result, dontTrimComplex, complex1, maxSpecificity)) {
        // std::cerr << "HAS RESULT " << i << "\n";
        continue;
      }
      if (hasSubAny(selectors, i, dontTrimComplex, complex1, maxSpecificity)) {
        // std::cerr << "HAS SELECTOR " << i << "\n";
        continue;
      }

      // std::cerr << "ADD FOREIGN " << i << "\n";
      result.insert(result.begin(), complex1);

    }

    return result;

  }

}
