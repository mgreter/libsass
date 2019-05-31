#include "sass.hpp"
#include "ast.hpp"
#include "context.hpp"
#include "node.hpp"
#include "eval.hpp"
#include "extend.hpp"
#include "emitter.hpp"
#include "color_maps.hpp"
#include "ast_fwd_decl.hpp"
#include <set>
#include <queue>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <functional>
#include <cctype>
#include <locale>

#include "ast_fwd_decl.hpp"
#include "ast_selectors.hpp"
#include "debugger.hpp"

// #define DEBUG_UNIFY

namespace Sass {

  std::vector<std::vector<CompoundOrCombinator_Obj>> weave(
    std::vector<std::vector<CompoundOrCombinator_Obj>> complexes);


  // https://www.geeksforgeeks.org/longest-common-subsequence/
// https://www.geeksforgeeks.org/printing-longest-common-subsequence/
  template <class T>
  std::vector<SharedImpl<T>> lcs(std::vector<SharedImpl<T>>& X, std::vector<SharedImpl<T>>& Y) {
    std::size_t m = X.size() - 1;
    std::size_t n = Y.size() - 1;

    std::vector<SharedImpl<T>> lcs;
    if (m == std::string::npos) return lcs;
    if (n == std::string::npos) return lcs;

#define L(i,j) l[ (i) * m + j ]
    std::size_t* l = new std::size_t[(m + 1) * (n + 1)];

    /* Following steps build L[m+1][n+1] in bottom up fashion. Note
      that L[i][j] contains length of LCS of X[0..i-1] and Y[0..j-1] */
    for (size_t i = 0; i <= m; i++) {
      for (size_t j = 0; j <= n; j++) {
        if (i == 0 || j == 0)
          L(i, j) = 0;
        else if (*X[i - 1] == *Y[j - 1])
          L(i, j) = L(i - 1, j - 1) + 1;
        else
          L(i, j) = std::max(L(i - 1, j), L(i, j - 1));
      }
    }

    // Following code is used to print LCS
    std::size_t index = L(m, n);

    // Create an array to store the lcs groups
    // std::vector<Selector_Group_Obj> lcs(index);

    // Start from the right-most-bottom-most corner and
    // one by one store objects in lcs[]
    std::size_t i = m, j = n;
    while (i > 0 && j > 0) {

      // If current objects in X[] and Y are same, then
      // current object is part of LCS
      if (*X[i - 1] == *Y[j - 1])
      {
        lcs[index - 1] = X[i - 1]; // Put current object in result
        i--; j--; index--;     // reduce values of i, j and index
      }

      // If not same, then find the larger of two and
      // go in the direction of larger value
      else if (L(i - 1, j) > L(i, j - 1))
        i--;
      else
        j--;
    }

    delete[] l;
    return lcs;
  }

  // std::vector<CompoundOrCombinator_Obj> cmpGroups(std::vector<CompoundOrCombinator_Obj> group1, std::vector<CompoundOrCombinator_Obj> group2) {
  #define SS(i,j) s[ (i) * (m+1) + j ]
  #define LL(i,j) l[ (i) * (m+1) + j ]
  #define BB(i,j) b[ (i) * (m+1) + j ]

  template <class T>
  std::vector<T> backtrack(size_t i, size_t j, T* s, std::size_t* l, bool* b, std::size_t m, std::size_t n) {
    if (i == std::string::npos || j == std::string::npos) return {};
    T selection = SS(i, j);
    if (BB(i, j)) {
      auto rv = backtrack(i - 1, j - 1, s, l, b, m, n);
      rv.push_back(selection);
      return rv;
    }

    return LL(i + 1, j) > LL(i, j + 1)
      ? backtrack(i, j - 1, s, l, b, m, n)
      : backtrack(i - 1, j, s, l, b, m, n);
  }


  template <class T>
  std::vector<T> lcs2(std::vector<T>& X, std::vector<T>& Y, bool(*select)(T, T, T&)) {
    std::size_t m = X.size() - 1;
    std::size_t n = Y.size() - 1;

    // std::cerr << "LCS IN X: " << debug_vec(X) << "\n";
    // std::cerr << "LCS IN Y: " << debug_vec(Y) << "\n";

    std::vector<T> lcs;
    if (m == std::string::npos) return lcs;
    if (n == std::string::npos) return lcs;

    // std::cerr << "ALLOCATE: " << (m + 2) * (n + 2) << "\n";
    T* s = new T[(m + 2) * (n + 2)];
    bool* b = new bool[(m + 2) * (n + 2)];
    std::size_t* l = new std::size_t[(m + 2) * (n + 2)];

    // Initialize with zeroes
    for (size_t i = 0; i <= m + 1; i++) {
      for (size_t j = 0; j <= n + 1; j++) {
        // std::cerr << "SET ZERO\n";
        LL(i, j) = 0;
      }
    }

    /* Following steps build L[m+1][n+1] in bottom up fashion. Note
      that L[i][j] contains length of LCS of X[0..i-1] and Y[0..j-1] */
    for (size_t i = 0; i <= m; i++) {
      for (size_t j = 0; j <= n; j++) {
        BB(i, j) = select(X[i], Y[j], SS(i, j));
        if (BB(i, j) == false) {
          LL(i + 1, j + 1) = std::max(LL(i + 1, j), LL(i, j + 1));
          // std::cerr << "SET " << (i + 1) << "/" << (j + 1) << " = " << LL(i + 1, j + 1) << "\n";
        }
        else {
          LL(i + 1, j + 1) = LL(i, j) + 1;
          // std::cerr << "SET2 " << (i + 1) << "/" << (j + 1) << " = " << LL(i, j) << "\n";
        }
      }
    }
    /*
    std::cerr << "L: [";
    for (size_t i = 0; i <= m + 1; i++) {
      if (i > 0) std::cerr << ", ";
      std::cerr << "[";
      for (size_t j = 0; j <= n + 1; j++) {
        if (j > 0) std::cerr << ", ";
        std::cerr << LL(i, j);
      }
      std::cerr << "]";
    }
    std::cerr << "]\n";
    */

    // Following code is used to print LCS
    auto rv = backtrack(m, n, s, l, b, m, n);

    delete[] l;
    delete[] s;
    delete[] b;

    return rv;


    // std::size_t index = LL(m, n);

    // Create an array to store the lcs groups
    // std::vector<Selector_Group_Obj> lcs(index);
    /*
    // Start from the right-most-bottom-most corner and
    // one by one store objects in lcs[]
    std::size_t i = m, j = n;
    while (i > 0 && j > 0) {

      // If current objects in X[] and Y are same, then
      // current object is part of LCS
      // if (*X[i - 1] == *Y[j - 1])
      if (cmp(X[i - 1], Y[j - 1]).empty())
      {
        lcs[index - 1] = X[i - 1]; // Put current object in result
        i--; j--; index--;     // reduce values of i, j and index
      }

      // If not same, then find the larger of two and
      // go in the direction of larger value
      else if (L(i - 1, j) > L(i, j - 1))
        i--;
      else
        j--;
    }
    */
    return lcs;
  }

