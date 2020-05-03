#ifndef SASS_ENVIRONMENT_H
#define SASS_ENVIRONMENT_H

// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include <map>
#include <string>
#include "ast_fwd_decl.hpp"
#include "ast_def_macros.hpp"
#include "ordered_map.hpp"

#include "flat_map.hpp"
#include "environment_key.hpp"
#include "environment_cnt.hpp"

#include "memory.hpp"

/* OPTIMIZATIONS */

// Each scope that opens a new env should have already an env populated
// Basically it should consist of all hoisted variables
// We want to pre-compile assignments and variable access
// This should probably be fully possible in parse step

namespace std {
  template <>
  struct hash<Sass::EnvKey> {
  public:
    inline size_t operator()(const Sass::EnvKey& name) const
    {
      return name.hash();
    }
  };
};

namespace Sass {

  sass::string pluralize(const sass::string& singular, size_t size, const sass::string& plural = "");
  sass::string toSentence(EnvKeyFlatMap<ValueObj>& names, const sass::string& conjunction = "and");

}

#endif
