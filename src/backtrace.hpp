#ifndef SASS_BACKTRACE_H
#define SASS_BACKTRACE_H

#include <vector>
#include <sstream>
#include "file.hpp"
#include "strings.hpp"
#include "memory.hpp"
#include "source_span.hpp"

// During runtime we need stack traces in order to produce meaningful
// error messages. Since the error catching might be done outside of
// the main compile function, certain values might already be garbage
// collected. Therefore we need to carry copies of those in any error.
// In order to optimize runtime, we don't want to create these copies
// during the evaluation stage, as most of the time we would throw them
// out right away. Therefore we only keep references during that phase
// (BackTrace), and copy them once an actual error is thrown (StackTrace).

namespace Sass {

  // Holding actual copies
  struct StackTrace {

    SourceSpan pstate;
    sass::string name;
    bool fn;

    StackTrace(
      SourceSpan pstate,
      sass::string name = Strings::empty,
      bool fn = false) :
      pstate(pstate),
      name(name),
      fn(fn)
    {}

  };

  // Holding only references
  struct BackTrace {

    const SourceSpan& pstate;
    const sass::string& name;
    bool fn;

    BackTrace(
      const SourceSpan& pstate,
      const sass::string& name = Strings::empty,
      bool fn = false) :
      pstate(pstate),
      name(name),
      fn(fn)
    {}

    // Create copies on convert
    operator StackTrace()
    {
      return StackTrace(
        pstate, name, fn);
    }

  };

  // Some related and often used aliases
  typedef sass::vector<BackTrace> BackTraces;
  typedef sass::vector<StackTrace> StackTraces;

}

#endif
