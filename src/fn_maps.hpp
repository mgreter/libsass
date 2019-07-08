#ifndef SASS_FN_MAPS_H
#define SASS_FN_MAPS_H

#include "fn_utils.hpp"

namespace Sass {

  namespace Functions {

    namespace Maps {
      BUILT_IN_FN(get);
      BUILT_IN_FN(merge);
      BUILT_IN_FN(remove_one);
      BUILT_IN_FN(remove_many);
      BUILT_IN_FN(keys);
      BUILT_IN_FN(values);
      BUILT_IN_FN(hasKey);
    }

  }

}

#endif
