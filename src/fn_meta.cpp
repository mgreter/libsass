// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include "ast.hpp"
#include "fn_meta.hpp"
#include "fn_utils.hpp"

namespace Sass {

  namespace Functions {

    namespace Meta {

      BUILT_IN_FN(typeOf)
      {
        return SASS_MEMORY_NEW(SassString,
          pstate, "typeOf");
      }

      BUILT_IN_FN(inspect)
      {
        return SASS_MEMORY_NEW(SassString,
          pstate, arguments[0]->inspect());
      }

      BUILT_IN_FN(keywords)
      {
        return SASS_MEMORY_NEW(SassString,
          pstate, "keywords");
      }

      BUILT_IN_FN(featureExists)
      {
        return SASS_MEMORY_NEW(SassString,
          pstate, "featureExists");
      }

    }

  }

}
