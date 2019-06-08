#ifndef SASS_PATHS_H
#define SASS_PATHS_H

#include <string>
#include <vector>
#include <sstream>
#include "debugger.hpp"


template<typename T>
std::string vector_to_string(std::vector<T> v)
{
  std::stringstream buffer;
  buffer << "[";

  if (!v.empty())
  {  buffer << v[0]; }
  else
  { buffer << "]"; }

  if (v.size() == 1)
  { buffer << "]"; }
  else
  {
    for (size_t i = 1, S = v.size(); i < S; ++i) buffer << ", " << v[i];
    buffer << "]";
  }

  return buffer.str();
}

namespace Sass {




  template <class T>
  std::vector<std::vector<T>>
    permutate3(std::vector<std::vector<T>> in) {

    size_t L = in.size();
    size_t n = 0;
    size_t* state = new size_t[L];
    std::vector< std::vector<T>> out;

    // First initialize all states for every permutation group
    for (size_t i = 0; i < L; i += 1) {
      if (in[i].size() == 0) return {};
      state[i] = in[i].size() - 1;
    }
    // std::cerr << "\n";
    // int cnt = 0;
    while (true) {
      // if (cnt++ > 10) exit(1);
      for (size_t p = 0; p < L; p++)
      {
        // std::cerr << state[p] << " ";
      }
      // std::cerr << "\n";

      std::vector<T> perm;
      // Create one permutation for state
      for (size_t i = 0; i < L; i += 1) {
        perm.push_back(in.at(i).at(in[i].size() - state[i] - 1));
      }
      // Current group finished
      if (state[n] == 0) {
        // Find position of next decrement
        while (n < L && state[++n] == 0)
        {

        }

        if (n == L) {
          out.push_back(perm);
          break;
        }

        state[n] -= 1;

        for (size_t p = 0; p < n; p += 1) {
          state[p] = in[p].size() - 1;
        }

        // Restart from front
        n = 0;

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


    auto qwe = permutate3(choices);

    // std::cerr << "PERMUTATED " << debug_vec(qwe) << "\n";

    return qwe;

  }


}

#endif
