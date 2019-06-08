#include "extender.hpp"
#include "extender.hpp"
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
            // std::cerr << "INSERT ORIGINAL4 " << debug_vec(sel) << "\n";
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

  }

  // Adds [selector] to this extender, with [selectorSpan] as the span covering
  // the selector and [ruleSpan] as the span covering the entire style rule.
  // Extends [selector] using any registered extensions, then returns an empty
  // [ModifiableCssStyleRule] with the resulting selector. If any more relevant
  // extensions are added, the returned rule is automatically updated.
  // The [mediaContext] is the media query context in which the selector was
  // defined, or `null` if it was defined at the top level of the document.
  void Extender::addSelector(SelectorList_Obj selector)
  {

    SelectorList_Obj original = selector;
    // if (!originalSelector->isInvisible()) {
    // std::cerr << "ADD SELECTOR " << /* selector.ptr() <<  " - " << */ debug_vec(original) << "\n";

    // Remember all original complex selectors
    for (auto complex : selector->elements()) {
      // std::cerr << "INSERT ORIGINAL1 " << debug_vec(complex) << " - " << complex.ptr() << "\n";
      originals.insert(complex);
    }
    // }

    if (!extensions.empty()) {
      // std::cerr << "has extensions (" << debug_vec(original) << ") WITH " << debug_keys(extensions) << "\n";
      // ToDo: media context is passed here
      // ToDo: this can throw in dart sass
      // std::cerr << "  _extensions: " << debug_vec(extensions) << "\n";
      // std::cerr << "  _selectors: " << debug_keys(selectors) << "\n";
      auto res = extendList(original, extensions);
      // std::cerr << "selector extended " << debug_vec(res) << "\n";
      // std::cerr << "  _extensions: " << debug_vec(extensions) << "\n";
      selector->elements(res->elements());
      /*
      on SassException catch (error) {
        throw SassException(
            "From ${error.span.message('')}\n"
            "${error.message}",
            selectorSpan);
      }
      */
    }

    registerSelector(selector /*, rule*/);

    // std::cerr << "after reg sel - selectors " << debug_keys(selectors) << "\n";
    // std::cerr << "  " << debug_vec(extensions) << "\n";

    // this must exit
    if (!extensions.empty()) {
      // exit(0);
    }
  }

  // Registers the [SimpleSelector]s in [list]
  // to point to [rule] in [selectors].
  void Extender::registerSelector(SelectorList_Obj list)
  {
    // std::cerr << "REGSEL " << debug_vec(list) << "\n";
    if (list.isNull() || list->empty()) return;
    for (auto complex : list->elements()) {
      for (auto component : complex->elements()) {
        if (auto compound = component->getCompound()) {
          for (auto simple : compound->elements()) {
            // _selectors.putIfAbsent(simple, () = > Set()).add(rule);
            // std::cerr << "  GED " << debug_vec(simple) << "\n";
            selectors[simple].insert(list);
            if (auto pseudo = simple->getPseudoSelector()) {
              if (pseudo->selector2()) {
                registerSelector(pseudo->selector2() /*, rule */);
              }
              // simple is PseudoSelector && simple.selector != null) {
              // _registerSelector(simple.selector, rule);
            }
          }
        }
      }
    }

  }

  Extension2 mergeExtension(Extension2 lhs, Extension2 rhs)
  {
    /*
    if (left.extender != right.extender || left.target != right.target) {
      throw ArgumentError("$left and $right aren't the same extension.");
    }

    if (left.mediaContext != null &&
        right.mediaContext != null &&
        !listEquals(left.mediaContext, right.mediaContext)) {
      throw SassException(
          "From ${left.span.message('')}\n"
          "You may not @extend the same selector from within different media "
          "queries.",
          right.span);
    }

    */

    // If one extension is optional and doesn't add a
    // special media context, it doesn't need to be merged.
    // if (right.isOptional && right.mediaContext == null) return left;
    // if (left.isOptional && left.mediaContext == null) return right;

    if (rhs.isOptional) {
      // std::cerr << "MERGE LEFT " << debug_vec(lhs) << "\n";
      return lhs;
    }
    if (lhs.isOptional) {
      // std::cerr << "MERGE RIGHT " << debug_vec(rhs) << "\n";
      return rhs;
    }

    // std::cerr << "MUST MERGE EXTENSION\n";
    Extension2 rv(lhs);
    rv.isOptional = true;
    rv.isOriginal = false;
    return rv;


  }
  void mapAddAll2(ExtSelExtMap& dest, ExtSelExtMap& source)
  {
    for (auto it : source) {
      auto key = it.first;
      auto& inner = it.second;
      ExtSelExtMap::iterator dmap = dest.find(key);
      if (dmap == dest.end()) {
        // std::cerr << "ADD FULLY\n";
        dest[key] = inner;
      }
      else {
        for (auto it2 : inner) {
          ExtSelExtMapEntry& imap = dest[key];
          imap[it2.first] = it2.second;
        }
      }
    }
  }

  // ToDo: rename extender to parent, since it is not involved in extending stuff
  // ToDo: check why dart sass passes the ExtendRule around (is this the main selector?)
  void Extender::addExtension(SelectorList_Obj extender, Simple_Selector_Obj target, ExtendRule_Obj extend /*, Extension_Obj target *//*, media context */)
  {
    // std::cerr << "addExtension for " << debug_vec(target) << " in " << debug_vec(extender) << "\n";

    auto rules = selectors.find(target);
    bool hasRule = rules != selectors.end();

    ExtSelExtMapEntry newExtensions;

    auto existingExtensions = extensionsByExtender.find(target);
    bool hasExistingExtensions = existingExtensions != extensionsByExtender.end();

    if (hasRule) {
      // std::cerr << "addExtension already has rules\n";
    }

    if (hasExistingExtensions) {
      // std::cerr << "addExtension already has existingExtensions\n";
    }


    // bool hasExistingExtensions = hasExistingExtensions;
    // std::cerr << "PUT EXT1 " << debug_vec(target) << "\n";
    if (hasExistingExtensions) {
      // // std::cerr << "EXISTS " << existingExtensions->second.size() << "\n";
    }
    ExtSelExtMapEntry& sources = extensions[target];

    for (auto complex : extender->elements()) {
      Extension2 state(complex);
      // ToDo: fine-tune public API
      state.target = target;
      state.isOptional = extend->isOptional();

      // std::cerr << "SOURCES " << debug_keys(sources) << "\n";
      auto existingState = sources.find(complex);
      if (existingState != sources.end()) {
        // If there's already an extend from [extender] to [target],
        // we don't need to re-run the extension. We may need to
        // mark the extension as mandatory, though.
        // sources.insert(complex, mergeExtension(existingState->second, state);
        // std::cerr << "merge extensions\n";
        exit(1);
        continue;
      }

      sources[complex] = state;

      for (auto component : complex->elements()) {
        if (auto compound = component->getCompound()) {
          for (auto simple : compound->elements()) {
            // std::cerr << "PUT extensionsByExtender " << debug_vec(simple) << "\n";
            extensionsByExtender[simple].push_back(state);
            if (sourceSpecificity.find(simple) == sourceSpecificity.end()) {
              sourceSpecificity[simple] = complex->maxSpecificity();
              // std::cerr << "SETTTTING TO " << complex->maxSpecificity() << "\n";
            }
          }
        }
      }

      if (hasRule) {
        // std::cerr << "DADA RULES IS NOT NULL\n";
      }
      else if (hasExistingExtensions) {
        // std::cerr << "existingExtensions IS " << debug_vec(existingExtensions->second) << "\n";
      }
      else {
        // std::cerr << "BOTH ARE NULL\n";
      }

      if (hasRule || hasExistingExtensions) {
        // std::cerr << "HAS NEW EXT " << debug_vec(complex) << "\n";
        newExtensions[complex] = state;
      }

    }

    if (newExtensions.empty()) {
      // std::cerr << "DONE\n";
      return;
    }

    // std::cerr << "HAS NEW FOR " << debug_vec(newExtensions) << "\n";

    //     var newExtensionsByTarget = {target: newExtensions};

    ExtSelExtMap newExtensionsByTarget;
    newExtensionsByTarget[target] = newExtensions;

    existingExtensions = extensionsByExtender.find(target);
    // hasExistingExtensions = existingExtensions != extensionsByExtender.end();


    if (hasExistingExtensions && !existingExtensions->second.empty()) {
      // std::cerr << "DO MAP " << debug_vec(existingExtensions->second) << "\n";
      // std::cerr << "  _extensions2 " << debug_vec(extensions) << "\n";
      auto additionalExtensions =
        extendExistingExtensions(existingExtensions->second, newExtensionsByTarget);
      // std::cerr << "  _extensions3 " << debug_vec(extensions) << "\n";
      if (!additionalExtensions.empty()) {
        // std::cerr << "MAP ADD ALL2\n";
        mapAddAll2(newExtensionsByTarget, additionalExtensions);
        // std::cerr << "MAP ADDED " << debug_keys(newExtensionsByTarget) << "\n";
      }
    }

    if (hasRule) {
      // std::cerr << "Extend existing style rules" << "\n";
      auto& rules2 = selectors[target];
      extendExistingStyleRules(rules2, newExtensionsByTarget);
      for (auto rule : rules2) {
        // std::cerr << "FINAL " << rule.ptr() << " - " << debug_vec(rule) << "\n";
      }
    }

    // std::cerr << "DONE\n";

  }


  /// Extend [extensions] using [newExtensions].
  void Extender::extendExistingStyleRules(ExtListSelSet& rules,
    ExtSelExtMap& newExtensions
  )
  {
    // std::cerr << "in extendStyle " << debug_vec(rules) << "\n";
    // std::cerr << "in extendStyle " << debug_vals(newExtensions) << "\n";
    // Is a modifyableCssStyleRUle in dart sass
    for (SelectorList_Obj rule : rules) {
      SelectorList_Obj oldValue = rule->copy();
      // try
      // std::cerr << "extend rule " << debug_vec(oldValue) << "\n";
      SelectorList_Obj ext = extendList(rule, newExtensions);
      // std::cerr << " res " << debug_vec(ext) << "\n";
      // catch

      // If no extends actually happenedit (for example becaues unification
      // failed), we don't need to re-register the selector.
      // std::cerr << "CHECK IF CHANGED [" << debug_vec(oldValue) << "] vs [" << debug_vec(ext) << "]\n";
      if (*oldValue == *ext) {
        // std::cerr << "THEY ARE EQUAL\n";
        continue;
      }
      // std::cerr << "REGISTER NEW SELECTOR " << debug_vec(ext) << "\n";
      rule->elements(ext->elements());
      registerSelector(rule);

    }
    // std::cerr << "extendExistingStyleRules END\n";
  }

  /// Extend [extensions] using [newExtensions].
