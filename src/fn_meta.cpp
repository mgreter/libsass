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
        if (Cast<SassMap>(arguments[0])) return SASS_MEMORY_NEW(SassString, pstate, "map");
        if (Cast<SassList>(arguments[0])) return SASS_MEMORY_NEW(SassString, pstate, "list");
        if (Cast<SassNull>(arguments[0])) return SASS_MEMORY_NEW(SassString, pstate, "null");
        if (Cast<SassColor>(arguments[0])) return SASS_MEMORY_NEW(SassString, pstate, "color");
        if (Cast<SassString>(arguments[0])) return SASS_MEMORY_NEW(SassString, pstate, "string");
        if (Cast<SassNumber>(arguments[0])) return SASS_MEMORY_NEW(SassString, pstate, "number");
        if (Cast<SassBoolean>(arguments[0])) return SASS_MEMORY_NEW(SassString, pstate, "bool");
        if (Cast<SassArgumentList>(arguments[0])) return SASS_MEMORY_NEW(SassString, pstate, "arglist");
        throw Exception::SassRuntimeException("Invalid type for type-of.", pstate);
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
        SassString* feature = arguments[0]->assertString("feature");
        static const auto* const features =
          new std::unordered_set<std::string>{
          "global-variable-shadowing",
          "extend-selector-pseudoclass",
          "at-error",
          "units-level-3",
          "custom-property"
        };
        std::string name(feature->value());
        return SASS_MEMORY_NEW(Boolean,
          pstate, features->count(name) == 1);
      }

    }

  }

}