  /// Extracts leading [Combinator]s from [components1] and [components2] and
  /// merges them together into a single list of combinators.
  ///
  /// If there are no combinators to be merged, returns an empty list. If the
  /// combinators can't be merged, returns `null`.
  bool mergeInitialCombinators(
    std::vector<CompoundOrCombinator_Obj> components1,
    std::vector<CompoundOrCombinator_Obj> components2,
    std::vector<SelectorCombinator_Obj>& result) {

    std::vector<SelectorCombinator_Obj> combinators1;
    while (!components1.empty() && Cast<SelectorCombinator>(components1.front())) {
      SelectorCombinator_Obj front = Cast<SelectorCombinator>(components1.front());
      components1.erase(components1.begin());
      combinators1.push_back(front);
    }

    std::vector<SelectorCombinator_Obj> combinators2;
    while (!combinators2.empty() && Cast<SelectorCombinator>(combinators2.front())) {
      SelectorCombinator_Obj front = Cast<SelectorCombinator>(components2.front());
      components2.erase(components2.begin());
      combinators2.push_back(front);
    }

    // If neither sequence of combinators is a subsequence of the other, they
    // cannot be merged successfully.
    auto LCS = lcs<SelectorCombinator>(combinators1, combinators2);

    if (std::equal(LCS.begin(), LCS.end(), combinators1.begin(), ComparePtrsFunction<SelectorCombinator>)) {
      result = combinators2; // Does this work?
      return true;
    }
    if (std::equal(LCS.begin(), LCS.end(), combinators2.begin(), ComparePtrsFunction<SelectorCombinator>)) {
      result = combinators1; // Does this work?
      return true;
    }

    return false; // null

  }


