#ifndef SASS_FN_LISTS_H
#define SASS_FN_LISTS_H

#include "fn_utils.hpp"

namespace Sass {

  namespace Functions {

    namespace Lists {
      BUILT_IN_FN(length);
      BUILT_IN_FN(nth);
      BUILT_IN_FN(setNth);
      BUILT_IN_FN(join);
      BUILT_IN_FN(append);
      BUILT_IN_FN(zip);
      BUILT_IN_FN(index);
      BUILT_IN_FN(separator);
      BUILT_IN_FN(isBracketed);
    }

  }

}

#endif
