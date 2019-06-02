// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

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
    std::map<
      ComplexSelector_Obj, // key
      Extension2, // value
      OrderNodes // Order
    > extenders;

    for (auto complex : source->elements()) {
      extenders[complex] = Extension2(complex);
    }

    std::cerr << "MAP: " << debug_vec(extenders) << "\n";

    for (auto complex : targets->elements()) {

      if (complex->length() != 1) {
        // throw "can't extend complex selector $complex."
        std::cerr << "MUST THROW\n";
      }

      if (auto compound = Cast<CompoundSelector>(complex->first())) {

        std::map<
          Simple_Selector_Obj, // key
          std::map<
            ComplexSelector_Obj, // key
            Extension2, // value
            OrderNodes // Order
          >, // value
          OrderNodes // Order
        > extensions;

        for (auto simple : compound->elements()) {
          extensions[simple] = extenders;
        }

        std::cerr << "EXTS: " << debug_vec(extensions) << "\n";

        Extender extender(mode);

        if (!selector->is_invisible()) {
          for (auto sel : selector->elements()) {
            extender.originals.insert(sel);
          }
        }

        std::cerr << "ORIGS: " << debug_vec(extender.originals) << "\n";

        extender.extendList(selector, extensions);

      }

    }




    exit(1);

    /*

    var extenders = Map<ComplexSelector, Extension>.fromIterable(
      source.components,
      value: (complex) = > Extension.oneOff(complex as ComplexSelector));
      */

    // return _extendOrReplace(selector, source, targets, ExtendMode.REPLACE);
  }


  SelectorList_Obj Extender::extendList(SelectorList_Obj list, ExtSelExtMap extensions)
  {

    // This could be written more simply using [List.map], but we want to avoid
    // any allocations in the common case where no extends apply.
    std::vector<ComplexSelector_Obj> extended;
    for (size_t i = 0; i < list->length(); i++) {
      ComplexSelector_Obj complex = list->get(i);
      std::vector<ComplexSelector_Obj> result = extendComplex(complex, extensions);
    }

    return {};
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

    std::vector<std::vector<ComplexSelector_Obj>> extendedNotExpanded;
    bool isOriginal = originals.find(complex) != originals.end();
    for (size_t i = 0; i < complex->length(); i += 1) {
      CompoundOrCombinator_Obj component = complex->get(i);
      if (CompoundSelector_Obj compound = Cast<CompoundSelector>(component)) {
        std::vector<ComplexSelector_Obj> extended = extendCompound(compound, extensions, isOriginal);
        if (!extended.empty()) {
          // Note: dart-sass checks for null!?
          if (!extendedNotExpanded.empty()) {
            extendedNotExpanded.push_back({
              compound->wrapInComplex()
            });
          }
        }
        else {
          // Note: dart-sass checks for null!?
          if (!extendedNotExpanded.empty()) {
            for (size_t n = 0; n < i; n++) {
              if (CompoundSelector_Obj compound = Cast<CompoundSelector>(complex->at(n))) {
                extendedNotExpanded.push_back({
                  compound->wrapInComplex()
                });
              }
            }
          }
          extendedNotExpanded.push_back(extended);
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

    std::vector<std::vector<ComplexSelector_Obj>>
      sels = paths(extendedNotExpanded);

    for (std::vector<ComplexSelector_Obj> path : sels) {
      // weave()
    }

    return {};
  }

  std::vector<ComplexSelector_Obj> Extender::extendCompound(CompoundSelector_Obj compound, ExtSelExtMap extensions, bool inOriginal)
  {

    // If there's more than one target and they all need to
    // match, we track which targets are actually extended.
    // bool targetsUsed = mode == ExtendMode::NORMAL || extensions.length < 2
    //   ? null
    //   : Set<SimpleSelector>();
    ExtSmplSelSet targetsUsed;

    // The complex selectors produced from each component of [compound].
    std::vector<std::vector<Extension2>> options;

    for (size_t i = 0; i < compound->length(); i++) {
      Simple_Selector_Obj simple = compound->get(i);
      auto extended = extendSimple(simple, extensions, targetsUsed);
      if (extended.empty() /* == null */) {
        // options ? .add([_extensionForSimple(simple)]);
      }
      else {
        if (options.empty() /* == null */) {
          // options = [];
          if (i != 0) {
            // options.add([_extensionForCompound(compound.components.take(i))]);
          }
        }
      }
    }
    // if (options == null) return null;


    return {};
  }

  std::vector<std::vector<Extension2>> Extender::extendSimple(Simple_Selector_Obj simple, ExtSelExtMap extensions, ExtSmplSelSet targetsUsed) {

    if (Pseudo_Selector_Obj pseudo = Cast<Pseudo_Selector>(simple)) {
      // if (simple.selector != null) // Implement/Checks what this does?
      auto extended = extendPseudo(pseudo, extensions);
    }


    return {};
  }


  std::vector<Pseudo_Selector_Obj> Extender::extendPseudo(Pseudo_Selector_Obj pseudo, ExtSelExtMap extensions)
  {
    // auto extended = extendList(pseudo.selector, extensions);
    std::cerr << "IMPLEMENT extendPseudo\n";
    return {};
  }

  bool dontTrimComplex(ComplexSelector_Obj complex2, ComplexSelector_Obj complex1, size_t maxSpecificity)
  {
    Complex_Selector_Obj c1 = complex1->toComplexSelector();
    Complex_Selector_Obj c2 = complex2->toComplexSelector();
    return c2->is_superselector_of(c1);
      // false; // complex2.minSpecificity >= maxSpecificity && complex2->is_superselector_of(complex1));
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

      std::cerr << "LOOP " << i << "\n";
      if (cnt++ > 10) break;
      ComplexSelector_Obj complex1 = selectors.at(i);
      if (isOriginal(complex1)) {
        std::cerr << "HAS ORG " << i << "\n";
        // Make sure we don't include duplicate originals, which could
        // happen if a style rule extends a component of its own selector.

        for (size_t j = 0; j < numOriginals; j++) {
          if (*result[j] == *complex1) {
            rotateSlice(result, 0, j + 1);
            goto redo;
          }
        }

        numOriginals++;
        std::cerr << "ADD ORIGINAL " << i << "\n";
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
        std::cerr << "HAS RESULT " << i << "\n";
        continue;
      }
      if (hasSubAny(selectors, i, dontTrimComplex, complex1, maxSpecificity)) {
        std::cerr << "HAS SELECTOR " << i << "\n";
        continue;
      }

      std::cerr << "ADD FOREIGN " << i << "\n";
      result.insert(result.begin(), complex1);

    }

    return result;

  }

}
