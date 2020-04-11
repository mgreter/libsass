#ifndef SASS_ENVIRONMENT_CNT_H
#define SASS_ENVIRONMENT_CNT_H

// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

namespace Sass {

  template<class T>
  using EnvKeyMap = UnorderedMap<
    const EnvKey, T, hashEnvKey, equalsEnvKey,
    Sass::Allocator<std::pair<const EnvKey, T>>
  >;

  using EnvKeySet = UnorderedSet<
    EnvKey, hashEnvKey, equalsEnvKey,
    Sass::Allocator<EnvKey>
  >;

  template<typename T>
  // Performance comparisons on MSVC and bolt-bench:
  // tsl::hopscotch_map is 10% slower than Sass::FlatMap
  // std::unordered_map a bit faster than tsl::hopscotch_map
  // Sass::FlapMap is 10% faster than any other container
  // Note: only due to our very specific usage patterns!
  using EnvKeyFlatMap = FlatMap<EnvKey, T>;

};

#endif
