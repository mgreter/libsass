#ifndef SASS_EXTENDER_H
#define SASS_EXTENDER_H

#include <set>
#include <map>
#include <string>

#include "ast_fwd_decl.hpp"
#include "operation.hpp"
#include "extension.hpp"
#include "backtrace.hpp"

#include "tsl/ordered_map.h"
#include "tsl/ordered_map.h"
#include "tsl/ordered_set.h"

namespace Sass {


  typedef std::tuple<
    Selector_List_Obj, // modified
    Selector_List_Obj // original
  > ExtSelTuple;

  // This is special (ptrs!)
  typedef tsl::ordered_set<
    ComplexSelector_Obj,
    HashPtrNodes,
    ComparePtrNodes
  > ExtCplxSelSet;

  typedef tsl::ordered_set<
    Simple_Selector_Obj,
    HashNodes,
    CompareNodes
  > ExtSmplSelSet;

  typedef tsl::ordered_set<
    SelectorList_Obj,
    HashNodes,
    CompareNodes
  > ExtListSelSet;

  typedef tsl::ordered_map<
    Simple_Selector_Obj,
    ExtListSelSet,
    HashNodes,
    CompareNodes
  > ExtSelMap;

  typedef tsl::ordered_map <
    ComplexSelector_Obj,
    Extension,
    HashNodes,
    CompareNodes
  > ExtSelExtMapEntry;

  typedef tsl::ordered_map<
    Simple_Selector_Obj,
    ExtSelExtMapEntry,
    HashNodes,
    CompareNodes
  > ExtSelExtMap;

  typedef tsl::ordered_map <
    Simple_Selector_Obj,
    std::vector<
      Extension
    >,
    HashNodes,
    CompareNodes
  > ExtByExtMap;


  class Extender : public Operation_CRTP<void, Extender> {

  public:

    enum ExtendMode { TARGETS, REPLACE, NORMAL, };

  public:

    // The mode that controls this extender's behavior.
    ExtendMode mode;

    // Shared backtraces with context and expander. Needed the throw
    // errors when e.g. extending accross media query boundaries.
    Backtraces& traces;

    // A map from all simple selectors in the stylesheet to the rules that
    // contain them.This is used to find which rules an `@extend` applies to.
    ExtSelMap selectors;

    // A map from all extended simple selectors
    // to the sources of those extensions.
    ExtSelExtMap extensions;

    // A map from all simple selectors in extenders to
    // the extensions that those extenders define.
    ExtByExtMap extensionsByExtender;

    // A map from CSS rules to the media query contexts they're defined in.
    // This tracks the contexts in which each style rule is defined.
    // If a rule is defined at the top level, it doesn't have an entry.
    tsl::ordered_map<
      SelectorList_Obj,
      Media_Block_Obj,
      HashPtrNodes,
      ComparePtrNodes
    > mediaContexts;

    
    std::unordered_map<
      Simple_Selector_Obj,
      size_t,
      HashNodes,
      CompareNodes
    > sourceSpecificity;

    // A set of [ComplexSelector]s that were originally part of their
    // component [SelectorList]s, as opposed to being added by `@extend`.
    // This allows us to ensure that we don't trim any selectors
    // that need to exist to satisfy the [first law of extend][].
    ExtCplxSelSet originals;


  public:

    Extender(Backtraces& traces);;
    ~Extender() {};


    Extender(ExtendMode mode, Backtraces& traces);

    ExtSmplSelSet getSimpleSelectors() const;

  public:
    static SelectorList_Obj extend(SelectorList_Obj selector, SelectorList_Obj source, SelectorList_Obj target, Backtraces& traces);
    static SelectorList_Obj replace(SelectorList_Obj selector, SelectorList_Obj source, SelectorList_Obj target, Backtraces& traces);

    // Extends [list] using [extensions].
    /*, List<CssMediaQuery> mediaQueryContext*/
    void addExtension(SelectorList_Obj extender, Simple_Selector_Obj target, ExtendRule_Obj extend, Media_Block_Obj mediaQueryContext);
    SelectorList_Obj extendList(SelectorList_Obj list, ExtSelExtMap& extensions, Media_Block_Obj mediaContext);

