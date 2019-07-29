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
        if (arguments[0] == nullptr) {
          return SASS_MEMORY_NEW(
            SassString, pstate, "null");
        }
        return SASS_MEMORY_NEW(String_Quoted,
          pstate, arguments[0]->inspect(), '*');
      }

      BUILT_IN_FN(keywords)
      {
        SassArgumentList* argumentList = arguments[0]->assertArgumentList("args");
        NormalizedMap<ValueObj> keywords = argumentList->keywords();
        SassMapObj map = SASS_MEMORY_NEW(SassMap, arguments[0]->pstate());
        for (auto kv : keywords) {
          // Wrap string key into a sass value
          map->insert(SASS_MEMORY_NEW(String_Constant,
            kv.second->pstate(), kv.first), kv.second);
        }
        return map.detach();
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

      BUILT_IN_FN(globalVariableExists)
      {
        SassString* variable = arguments[0]->assertString("name");
        SassString* plugin = arguments[1]->assertStringOrNull("module");
        if (plugin != nullptr) {
          throw Exception::SassRuntimeException(
            "Modules are not supported yet", pstate);
        }
        return SASS_MEMORY_NEW(SassBoolean, pstate,
          closure.has_global(variable->value()));
      }

      BUILT_IN_FN(variableExists)
      {
        SassString* variable = arguments[0]->assertString("name");
        return SASS_MEMORY_NEW(SassBoolean, pstate,
          closure.has(variable->value()));
      }

      BUILT_IN_FN(functionExists)
      {
        SassString* variable = arguments[0]->assertString("name");
        SassString* plugin = arguments[1]->assertStringOrNull("module");
        if (plugin != nullptr) {
          throw Exception::SassRuntimeException(
            "Modules are not supported yet", pstate);
        }
        return SASS_MEMORY_NEW(SassBoolean, pstate,
          closure.has(variable->value() + "[f]"));
      }

      BUILT_IN_FN(mixinExists)
      {
        SassString* variable = arguments[0]->assertString("name");
        SassString* plugin = arguments[1]->assertStringOrNull("module");
        if (plugin != nullptr) {
          throw Exception::SassRuntimeException(
            "Modules are not supported yet", pstate);
        }
        return SASS_MEMORY_NEW(SassBoolean, pstate,
          closure.has(variable->value() + "[m]"));
      }

      BUILT_IN_FN(contentExists)
      {
        if (!closure.has_global("is_in_mixin")) {
          throw Exception::SassRuntimeException(
            "content-exists() may only be called within a mixin.",
            pstate);
        }
        return SASS_MEMORY_NEW(Boolean, pstate,
          closure.has_lexical("@content[m]"));
      }

      BUILT_IN_FN(moduleVariables)
      {
        // SassString* variable = arguments[0]->assertString("name");
        // SassString* plugin = arguments[1]->assertStringOrNull("module");
        throw Exception::SassRuntimeException(
          "Modules are not supported yet", pstate);
      }

      BUILT_IN_FN(moduleFunctions)
      {
        // SassString* variable = arguments[0]->assertString("name");
        // SassString* plugin = arguments[1]->assertStringOrNull("module");
        throw Exception::SassRuntimeException(
          "Modules are not supported yet", pstate);
      }

      BUILT_IN_FN(getFunction)
      {
        return SASS_MEMORY_NEW(SassString, pstate, "getFunction");
      }

      BUILT_IN_FN(call)
      {
        return SASS_MEMORY_NEW(SassString, pstate, "call");
      }


    }

  }

}
