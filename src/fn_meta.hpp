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

      // ToDo: dart-sass keeps them on the local environment scope, see below:
      // These functions are defined in the context of the evaluator because
      // they need access to the [_environment] or other local state.
      BUILT_IN_FN(globalVariableExists);
      BUILT_IN_FN(variableExists);
      BUILT_IN_FN(functionExists);
      BUILT_IN_FN(mixinExists);
      BUILT_IN_FN(contentExists);
      // BUILT_IN_FN(moduleVariables);
      // BUILT_IN_FN(moduleFunctions);
      BUILT_IN_FN(getFunction);
      BUILT_IN_FN(call);

      void registerFunctions(Context& ctx)
      {

        // Meta functions
        ctx.registerBuiltInFunction("feature-exists", "$feature", featureExists);
        ctx.registerBuiltInFunction("type-of", "$value", typeOf);
        ctx.registerBuiltInFunction("inspect", "$value", inspect);
        ctx.registerBuiltInFunction("keywords", "$args", keywords);

        // ToDo: dart-sass keeps them on the local environment scope, see below:
        // These functions are defined in the context of the evaluator because
        // they need access to the [_environment] or other local state.
        ctx.registerBuiltInFunction("global-variable-exists", "$name, $module: null", globalVariableExists);
        ctx.registerBuiltInFunction("variable-exists", "$name", variableExists);
        ctx.registerBuiltInFunction("function-exists", "$name, $module: null", functionExists);
        ctx.registerBuiltInFunction("mixin-exists", "$name, $module: null", mixinExists);
        ctx.registerBuiltInFunction("content-exists", "", contentExists);
        // ctx.registerBuiltInFunction("module-variables", "$module", moduleVariables);
        // ctx.registerBuiltInFunction("module-functions", "$module", moduleFunctions);
        ctx.registerBuiltInFunction("get-function", "$name, $css: false, $module: null", getFunction);
        ctx.registerBuiltInFunction("call", "$function, $args...", call);

      }

    }

  }

}

#endif
