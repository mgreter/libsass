#ifndef SASS_FN_SELECTORS_H
#define SASS_FN_SELECTORS_H

#include "fn_utils.hpp"

namespace Sass {

  namespace Functions {

    namespace Selectors {
      BUILT_IN_FN(nest);
      BUILT_IN_FN(append);
      BUILT_IN_FN(extend);
      BUILT_IN_FN(replace);
      BUILT_IN_FN(unify);
      BUILT_IN_FN(isSuper);
      BUILT_IN_FN(simple);
      BUILT_IN_FN(parse);
    }

  }

}

#endif
