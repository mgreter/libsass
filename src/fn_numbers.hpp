#ifndef SASS_FN_NUMBERS_H
#define SASS_FN_NUMBERS_H

#include "fn_utils.hpp"

namespace Sass {

  namespace Functions {

    namespace Math {
      BUILT_IN_FN(round);
      BUILT_IN_FN(ceil);
      BUILT_IN_FN(floor);
      BUILT_IN_FN(abs);
      BUILT_IN_FN(max);
      BUILT_IN_FN(min);
      BUILT_IN_FN(random);
      BUILT_IN_FN(unit);
      BUILT_IN_FN(isUnitless);
      BUILT_IN_FN(percentage);
      BUILT_IN_FN(compatible);   
    }

  }

}

#endif
