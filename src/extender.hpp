#ifndef SASS_EXTENDER_H
#define SASS_EXTENDER_H

#include <string>
#include <set>

#include "ast.hpp"
#include "node.hpp"
#include "eval.hpp"
#include "extension.hpp"
#include "operation.hpp"
#include "subset_map.hpp"
#include "ast_fwd_decl.hpp"

#include "tsl/ordered_map.h"
#include "tsl/ordered_set.h"

namespace Sass {

  typedef tsl::ordered_set<
    ComplexSelector_Obj,
    HashNodes,
    CompareNodes
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
    Extension2,
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
      Extension2
    >,
    HashNodes,
    CompareNodes
  > ExtByExtMap;


  class Extender : public Operation_CRTP<void, Extender> {

  public:

    enum ExtendMode { TARGETS, REPLACE, NORMAL, };

  private:

    // The mode that controls this extender's behavior.
    ExtendMode mode;

    // A map from all simple selectors in the stylesheet to the rules that
    // contain them.This is used to find which rules an `@extend` applies to.
    ExtSelMap selectors;

    // A map from all extended simple selectors
    // to the sources of those extensions.
    ExtSelExtMap extensions;

    // A map from all simple selectors in extenders to
    // the extensions that those extenders define.
    ExtByExtMap extensionsByExtender;

    // A set of [ComplexSelector]s that were originally part of their
    // component [SelectorList]s, as opposed to being added by `@extend`.
    // This allows us to ensure that we don't trim any selectors
    // that need to exist to satisfy the [first law of extend][].
    ExtCplxSelSet originals;

  public:

    Extender() {};
    ~Extender() {};


    Extender(ExtendMode mode) :
      mode(mode),
      selectors(),
      extensions(),
      extensionsByExtender(),
      // mediaContexts(),
      // sourceSpecificity(),
      originals()
    {};

  public:
    static SelectorList_Obj extend(SelectorList_Obj selector, SelectorList_Obj source, SelectorList_Obj target);
    static SelectorList_Obj replace(SelectorList_Obj selector, SelectorList_Obj source, SelectorList_Obj target);

    // Extends [list] using [extensions].
    /*, List<CssMediaQuery> mediaQueryContext*/
    SelectorList_Obj extendList(SelectorList_Obj list, ExtSelExtMap extensions);
    std::vector<ComplexSelector_Obj> extendComplex(ComplexSelector_Obj list, ExtSelExtMap extensions);
    std::vector<ComplexSelector_Obj> extendCompound(CompoundSelector_Obj compound, ExtSelExtMap extensions, bool inOriginal = false);
    std::vector<std::vector<Extension2>> extendSimple(Simple_Selector_Obj simple, ExtSelExtMap extensions, ExtSmplSelSet& targetsUsed);

    std::vector<Pseudo_Selector_Obj> extendPseudo(Pseudo_Selector_Obj pseudo, ExtSelExtMap extensions);

    std::vector<ComplexSelector_Obj> trim(std::vector<ComplexSelector_Obj> selectors, ExtCplxSelSet set);
    std::vector<ComplexSelector_Obj> trim(std::vector<ComplexSelector_Obj> selectors, bool (*isOriginal)(ComplexSelector_Obj complex));
    


  private:
    std::vector<Extension2> extendWithoutPseudo(Simple_Selector_Obj simple, ExtSelExtMap extensions, ExtSmplSelSet& targetsUsed);
    static SelectorList_Obj _extendOrReplace(SelectorList_Obj selector, SelectorList_Obj source, SelectorList_Obj target, ExtendMode mode);

  public:

    // An [Extender] that contains no extensions and can have no extensions added.
    // static const empty = EmptyExtender();

    // A map from all simple selectors in the
    // stylesheet to the rules that contain them.
    // This is used to find which rules an `@extend` applies to.
    // std::map<Simple_Selector_Obj, Set<ModifiableCssStyleRule>> _selectors;

    // A map from all extended simple selectors to the sources of those extensions.
    // std::map<Simple_Selector_Obj, std::map<ComplexSelector_Obj, Extension2, OrderNodes>> extensions;

    // A map from all simple selectors in extenders to the extensions that those extenders define.
    // std::map<Simple_Selector_Obj, std::vector<Extension2>> extensionsByExtender;

    /// A set of [ComplexSelector]s that were originally part of
    /// their component [SelectorList]s, as opposed to being added by `@extend`.
    ///
    /// This allows us to ensure that we don't trim any selectors that need to
    /// exist to satisfy the [first law of extend][].
    ///
    /// [first law of extend]: https://github.com/sass/sass/issues/324#issuecomment-4607184
    // std::set<ComplexSelector_Obj> originals;



  };

}

#endif
