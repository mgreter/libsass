// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include "ast.hpp"
#include "eval.hpp"
#include "fn_meta.hpp"
#include "fn_utils.hpp"
#include "debugger.hpp"

namespace Sass {

  namespace Functions {

    namespace Meta {

      BUILT_IN_FN(typeOf)
      {
        return SASS_MEMORY_NEW(String, pstate, arguments[0]->type());
      }

      BUILT_IN_FN(inspect)
      {
        if (arguments[0] == nullptr) {
          return SASS_MEMORY_NEW(
            String, pstate, "null");
        }
        return SASS_MEMORY_NEW(String,
          pstate, arguments[0]->inspect({
            ctx.precision
          }));
      }

      BUILT_IN_FN(keywords)
      {
        ArgumentList* argumentList = arguments[0]->assertArgumentList(*ctx.logger123, Sass::Strings::args);
        const EnvKeyFlatMap<ValueObj>& keywords = argumentList->keywords();
        MapObj map = SASS_MEMORY_NEW(Map, arguments[0]->pstate());
        for (auto kv : keywords) {
          sass::string key = kv.first.norm().substr(1);
          // Util::ascii_normalize_underscore(key);
          // Wrap string key into a sass value
          map->insert(SASS_MEMORY_NEW(String,
            kv.second->pstate(), key), kv.second);
        }
        return map.detach();
      }

      BUILT_IN_FN(featureExists)
      {
        String* feature = arguments[0]->assertString(*ctx.logger123, pstate, "feature");
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

      BUILT_IN_FN(globalVariableExists)
      {
        String* variable = arguments[0]->assertString(*ctx.logger123, pstate, Sass::Strings::name);
        String* plugin = arguments[1]->assertStringOrNull(*ctx.logger123, pstate, Sass::Strings::module);
        if (plugin != nullptr) {
          throw Exception::SassRuntimeException2(
            "Modules are not supported yet",
            *ctx.logger123);
        }

        // return SASS_MEMORY_NEW(Boolean, pstate,
        //   closure.has_global("$" + variable->value()));

        return SASS_MEMORY_NEW(Boolean, pstate,
          ctx.getGlobalVariable("$" + variable->value()));

      }

      BUILT_IN_FN(variableExists)
      {
        String* variable = arguments[0]->assertString(*ctx.logger123, pstate, Sass::Strings::name);
        ExpressionObj ex = ctx.getLexicalVariable("$" + variable->value());
        return SASS_MEMORY_NEW(Boolean, pstate, !ex.isNull());
      }

      BUILT_IN_FN(functionExists)
      {
        String* variable = arguments[0]->assertString(*ctx.logger123, pstate, Sass::Strings::name);
        String* plugin = arguments[1]->assertStringOrNull(*ctx.logger123, pstate, Sass::Strings::module);
        if (plugin != nullptr) {
          throw Exception::SassRuntimeException2(
            "Modules are not supported yet",
            *ctx.logger123);
        }
        CallableObj fn = ctx.getLexicalFunction(variable->value());
        return SASS_MEMORY_NEW(Boolean, pstate, !fn.isNull());
      }

      BUILT_IN_FN(mixinExists)
      {
        String* variable = arguments[0]->assertString(*ctx.logger123, pstate, Sass::Strings::name);
        String* plugin = arguments[1]->assertStringOrNull(*ctx.logger123, pstate, Sass::Strings::module);
        if (plugin != nullptr) {
          throw Exception::SassRuntimeException2(
            "Modules are not supported yet",
            *ctx.logger123);
        }
        CallableObj fn = ctx.getLexicalMixin(variable->value());
        return SASS_MEMORY_NEW(Boolean, pstate, !fn.isNull());
      }

      BUILT_IN_FN(contentExists)
      {
        if (!eval.inMixin) {
          throw Exception::SassRuntimeException2(
            "content-exists() may only be called within a mixin.",
            *ctx.logger123);
        }
        return SASS_MEMORY_NEW(Boolean, pstate,
          eval.content88 != nullptr);
      }

      BUILT_IN_FN(moduleVariables)
      {
        // String* variable = arguments[0]->assertString(Sass::Strings::name);
        // String* plugin = arguments[1]->assertStringOrNull(Sass::Strings::module);
        throw Exception::SassRuntimeException2(
          "Modules are not supported yet",
          *ctx.logger123);
      }

      BUILT_IN_FN(moduleFunctions)
      {
        // String* variable = arguments[0]->assertString(Sass::Strings::name);
        // String* plugin = arguments[1]->assertStringOrNull(Sass::Strings::module);
        throw Exception::SassRuntimeException2(
          "Modules are not supported yet",
          *ctx.logger123);
      }

      /// Like `_environment.getFunction`, but also returns built-in
      /// globally-available functions.
      Callable* _getFunction(const EnvKey& name, Context& ctx, const sass::string& ns = "") {
        return ctx.getLexicalFunction(name);
      }

      BUILT_IN_FN(getFunction)
      {

        String* name = arguments[0]->assertString(*ctx.logger123, pstate, Sass::Strings::name);
        bool css = arguments[1]->isTruthy(); // supports all values
        String* plugin = arguments[2]->assertStringOrNull(*ctx.logger123, pstate, Sass::Strings::module);

        if (css && plugin != nullptr) {
          throw Exception::SassRuntimeException2(
            "$css and $module may not both be passed at once.",
            *ctx.logger123);
        }

        CallableObj callable = css
          ? SASS_MEMORY_NEW(PlainCssCallable, pstate, name->value())
          : _getFunction(name->value(), ctx);

        if (callable == nullptr) throw
          Exception::SassRuntimeException2(
            "Function not found: " + name->value(),
            *ctx.logger123);

        return SASS_MEMORY_NEW(Function, pstate, callable);

      }

      BUILT_IN_FN(call)
      {

        Value* function = arguments[0]->assertValue(*ctx.logger123, "function");
        ArgumentList* args = arguments[1]->assertArgumentList(*ctx.logger123, Sass::Strings::args);

        // sass::vector<ExpressionObj> positional,
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
          ArgumentInvocation, pstate, sass::vector<ExpressionObj>{}, {}, restArg, kwdRest);

        if (String * str = function->isString()) {
          sass::string name = unquote(str->value());
          ctx.logger123->addDeprecation(
            "Passing a string to call() is deprecated and will be illegal in LibSass "
            "4.1.0. Use call(get-function(" + str->toValString(logger642) + ")) instead.",
            str->pstate());

          InterpolationObj itpl = SASS_MEMORY_NEW(Interpolation, pstate);
          itpl->append(SASS_MEMORY_NEW(String, pstate, str->value()));
          FunctionExpressionObj expression = SASS_MEMORY_NEW(
            FunctionExpression, pstate, itpl, invocation);

          return expression->perform(&eval);

        }

        Callable* callable = function->assertFunction(*ctx.logger123, "function")->callable();
        return eval._runFunctionCallable(invocation, callable, pstate, false);


      }

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
