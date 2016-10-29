#ifndef SASS_EXTEND_H
#define SASS_EXTEND_H

#include <string>
#include <set>

#include "ast.hpp"
#include "operation.hpp"
#include "subset_map.hpp"

namespace Sass {

  class Context;
  class Node;

  typedef Subset_Map<std::string, std::pair<Complex_Selector_Ptr, Compound_Selector_Ptr> > ExtensionSubsetMap;

  class Extend : public Operation_CRTP<void, Extend> {

    Context&            ctx;
    ExtensionSubsetMap& subset_map;

    void fallback_impl(AST_Node_Ptr n) { }

  public:
    static Node subweave(Node& one, Node& two, Context& ctx);
    static CommaComplex_Selector_Ptr extendSelectorList(CommaComplex_Selector_Obj pSelectorList, Context& ctx, ExtensionSubsetMap& subset_map, bool isReplace, bool& extendedSomething, std::set<Compound_Selector>& seen);
    static CommaComplex_Selector_Ptr extendSelectorList(CommaComplex_Selector_Obj pSelectorList, Context& ctx, ExtensionSubsetMap& subset_map, bool isReplace, bool& extendedSomething);
    static CommaComplex_Selector_Ptr extendSelectorList(CommaComplex_Selector_Obj pSelectorList, Context& ctx, ExtensionSubsetMap& subset_map, bool isReplace = false) {
      bool extendedSomething = false;
      return extendSelectorList(pSelectorList, ctx, subset_map, isReplace, extendedSomething);
    }
    static CommaComplex_Selector_Ptr extendSelectorList(CommaComplex_Selector_Obj pSelectorList, Context& ctx, ExtensionSubsetMap& subset_map, std::set<Compound_Selector>& seen) {
      bool isReplace = false;
      bool extendedSomething = false;
      return extendSelectorList(pSelectorList, ctx, subset_map, isReplace, extendedSomething, seen);
    }
    Extend(Context&, ExtensionSubsetMap&);
    ~Extend() { }

    void operator()(Block_Ptr);
    void operator()(Ruleset_Ptr);
    void operator()(Supports_Block_Ptr);
    void operator()(Media_Block_Ptr);
    void operator()(Directive_Ptr);

    template <typename U>
    void fallback(U x) { return fallback_impl(x); }
  };

}

#endif
