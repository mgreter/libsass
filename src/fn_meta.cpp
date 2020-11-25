/*****************************************************************************/
/* Part of LibSass, released under the MIT license (See LICENSE.txt).        */
/*****************************************************************************/
#include "fn_meta.hpp"

#include "eval.hpp"
#include "compiler.hpp"
#include "exceptions.hpp"
#include "ast_values.hpp"
#include "ast_callables.hpp"
#include "ast_expressions.hpp"
#include "string_utils.hpp"

#include "debugger.hpp"

namespace Sass {

  namespace Functions {

    /////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////

    namespace Meta {

      /*******************************************************************/

      BUILT_IN_FN(typeOf)
      {
        sass::string copy(arguments[0]->type());
        return SASS_MEMORY_NEW(String,
          pstate, std::move(copy));
      }

      /*******************************************************************/

      BUILT_IN_FN(inspect)
      {
        if (arguments[0] == nullptr) {
          return SASS_MEMORY_NEW(
            String, pstate, "null");
        }
        return SASS_MEMORY_NEW(String,
          pstate, arguments[0]->inspect());
      }

      /*******************************************************************/

      BUILT_IN_FN(fnIf)
      {
        // Always evaluates both sides!
        return arguments[0]->isTruthy() ?
          arguments[1] : arguments[0];
      }

      BUILT_IN_FN(keywords)
      {
        ArgumentList* argumentList = arguments[0]->assertArgumentList(compiler, Sass::Strings::args);
        const ValueFlatMap& keywords = argumentList->keywords();
        MapObj map = SASS_MEMORY_NEW(Map, arguments[0]->pstate());
        for (auto kv : keywords) {
          sass::string key = kv.first.norm(); // .substr(1);
          // Util::ascii_normalize_underscore(key);
          // Wrap string key into a sass value
          map->insert(SASS_MEMORY_NEW(String,
            kv.second->pstate(), std::move(key)
          ), kv.second);
        }
        return map.detach();
      }

      /*******************************************************************/

      BUILT_IN_FN(featureExists)
      {
        String* feature = arguments[0]->assertString(compiler, "feature");
        static const auto* const features =
          new std::unordered_set<sass::string>{
          "global-variable-shadowing",
          "extend-selector-pseudoclass",
          "at-error",
          "units-level-3",
          "custom-property"
        };
        sass::string name(feature->value());
        return SASS_MEMORY_NEW(Boolean,
          pstate, features->count(name) == 1);
      }

      /*******************************************************************/

      BUILT_IN_FN(globalVariableExists)
      {
        String* variable = arguments[0]->assertString(compiler, Sass::Strings::name);
        String* plugin = arguments[1]->assertStringOrNull(compiler, Sass::Strings::module);
        auto parent = compiler.getCurrentModule();
        if (plugin != nullptr) {
          auto pp = parent->module->moduse.find(plugin->value());
          if (pp != parent->module->moduse.end()) {
            VarRefs* module = pp->second.first;
            auto it = module->varIdxs.find(variable->value());
            return SASS_MEMORY_NEW(Boolean, pstate,
              it != module->varIdxs.end());
          }
          else {
            throw Exception::RuntimeException(compiler,
              "There is no module with the namespace \"" + plugin->value() + "\".");
          }
          return SASS_MEMORY_NEW(Boolean, pstate, false);
        }
        bool hasVar = false;
        for (auto global : parent->fwdGlobal55) {
          if (global->varIdxs.count(variable->value()) != 0) {
            if (hasVar) {
              throw Exception::RuntimeException(compiler,
                "This variable is available from multiple global modules.");
            }
            hasVar = true;
          }
        }
        if (hasVar) return SASS_MEMORY_NEW(Boolean, pstate, true);
        return SASS_MEMORY_NEW(Boolean, pstate,
          compiler.findVariable(variable->value(), true));

      }

      /*******************************************************************/

      BUILT_IN_FN(variableExists)
      {
        String* variable = arguments[0]->assertString(compiler, Sass::Strings::name);
        ValueObj ex = compiler.findVariable(variable->value());

        bool hasVar = false;
        auto parent = compiler.getCurrentModule();
        for (auto global : parent->fwdGlobal55) {
          if (global->varIdxs.count(variable->value()) != 0) {
            if (hasVar) {
              throw Exception::RuntimeException(compiler,
                "This variable is available from multiple global modules.");
            }
            hasVar = true;
          }
        }
        if (hasVar) return SASS_MEMORY_NEW(Boolean, pstate, true);
        return SASS_MEMORY_NEW(Boolean, pstate, !ex.isNull());
      }

