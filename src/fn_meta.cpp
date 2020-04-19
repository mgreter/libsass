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
        return SASS_MEMORY_NEW(SassString, pstate, arguments[0]->type());
      }

      BUILT_IN_FN(inspect)
      {
        if (arguments[0] == nullptr) {
          return SASS_MEMORY_NEW(
            SassString, pstate, "null");
        }
        return SASS_MEMORY_NEW(SassString,
          pstate, arguments[0]->to_string({
            INSPECT, ctx.precision
          }));
      }

      BUILT_IN_FN(keywords)
      {
        ArgumentList* argumentList = arguments[0]->assertArgumentList(compiler.logger, Sass::Strings::args);
        const EnvKeyFlatMap<ValueObj>& keywords = argumentList->keywords();
        MapObj map = SASS_MEMORY_NEW(Map, arguments[0]->pstate());
        for (auto kv : keywords) {
          sass::string key = kv.first.norm().substr(1);
          // Util::ascii_normalize_underscore(key);
          // Wrap string key into a sass value
          map->insert(SASS_MEMORY_NEW(SassString,
            kv.second->pstate(), key), kv.second);
        }
        return map.detach();
      }

      BUILT_IN_FN(featureExists)
      {
        SassString* feature = arguments[0]->assertString(compiler.logger, pstate, "feature");
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
        SassString* variable = arguments[0]->assertString(compiler.logger, pstate, Sass::Strings::name);
        SassString* plugin = arguments[1]->assertStringOrNull(compiler.logger, pstate, Sass::Strings::module);
        if (plugin != nullptr) {
          throw Exception::SassRuntimeException2(
            "Modules are not supported yet",
            compiler.logger);
        }

        // return SASS_MEMORY_NEW(Boolean, pstate,
        //   closure.has_global("$" + variable->value()));

        return SASS_MEMORY_NEW(Boolean, pstate,
          ctx.getGlobalVariable("$" + variable->value()));

      }

      BUILT_IN_FN(variableExists)
      {
        SassString* variable = arguments[0]->assertString(compiler.logger, pstate, Sass::Strings::name);
        ExpressionObj ex = ctx.getLexicalVariable("$" + variable->value());
        return SASS_MEMORY_NEW(Boolean, pstate, !ex.isNull());
      }

      BUILT_IN_FN(functionExists)
      {
        SassString* variable = arguments[0]->assertString(compiler.logger, pstate, Sass::Strings::name);
        SassString* plugin = arguments[1]->assertStringOrNull(compiler.logger, pstate, Sass::Strings::module);
        if (plugin != nullptr) {
          throw Exception::SassRuntimeException2(
            "Modules are not supported yet",
            compiler.logger);
        }
        CallableObj fn = ctx.getLexicalFunction(variable->value());
        return SASS_MEMORY_NEW(Boolean, pstate, !fn.isNull());
      }

      BUILT_IN_FN(mixinExists)
      {
        SassString* variable = arguments[0]->assertString(compiler.logger, pstate, Sass::Strings::name);
        SassString* plugin = arguments[1]->assertStringOrNull(compiler.logger, pstate, Sass::Strings::module);
        if (plugin != nullptr) {
          throw Exception::SassRuntimeException2(
            "Modules are not supported yet",
            compiler.logger);
        }
        CallableObj fn = ctx.getLexicalMixin(variable->value());
        return SASS_MEMORY_NEW(Boolean, pstate, !fn.isNull());
      }

      BUILT_IN_FN(contentExists)
      {
        if (!eval.inMixin) {
          throw Exception::SassRuntimeException2(
            "content-exists() may only be called within a mixin.",
            compiler.logger);
        }
        return SASS_MEMORY_NEW(Boolean, pstate,
          eval.content88 != nullptr);
      }

      BUILT_IN_FN(moduleVariables)
      {
        // SassString* variable = arguments[0]->assertString(Sass::Strings::name);
        // SassString* plugin = arguments[1]->assertStringOrNull(Sass::Strings::module);
        throw Exception::SassRuntimeException2(
          "Modules are not supported yet",
          compiler.logger);
      }

      BUILT_IN_FN(moduleFunctions)
      {
        // SassString* variable = arguments[0]->assertString(Sass::Strings::name);
        // SassString* plugin = arguments[1]->assertStringOrNull(Sass::Strings::module);
        throw Exception::SassRuntimeException2(
          "Modules are not supported yet",
          compiler.logger);
      }

      /// Like `_environment.getFunction`, but also returns built-in
      /// globally-available functions.
      Callable* _getFunction(const EnvKey& name, Context& ctx, const sass::string& ns = "") {
        return ctx.getLexicalFunction(name);
      }

      BUILT_IN_FN(getFunction)
      {

        SassString* name = arguments[0]->assertString(compiler.logger, pstate, Sass::Strings::name);
        bool css = arguments[1]->isTruthy(); // supports all values
        SassString* plugin = arguments[2]->assertStringOrNull(compiler.logger, pstate, Sass::Strings::module);

        if (css && plugin != nullptr) {
          Exception::SassRuntimeException2(
            "$css and $module may not both be passed at once.",
            compiler.logger);
        }

        CallableObj callable = css
          ? SASS_MEMORY_NEW(PlainCssCallable, pstate, name->value())
          : _getFunction(name->value(), ctx);

        if (callable == nullptr) throw
          Exception::SassRuntimeException2(
            "Function not found: " + name->value(),
            compiler.logger);

        return SASS_MEMORY_NEW(SassFunction, pstate, callable);

      }

      BUILT_IN_FN(call)
      {

        Value* function = arguments[0]->assertValue(compiler.logger, "function");
        ArgumentList* args = arguments[1]->assertArgumentList(compiler.logger, Sass::Strings::args);

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
          ArgumentInvocation, pstate, sass::vector<ExpressionObj>{}, EnvKeyFlatMap2{ SourceSpan::tmp("null"), {} }, restArg, kwdRest);

        if (SassString * str = function->isString()) {
          sass::string name = unquote(str->value());
          compiler.logger.addWarn33(
            "Passing a string to call() is deprecated and will be illegal in LibSass "
            "4.1.0. Use call(get-function(" + str->to_css(true) + ")) instead.",
            str->pstate(), true);

          InterpolationObj itpl = SASS_MEMORY_NEW(Interpolation, pstate);
          itpl->append(SASS_MEMORY_NEW(SassString, pstate, str->value()));
          FunctionExpressionObj expression = SASS_MEMORY_NEW(
            FunctionExpression, pstate, itpl, invocation);

          return expression->perform(&eval);

        }

        Callable* callable = function->assertFunction(compiler.logger, "function")->callable();
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