  /// Extracts trailing [Combinator]s, and the selectors to which they apply, from
  /// [components1] and [components2] and merges them together into a single list.
  ///
  /// If there are no combinators to be merged, returns an empty list. If the
  /// sequences can't be merged, returns `null`.
  bool mergeFinalCombinators(
    std::vector<CompoundOrCombinator_Obj> components1,
    std::vector<CompoundOrCombinator_Obj> components2,
    std::vector<std::vector<std::vector<CompoundOrCombinator_Obj>>>& result) {

    if (components1.empty() || !Cast<SelectorCombinator>(components1.back())) {
      if (components2.empty() || !Cast<SelectorCombinator>(components2.back())) {
        return true;
      }
    }

    std::vector<SelectorCombinator_Obj> combinators1;
    while (!components1.empty() && Cast<SelectorCombinator>(components1.back())) {
      SelectorCombinator_Obj back = Cast<SelectorCombinator>(components1.back());
      components1.erase(components1.end() - 1);
      combinators1.push_back(back);
    }

    std::vector<SelectorCombinator_Obj> combinators2;
    while (!components2.empty() && Cast<SelectorCombinator>(components2.back())) {
      SelectorCombinator_Obj back = Cast<SelectorCombinator>(components2.back());
      components2.erase(components2.end() - 1);
      combinators2.push_back(back);
    }

    if (combinators1.size() > 1 || combinators2.size() > 1) {
      // If there are multiple combinators, something hacky's going on. If one
      // is a supersequence of the other, use that, otherwise give up.
      auto LCS = lcs<SelectorCombinator>(combinators1, combinators2);

      if (std::equal(LCS.begin(), LCS.end(), combinators1.begin(), ComparePtrsFunction<SelectorCombinator>)) {
        std::vector<std::vector<CompoundOrCombinator_Obj>> items;
        std::vector<CompoundOrCombinator_Obj> reversed;
        reversed.insert(reversed.begin(), combinators2.rbegin(), combinators2.rend());
        items.push_back(reversed);
        result.insert(result.begin(), items);
      }
      else if (std::equal(LCS.begin(), LCS.end(), combinators2.begin(), ComparePtrsFunction<SelectorCombinator>)) {
        std::vector<std::vector<CompoundOrCombinator_Obj>> items;
        std::vector<CompoundOrCombinator_Obj> reversed;
        reversed.insert(reversed.begin(), combinators1.rbegin(), combinators1.rend());
        items.push_back(reversed);
        result.insert(result.begin(), items);
      }
      else {
        return false; // null
      }
      return true;
    }

    // This code looks complicated, but it's actually just a bunch of special
    // cases for interactions between different combinators.
    SelectorCombinator_Obj combinator1, combinator2;
    if (!combinators1.empty()) combinator1 = combinators1.front();
    if (!combinators2.empty()) combinator2 = combinators2.front();
    if (!combinator1.isNull() && !combinator2.isNull()) {
      auto compound1 = Cast<CompoundSelector>(components1.back());
      auto compound2 = Cast<CompoundSelector>(components2.back());
      components1.pop_back(); components2.pop_back();

      if (combinator1->isGeneralCombinator() && combinator2->isGeneralCombinator()) {
        if (compound1->toCompoundSelector()->is_superselector_of(compound2->toCompoundSelector())) { // XXX
          std::vector< std::vector< CompoundOrCombinator_Obj >> items;
          std::vector< CompoundOrCombinator_Obj> item;
          item.push_back(compound2);
          item.push_back(combinator2);
          items.push_back(item);
          result.insert(result.begin(), items);
        }
        else if (compound2->toCompoundSelector()->is_superselector_of(compound1->toCompoundSelector())) { // XXX
          std::vector< std::vector< CompoundOrCombinator_Obj >> items;
          std::vector< CompoundOrCombinator_Obj> item;
          item.push_back(compound1);
          item.push_back(combinator1);
          items.push_back(item);
          result.insert(result.begin(), items);
        }
        else {
          std::vector< std::vector< CompoundOrCombinator_Obj >> choices;
          std::vector< CompoundOrCombinator_Obj > choice1, choice2, choice3;
          choice1.push_back(compound1);
          choice1.push_back(combinator1);
          choice1.push_back(compound2);
          choice1.push_back(combinator2);
          choice2.push_back(compound2);
          choice2.push_back(combinator2);
          choice2.push_back(compound1);
          choice2.push_back(combinator1);
          choices.push_back(choice1);
          choices.push_back(choice2);
          CompoundSelector_Obj unified = compound1->unify_with(compound2);
          if (!unified.isNull()) {
            choice3.push_back(unified);
            choice3.push_back(combinator1);
            choices.push_back(choice3);
          }
          result.insert(result.begin(), choices);
        }
      }
      else if ((combinator1->isGeneralCombinator() && combinator2->isChildCombinator()) ||
        (combinator1->isChildCombinator() && combinator2->isGeneralCombinator())) {

        CompoundSelector_Obj followingSiblingSelector = combinator1->isFollowingSibling() ? compound1 : compound2;
        CompoundSelector_Obj nextSiblingSelector = combinator1->isFollowingSibling() ? compound2 : compound1;

        SelectorCombinator_Obj followingSiblingCombinator = combinator1->isFollowingSibling() ? combinator1 : combinator2;
        SelectorCombinator_Obj nextSiblingCombinator = combinator1->isFollowingSibling() ? combinator2 : combinator1;

        if (followingSiblingSelector->is_superselector_of(nextSiblingSelector)) {
          result.insert(result.begin(), { { nextSiblingSelector, nextSiblingCombinator } });
        }
        else {
          CompoundSelector_Obj unified = compound1->unify_with(compound2);
          std::vector<std::vector<CompoundOrCombinator_Obj>> items;
          items.insert(items.begin(), {
            followingSiblingSelector,
            followingSiblingCombinator,
            nextSiblingSelector,
            nextSiblingCombinator,
            });
          if (!unified.isNull()) {
            items.insert(items.begin(), {
              unified, nextSiblingCombinator,
              });
          }
          result.insert(result.begin(), { items });
        }
      }
      else if (combinator1->isChildCombinator() &&
        (combinator2->isNextSibling() ||
          combinator2->isFollowingSibling())) {
        result.insert(result.begin(), { {
            compound2, combinator2,
        } });
        components1.push_back(compound1);
        components1.push_back(combinator1);
      }
      else if (combinator2->isChildCombinator() &&
        (combinator1->isNextSibling() ||
          combinator1->isFollowingSibling())) {
        result.insert(result.begin(), { {
            compound1, combinator1,
        } });
        components2.push_back(compound2);
        components2.push_back(combinator2);
      }
      else if (*combinator1 == *combinator2) {
        CompoundSelector_Obj unified = compound1->unify_with(compound2);
        if (unified.isNull()) return false; // null
        result.insert(result.begin(), { {
            unified, combinator1,
        } });
      }
      else {
        return false; // null
      }

      return mergeFinalCombinators(components1, components2, result);
    }
    else if (!combinator1.isNull()) {
      if (combinator1->isChildCombinator() && !components2.empty()) {
        CompoundSelector_Obj back1 = Cast<CompoundSelector>(components1.back());
        CompoundSelector_Obj back2 = Cast<CompoundSelector>(components2.back());
        if (back1 && back2 && back2->is_superselector_of(back1)) {
          components2.pop_back();
        }
      }
      result.insert(result.begin(), { {
          components1.back(), combinator1,
      } });
      components1.pop_back();

      return mergeFinalCombinators(components1, components2, result);
    }
    else {
      if (combinator2->isChildCombinator() && !components1.empty()) {
        CompoundSelector_Obj back1 = Cast<CompoundSelector>(components1.back());
        CompoundSelector_Obj back2 = Cast<CompoundSelector>(components2.back());
        if (back1 && back2 && back1->is_superselector_of(back2)) {
          components1.pop_back();
        }
      }
      result.insert(result.begin(), { {
          components2.back(), combinator2,
      } });
      components2.pop_back();

      return mergeFinalCombinators(components1, components2, result);
    }
  }

  /// Returns whether or not [compound] contains a `::root` selector.
  bool _hasRoot(CompoundSelector compound) {
    return false;
  }
  /// If the first element of [queue] has a `::root` selector, removes and returns
  /// that element.
  CompoundSelector_Obj _firstIfRoot(std::vector<CompoundOrCombinator_Obj> queue) {
    if (queue.empty()) return {};
    auto first = queue.front();
    if (CompoundSelector_Obj sel = Cast<CompoundSelector>(first)) {
      if (!_hasRoot(sel)) return {};

      queue.erase(queue.begin());
      return sel;
    }
    else {
      return {};
    }
  }