      BUILT_IN_FN(functionExists)
      {
        String* variable = arguments[0]->assertString(compiler, Sass::Strings::name);
        String* plugin = arguments[1]->assertStringOrNull(compiler, Sass::Strings::module);
        auto parent = compiler.getCurrentModule();
        if (plugin != nullptr) {
          auto pp = parent->module->moduse.find(plugin->value());
          if (pp != parent->module->moduse.end()) {
            VarRefs* module = pp->second.first;
            auto it = module->fnIdxs.find(variable->value());
            return SASS_MEMORY_NEW(Boolean, pstate,
              it != module->fnIdxs.end());
          }
          else {
            throw Exception::RuntimeException(compiler,
              "There is no module with the namespace \"" + plugin->value() + "\".");
          }
          return SASS_MEMORY_NEW(Boolean, pstate, false);
        }
        bool hasFn = false;
        for (auto global : parent->fwdGlobal55) {
          if (global->fnIdxs.count(variable->value()) != 0) {
            if (hasFn) {
              throw Exception::RuntimeException(compiler,
                "This function is available from multiple global modules.");
            }
            hasFn = true;
          }
        }
        if (hasFn) return SASS_MEMORY_NEW(Boolean, pstate, true);
        CallableObj* fn = compiler.findFunction(variable->value());
        return SASS_MEMORY_NEW(Boolean, pstate, fn && *fn);
      }

      BUILT_IN_FN(mixinExists)
      {
        String* variable = arguments[0]->assertString(compiler, Sass::Strings::name);
        String* plugin = arguments[1]->assertStringOrNull(compiler, Sass::Strings::module);

        auto parent = compiler.getCurrentModule();
        if (plugin != nullptr) {
          auto pp = parent->module->moduse.find(plugin->value());
          if (pp != parent->module->moduse.end()) {
            VarRefs* module = pp->second.first;
            auto it = module->mixIdxs.find(variable->value());
            return SASS_MEMORY_NEW(Boolean, pstate,
              it != module->mixIdxs.end());
          }
          else {
            throw Exception::RuntimeException(compiler,
              "There is no module with the namespace \"" + plugin->value() + "\".");
          }
          return SASS_MEMORY_NEW(Boolean, pstate, false);
        }
        bool hasFn = false;
        for (auto global : parent->fwdGlobal55) {
          if (global->mixIdxs.count(variable->value()) != 0) {
            if (hasFn) {
              throw Exception::RuntimeException(compiler,
                "This function is available from multiple global modules.");
            }
            hasFn = true;
          }
        }
        if (hasFn) return SASS_MEMORY_NEW(Boolean, pstate, true);

        CallableObj fn = compiler.findMixin(variable->value());
        return SASS_MEMORY_NEW(Boolean, pstate, !fn.isNull());
      }

      BUILT_IN_FN(contentExists)
      {
        if (!eval.isInMixin()) {
          throw Exception::RuntimeException(compiler,
            "content-exists() may only be called within a mixin.");
        }
        return SASS_MEMORY_NEW(Boolean, pstate,
          eval.hasContentBlock());
      }

      BUILT_IN_FN(moduleVariables)
      {
        String* ns = arguments[0]->assertStringOrNull(compiler, Sass::Strings::module);
        MapObj list = SASS_MEMORY_NEW(Map, pstate);
        auto module = compiler.getCurrentModule();
        auto it = module->module->moduse.find(ns->value());
        if (it != module->module->moduse.end()) {
          VarRefs* refs = it->second.first;
          Module* root = it->second.second;
          if (root && !root->isCompiled) {
            throw Exception::RuntimeException(compiler, "There is "
              "no module with namespace \"" + ns->value() + "\".");
          }
          for (auto entry : refs->varIdxs) {
            auto name = SASS_MEMORY_NEW(String, pstate,
              sass::string(entry.first.norm()), true);
            VarRef vidx(refs->varFrame, entry.second);
            list->insert({ name, compiler.
              varRoot.getVariable(vidx) });
          }
          if (root)
          for (auto entry : root->mergedFwdVar) {
            auto name = SASS_MEMORY_NEW(String, pstate,
              sass::string(entry.first.norm()), true);
            VarRef vidx(0xFFFFFFFF, entry.second);
            list->insert({ name, compiler.
              varRoot.getVariable(vidx) });
          }
        }
        else {
          throw Exception::RuntimeException(compiler, "There is "
            "no module with namespace \"" + ns->value() + "\".");
        }
        return list.detach();
      }

