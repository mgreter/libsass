#ifndef SASS_LISTIZE_H
#define SASS_LISTIZE_H

// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include "ast_fwd_decl.hpp"
#include "operation.hpp"

namespace Sass {

  struct Backtrace;

  class Listize {

  public:

    static Value* listize(SelectorList*);
    static Value* listize(ComplexSelector*);

  };

}

#endif