  std::vector<std::vector<CompoundOrCombinator_Obj>> _groupSelectors(
    std::vector<CompoundOrCombinator_Obj> components) {

    // std::cerr << "COMPONENTS: " << debug_vec(components) << "\n";

    std::vector<std::vector<CompoundOrCombinator_Obj>> groups;

    size_t i = 0;
    while (i < components.size()) {
      std::vector<CompoundOrCombinator_Obj> group;
      while (i < components.size()) {
        // std::cerr << "ADD ITEM: " << std::string(components[i]) << "\n";
        group.push_back(components[i++]);
        // std::cerr << "ADDED ITEM: " << "\n";
        if (i == components.size()) break;
        if (Cast<SelectorCombinator>(components[i])) {
          // std::cerr << "SKIP 1" << "\n";
          break;
        }
        // std::cerr << "MORE\n";
        if (Cast<SelectorCombinator>(group.back())) {
          // std::cerr << "SKIP 2" << "\n";
          break;
        }
        // std::cerr << "GO ON" << "\n";
      }
      groups.push_back(group);
    }

    // std::cerr << "GROUPS OUT: " << debug_vec(groups) << "\n";

    return groups;
  }

  /// Like [complexIsSuperselector], but compares [complex1] and [complex2] as
/// though they shared an implicit base [SimpleSelector].
///
/// For example, `B` is not normally a superselector of `B A`, since it doesn't
/// match elements that match `A`. However, it *is* a parent superselector,
/// since `B X` is a superselector of `B A X`.
  bool complexIsParentSuperselector(std::vector<CompoundOrCombinator_Obj> complex1,
    std::vector<CompoundOrCombinator_Obj> complex2) {
    // Try some simple heuristics to see if we can avoid allocations.
    if (Cast<SelectorCombinator>(complex1.front())) return false;
    if (Cast<SelectorCombinator>(complex2.front())) return false; 
    if (complex1.size() > complex2.size()) return false;
    // std::cerr << "HERE 1\n";
    CompoundSelector_Obj base = SASS_MEMORY_NEW(CompoundSelector, ParserState("[tmp]"));
    ComplexSelector_Obj cplx1 = SASS_MEMORY_NEW(ComplexSelector, ParserState("[tmp]"));
    ComplexSelector_Obj cplx2 = SASS_MEMORY_NEW(ComplexSelector, ParserState("[tmp]"));
    // std::cerr << "HERE 2\n";
    cplx1->concat(complex1); cplx1->append(base);
    // std::cerr << "HERE 3\n";
    cplx2->concat(complex2); cplx2->append(base);
    // std::cerr << "HERE 4\n";
    return cplx1->toComplexSelector()->is_superselector_of(cplx2->toComplexSelector());
  }

  std::vector<std::vector<CompoundOrCombinator_Obj>> unifyComplex(
    std::vector<std::vector<CompoundOrCombinator_Obj>> complexes) {

    SASS_ASSERT(!complexes.empty(), "Can't unify empty list");
    if (complexes.size() == 1) return complexes;

    CompoundSelector_Obj unifiedBase = SASS_MEMORY_NEW(CompoundSelector, ParserState("[tmp]"));
    for (auto complex : complexes) {
      CompoundOrCombinator_Obj base = complex.back();
      if (CompoundSelector * comp = Cast<CompoundSelector>(base)) {
        if (unifiedBase->empty()) {
          unifiedBase->concat(comp);
        }
        else {
          for (Simple_Selector_Obj simple : comp->elements()) {
            unifiedBase = simple->unify_with(unifiedBase);
            if (unifiedBase.isNull()) return {};
          }
        }
      }
      else {
        return {};
      }
    }

    std::vector<std::vector<CompoundOrCombinator_Obj>> complexesWithoutBases;
    for (size_t i = 0; i < complexes.size(); i += 1) {
      std::vector<CompoundOrCombinator_Obj> sel = complexes.at(i);
      sel.pop_back(); // remove last item (base) from the list
      complexesWithoutBases.push_back(sel);
    }

    // [[], [*]]
    // std::cerr << "here 2\n";
    complexesWithoutBases.back().push_back(unifiedBase);

    // debug_ast(complexesWithoutBases, "BASES: ");

    auto rv = weave(complexesWithoutBases);

    // debug_ast(rv, "WEAVE: ");

    return rv;

  }

  /// Returns whether a [CompoundSelector] may contain only one simple selector of
  /// the same type as [simple].
  bool _isUnique(Simple_Selector_Obj simple) {
    if (Cast<Id_Selector>(simple)) return true;
    if (Pseudo_Selector_Obj pseudo = Cast<Pseudo_Selector>(simple)) {
      if (pseudo->is_pseudo_element()) return true;
    }
    return false;
  }

  bool _mustUnify(std::vector<CompoundOrCombinator_Obj> complex1,
    std::vector<CompoundOrCombinator_Obj> complex2) {

    std::vector<Simple_Selector_Obj> uniqueSelectors1;
    for (CompoundOrCombinator_Obj component : complex1) {
      if (CompoundSelector_Obj compound = Cast<CompoundSelector>(component)) {
        for (Simple_Selector_Obj sel : compound->elements()) {
          if (_isUnique(sel)) {
            uniqueSelectors1.push_back(sel);
          }
        }
      }
    }
    if (uniqueSelectors1.empty()) return false;

    // ToDo: unsure if this is correct
    for (CompoundOrCombinator_Obj component : complex2) {
      if (CompoundSelector_Obj compound = Cast<CompoundSelector>(component)) {
        for (auto sel : compound->elements()) {
          if (_isUnique(sel)) {
            for (auto check : uniqueSelectors1) {
              if (*check == *sel) return true;
            }
          }
        }
      }
    }

    return false;

  }

