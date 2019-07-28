#ifndef SASS_FN_META_H
#define SASS_FN_META_H

#include "fn_utils.hpp"

namespace Sass {

  namespace Functions {

    namespace Meta {
      BUILT_IN_FN(typeOf);
      BUILT_IN_FN(inspect);
      BUILT_IN_FN(keywords);
      BUILT_IN_FN(featureExists);
    }

  }

}

#endif