///
/// Note that this does duplicate some work done by
/// [_extendExistingStyleRules], but it's necessary to expand each extension's
/// extender separately without reference to the full selector list, so that
/// relevant results don't get trimmed too early.
///
/// Returns extensions that should be added to [newExtensions] before
/// extending selectors in order to properly handle extension loops such as:
///
///     .c {x: y; @extend .a}
///     .x.y.a {@extend .b}
///     .z.b {@extend .c}
///
/// Returns `null` if there are no extensions to add.
  ExtSelExtMap Extender::extendExistingExtensions(
    std::vector<Extension2> oldExtensions,
    ExtSelExtMap& newExtensions)
  {

    ExtSelExtMap additionalExtensions;

    // std::cerr << "extendExistingExtensions" << "\n";

    for (Extension2 extension : oldExtensions) {
      ExtSelExtMapEntry& sources = extensions[extension.target];
      std::vector<ComplexSelector_Obj> selectors;
      // try {
      selectors = extendComplex(
        extension.extender,
        newExtensions
      );
      if (selectors.empty()) {
        continue;
      }

      // } catch () {
      /*
on SassException catch (error) {
        throw SassException(
            "From ${extension.extenderSpan.message('')}\n"
            "${error.message}",
            error.span);
      */
      // }

      bool containsExtension = *selectors.front() == *extension.extender;
      bool first = false;
      for (ComplexSelector_Obj complex : selectors) {
        // If the output contains the original complex 
        // selector, there's no need to recreate it.
        if (containsExtension && first) {
          first = false;
          continue;
        }

        Extension2 withExtender2 = extension.withExtender(complex);
        ExtSelExtMapEntry::iterator existingExtension = sources.find(complex); // 
        // std::cerr << "SOURCES " << debug_keys(sources) << "\n";
        // std::cerr << " LOOKFOR " << debug_vec(complex) << "\n";
        // debug_ast(sources.begin()->first);
        // debug_ast(complex);
        if (sources.find(complex) != sources.end()) {
          auto sec = existingExtension->second;
          // std::cerr << "MERGED LHS " << debug_vec(sec) << "\n";
          // std::cerr << "MERGED RHS " << debug_vec(withExtender2) << "\n";
          Extension2 merged = mergeExtension(existingExtension->second, withExtender2);
          // std::cerr << "MERGED " << debug_vec(merged) << "\n";
          // existingExtension->second = merged; // compiler error???
          sources[complex] = merged;
        }
        else {
          // std::cerr << "WITH EXTENDER " + debug_vec(withExtender2) << "\n";
          sources[complex] = withExtender2;
          for (auto component : complex->elements()) {
            if (auto compound = component->getCompound()) {
              for (auto simple : compound->elements()) {
                // std::cerr << "  EXTBY " << debug_vec(simple) << "\n";
                extensionsByExtender[simple].push_back(withExtender2);
              }
            }
          }

          if (newExtensions.find(extension.target) != newExtensions.end()) {
            // std::cerr << " ADDITIONAL SOURCE\n";
            auto& additionalSources = additionalExtensions[extension.target];
            additionalSources[complex] = withExtender2;
          }

        }

        // std::cerr << "OUT MERGE\n";

      }

      // If [selectors] doesn't contain [extension.extender], for example if it
      // was replaced due to :not() expansion, we must get rid of the old
      // version.
      if (!containsExtension) {
        // std::cerr << "ERASE " << debug_vec(extension.extender) << "\n";
        sources.erase(extension.extender);
      }


    }

    // std::cerr << "RV ADDIT " + debug_vec(additionalExtensions) << "\n";

    return additionalExtensions;

  }

  template <class T, class U>
  bool setContains(T key, std::set<T, U> set)
  {
    return set.find(key) != set.end();
  }

  SelectorList_Obj Extender::extendList(SelectorList_Obj list, ExtSelExtMap& extensions)
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

    // std::cerr << "EXTENDED2 " << debug_vec(extended) << "\n";
    // std::cerr << "ORIGINALS " << debug_vec(originals) << "\n";

    auto tt = trim(extended, originals);

    // std::cerr << "TRIMMED " << debug_vec(tt) << "\n";

    SelectorList_Obj rv = SASS_MEMORY_NEW(SelectorList, list->pstate());
    rv->concat(tt);

    // std::cerr << "RETURN extendList " << debug_vec(rv) << "\n";

    return rv;
  }

  std::vector<ComplexSelector_Obj> Extender::extendComplex(ComplexSelector_Obj complex, ExtSelExtMap& extensions)
  {
    // std::cerr << "extendComplex " << debug_vec(complex) << "\n";
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
      // std::cerr << "COMPONENT " << debug_vec(component) << "\n";
      if (CompoundSelector_Obj compound = Cast<CompoundSelector>(component)) {
        // std::cerr << "COMP IN " << debug_vec(compound) << "\n";
        std::vector<ComplexSelector_Obj> extended = extendCompound(compound, extensions, isOriginal);
        // std::cerr << "COMP RV " << debug_vec(extended) << "\n";
        if (extended.empty()) {
          // std::cerr << "ADD AS EXT IS EMPTY\n";
          // Note: dart-sass checks for null!?
          if (!extendedNotExpanded.empty()) {
            extendedNotExpanded.push_back({
              compound->wrapInComplex()
            });
          }
        }
        else {

          // Note: dart-sass checks for null!?
          if (extendedNotExpanded.empty()) {
            for (size_t n = 0; n < i; n++) {
              // std::cerr << "ADD ITEM " << debug_vec(complex->at(n)) << "\n";
                extendedNotExpanded.push_back({
                  complex->at(n)->wrapInComplex()
                });
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
            component->wrapInComplex()
          });
        }
      }
    }

    // std::cerr << "Now Etend\n";

    // Note: dart-sass checks for null!?
    if (extendedNotExpanded.empty()) return {};

    // std::cerr << "Now Etend2\n";

    bool first = true;

    // std::cerr << "EXT WEAVE " << debug_vec(extendedNotExpanded) << "\n";

    std::vector<std::vector<ComplexSelector_Obj>>
      sels = paths(extendedNotExpanded);
    
    // std::cerr << "EXT PATHS1 " << debug_vec(sels) << "\n";

    for (std::vector<ComplexSelector_Obj> path : sels) {
      std::vector<std::vector<CompoundOrCombinator_Obj>> qwe;
      for (ComplexSelector_Obj sel : path) {
        qwe.insert(qwe.end(), sel->elements());
      }
      // std::cerr << "EXT WIN " << debug_vec(qwe) << "\n";
      std::vector<std::vector<CompoundOrCombinator_Obj>> ww = weave(qwe);
      // std::cerr << "EXT WEAVED " << debug_vec(ww) << "\n";
      // qwe.insert(qwe.begin(), path)

      // path.any((inputComplex) = > inputComplex.lineBreak)

      for (std::vector<CompoundOrCombinator_Obj> components : ww) {

        ComplexSelector_Obj cplx = SASS_MEMORY_NEW(ComplexSelector, "[ext]");
        cplx->hasPreLineFeed(complex->hasPreLineFeed());
        for (auto pp : path) {
          if (pp->hasPreLineFeed()) {
            cplx->hasPreLineFeed(true);
          }
        }
        cplx->elements(components);

        // Make sure that copies of [complex] retain their status
        // as "original" selectors. This includes selectors that
        // are modified because a :not() was extended into.
        if (first && originals.find(complex) != originals.end()) {
          // std::cerr << "INSERT ORIGINAL2 " << debug_vec(cplx) << "\n";
          // std::cerr << "BEFORE " << debug_vec(originals) << "\n";
          size_t sz = originals.size();

          originals.insert(cplx);
          // std::cerr << "AFTER " << debug_vec(originals) << "\n";
        }
        first = false;

        wwrv.push_back(cplx);

      }

    }

    // std::cerr << "EXT PATHS2 " << debug_vec(wwrv) << "\n";

    return wwrv;
  }

  // Returns the maximum specificity of the given [simple] source selector.
  size_t Extender::maxSourceSpecificity(Simple_Selector_Obj simple)
  {
    auto it = sourceSpecificity.find(simple);
    if (it == sourceSpecificity.end()) return 0;
    return it->second;
  }

  // Returns the maximum specificity for sources that went into producing [compound].
  size_t Extender::maxSourceSpecificity(CompoundSelector_Obj compound)
  {
    size_t specificity = 0;
    for (auto simple : compound->elements()) {
      size_t src = maxSourceSpecificity(simple);
      // std::cerr << "  PART " << src << "\n";
      specificity = std::max(specificity,
        src);
    }
    return specificity;
  }

  Extension2 Extender::extensionForSimple(Simple_Selector_Obj simple)
  {
    Extension2 extension(simple->wrapInComplex());
    extension.specificity = maxSourceSpecificity(simple);
    extension.isOriginal = true;
    return extension;
  }

  Extension2 Extender::extensionForCompound(std::vector<Simple_Selector_Obj> simples)
  {
    CompoundSelector_Obj compound = SASS_MEMORY_NEW(CompoundSelector, ParserState("[ext]"));
    Extension2 extension(compound->concat(simples)->wrapInComplex());
    // extension.specificity = sourceSpecificity[simple];
    extension.isOriginal = true;
    return extension;
  }

  std::vector<ComplexSelector_Obj> Extender::extendCompound(CompoundSelector_Obj compound, ExtSelExtMap& extensions, bool inOriginal)
  {

    // If there's more than one target and they all need to
    // match, we track which targets are actually extended.
    // bool targetsUsed = mode == ExtendMode::NORMAL || extensions.length < 2
    //   ? null
    //   : Set<SimpleSelector>();
    ExtSmplSelSet targetsUsed2;

    ExtSmplSelSet* targetsUsed = nullptr;



    // std::cerr << "extendCompound IN " << std::string(compound) << "\n";
    // std::cerr << "extendCompound EXT " << debug_vec(extensions) << "\n";

    if (mode == ExtendMode::NORMAL || extensions.size() < 2) {

    }
    else {
      // std::cerr << "SETUP targetsUsed\n";
      targetsUsed = &targetsUsed2;
    }

    // std::cerr << "INITIALIZED targetsUsed to " << debug_vec(targetsUsed) << "\n";

    std::vector<ComplexSelector_Obj> result;

    // The complex selectors produced from each component of [compound].
    std::vector<std::vector<Extension2>> options;

    for (size_t i = 0; i < compound->length(); i++) {
      Simple_Selector_Obj simple = compound->get(i);
      // std::cerr << "SIMPLE IN " << debug_vec(simple) << "\n";
      if (extensions.find(simple) != extensions.end()) {
        // std::cerr << "STARTCOMP " << debug_values(extensions[simple]) << "\n";
      }
      auto extended = extendSimple(simple, extensions, targetsUsed);
      // std::cerr << "GOT2 " << debug_vec(extended) << "\n";
      // std::cerr << "targetUsed after " << debug_vec(targetsUsed) << "\n";
      // std::cerr << "  " << debug_vec(extended) << "\n";
      if (extended.empty()/* == null */) {
        if (!options.empty()) {
          // std::cerr << "Add option 1 " << debug_vec(options) << "\n";
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
            auto rv = extensionForCompound(in);
            // std::cerr << " RV " << debug_vec(in) << "\n";
            options.push_back({ rv });
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

    // // std::cerr << "CHECK HERE " << targetsUsed.size() << " vs " << extensions.size() << "\n";


    if (targetsUsed == nullptr) {
      // std::cerr << "IS NULL" << "\n";
    }

      // If [_mode] isn't [ExtendMode.normal] and we didn't use all
    // the targets in [extensions], extension fails for [compound].
    if (targetsUsed != nullptr) {

      if (targetsUsed->size() != extensions.size()) {
        if (!targetsUsed->empty()) {
          // std::cerr << "FAIL: SIZE MISMATCH\n";
          return {};
        }
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
      // std::cerr << "HAS ONLY ONE OPTION " << debug_vec(result) << "\n";
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
              // std::cerr << "INSERT ORIGINALS3 " << debug_vec(compound) << "\n";
              originals.insert(originals.end(), compound->last());
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
        if (complexes.empty()) {
          // std::cerr << "FOOBAR\n";
          return {};
        }

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

    // std::cerr << "UNIFIED " << debug_vec(unifiedPaths) << "\n";

    return unifiedPaths;
  }

  std::vector<Extension2> Extender::extendWithoutPseudo(Simple_Selector_Obj simple, ExtSelExtMap& extensions, ExtSmplSelSet* targetsUsed) {

    // Class_Selector_Obj cs = SASS_MEMORY_NEW(Class_Selector, "[tmp]", ".first"); if (extensions.find(cs) != extensions.end()) { // std::cerr << ".FIRST MAP " << debug_keys(extensions[cs]) << "\n"; }

    // std::cerr << "extendWithoutPseudo " << debug_vec(simple) << "\n";
    auto ext = extensions.find(simple);
    if (ext == extensions.end()) return {};
    ExtSelExtMapEntry extenders = ext->second;
    // std::cerr << "START " << debug_values(extenders) << "\n";
    // std::cerr << "targetsUsed " << debug_vec(targetsUsed) << "\n";

    // dart insert sometimes also in empty
    if (targetsUsed != nullptr) {
      // std::cerr << "insert one in used targets\n";
      targetsUsed->insert(simple);
    }
    else {
      // std::cerr << "targetUsed is NULL\n";
    }
    if (mode == ExtendMode::REPLACE) {
      auto rv = mapValues(extenders);
      // std::cerr << "HAS REPLACE MODE\n";
      return rv;
    }

    std::vector<Extension2> result;
    auto exts = extensionForSimple(simple);
    extenders = extensions[simple]; // re-fetch?
    // std::cerr << "EXT WITH " << debug_vec(exts) << "\n";
    // std::cerr << "PLUS " << debug_values(extenders) << "\n";
    result.push_back(exts);
    for (auto extender : extenders) {
      result.push_back(extender.second);
    }
    //    if (_mode == ExtendMode.replace) return extenders.values.toList();
    // std::cerr << "RESULTS " << debug_vec(result) << "\n";

    return result;
  }

  std::vector<std::vector<Extension2>> Extender::extendSimple(Simple_Selector_Obj simple, ExtSelExtMap& extensions, ExtSmplSelSet* targetsUsed) {

    // std::cerr << "start extendSimple\n";
    // std::cerr << "targetsUsed " << debug_vec(targetsUsed) << "\n";
    if (Pseudo_Selector_Obj pseudo = Cast<Pseudo_Selector>(simple)) {
      // std::cerr << "cast to pseudo ok\n";
      if (pseudo->selector2()) {
        // std::cerr << "pseudo does have selector\n";
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
    }

    // std::cerr << "WA PSEUDO " << debug_vec(simple) << "\n";
    // std::cerr << " FIND IN EXT " << debug_vec(extensions) << "\n";
    if (extensions.find(simple) != extensions.end()) {
      // std::cerr << "STARTSMP " << debug_values(extensions[simple]) << "\n";
    }
    std::vector<Extension2> result = extendWithoutPseudo(simple, extensions, targetsUsed);
    // std::cerr << "GOT1 " << debug_vec(result) << "\n";


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
    if (innerPseudo->selector2()) return { complex };

    std::string name(pseudo->normalized());

    if (name == "not") {
      // In theory, if there's a `:not` nested within another `:not`, the
      // inner `:not`'s contents should be unified with the return value.
      // For example, if `:not(.foo)` extends `.bar`, `:not(.bar)` should
      // become `.foo:not(.bar)`. However, this is a narrow edge case and
      // supporting it properly would make this code and the code calling it
      // a lot more complicated, so it's not supported for now.
      if (innerPseudo->normalized() != "matches") return {};
      return innerPseudo->selector2()->elements();
    }
    else if (name == "matches" && name == "any" && name == "current" && name == "nth-child" && name == "nth-last-child") {
      // As above, we could theoretically support :not within :matches, but
      // doing so would require this method and its callers to handle much
      // more complex cases that likely aren't worth the pain.
      if (innerPseudo->name() != pseudo->name()) return {};
      if (*innerPseudo->expression() != *pseudo->expression()) return {};
      return innerPseudo->selector2()->elements();
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

  bool hasMoreThanOne(ComplexSelector_Obj vec)
  {
    return vec->length() > 1;
  }


  std::vector<Pseudo_Selector_Obj> Extender::extendPseudo(Pseudo_Selector_Obj pseudo, ExtSelExtMap& extensions)
  {
    auto sel = pseudo->selector2();
    // std::cerr << "CALL extend list\n";
    SelectorList_Obj extended = extendList(sel, extensions);
    // std::cerr << "CALLED extend list\n";


    if (!extended || !pseudo || !pseudo->selector2()) {
      // std::cerr << "STUFF IS NULL\n";
      return {};
    }

    // debug_ast(extended);
    // debug_ast(pseudo->selector());
    // doDebug = true;

    if (*(pseudo->selector2()) == *extended) {
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
      bool b1 = hasAny(pseudo->selector2()->elements(), hasMoreThanOne);
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
      if (pseudo->selector2()->length() == 1) {
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
    // std::cerr << "Test " << complex2->minSpecificity() << " vs " << maxSpecificity << "\n";
    if (complex2->minSpecificity() < maxSpecificity) return false;
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

  std::vector<ComplexSelector_Obj> Extender::trim(std::vector<ComplexSelector_Obj> selectors, ExtCplxSelSet& set)
  {


    // Avoid truly horrific quadratic behavior.
    // TODO(nweiz): I think there may be a way to get perfect trimming 
    // without going quadratic by building some sort of trie-like
    // data structure that can be used to look up superselectors.
    // TODO(mgreter): Check how this perfoms in C++ (up the limit)
    if (selectors.size() > 100) return selectors;

    size_t cnt = 0;

    // std::cerr << "DO TRIM " << debug_vec(selectors) << "\n";
    // // std::cerr << "SET CONISTS " << debug_vec(set) << "\n";

    std::vector<ComplexSelector_Obj> result; size_t numOriginals = 0;
    // This is n² on the sequences, but only comparing between separate sequences
    // should limit the quadratic behavior. We iterate from last to first and reverse
    // the result so that, if two selectors are identical, we keep the first one.


    if (selectors.size() > 1) {
      // debug_ast(selectors.at(1));
      // debug_ast(set.back());

      if (*selectors.at(1) == *set.back()) {
        // std::cerr << "indentity ok\n";
      }

    }

  redo:

    // if (cnt++ > 50) exit(0);

    for (size_t i = selectors.size() - 1; i != std::string::npos; i--) {

      // std::cerr << "LOOP " << i << "\n";
    // if (cnt++ > 10) break;
    ComplexSelector_Obj complex1 = selectors.at(i);

    if (set.find(complex1) != set.end()) {
      // std::cerr << "FOUND IN ORIGINALS " << complex1.ptr() << "\n";

      // std::cerr << "HAS ORG " << i << "\n";
      // Make sure we don't include duplicate originals, which could
      // happen if a style rule extends a component of its own selector.

      for (size_t j = 0; j < numOriginals; j++) {
        if (*result[j] == *complex1) {
          rotateSlice(result, 0, j + 1);
          // std::cerr << "ROTATE & REDO\n";
          goto redo;
        }
      }

      numOriginals++;
      // std::cerr << "ADD ORIGINAL " << debug_vec(complex1) << "\n";
      // ToDo: convert to a dequeue?
      result.insert(result.begin(), complex1);
      // Do next iteration
      continue;
    }
    else {
      // std::cerr << "NOT FOUND IN ORIGINALS\n";
    }

    // The maximum specificity of the sources that caused [complex1]
    // to be generated. In order for [complex1] to be removed, there
    // must be another selector that's a superselector of it *and*
    // that has specificity greater or equal to this.
    size_t maxSpecificity = 0;
    for (CompoundOrCombinator_Obj component : complex1->elements()) {
      // std::cerr << "check max\n";
      if (CompoundSelector_Obj compound = Cast<CompoundSelector>(component)) {
        maxSpecificity = std::max(maxSpecificity, maxSourceSpecificity(compound));
        // std::cerr << "MAX NOW AT " << maxSpecificity << "\n";
      }
    }

    // Look in [result] rather than [selectors] for selectors after [i]. This
    // ensures we aren't comparing against a selector that's already been trimmed,
    // and thus that if there are two identical selectors only one is trimmed.
    // std::cerr << "RESULTS " << debug_vec(result) << "\n";
    if (hasAny(result, dontTrimComplex, complex1, maxSpecificity)) {
      // std::cerr << "TRIM FROM RESULT " << debug_vec(complex1) << "\n";
      continue;
    }
    std::vector<ComplexSelector_Obj> selectors2;
    for (size_t n = 0; n < i; n++) {
      selectors2.push_back(selectors[n]);
    }
    // std::cerr << "selectors " << debug_vec(selectors2) << "\n";
    if (hasSubAny(selectors, i, dontTrimComplex, complex1, maxSpecificity)) {
      // WE NEED SPECIFICITY CHECK HERE!!!!!! DAMANAMDNA
      // std::cerr << "TRIM FROM SELECTORS " << debug_vec(complex1) << "\n";
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