      BUILT_IN_FN(moduleFunctions)
      {
        String* ns = arguments[0]->assertStringOrNull(compiler, Sass::Strings::module);
        MapObj list = SASS_MEMORY_NEW(Map, pstate);
        auto module = compiler.getCurrentModule();
        auto it = module->module->moduse.find(ns->value());
        if (it != module->module->moduse.end()) {
          VarRefs* refs = it->second.first;
          Module* root = it->second.second;
          if (root && !root->isCompiled) {
            throw Exception::RuntimeException(compiler, "There is "
              "no module with namespace \"" + ns->value() + "\".");
          }
          for (auto entry : refs->fnIdxs) {
            auto name = SASS_MEMORY_NEW(String, pstate,
              sass::string(entry.first.norm()), true);
            VarRef fidx(refs->fnFrame, entry.second);
            auto callable = compiler.varRoot.getFunction(fidx);
            auto fn = SASS_MEMORY_NEW(Function, pstate, callable);
            list->insert({ name, fn });
          }
          if (root)
          for (auto entry : root->mergedFwdFn) {
            auto name = SASS_MEMORY_NEW(String, pstate,
              sass::string(entry.first.norm()), true);
            VarRef fidx(0xFFFFFFFF, entry.second);
            auto callable = compiler.varRoot.getFunction(fidx);
            auto fn = SASS_MEMORY_NEW(Function, pstate, callable);
            list->insert({ name, fn });
          }
        }
        else {
          throw Exception::RuntimeException(compiler, "There is "
            "no module with namespace \"" + ns->value() + "\".");
        }
        return list.detach();
      }

      /// Like `_environment.findFunction`, but also returns built-in
      /// globally-available functions.
      Callable* _getFunction(const EnvKey& name, Compiler& ctx, const sass::string& ns = "") {
        auto asd = ctx.findFunction(name);
        return asd ? *asd : nullptr; // no detach, is a reference anyway
      }

      BUILT_IN_FN(findFunction)
      {

        String* name = arguments[0]->assertString(compiler, Sass::Strings::name);
        bool css = arguments[1]->isTruthy(); // supports all values
        String* ns = arguments[2]->assertStringOrNull(compiler, Sass::Strings::module);

        if (css && ns != nullptr) {
          throw Exception::RuntimeException(compiler,
            "$css and $module may not both be passed at once.");
        }

        if (css) {
          return SASS_MEMORY_NEW(Function, pstate,
            SASS_MEMORY_NEW(PlainCssCallable, pstate,
              name->value()));
        }

        // CallableObj callable = css
        //   ? SASS_MEMORY_NEW(PlainCssCallable, pstate, name->value())
        //   : _getFunction(name->value(), compiler);


        CallableObj callable;

        auto parent = compiler.getCurrentModule();

        if (ns != nullptr) {
          auto pp = parent->module->moduse.find(ns->value());
          if (pp != parent->module->moduse.end()) {
            VarRefs* module = pp->second.first;
            auto it = module->fnIdxs.find(name->value());
            if (it != module->fnIdxs.end()) {
              VarRef fidx({ module->fnFrame, it->second });
              callable = compiler.varRoot.getFunction(fidx);
            }
          }
          else {
            throw Exception::RuntimeException(compiler,
              "There is no module with the namespace \"" + ns->value() + "\".");
          }
        }
        else {

          callable = _getFunction(name->value(), compiler);

          if (!callable) {

            for (auto global : parent->fwdGlobal55) {
              auto it = global->fnIdxs.find(name->value());
              if (it != global->fnIdxs.end()) {
                if (callable) {
                  throw Exception::RuntimeException(compiler,
                    "This function is available from multiple global modules.");
                }
                VarRef fidx({ global->fnFrame, it->second });
                callable = compiler.varRoot.getFunction(fidx);
                if (callable) break;
              }
            }
          }
        }


        if (callable == nullptr) {
          if (name->hasQuotes()) {
            throw
              Exception::RuntimeException(compiler,
                "Function not found: \"" + name->value() + "\"");
          }
          else {
            throw
              Exception::RuntimeException(compiler,
                "Function not found: " + name->value() + "");
          }
        }

        return SASS_MEMORY_NEW(Function, pstate, callable);

      }

