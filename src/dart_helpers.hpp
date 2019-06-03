#ifndef SASS_DART_HELPERS_H
#define SASS_DART_HELPERS_H

#include "tsl/ordered_map.h"
#include "tsl/ordered_set.h"

namespace Sass {

  template <class T, class U, class V>
  std::vector<U> mapValues(const std::map<T, U, V>& m) {
    std::vector<U> result;
    for (typename std::map<T, U, V>::const_iterator it = m.begin(); it != m.end(); ++it) {
      result.push_back(it->second);
    }
    return result;
  }

  template <class T, class U, class V, class W>
  std::vector<U> mapValues(const std::unordered_map<T, U, V, W>& m) {
    std::vector<U> result;
    for (typename std::unordered_map<T, U, V, W>::const_iterator it = m.begin(); it != m.end(); ++it) {
      result.push_back(it->second);
    }
    return result;
  }

  template <class T, class U, class V, class W>
  std::vector<U> mapValues(const tsl::ordered_map<T, U, V, W>& m) {
    std::vector<U> result;
    for (typename tsl::ordered_map<T, U, V, W>::const_iterator it = m.begin(); it != m.end(); ++it) {
      result.push_back(it->second);
    }
    return result;
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

  // Equivalent to dart `cnt.any`
  // Pass additional closure variables to `fn`
  template <class T, class U, typename ...Args>
  T expandListFn(T cnt, U fn, Args... args) {
    T flattened;
    for (auto item : cnt) {
      auto rv = fn(item, args...);
      flattened.insert(flattened.end(),
        rv.begin(), rv.end());
    }
    return flattened;
  }

  // Equivalent to dart `cnt.any`
  // Pass additional closure variables to `fn`
  template <class T, class U, typename ...Args>
  bool hasAny(T cnt, U fn, Args... args) {
    for (auto item : cnt) {
      if (fn(item, args...)) {
        return true;
      }
    }
    return false;
  }

  // Equivalent to dart `cnt.take(len).any`
  // Pass additional closure variables to `fn`
  template <class T, class U, typename ...Args>
  bool hasSubAny(T cnt, size_t len, U fn, Args... args) {
    for (size_t i = 0; i < len; i++) {
      if (fn(cnt[i], args...)) {
        return true;
      }
    }
    return false;
  }

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


}

#endif