  bool cmpGroups(std::vector<CompoundOrCombinator_Obj> group1, std::vector<CompoundOrCombinator_Obj> group2, std::vector<CompoundOrCombinator_Obj>& select) {

    if (std::equal(group1.begin(), group1.end(), group2.begin(), ComparePtrsFunction<CompoundOrCombinator>)) {
      // std::cerr << "GOT equal\n";
      select = group1;
      return true;
    }

    if (!Cast<CompoundSelector>(group1.front())) return false; // null
    if (!Cast<CompoundSelector>(group2.front())) return false; // null

    if (complexIsParentSuperselector(group1, group2)) {
      // std::cerr << "GOT equal 2\n";
      select = group2;
      return true;
    }
    if (complexIsParentSuperselector(group2, group1)) {
      // std::cerr << "GOT equal 3\n";
      select = group1;
      return true;
    }

    if (!_mustUnify(group1, group2)) return false;

    std::vector<std::vector<CompoundOrCombinator_Obj>> unified = unifyComplex({ group1, group2 });
    if (unified.empty()) return false; // null
    if (unified.size() > 1) return false; // null
    select = unified.front();
    // std::cerr << "GOT equal 4\n";
    return true;
  }

  /// Returns all orderings of initial subseqeuences of [queue1] and [queue2].
///
/// The [done] callback is used to determine the extent of the initial
/// subsequences. It's called with each queue until it returns `true`.
///
/// This destructively removes the initial subsequences of [queue1] and
/// [queue2].
///
/// For example, given `(A B C | D E)` and `(1 2 | 3 4 5)` (with `|` denoting
/// the boundary of the initial subsequence), this would return `[(A B C 1 2),
/// (1 2 A B C)]`. The queues would then contain `(D E)` and `(3 4 5)`.
  template <class T>
  std::vector<std::vector<T>> _chunks(
    std::vector<T> queue1, std::vector<T> queue2, T group, bool(*done)(std::vector<T>, T)) {

    std::vector<T> chunk1;
    while (!done(queue1, group)) {
      chunk1.push_back(queue1.front());
      queue1.erase(queue1.begin());
    }

    std::vector<T> chunk2;
    while (!done(queue2, group)) {
      chunk2.push_back(queue2.front());
      queue2.erase(queue2.begin());
    }

    if (chunk1.empty() && chunk2.empty()) return {};
    if (chunk1.empty()) return { chunk2 };
    if (chunk2.empty()) return { chunk1 };

    std::vector<T> choice1, choice2;
    choice1.insert(choice1.end(), chunk1.begin(), chunk1.end());
    choice1.insert(choice1.end(), chunk2.begin(), chunk2.end());
    choice2.insert(choice2.end(), chunk2.begin(), chunk2.end());
    choice2.insert(choice2.end(), chunk1.begin(), chunk1.end());

    return { choice1, choice2 };
  }

  bool cmpChunks(std::vector< std::vector<CompoundOrCombinator_Obj>> seq, std::vector<CompoundOrCombinator_Obj> group) {
    return seq.empty();
  }

  bool cmpChunks2(std::vector< std::vector<CompoundOrCombinator_Obj>> seq, std::vector<CompoundOrCombinator_Obj> group) {
    // std::cerr << "--- DO COMPARE 2: " << debug_vec(seq) << " vs " << debug_vec(group) << "\n";
    bool rv = complexIsParentSuperselector(seq.front(), group);
    // std::cerr << "--- COMPARE 2" << "\n";
    return rv;
  }

  template <class T>
  std::vector<std::vector<T>>
    permutate(std::vector<std::vector<T>> in) {

    size_t L = in.size();
    size_t n = in.size() - 1;
    size_t* state = new size_t[L];
    std::vector< std::vector<T>> out;

    // First initialize all states for every permutation group
    for (size_t i = 0; i < L; i += 1) {
      if (in[i].size() == 0) return {};
      state[i] = in[i].size() - 1;
    }

    while (true) {
      /*
      std::cerr << "PERM: ";
      for (size_t p = 0; p < L; p++)
      { std::cerr << state[p] << " "; }
      std::cerr << "\n";
      */
      std::vector<T> perm;
      // Create one permutation for state
      for (size_t i = 0; i < L; i += 1) {
        perm.push_back(in.at(i).at(in[i].size() - state[i] - 1));
      }
      // Current group finished
      if (state[n] == 0) {
        // Find position of next decrement
        while (n > 0 && state[--n] == 0)
        {

        }
        // Check for end condition
        if (state[n] != 0) {
          // Decrease next on the left side
          state[n] -= 1;
          // Reset all counters to the right
          for (size_t p = n + 1; p < L; p += 1) {
            state[p] = in[p].size() - 1;
          }
          // Restart from end
          n = L - 1;
        }
        else {
          out.push_back(perm);
          break;
        }
      }
      else {
        state[n] -= 1;
      }
      out.push_back(perm);
    }

    delete[] state;
    return out;
  }

  /// Returns a list of all possible paths through the given lists.
///
/// For example, given `[[1, 2], [3, 4], [5]]`, this returns:
///
/// ```
/// [[1, 3, 5],
///  [2, 3, 5],
///  [1, 4, 5],
///  [2, 4, 5]]
/// ```
  template <class T>
  std::vector<std::vector<T>> paths(std::vector<std::vector<T>> choices) {

    // std::cerr << "PERMUTATE " << debug_vec(choices) << "\n";

    auto qwe = permutate(choices);

    // std::cerr << "PERMUTATED " << debug_vec(qwe) << "\n";

    return qwe;

  }

  template <class T>
  std::vector<T> expandList(std::vector<std::vector<T>> vec) {
    std::vector<T> flattened;
    for (auto items : vec) {
      for (auto item : items) {
        flattened.push_back(item);
      }
    }
    return flattened;
  }