      BUILT_IN_FN(call)
      {

        Value* function = arguments[0]->assertValue(compiler, "function");
        ArgumentList* args = arguments[1]->assertArgumentList(compiler, Sass::Strings::args);

        // ExpressionVector positional,
        //   EnvKeyMap<ExpressionObj> named,
        //   Expression* restArgs = nullptr,
        //   Expression* kwdRest = nullptr);

        ValueExpression* restArg = SASS_MEMORY_NEW(
          ValueExpression, args->pstate(), args);

        ValueExpression* kwdRest = nullptr;
        if (!args->keywords().empty()) {
          Map* map = args->keywordsAsSassMap();
          kwdRest = SASS_MEMORY_NEW(
            ValueExpression, map->pstate(), map);
        }

        ArgumentInvocationObj invocation = SASS_MEMORY_NEW(
          ArgumentInvocation, pstate, ExpressionVector{}, {}, restArg, kwdRest);

        if (String * str = function->isaString()) {
          sass::string name = str->value();
          compiler.addDeprecation(
            "Passing a string to call() is deprecated and will be illegal in LibSass "
            "4.1.0. Use call(get-function(" + str->inspect() + ")) instead.",
            str->pstate());

          InterpolationObj itpl = SASS_MEMORY_NEW(Interpolation, pstate);
          itpl->append(SASS_MEMORY_NEW(String, pstate, sass::string(str->value())));
          FunctionExpressionObj expression = SASS_MEMORY_NEW(
            FunctionExpression, pstate, itpl, invocation,
            true); // Maybe pass flag into here!?
          return eval.acceptFunctionExpression(expression);
          // return expression->accept(&eval);

        }

        Callable* callable = function->assertFunction(compiler, "function")->callable();
        return callable->execute(eval, invocation, pstate);


      }



      void registerFunctions(Compiler& compiler)
	    {

        BuiltInMod& module(compiler.createModule("meta"));

        // ToDo: dart-sass keeps them on the local environment scope, see below:
        // These functions are defined in the context of the evaluator because
        // they need access to the [_environment] or other local state.

        compiler.registerBuiltInFunction(key_if, "$condition, $if-true, $if-false", fnIf);

        module.addMixin(key_load_css, compiler.createBuiltInMixin(key_load_css, "$url, $with: null", loadCss));

        module.addFunction(key_feature_exists, compiler.registerBuiltInFunction(key_feature_exists, "$feature", featureExists));
        module.addFunction(key_type_of, compiler.registerBuiltInFunction(key_type_of, "$value", typeOf));
        module.addFunction(key_inspect, compiler.registerBuiltInFunction(key_inspect, "$value", inspect));
        module.addFunction(key_keywords, compiler.registerBuiltInFunction(key_keywords, "$args", keywords));
        module.addFunction(key_global_variable_exists, compiler.registerBuiltInFunction(key_global_variable_exists, "$name, $module: null", globalVariableExists));
        module.addFunction(key_variable_exists, compiler.registerBuiltInFunction(key_variable_exists, "$name", variableExists));
        module.addFunction(key_function_exists, compiler.registerBuiltInFunction(key_function_exists, "$name, $module: null", functionExists));
        module.addFunction(key_mixin_exists, compiler.registerBuiltInFunction(key_mixin_exists, "$name, $module: null", mixinExists));
        module.addFunction(key_content_exists, compiler.registerBuiltInFunction(key_content_exists, "", contentExists));
        module.addFunction(key_module_variables, compiler.createBuiltInFunction(key_module_variables, "$module", moduleVariables));
        module.addFunction(key_module_functions, compiler.registerBuiltInFunction(key_module_functions, "$module", moduleFunctions));
        module.addFunction(key_get_function, compiler.registerBuiltInFunction(key_get_function, "$name, $css: false, $module: null", findFunction));
        module.addFunction(key_call, compiler.registerBuiltInFunction(key_call, "$function, $args...", call));

	    }

    }

  }

}
