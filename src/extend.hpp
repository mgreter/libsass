#ifndef SASS_EXTEND_H
#define SASS_EXTEND_H

#include <string>
#include <set>

#include "ast.hpp"
#include "node.hpp"
#include "eval.hpp"
#include "operation.hpp"
#include "subset_map.hpp"
#include "ast_fwd_decl.hpp"

namespace Sass {

  Node subweave(Node& one, Node& two);

  class Extend : public Operation_CRTP<void, Extend> {

    Subset_Map& subset_map;
    Eval* eval;

  private:

    std::unordered_map<
      Selector_List_Obj, // key
      Selector_List_Obj, // value
      HashNodes, // hasher
      CompareNodes // compare
    > memoizeList;

    std::unordered_map<
      Complex_Selector_Obj, // key
      Node, // value
      HashNodes, // hasher
      CompareNodes // compare
    > memoizeComplex;

    /* this turned out to be too much overhead
       re-evaluate once we store an ast selector
    std::unordered_map<
      Compound_Selector_Obj, // key
      Node, // value
      HashNodes, // hasher
      CompareNodes // compare
    > memoizeCompound;
    */

    void extendObjectWithSelectorAndBlock(Ruleset* pObject);
    Node extendComplexSelector(Complex_Selector* sel, CompoundSelectorSet& seen, bool isReplace, bool isOriginal);
    Node extendCompoundSelector(Compound_Selector* sel, CompoundSelectorSet& seen, bool isReplace);
    bool complexSelectorHasExtension(Complex_Selector* selector, CompoundSelectorSet& seen);
    Node trim(Node& seqses, bool isReplace);
    Node weave(Node& path);

  public:
    void setEval(Eval& eval);
    Selector_List* extendSelectorList(Selector_List_Obj pSelectorList, bool isReplace, bool& extendedSomething, CompoundSelectorSet& seen);
    Selector_List* extendSelectorList(Selector_List_Obj pSelectorList, bool isReplace = false) {
      bool extendedSomething = false;
      CompoundSelectorSet seen;
      return extendSelectorList(pSelectorList, isReplace, extendedSomething, seen);
    }
    Selector_List* extendSelectorList(Selector_List_Obj pSelectorList, CompoundSelectorSet& seen) {
      bool isReplace = false;
      bool extendedSomething = false;
      return extendSelectorList(pSelectorList, isReplace, extendedSomething, seen);
    }
    Extend(Subset_Map&);
    ~Extend() { }

    void operator()(Block*);
    void operator()(Ruleset*);
    void operator()(Supports_Block*);
    void operator()(Media_Block*);
    void operator()(Directive*);

    // ignore missed types
    template <typename U>
    void fallback(U x) {}

  };


  class Extend2 : public Operation_CRTP<void, Extend> {

    SubsetMap2& subset_map;
    Eval* eval;

  private:

    std::unordered_map<
      SelectorList_Obj, // key
      SelectorList_Obj, // value
      HashNodes, // hasher
      CompareNodes // compare
    > memoizeList2;

    std::unordered_map<
      ComplexSelector_Obj, // key
      Node, // value
      HashNodes, // hasher
      CompareNodes // compare
    > memoizeComplex2;

    /* this turned out to be too much overhead
       re-evaluate once we store an ast selector
    std::unordered_map<
      Compound_Selector_Obj, // key
      Node, // value
      HashNodes, // hasher
      CompareNodes // compare
    > memoizeCompound;
    */

    void extendObjectWithSelectorAndBlock(Ruleset* pObject);
    Node extendComplexSelector(ComplexSelector* sel, CompoundSelectorSet2& seen, bool isReplace, bool isOriginal);
    Node extendCompoundSelector(CompoundSelector* sel, CompoundSelectorSet2& seen, bool isReplace);
    bool complexSelectorHasExtension(ComplexSelector* selector, CompoundSelectorSet2& seen);

    // Node trim(Node& seqses, bool isReplace);
    // Node weave(Node& path);

  public:
    void setEval(Eval& eval);
    SelectorList* extendSelectorList(SelectorList_Obj pSelectorList, bool isReplace, bool& extendedSomething, CompoundSelectorSet2& seen);
    SelectorList* extendSelectorList(SelectorList_Obj pSelectorList, bool isReplace = false) {
      bool extendedSomething = false;
      CompoundSelectorSet2 seen;
      return extendSelectorList(pSelectorList, isReplace, extendedSomething, seen);
    }
    SelectorList* extendSelectorList(SelectorList_Obj pSelectorList, CompoundSelectorSet2& seen) {
      bool isReplace = false;
      bool extendedSomething = false;
      return extendSelectorList(pSelectorList, isReplace, extendedSomething, seen);
    }
    Extend2(SubsetMap2&);
    ~Extend2() { }

    void operator()(Block*);
    void operator()(Ruleset*);
    void operator()(Supports_Block*);
    void operator()(Media_Block*);
    void operator()(Directive*);

    // ignore missed types
    template <typename U>
    void fallback(U x) {}

  };

}

#endif
