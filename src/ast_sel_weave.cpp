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

#include "dart_helpers.hpp"
#include "ast_fwd_decl.hpp"
#include "ast_selectors.hpp"
#include "paths.hpp"
#include "debugger.hpp"

namespace Sass {


  bool cmpChunks(std::vector< std::vector<CompoundOrCombinator_Obj>> seq, std::vector<CompoundOrCombinator_Obj> group) {
    return seq.empty();
  }

  /// Returns whether or not [compound] contains a `::root` selector.
  bool _hasRoot(CompoundSelector compound) {
    return false;
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
    std::vector<T>& queue1, std::vector<T>& queue2, T group, bool(*done)(std::vector<T>, T)) {

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

    std::cerr << "CHUNK1 " << debug_vec(chunk1) << "\n";
    std::cerr << "CHUNK2 " << debug_vec(chunk2) << "\n";

    if (chunk1.empty() && chunk2.empty()) return {};
    if (chunk1.empty()) return { chunk2 };
    if (chunk2.empty()) return { chunk1 };

    std::vector<T> choice1, choice2;
    choice1.insert(choice1.end(), chunk1.begin(), chunk1.end());
    choice1.insert(choice1.end(), chunk2.begin(), chunk2.end());
    choice2.insert(choice2.end(), chunk2.begin(), chunk2.end());
    choice2.insert(choice2.end(), chunk1.begin(), chunk1.end());

    std::cerr << "CHOICE1 " << debug_vec(choice1) << "\n";
    std::cerr << "CHOICE2 " << debug_vec(choice2) << "\n";

    return { choice1, choice2 };
  }

  bool cmpGroups(std::vector<CompoundOrCombinator_Obj> group1, std::vector<CompoundOrCombinator_Obj> group2, std::vector<CompoundOrCombinator_Obj>& select) {

    std::cerr << "cmp group 1\n";

    std::cerr << "group1" << debug_vec(group1) << "\n";
    std::cerr << "group2" << debug_vec(group2) << "\n";;

    if (group1.size() == group2.size() && std::equal(group1.begin(), group1.end(), group2.begin(), ComparePtrsFunction<CompoundOrCombinator>)) {
      select = group1;
      std::cerr << "GOT equal " << debug_vec(select) << "\n";
      return true;
    }

    if (!Cast<CompoundSelector>(group1.front())) {
      std::cerr << "group1 no front compound\n";
      return false; // null
    }
    if (!Cast<CompoundSelector>(group2.front())) {
      std::cerr << "group2 no front compound\n";
      select = {};
      return false; // null
    }

    if (complexIsParentSuperselector(group1, group2)) {
      select = group2;
      std::cerr << "GOT equal 2 " << debug_vec(select) << "\n";
      return true;
    }
    if (complexIsParentSuperselector(group2, group1)) {
      select = group1;
      std::cerr << "GOT equal 3 " << debug_vec(select) << "\n";
      return true;
    }

    if (!_mustUnify(group1, group2)) {
      std::cerr << "GOT MUST UNIFY\n";
      select = {};
      return false;
    }

    std::cerr << "unify complex now\n";

    std::vector<std::vector<CompoundOrCombinator_Obj>> unified = unifyComplex({ group1, group2 });
    if (unified.empty()) return false; // null
    if (unified.size() > 1) return false; // null
    select = unified.front();
    std::cerr << "GOT equal 4 " << debug_vec(select) << "\n";
    return true;
  }