  std::vector<std::vector<CompoundOrCombinator_Obj>> weaveParents(
    std::vector<CompoundOrCombinator_Obj> parents1,
    std::vector<CompoundOrCombinator_Obj> parents2) {

    // std::cerr << "weave parent1: " << debug_vec(parents1) << "\n";
    // std::cerr << "weave parent2: " << debug_vec(parents2) << "\n";

    std::vector<CompoundOrCombinator_Obj>& queue1 = parents1;
    std::vector<CompoundOrCombinator_Obj>& queue2 = parents2;

    std::vector<SelectorCombinator_Obj> initialCombinators;
    std::vector<std::vector<std::vector<CompoundOrCombinator_Obj>>> finalCombinators;
    if (!mergeInitialCombinators(queue1, queue2, initialCombinators)) return {};
    if (!mergeFinalCombinators(queue1, queue2, finalCombinators)) return {};

    // std::cerr << "weave initial: " << debug_vec(initialCombinators) << "\n";
    // std::cerr << "weave final: " << debug_vec(finalCombinators) << "\n";

    // std::cerr << "weave parent1: " << debug_vec(parents1) << "\n";
    // std::cerr << "weave parent2: " << debug_vec(parents2) << "\n";

    // Make sure there's at most one `:root` in the output.
    CompoundSelector_Obj root1 = _firstIfRoot(queue1);
    CompoundSelector_Obj root2 = _firstIfRoot(queue2);

    // std::cerr << "weave root1: " << std::string(root1) << "\n";
    // std::cerr << "weave root2: " << std::string(root2) << "\n";

    if (!root1.isNull() && !root2.isNull()) {
      CompoundSelector_Obj root = root1->unify_with(root2);
      if (root.isNull()) return {}; // null
      queue1.insert(queue1.begin(), root);
      queue2.insert(queue2.begin(), root);
    }
    else if (!root1.isNull()) {
      queue2.insert(queue2.begin(), root1);
    }
    else if (!root2.isNull()) {
      queue1.insert(queue1.begin(), root2);
    }

    // std::cerr << "weave queue1: " << debug_vec(queue1) << "\n";
    // std::cerr << "weave queue2: " << debug_vec(queue2) << "\n";

    std::vector<std::vector<CompoundOrCombinator_Obj>> groups1 = _groupSelectors(queue1);
    std::vector<std::vector<CompoundOrCombinator_Obj>> groups2 = _groupSelectors(queue2);

    // std::cerr << "weave groups1: " << debug_vec(groups1) << "\n";
    // std::cerr << "weave groups2: " << debug_vec(groups2) << "\n";

    // ToDo: this path is not fully implemented yet
    std::vector<std::vector<CompoundOrCombinator_Obj>> LCS = lcs2<std::vector<CompoundOrCombinator_Obj>>(groups2, groups1, cmpGroups);

    // std::cerr << "weave LCS: " << debug_vec(LCS) << "\n";

    // ToDo: this should be easier to downcase vector elements?
    std::vector<std::vector<std::vector<CompoundOrCombinator_Obj>>> choices;
    std::vector<CompoundOrCombinator_Obj> choice;
    choice.insert(choice.begin(), initialCombinators.begin(), initialCombinators.end());
    choices.push_back({ choice });

    // std::cerr << "weave choices: " << debug_vec(choices) << "\n";

    for (auto group : LCS) {
      std::vector<std::vector<std::vector<CompoundOrCombinator_Obj>>> chunks = _chunks(groups1, groups2, group, cmpChunks2);
      // std::cerr << "LCS chunks: " << debug_vec(chunks) << "\n";
      std::vector<std::vector<CompoundOrCombinator_Obj>> expanded = expandList(chunks);
      choices.push_back(expanded);
      choices.push_back({ group });
      groups1.erase(groups1.begin());
      groups2.erase(groups2.begin());
    }

    std::vector<std::vector<std::vector<CompoundOrCombinator_Obj>>> chunks = _chunks(groups1, groups2, {}, cmpChunks);

    // std::cerr << "weave chunks: " << debug_vec(chunks) << "\n";

    std::vector<std::vector<CompoundOrCombinator_Obj>> choicer;

    for (std::vector<std::vector<CompoundOrCombinator_Obj>> chunk : chunks) {
      std::vector<CompoundOrCombinator_Obj> flat;
      for (std::vector<CompoundOrCombinator_Obj> item : chunk) {
        for (CompoundOrCombinator_Obj sel : item) {
          flat.push_back(sel);
        }
      }
      choicer.push_back(flat);
    }

    // std::cerr << "weave choice: " << debug_vec(choicer) << "\n";

    choices.push_back(choicer);
    // std::cerr << "weave choices0: " << debug_vec(choices) << "\n";
    for (auto fin : finalCombinators) {
      choices.push_back(fin);
    }
    // std::cerr << "weave choices1: " << debug_vec(choices) << "\n";

    std::vector<std::vector<std::vector<CompoundOrCombinator_Obj>>> choices2;
    for (auto choice : choices) {
      if (!choice.empty()) {
        choices2.push_back(choice);
      }
    }


    // ToDo: remove empty choices

    // std::cerr << "weave choices: " << debug_vec(choices2) << "\n";

    std::vector<std::vector<std::vector<CompoundOrCombinator_Obj>>> pathes = paths(choices2);

    // std::cerr << "weave paths: " << debug_vec(pathes) << "\n";

    std::vector<std::vector<CompoundOrCombinator_Obj>> rv;

    for (std::vector<std::vector<CompoundOrCombinator_Obj>> path : pathes) {
      std::vector<CompoundOrCombinator_Obj> flat;
      for (std::vector<CompoundOrCombinator_Obj> item : path) {
        for (CompoundOrCombinator_Obj sel : item) {
          flat.push_back(sel);
        }
      }
      rv.push_back(flat);
    }


    return rv;

  }