    void extendExistingStyleRules(
      ExtListSelSet& rules,
      ExtSelExtMap& newExtensions);

    ExtSelExtMap extendExistingExtensions(
      std::vector<Extension> extensions,
      ExtSelExtMap& newExtensions);


    size_t maxSourceSpecificity(Simple_Selector_Obj simple);
    size_t maxSourceSpecificity(CompoundSelector_Obj compound);
    Extension extensionForSimple(Simple_Selector_Obj simple);
    Extension extensionForCompound(std::vector<Simple_Selector_Obj> simples);


    std::vector<ComplexSelector_Obj> extendComplex(ComplexSelector_Obj list, ExtSelExtMap& extensions, Media_Block_Obj mediaQueryContext);
    std::vector<ComplexSelector_Obj> extendCompound(CompoundSelector_Obj compound, ExtSelExtMap& extensions, Media_Block_Obj mediaQueryContext, bool inOriginal = false);
    std::vector<std::vector<Extension>> extendSimple(Simple_Selector_Obj simple, ExtSelExtMap& extensions, Media_Block_Obj mediaQueryContext, ExtSmplSelSet* targetsUsed);

    std::vector<Pseudo_Selector_Obj> extendPseudo(Pseudo_Selector_Obj pseudo, ExtSelExtMap& extensions, Media_Block_Obj mediaQueryContext);

    std::vector<ComplexSelector_Obj> trim(std::vector<ComplexSelector_Obj> selectors, ExtCplxSelSet& set);
    std::vector<ComplexSelector_Obj> trim(std::vector<ComplexSelector_Obj> selectors, bool (*isOriginal)(ComplexSelector_Obj complex));
    


  private:
    std::vector<Extension> extendWithoutPseudo(Simple_Selector_Obj simple, ExtSelExtMap& extensions, ExtSmplSelSet* targetsUsed);
    static SelectorList_Obj _extendOrReplace(SelectorList_Obj selector, SelectorList_Obj source, SelectorList_Obj target, ExtendMode mode, Backtraces& traces);

  public:

    // An [Extender] that contains no extensions and can have no extensions added.
    // static const empty = EmptyExtender();

    // A map from all simple selectors in the
    // stylesheet to the rules that contain them.
    // This is used to find which rules an `@extend` applies to.
    // std::map<Simple_Selector_Obj, Set<ModifiableCssStyleRule>> _selectors;

    // A map from all extended simple selectors to the sources of those extensions.
    // std::map<Simple_Selector_Obj, std::map<ComplexSelector_Obj, Extension, OrderNodes>> extensions;

    // A map from all simple selectors in extenders to the extensions that those extenders define.
    // std::map<Simple_Selector_Obj, std::vector<Extension>> extensionsByExtender;

    /// A set of [ComplexSelector]s that were originally part of
    /// their component [SelectorList]s, as opposed to being added by `@extend`.
    ///
    /// This allows us to ensure that we don't trim any selectors that need to
    /// exist to satisfy the [first law of extend][].
    ///
    /// [first law of extend]: https://github.com/sass/sass/issues/324#issuecomment-4607184
    // std::set<ComplexSelector_Obj> originals;





    // Adds [selector] to this extender, with [selectorSpan] as the span covering
    // the selector and [ruleSpan] as the span covering the entire style rule.
    // Extends [selector] using any registered extensions, then returns an empty
    // [ModifiableCssStyleRule] with the resulting selector. If any more relevant
    // extensions are added, the returned rule is automatically updated.
    // The [mediaContext] is the media query context in which the selector was
    // defined, or `null` if it was defined at the top level of the document.
    void addSelector(SelectorList_Obj selector, Media_Block_Obj mediaContext);

    // Registers the [SimpleSelector]s in [list]
    // to point to [rule] in [selectors].
    void registerSelector(SelectorList_Obj list, SelectorList_Obj rule);


  };

}

#endif