  bool cmpChunks2(std::vector< std::vector<CompoundOrCombinator_Obj>> seq, std::vector<CompoundOrCombinator_Obj> group) {
    if (seq.empty()) return true;
    std::cerr << "--- DO COMPARE 2: "<< "\n";
    std::cerr << "  lhs: " << debug_vec(seq.front()) << "\n";
    std::cerr << "  rhs: " << debug_vec(group) << "\n";
    bool rv = complexIsParentSuperselector(seq.front(), group);
    std::cerr << "  ==== " << debug_bool(rv) << "\n";
    return rv;
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

  // Returns [complex], grouped into sub-lists such that no sub-list
  // contains two adjacent [ComplexSelector]s. For example,
  // `(A B > C D + E ~ > G)` is grouped into `[(A) (B > C) (D + E ~ > G)]`.
  std::vector<std::vector<CompoundOrCombinator_Obj>> _groupSelectors(
    std::vector<CompoundOrCombinator_Obj> components) {

    // std::cerr << "COMPONENTS: " << debug_vec(components) << "\n";

    std::vector<std::vector<CompoundOrCombinator_Obj>> groups;

    bool lastWasCompound = false;
    std::vector<CompoundOrCombinator_Obj> group;
    for (size_t i = 0; i < components.size(); i += 1) {
      if (CompoundSelector_Obj compound = components[i]->getCompound()) {
        if (lastWasCompound) {
          groups.push_back(group);
          group.clear();
        }
        group.push_back(compound);
        lastWasCompound = true;
      }
      else if (SelectorCombinator_Obj combinator = components[i]->getCombinator()) {
        group.push_back(combinator);
        lastWasCompound = false;
      }
    }
    if (!group.empty()) {
      groups.push_back(group);
    }

    return groups;

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

  

  /// Extracts leading [Combinator]s from [components1] and [components2] and
  /// merges them together into a single list of combinators.
  ///
  /// If there are no combinators to be merged, returns an empty list. If the
  /// combinators can't be merged, returns `null`.
  bool mergeInitialCombinators(
    std::vector<CompoundOrCombinator_Obj>& components1,
    std::vector<CompoundOrCombinator_Obj>& components2,
    std::vector<SelectorCombinator_Obj>& result) {

    std::vector<SelectorCombinator_Obj> combinators1;
    while (!components1.empty() && Cast<SelectorCombinator>(components1.front())) {
      SelectorCombinator_Obj front = Cast<SelectorCombinator>(components1.front());
      std::cerr << "erase from components1\n";
      components1.erase(components1.begin());
      combinators1.push_back(front);
    }

    std::vector<SelectorCombinator_Obj> combinators2;
    while (!components2.empty() && Cast<SelectorCombinator>(components2.front())) {
      SelectorCombinator_Obj front = Cast<SelectorCombinator>(components2.front());
      std::cerr << "erase from components2\n";
      components2.erase(components2.begin());
      combinators2.push_back(front);
    }

    // If neither sequence of combinators is a subsequence of the other, they
    // cannot be merged successfully.
    std::cerr << "LCS in " << debug_vec(combinators1) << " & " << debug_vec(combinators2) << "\n";
    
    std::vector< SelectorCombinator_Obj> LCS = lcs<SelectorCombinator>(combinators1, combinators2);

    std::cerr << "LCS " << debug_vec(LCS) << "\n";

    if (LCS.size() == combinators1.size() && std::equal(LCS.begin(), LCS.end(), combinators1.begin(), ComparePtrsFunction<SelectorCombinator>)) {
      std::cerr << "return combinators2\n";
      result = combinators2; // Does this work?
      return true;
    }
    if (LCS.size() == combinators2.size() && std::equal(LCS.begin(), LCS.end(), combinators2.begin(), ComparePtrsFunction<SelectorCombinator>)) {
      std::cerr << "return combinators1\n";
      result = combinators1; // Does this work?
      return true;
    }

    std::cerr << "failed to merge initial\n";

    return false; // null

  }


  /// Extracts trailing [Combinator]s, and the selectors to which they apply, from
  /// [components1] and [components2] and merges them together into a single list.
  ///
  /// If there are no combinators to be merged, returns an empty list. If the
  /// sequences can't be merged, returns `null`.
  bool mergeFinalCombinators(
    std::vector<CompoundOrCombinator_Obj>& components1,
    std::vector<CompoundOrCombinator_Obj>& components2,
    std::vector<std::vector<std::vector<CompoundOrCombinator_Obj>>>& result) {

    std::cerr << "mergeFinalCombinators c1 " << debug_vec(components1) << "\n";
    std::cerr << "mergeFinalCombinators c2 " << debug_vec(components2) << "\n";

    if (components1.empty() || !Cast<SelectorCombinator>(components1.back())) {
      if (components2.empty() || !Cast<SelectorCombinator>(components2.back())) {
        std::cerr << "NOT COMPOUNDY FINAL " << debug_vec(result) << "\n";
        return true;
      }
    }
    
    std::vector<SelectorCombinator_Obj> combinators1;
    while (!components1.empty() && Cast<SelectorCombinator>(components1.back())) {
      std::cerr << "ERASE C1\n";
      SelectorCombinator_Obj back = Cast<SelectorCombinator>(components1.back());
      components1.erase(components1.end() - 1);
      combinators1.push_back(back);
    }

    std::vector<SelectorCombinator_Obj> combinators2;
    while (!components2.empty() && Cast<SelectorCombinator>(components2.back())) {
      std::cerr << "ERASE C2\n";
      SelectorCombinator_Obj back = Cast<SelectorCombinator>(components2.back());
      components2.erase(components2.end() - 1);
      combinators2.push_back(back);
    }

    std::cerr << "c1 " << debug_vec(components1) << "\n";
    std::cerr << "c2 " << debug_vec(components2) << "\n";

    if (combinators1.size() > 1 || combinators2.size() > 1) {
      // If there are multiple combinators, something hacky's going on. If one
      // is a supersequence of the other, use that, otherwise give up.
      std::cerr << "DO LCS " << debug_vec(combinators1) << "\n";
      std::cerr << "DO LCS " << debug_vec(combinators2) << "\n";
      auto LCS = lcs<SelectorCombinator>(combinators1, combinators2);
      std::cerr << "DID LCS " << debug_vec(LCS) << "\n";

      if (LCS.size() == combinators1.size() && std::equal(LCS.begin(), LCS.end(), combinators1.begin(), ComparePtrsFunction<SelectorCombinator>)) {
        std::vector<std::vector<CompoundOrCombinator_Obj>> items;
        std::vector<CompoundOrCombinator_Obj> reversed;
        reversed.insert(reversed.begin(), combinators2.rbegin(), combinators2.rend());
        items.push_back(reversed);
        result.insert(result.begin(), items);
      }
      else if (LCS.size() == combinators2.size() && std::equal(LCS.begin(), LCS.end(), combinators2.begin(), ComparePtrsFunction<SelectorCombinator>)) {
        std::vector<std::vector<CompoundOrCombinator_Obj>> items;
        std::vector<CompoundOrCombinator_Obj> reversed;
        reversed.insert(reversed.begin(), combinators1.rbegin(), combinators1.rend());
        items.push_back(reversed);
        result.insert(result.begin(), items);
      }
      else {
        return false; // null
      }

      std::cerr << "RESULT " << debug_vec(result) << "\n";

      return true;
    }

    // This code looks complicated, but it's actually just a bunch of special
    // cases for interactions between different combinators.
    SelectorCombinator_Obj combinator1, combinator2;
    if (!combinators1.empty()) combinator1 = combinators1.front();
    if (!combinators2.empty()) combinator2 = combinators2.front();

    std::cerr << "combinator1 " << debug_vec(combinator1) << "\n";
    std::cerr << "combinator2 " << debug_vec(combinator2) << "\n";

    if (!combinator1.isNull() && !combinator2.isNull()) {
      std::cerr << "GO INTO IF 1\n";
      auto compound1 = Cast<CompoundSelector>(components1.back());
      auto compound2 = Cast<CompoundSelector>(components2.back());
      components1.pop_back(); components2.pop_back();

      std::cerr << "c1 " << debug_vec(components1) << "\n";
      std::cerr << "c2 " << debug_vec(components2) << "\n";

      if (combinator1->isGeneralCombinator() && combinator2->isGeneralCombinator()) {
        std::cerr << "both general combinator\n";
        if (compound1->isSuperselectorOf(compound2)) { // XXX
          std::vector< std::vector< CompoundOrCombinator_Obj >> items;
          std::vector< CompoundOrCombinator_Obj> item;
          item.push_back(compound2);
          item.push_back(combinator2);
          items.push_back(item);
          result.insert(result.begin(), items);
        }
        else if (compound2->isSuperselectorOf(compound1)) { // XXX
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
      else if ((combinator1->isGeneralCombinator() && combinator2->isNextSibling()) ||
        (combinator1->isNextSibling() && combinator2->isGeneralCombinator())) {

        std::cerr << "second case\n";

        std::cerr << "compound1 " << debug_vec(compound1) << "\n";
        std::cerr << "compound2 " << debug_vec(compound2) << "\n";

        CompoundSelector_Obj followingSiblingSelector = combinator1->isFollowingSibling() ? compound1 : compound2;
        CompoundSelector_Obj nextSiblingSelector = combinator1->isFollowingSibling() ? compound2 : compound1;

        SelectorCombinator_Obj followingSiblingCombinator = combinator1->isFollowingSibling() ? combinator1 : combinator2;
        SelectorCombinator_Obj nextSiblingCombinator = combinator1->isFollowingSibling() ? combinator2 : combinator1;

        std::cerr << "followingSiblingSelector " << debug_vec(followingSiblingSelector) << "\n";
        std::cerr << "nextSiblingSelector " << debug_vec(nextSiblingSelector) << "\n";

        if (followingSiblingSelector->isSuperselectorOf(nextSiblingSelector)) {
          std::cerr << "IS SUPERSELECTOR\n";
          result.insert(result.begin(), { { nextSiblingSelector, nextSiblingCombinator } });
        }
        else {
          std::cerr << "Unify compounds\n";
          CompoundSelector_Obj unified = compound1->unify_with(compound2);
          std::cerr << "unified " << debug_vec(unified) << "\n";
          std::vector<std::vector<CompoundOrCombinator_Obj>> items;
          std::vector<CompoundOrCombinator_Obj> head = {
            followingSiblingSelector,
            followingSiblingCombinator,
            nextSiblingSelector,
            nextSiblingCombinator,
          };
          std::cerr << "head " << debug_vec(head) << "\n";

          if (!unified.isNull()) {
            items.insert(items.begin(), {
              unified, nextSiblingCombinator,
              });
          }

          items.insert(items.begin(), head);

          result.insert(result.begin(), { items });
        }

        std::cerr << "2 OVER " << debug_vec(result) << "\n";

      }
      else if (combinator1->isChildCombinator() &&
        (combinator2->isNextSibling() ||
          combinator2->isFollowingSibling())) {
        std::cerr << "third case\n";
        result.insert(result.begin(), { {
            compound2, combinator2,
        } });
        components1.push_back(compound1);
        components1.push_back(combinator1);
      }
      else if (combinator2->isChildCombinator() &&
        (combinator1->isNextSibling() ||
          combinator1->isFollowingSibling())) {
        std::cerr << "fourth case\n";
        result.insert(result.begin(), { {
            compound1, combinator1,
        } });
        components2.push_back(compound2);
        components2.push_back(combinator2);
      }
      else if (*combinator1 == *combinator2) {
        std::cerr << "fifth case\n";
        CompoundSelector_Obj unified = compound1->unify_with(compound2);
        if (unified.isNull()) return false; // null
        result.insert(result.begin(), { {
            unified, combinator1,
        } });
      }
      else {
        std::cerr << "last case\n";
        return false; // null
      }

      std::cerr << "MERGE AGAIN1 " << debug_vec(result) << "\n";

      return mergeFinalCombinators(components1, components2, result);
    }
    else if (!combinator1.isNull()) {
      std::cerr << "CASE NR 6\n";
      if (combinator1->isChildCombinator() && !components2.empty()) {
        std::cerr << "Has child with c2\n";
        CompoundSelector_Obj back1 = Cast<CompoundSelector>(components1.back());
        CompoundSelector_Obj back2 = Cast<CompoundSelector>(components2.back());
        if (back1 && back2 && back2->isSuperselectorOf(back1)) {
          components2.pop_back();
        }
      }
      result.insert(result.begin(), { {
          components1.back(), combinator1,
      } });
      components1.pop_back();

      std::cerr << "components1 " << debug_vec(components1) << "\n";
      std::cerr << "components2 " << debug_vec(components2) << "\n";
      std::cerr << "result " << debug_vec(result) << "\n";

      return mergeFinalCombinators(components1, components2, result);
    }
    else {
      std::cerr << "CASE NR 7\n";
      if (combinator2->isChildCombinator() && !components1.empty()) {
        CompoundSelector_Obj back1 = Cast<CompoundSelector>(components1.back());
        CompoundSelector_Obj back2 = Cast<CompoundSelector>(components2.back());
        if (back1 && back2 && back1->isSuperselectorOf(back2)) {
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

  std::vector<std::vector<CompoundOrCombinator_Obj>> weave(
    std::vector<std::vector<CompoundOrCombinator_Obj>> complexes) {

    std::vector<std::vector<CompoundOrCombinator_Obj>> prefixes;
    prefixes.push_back(complexes.at(0));

    std::cerr << "PREFIXES IN: " << debug_vec(prefixes) << "\n";
    std::cerr << "COMPLEXES IN: " << debug_vec(complexes) << "\n";

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
        std::cerr << "CONTINUE1\n";
        continue;
      }
      std::cerr << "NOT REACHING YET\n";
      std::cerr << "COMPLEX: " << debug_vec(complex) << "\n";
      std::vector<CompoundOrCombinator_Obj> parents = complex;
      parents.pop_back();
      std::cerr << "PARENTS: " << debug_vec(parents) << "\n";

      std::vector<std::vector<CompoundOrCombinator_Obj>> newPrefixes;
      for (std::vector<CompoundOrCombinator_Obj> prefix : prefixes) {
        std::cerr << "WEAVE PREFIXE: " << debug_vec(prefix) << "\n";
        std::cerr << "    W PARENTS: " << debug_vec(parents) << "\n";
        auto parentPrefixes = weaveParents(prefix, parents);
        std::cerr << "WEAVE RV: " << debug_vec(parentPrefixes) << "\n";
        if (parentPrefixes.empty()) continue;

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

  // https://www.geeksforgeeks.org/longest-common-subsequence/
  // https://www.geeksforgeeks.org/printing-longest-common-subsequence/
  template <class T>
  std::vector<T> lcs23(std::vector<T>& X, std::vector<T>& Y, bool(*select)(T, T, T&)) {
    std::size_t m = X.size();
    std::size_t n = Y.size();

    std::vector<T> lcs;
    if (m == 0) return lcs;
    if (n == 0) return lcs;

    size_t L[m + 1][n + 1];
    bool B[m + 1][n + 1];
    T R[m + 1][n + 1];

    /* Following steps build L[m+1][n+1] in bottom up fashion. Note
      that L[i][j] contains length of LCS of X[0..i-1] and Y[0..j-1] */
    for (size_t i = 0; i <= m; i++) {
      for (size_t j = 0; j <= n; j++) {
        if (i == 0 || j == 0)
          L[i][j] = 0;
        else {
          B[i - 1][j - 1] = select(X[i - 1], Y[j - 1], R[i - 1][j - 1]);
          if (B[i-1][j-1])
            L[i][j] = L[i - 1][j - 1] + 1;
          else
            L[i][j] = std::max(L[i - 1][j], L[i][j - 1]);
        }
      }
    }

    // Following code is used to print LCS
    std::size_t index = L[m][n];

    lcs.reserve(index);
    for (size_t o = 0; o < index; o++) {
      lcs.push_back({});
    }

    // Create an array to store the lcs groups
    // std::vector<Selector_Group_Obj> lcs(index);

    // Start from the right-most-bottom-most corner and
    // one by one store objects in lcs[]
    std::size_t i = m, j = n;
    // // std::cerr << "LCS 5\n";
    while (i > 0 && j > 0) {

      // If current objects in X[] and Y are same, then
      // current object is part of LCS
      // // std::cerr << "LCS 6 " << i << " : " << j << "\n";
      // // std::cerr << debug_vec(Y[j - 1]) << "\n";
      // // std::cerr << debug_vec(X[j - 1]) << "\n";
      if (B[i - 1][j - 1])
      {
        // // std::cerr << "they are the same\n";
        lcs[index - 1] = R[i - 1][j - 1]; // Put current object in result
        // // std::cerr << "assigned\n";
        i--; j--; index--;     // reduce values of i, j and index
      }

      // If not same, then find the larger of two and
      // go in the direction of larger value
      else if (L[i - 1][j] > L[i][j - 1])
        i--;
      else
        j--;

      //  // std::cerr << "LCS 8\n";

    }

    return lcs;
  }

  std::vector<std::vector<CompoundOrCombinator_Obj>> weaveParents(
    std::vector<CompoundOrCombinator_Obj> parents1,
    std::vector<CompoundOrCombinator_Obj> parents2) {

    std::cerr << "weave parent1: " << debug_vec(parents1) << "\n";
    std::cerr << "weave parent2: " << debug_vec(parents2) << "\n";

    std::vector<CompoundOrCombinator_Obj>& queue1 = parents1;
    std::vector<CompoundOrCombinator_Obj>& queue2 = parents2;

    std::vector<SelectorCombinator_Obj> initialCombinators;
    std::vector<std::vector<std::vector<CompoundOrCombinator_Obj>>> finalCombinators;
    if (!mergeInitialCombinators(queue1, queue2, initialCombinators)) {
      std::cerr << "could not merge initial\n";
      return {};
    }
    if (!mergeFinalCombinators(queue1, queue2, finalCombinators)) {
      std::cerr << "could not merge final\n";
      return {};
    }

    std::cerr << "weave initial: " << debug_vec(initialCombinators) << "\n";
    std::cerr << "weave final: " << debug_vec(finalCombinators) << "\n";

    std::cerr << "weave queue1: " << debug_vec(queue1) << "\n";
    std::cerr << "weave queue2: " << debug_vec(queue2) << "\n";

    // Make sure there's at most one `:root` in the output.
    CompoundSelector_Obj root1 = _firstIfRoot(queue1);
    CompoundSelector_Obj root2 = _firstIfRoot(queue2);

    std::cerr << "weave root1: " << std::string(root1) << "\n";
    std::cerr << "weave root2: " << std::string(root2) << "\n";

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
    else {
      std::cerr << "BOTH ROOTS NULL\n";
    }

    std::cerr << "weave queue1: " << debug_vec(queue1) << "\n";
    std::cerr << "weave queue2: " << debug_vec(queue2) << "\n";
    
    std::vector<std::vector<CompoundOrCombinator_Obj>> groups1 = _groupSelectors(queue1);
    std::vector<std::vector<CompoundOrCombinator_Obj>> groups2 = _groupSelectors(queue2);

    std::cerr << "weave groups1: " << debug_vec(groups1) << "\n";
    std::cerr << "weave groups2: " << debug_vec(groups2) << "\n";

    // ToDo: this path is not fully implemented yet
    std::vector<std::vector<CompoundOrCombinator_Obj>> LCS =
      lcs23<std::vector<CompoundOrCombinator_Obj>>(groups1, groups2, cmpGroups);

    std::cerr << "weave LCS23: " << debug_vec(LCS) << "\n";

    // ToDo: this should be easier to downcase vector elements?
    std::vector<std::vector<std::vector<CompoundOrCombinator_Obj>>> choices;
    std::vector<CompoundOrCombinator_Obj> choice;
    choice.insert(choice.begin(), initialCombinators.begin(), initialCombinators.end());
    choices.push_back({ choice });

    // std::cerr << "weave choices: " << debug_vec(choices) << "\n";

    for (auto group : LCS) {
      std::vector<std::vector<std::vector<CompoundOrCombinator_Obj>>> chunks2 = _chunks(groups1, groups2, group, cmpChunks2);

      std::vector<std::vector<CompoundOrCombinator_Obj>> expanded;

      // now do the expansion
      for (auto chk : chunks2) {
        expanded.push_back(expandList(chk));
      }
      std::vector<std::vector<CompoundOrCombinator_Obj>> chunks = expandList(chunks2);
      std::cerr << "CHUNKED " << debug_vec(expanded) << "\n";
      // (chunk) => chunk.expand((group) => group)

      // std::cerr << "LCS chunks: " << debug_vec(chunks) << "\n";
      // std::vector<std::vector<CompoundOrCombinator_Obj>> expanded = expandList(chunks2);
      choices.push_back(expanded);
      choices.push_back({ group });
      groups1.erase(groups1.begin());
      groups2.erase(groups2.begin());

      std::cerr << "CHOICES " << debug_vec(choices) << "\n";
      std::cerr << "GROUPS1 " << debug_vec(groups1) << "\n";
      std::cerr << "GROUPS2 " << debug_vec(groups2) << "\n";

    }

    std::vector<std::vector<std::vector<CompoundOrCombinator_Obj>>> chunks = _chunks(groups1, groups2, {}, cmpChunks);

    // std::cerr << "weave chunks: " << debug_vec(chunks) << "\n";

    std::vector<std::vector<CompoundOrCombinator_Obj>> choicer;

    for (std::vector<std::vector<CompoundOrCombinator_Obj>> chunk : chunks) {
      choicer.push_back(expandList(chunk));
    }



    choices.push_back(choicer);

    std::cerr << "CHOICER1 " << debug_vec(choices) << "\n";

    for (auto fin : finalCombinators) {
      choices.push_back(fin);
    }

    std::cerr << "CHOICER2 " << debug_vec(choices) << "\n";

    std::vector<std::vector<std::vector<CompoundOrCombinator_Obj>>> choices2;
    for (auto choice : choices) {
      if (!choice.empty()) {
        choices2.push_back(choice);
      }
    }

    std::cerr << "CHOICER3 " << debug_vec(choices2) << "\n";

    // ToDo: remove empty choices

    // std::cerr << "weave choices: " << debug_vec(choices2) << "\n";

    std::vector<std::vector<std::vector<CompoundOrCombinator_Obj>>> pathes = paths(choices2);

    std::cerr << "PATHS " << debug_vec(pathes) << "\n";

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

}