  std::vector<std::vector<CompoundOrCombinator_Obj>> weave(
  std::vector<std::vector<CompoundOrCombinator_Obj>> complexes) {

    std::vector<std::vector<CompoundOrCombinator_Obj>> prefixes;
    prefixes.push_back(complexes.at(0));

    // std::cerr << "PREFIXES IN: " << debug_vec(prefixes) << "\n";
    // std::cerr << "COMPLEXES IN: " << debug_vec(complexes) << "\n";

    // debug_ast(prefixes, "PREFIXES IN ");
    
    for (size_t i = 1; i < complexes.size(); i += 1) {
      
      if (complexes[i].empty()) {
        // std::cerr << "SKIP EMPTY complexes[" << i << "]\n";
        continue;
      }
      std::vector<CompoundOrCombinator_Obj>& complex = complexes[i];
      CompoundOrCombinator_Obj target = complex.back();
      if (complex.size() == 1) {
        for (auto& prefix : prefixes) {
          // std::cerr << "ADD " << target->to_string() << "\n";
          prefix.push_back(target);
        }
        // std::cerr << "CONTINUE\n";
        continue;
      }
      // std::cerr << "NOT REACHING YET\n";
     //  std::cerr << "COMPLEX: " << debug_vec(complex) << "\n";
      std::vector<CompoundOrCombinator_Obj> parents = complex;
      parents.pop_back();
      // std::cerr << "PARENTS: " << debug_vec(parents) << "\n";

      std::vector<std::vector<CompoundOrCombinator_Obj>> newPrefixes;
      for (std::vector<CompoundOrCombinator_Obj> prefix : prefixes) {
        // std::cerr << "WEAVE PREFIXE: " << debug_vec(prefix) << "\n";
        // std::cerr << "    W PARENTS: " << debug_vec(parents) << "\n";
        auto parentPrefixes = weaveParents(prefix, parents);
        //std::cerr << "WEAVE RV: " << debug_vec(parentPrefixes) << "\n";
        // if (parentPrefixes.isNull()) continue;

        for (auto& parentPrefix : parentPrefixes) {
          parentPrefix.push_back(target);
          newPrefixes.push_back(parentPrefix);
        }
      }
      prefixes = newPrefixes;

    }
    // std::cerr << "PREFIXES OUT: " << debug_vec(prefixes) << "\n";
    return prefixes;

  }

  /*
  Compound_Selector* Compound_Selector::unify_with(Compound_Selector* rhs)
  {
    CompoundSelector_Obj asd = toCompoundSelector(this);
    CompoundSelector_Obj qwe = toCompoundSelector(rhs);
    CompoundSelector_Obj sel = asd->unify_with(qwe);
    return toCompound_Selector(sel).detach();
  }
  */

  CompoundSelector* CompoundSelector::unify_with(CompoundSelector* rhs)
  {
#ifdef DEBUG_UNIFY
    const std::string debug_call = "unify(Compound[" + this->to_string() + "], Compound[" + rhs->to_string() + "])";
    std::cerr << debug_call << std::endl;
#endif

    if (empty()) return rhs;
    CompoundSelector_Obj unified = SASS_MEMORY_COPY(rhs);
    for (const Simple_Selector_Obj& sel : elements()) {
      CompoundSelector_Obj cp = unified;
      unified = sel->unify_with(unified);
      if (unified.isNull()) break;
    }

#ifdef DEBUG_UNIFY
    std::cerr << "> " << debug_call << " = Compound[" << unified->to_string() << "]" << std::endl;
#endif
    return unified.detach();
  }

  Compound_Selector* Simple_Selector::unify_with(Compound_Selector* rhs)
  {
    return unify_with(rhs->toCompSelector())->toCompoundSelector().detach();
  }

  CompoundSelector* Simple_Selector::unify_with(CompoundSelector* rhs)
  {
#ifdef DEBUG_UNIFY
    const std::string debug_call = "unify(Simple[" + this->to_string() + "], Compound[" + rhs->to_string() + "])";
    std::cerr << debug_call << std::endl;
#endif

    if (rhs->length() == 1) {
      if (rhs->at(0)->is_universal()) {
        CompoundSelector* this_compound = SASS_MEMORY_NEW(CompoundSelector, pstate());
        this_compound->append(SASS_MEMORY_COPY(this));
        CompoundSelector* unified = rhs->at(0)->unify_with(this_compound);
        if (unified == nullptr || unified != this_compound) delete this_compound;

#ifdef DEBUG_UNIFY
        std::cerr << "> " << debug_call << " = " << "Compound[" << unified->to_string() << "]" << std::endl;
#endif
        return unified;
      }
    }
    for (const Simple_Selector_Obj& sel : rhs->elements()) {
      if (*this == *sel) {
#ifdef DEBUG_UNIFY
        std::cerr << "> " << debug_call << " = " << "Compound[" << rhs->to_string() << "]" << std::endl;
#endif
        return rhs;
      }
    }
    const int lhs_order = this->unification_order();
    size_t i = rhs->length();
    while (i > 0 && lhs_order < rhs->at(i - 1)->unification_order()) --i;
    rhs->insert(rhs->begin() + i, this);
#ifdef DEBUG_UNIFY
    std::cerr << "> " << debug_call << " = " << "Compound[" << rhs->to_string() << "]" << std::endl;
#endif
    return rhs;
  }

  Simple_Selector* Type_Selector::unify_with(Simple_Selector* rhs)
  {
    #ifdef DEBUG_UNIFY
    const std::string debug_call = "unify(Type[" + this->to_string() + "], Simple[" + rhs->to_string() + "])";
    std::cerr << debug_call << std::endl;
    #endif

    bool rhs_ns = false;
    if (!(is_ns_eq(*rhs) || rhs->is_universal_ns())) {
      if (!is_universal_ns()) {
        #ifdef DEBUG_UNIFY
        std::cerr << "> " << debug_call << " = nullptr" << std::endl;
        #endif
        return nullptr;
      }
      rhs_ns = true;
    }
    bool rhs_name = false;
    if (!(name_ == rhs->name() || rhs->is_universal())) {
      if (!(is_universal())) {
        #ifdef DEBUG_UNIFY
        std::cerr << "> " << debug_call << " = nullptr" << std::endl;
        #endif
        return nullptr;
      }
      rhs_name = true;
    }
    if (rhs_ns) {
      ns(rhs->ns());
      has_ns(rhs->has_ns());
    }
    if (rhs_name) name(rhs->name());
    #ifdef DEBUG_UNIFY
    std::cerr << "> " << debug_call << " = Simple[" << this->to_string() << "]" << std::endl;
    #endif
    return this;
  }

