#ifndef SASS_DART_HELPERS_H
#define SASS_DART_HELPERS_H

#include <vector>
#include <utility>
#include <iterator>
#include <functional>

namespace Sass {

  // ##########################################################################
  // Flatten `vector<vector<T>>` to `vector<T>`
  // ##########################################################################
  template < 
    template < typename... > class R = std::vector,
    typename Top,
    typename Sub = typename Top::value_type,
    typename Inner = typename Sub::value_type
  >
    R<Inner> flatten(Top all)
  {
    R<Inner> list;
    for (auto rv : all) {
      std::move(std::begin(rv), std::end(rv),
        std::inserter(list, std::end(list)));
    }
    return list;
  }
  // EO flatten

  // ##########################################################################
  // Expands each element of this Iterable into zero or more elements.
  // Calls a function on every element and ads all results to flat array
  // ##########################################################################
  template <
    class Top, class RV, typename ...Args,
    template< typename... > class R = std::vector,
    typename Sub = typename Top::value_type,
    typename RT = typename RV::value_type
  >
    R<RT> expand(Top cnt, RV(*fn)(Sub, Args...), Args... args)
  {
    R<RT> list;
    for (Sub sub : cnt) {
      RV rv = fn(sub, args...);
      std::move(std::begin(rv), std::end(rv),
        std::inserter(list, std::end(list)));
    }
    return list;
  }
  // EO expand

  // ##########################################################################
  // Calls `fn` on every child and store result on list
  // ##########################################################################
  template <
    template< typename... > class R = std::vector,
    class Top, typename ...Args, class RV,
    typename Sub = typename Top::value_type
  >
    R<RV> forEach(Top cnt, RV(*fn)(Sub, Args...), Args... args)
  {
    R<RV> list;
    for (Sub sub : cnt) {
      list.emplace_back(std::move(fn(sub, args...)));
    }
    return list;
  }
  // EO forEach

  // ##########################################################################
  // Equivalent to dart `cnt.any`
  // Pass additional closure variables to `fn`
  // ##########################################################################
  template <class T, class U, typename ...Args>
  bool hasAny(T cnt, U fn, Args... args) {
    for (auto item : cnt) {
      if (fn(item, args...)) {
        return true;
      }
    }
    return false;
  }
  // EO hasAny

  // ##########################################################################
  // Equivalent to dart `cnt.take(len).any`
  // Pass additional closure variables to `fn`
  // ##########################################################################
  template <class T, class U, typename ...Args>
  bool hasSubAny(T cnt, size_t len, U fn, Args... args) {
    for (size_t i = 0; i < len; i++) {
      if (fn(cnt[i], args...)) {
        return true;
      }
    }
    return false;
  }

  // ##########################################################################
  // Default predicate for lcs algorithm
  // ##########################################################################
  template <class T>
  inline bool lcsIdentityCmp(const T& X, const T& Y, T& result)
  {
    // Assert equality
    if (!ObjEqualityFn(X, Y)) {
      return false;
    }
    // Store in reference
    result = X;
    // Return success
    return true;
  }
  // EO lcsIdentityCmp

  // ##########################################################################
  // Longest common subsequence with predicate
  // ##########################################################################
  template <class T>
  std::vector<T> lcs(
    const std::vector<T>& X, const std::vector<T>& Y,
    bool(*select)(const T&, const T&, T&) = lcsIdentityCmp<T>)
  {

    std::size_t m = X.size(), mm = X.size() + 1;
    std::size_t n = Y.size(), nn = Y.size() + 1;

    if (m == 0) return {};
    if (n == 0) return {};

    // MSVC does not support variable-length arrays
    // To circumvent, allocate one array on the heap
    // Then use a macro to access via double index
    // e.g. `size_t L[m][n]` is supported by gcc
    size_t* len = new size_t[mm * nn + 1];
    bool* acc = new bool[mm * nn + 1];
    T* res = new T[mm * nn + 1];

    #define LEN(x, y) len[(x) * nn + (y)]
    #define ACC(x, y) acc[(x) * nn + (y)]
    #define RES(x, y) res[(x) * nn + (y)]

    /* Following steps build L[m+1][n+1] in bottom up fashion. Note
      that L[i][j] contains length of LCS of X[0..i-1] and Y[0..j-1] */
    for (size_t i = 0; i <= m; i++) {
      for (size_t j = 0; j <= n; j++) {
        if (i == 0 || j == 0)
          LEN(i, j) = 0;
        else {
          ACC(i - 1, j - 1) = select(X[i - 1], Y[j - 1], RES(i - 1, j - 1));
          if (ACC(i - 1, j - 1))
            LEN(i, j) = LEN(i - 1, j - 1) + 1;
          else
            LEN(i, j) = std::max(LEN(i - 1, j), LEN(i, j - 1));
        }
      }
    }


    // Following code is used to print LCS
    std::vector<T> lcs;
    std::size_t index = LEN(m, n);
    lcs.reserve(index);

    // Start from the right-most-bottom-most corner
    // and one by one store objects in lcs[]
    std::size_t i = m, j = n;
    while (i > 0 && j > 0) {

      // If current objects in X[] and Y are same,
      // then current object is part of LCS
      if (ACC(i - 1, j - 1))
      {
        // Put the stored object in result
        // Note: we push instead of unshift
        // Note: reverse the vector later
        // ToDo: is deque more performant?
        lcs.push_back(RES(i - 1, j - 1));
        // reduce values of i, j and index
        i -= 1; j -= 1; index -= 1;
      }

      // If not same, then find the larger of two and
      // go in the direction of larger value
      else if (LEN(i - 1, j) > LEN(i, j - 1)) {
        i--;
      }
      else {
        j--;
      }

    }

    // reverse now as we used push_back
    std::reverse(lcs.begin(), lcs.end());

    // Delete temp memory on heap
    delete[] len;
    delete[] acc;
    delete[] res;

    #undef LEN
    #undef ACC
    #undef RES

    return lcs;
  }
  // EO lcs

  // ##########################################################################
  // ##########################################################################

}

#endif
