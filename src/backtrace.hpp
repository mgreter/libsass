#ifndef SASS_BACKTRACE_H
#define SASS_BACKTRACE_H

#include <vector>
#include <sstream>
#include "file.hpp"
#include "allocator.hpp"
#include "source_span.hpp"

namespace Sass {

  struct Backtrace {

    SourceSpan pstate;
    sass::string name;
    bool fn;

    Backtrace(SourceSpan pstate, sass::string name = "", bool fn = false) :
      pstate(std::move(pstate)), name(std::move(name)), fn(fn) {}

    Backtrace(SourceSpan&& pstate, sass::string&& name, bool fn = false) :
      pstate(std::move(pstate)), name(std::move(name)), fn(fn) {}

  };

  typedef sass::vector<Backtrace> Backtraces;

}

#endif