  Compound_Selector* Type_Selector::unify_with(Compound_Selector* rhs)
  {
    return toCompound_Selector(unify_with(toCompoundSelector(rhs))).detach();
  }

  CompoundSelector* Type_Selector::unify_with(CompoundSelector* rhs)
  {
#ifdef DEBUG_UNIFY
    const std::string debug_call = "unify(Type[" + this->to_string() + "], Compound[" + rhs->to_string() + "])";
    std::cerr << debug_call << std::endl;
#endif

    if (rhs->empty()) {
      rhs->append(this);
#ifdef DEBUG_UNIFY
      std::cerr << "> " << debug_call << " = Compound[" << rhs->to_string() << "]" << std::endl;
#endif
      return rhs;
    }
    Type_Selector* rhs_0 = Cast<Type_Selector>(rhs->at(0));
    if (rhs_0 != nullptr) {
      Simple_Selector* unified = unify_with(rhs_0);
      if (unified == nullptr) {
#ifdef DEBUG_UNIFY
        std::cerr << "> " << debug_call << " = nullptr" << std::endl;
#endif
        return nullptr;
      }
      rhs->elements()[0] = unified;
    }
    else if (!is_universal() || (has_ns_ && ns_ != "*")) {
      rhs->insert(rhs->begin(), this);
    }
#ifdef DEBUG_UNIFY
    std::cerr << "> " << debug_call << " = Compound[" << rhs->to_string() << "]" << std::endl;
#endif
    return rhs;
  }

  Compound_Selector* Class_Selector::unify_with(Compound_Selector* rhs)
  {
    return toCompound_Selector(unify_with(toCompoundSelector(rhs))).detach();
  }

  CompoundSelector* Class_Selector::unify_with(CompoundSelector* rhs)
  {
    rhs->has_line_break(has_line_break());
    return Simple_Selector::unify_with(rhs);
  }

  Compound_Selector* Id_Selector::unify_with(Compound_Selector* rhs)
  {
    return toCompound_Selector(unify_with(toCompoundSelector(rhs))).detach();
  }

  CompoundSelector* Id_Selector::unify_with(CompoundSelector* rhs)
  {
    for (const Simple_Selector_Obj& sel : rhs->elements()) {
      if (Id_Selector * id_sel = Cast<Id_Selector>(sel)) {
        if (id_sel->name() != name()) return nullptr;
      }
    }
    rhs->has_line_break(has_line_break());
    return Simple_Selector::unify_with(rhs);
  }

  Compound_Selector* Pseudo_Selector::unify_with(Compound_Selector* rhs)
  {
    return toCompound_Selector(unify_with(toCompoundSelector(rhs))).detach();
  }

  CompoundSelector* Pseudo_Selector::unify_with(CompoundSelector* rhs)
  {
    if (is_pseudo_element()) {
      for (const Simple_Selector_Obj& sel : rhs->elements()) {
        if (Pseudo_Selector * pseudo_sel = Cast<Pseudo_Selector>(sel)) {
          if (pseudo_sel->is_pseudo_element() && pseudo_sel->name() != name()) return nullptr;
        }
      }
    }
    return Simple_Selector::unify_with(rhs);
  }

  SelectorList* ComplexSelector::unify_with(ComplexSelector* rhs)
  {

    SelectorList_Obj list = SASS_MEMORY_NEW(SelectorList, pstate());

    std::vector<std::vector<CompoundOrCombinator_Obj>> rv = unifyComplex({ elements(), rhs->elements() });

    // std::cerr << "here 91\n";

    for (std::vector<CompoundOrCombinator_Obj> items : rv) {
      ComplexSelector_Obj sel = SASS_MEMORY_NEW(ComplexSelector, pstate());
      // std::cerr << "foobar \n";
      sel->concat(items);
      // debug_ast(sel);
      list->append(sel);
    }

    // std::cerr << "here 99\n";
    // debug_ast(list);
    return list.detach();

  }

  SelectorList* Complex_Selector::unify_with(ComplexSelector* rhs)
  {
    ComplexSelector_Obj self = toComplexSelector(this);
    SelectorList_Obj rv = self->unify_with(rhs);
    return rv.detach();
  }

  SelectorList* Selector_List::unify_with(SelectorList* rhs) {

    SelectorList_Obj self = toSelectorList(this);
    return self->unify_with(rhs);

  }

  Selector_List* Selector_List::unify_with(Selector_List* rhs) {

    SelectorList_Obj r = toSelectorList(rhs);
    SelectorList_Obj rv = unify_with(r);
    Selector_List_Obj rvv = toSelector_List(rv);
    return rvv.detach();

  }

  SelectorList* SelectorList::unify_with(SelectorList* rhs) {

    std::vector<ComplexSelector_Obj> result;
    // Unify all of children with RHS's children, storing the results in `unified_complex_selectors`
    for (ComplexSelector_Obj& seq1 : elements()) {
      for (ComplexSelector_Obj& seq2 : rhs->elements()) {
        ComplexSelector_Obj seq1_copy = seq1->clone();
        ComplexSelector_Obj seq2_copy = seq2->clone();
        SelectorList_Obj unified = seq1_copy->unify_with(seq2_copy);
        if (unified) {
          result.reserve(result.size() + unified->length());
          std::copy(unified->begin(), unified->end(), std::back_inserter(result));
        }
      }
    }

    // Creates the final Selector_List by combining all the complex selectors
    SelectorList* final_result = SASS_MEMORY_NEW(SelectorList, pstate(), result.size());
    for (ComplexSelector_Obj& sel : result) {
      final_result->append(sel);
    }

    return final_result;

  }

}
