#ifndef SASS_FN_STRINGS_H
#define SASS_FN_STRINGS_H

#include "fn_utils.hpp"

namespace Sass {

  namespace Functions {

    namespace Strings {
      BUILT_IN_FN(unquote);
      BUILT_IN_FN(quote);
      BUILT_IN_FN(toUpperCase);
      BUILT_IN_FN(toLowerCase);
      BUILT_IN_FN(length);
      BUILT_IN_FN(insert);
      BUILT_IN_FN(index);
      BUILT_IN_FN(slice);
      BUILT_IN_FN(uniqueId);
    }

  }

}

#endif
