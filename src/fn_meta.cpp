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
        if (Cast<SassMap>(arguments[0])) return SASS_MEMORY_NEW(SassString, pstate, "map");
        if (Cast<SassList>(arguments[0])) return SASS_MEMORY_NEW(SassString, pstate, "list");
        if (Cast<SassNull>(arguments[0])) return SASS_MEMORY_NEW(SassString, pstate, "null");
        if (Cast<SassColor>(arguments[0])) return SASS_MEMORY_NEW(SassString, pstate, "color");
        if (Cast<SassString>(arguments[0])) return SASS_MEMORY_NEW(SassString, pstate, "string");
        if (Cast<SassNumber>(arguments[0])) return SASS_MEMORY_NEW(SassString, pstate, "number");
        if (Cast<SassBoolean>(arguments[0])) return SASS_MEMORY_NEW(SassString, pstate, "bool");
        if (Cast<SassFunction>(arguments[0])) return SASS_MEMORY_NEW(SassString, pstate, "function");
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
          pstate, arguments[0]->to_sass(), '*');
      }

      BUILT_IN_FN(keywords)
      {
        SassArgumentList* argumentList = arguments[0]->assertArgumentList("args");
        NormalizedMap<ValueObj> keywords = argumentList->keywords();
        SassMapObj map = SASS_MEMORY_NEW(SassMap, arguments[0]->pstate());
        for (auto kv : keywords) {
          // Wrap string key into a sass value
          map->insert(SASS_MEMORY_NEW(String_Constant,
            kv.second->pstate(), kv.first.substr(1)), kv.second);
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
          closure.has_global("$" + variable->value()));
      }

      BUILT_IN_FN(variableExists)
      {
        SassString* variable = arguments[0]->assertString("name");
        return SASS_MEMORY_NEW(SassBoolean, pstate,
          closure.has("$" + variable->value()));
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

      /// Like `_environment.getFunction`, but also returns built-in
      /// globally-avaialble functions.
      Callable* _getFunction(std::string name, Env* env, Context& ctx, std::string ns = "") {

        std::string full_name(name + "[f]");
        if (!env->has(full_name)) {
          // look for dollar overload
        }

        if (env->has(full_name)) {
          Callable* callable = Cast<Callable>(env->get(full_name));
          // std::cerr << "Holla " << (void*)callable << "\n";
          return callable;
        }
        else if (ctx.builtins.count(name) == 1) {
          BuiltInCallable* cb = ctx.builtins[name];
          return cb;
        }

        // var local = _environment.getFunction(name, ns);
        // if (local != null || namespace != null) return local;
        // return _builtInFunctions[name];
        return nullptr;
      }

      BUILT_IN_FN(getFunction)
      {

        SassString* name = arguments[0]->assertString("name");
        bool css = arguments[1]->isTruthy(); // supports all values
        SassString* plugin = arguments[2]->assertStringOrNull("module");

        if (css && plugin != nullptr) {
          throw "$css and $module may not both be passed at once.";
        }

        CallableObj callable = css
          ? SASS_MEMORY_NEW(PlainCssCallable, pstate, name->value())
          : _getFunction(name->value(), &closure, ctx);

        if (callable == nullptr) throw "Function not found: " + name->value();

        return SASS_MEMORY_NEW(SassFunction, pstate, callable);

      }

      BUILT_IN_FN(call)
      {

        Value* function = arguments[0]->assertValue("function");
        SassArgumentList* args = arguments[1]->assertArgumentList("args");

        // std::vector<ExpressionObj> positional,
        //   NormalizedMap<ExpressionObj> named,
        //   Expression* restArgs = nullptr,
        //   Expression* kwdRest = nullptr);

        ValueExpression* restArg = SASS_MEMORY_NEW(
          ValueExpression, args->pstate(), args);

        ValueExpression* kwdRest = nullptr;
        if (args->keywords().empty()) {
          SassMap* map = args->keywordsAsSassMap();
        }

        ArgumentInvocation* invocation = SASS_MEMORY_NEW(
          ArgumentInvocation, pstate, {}, {}, restArg, kwdRest);

        if (SassString* str = Cast<SassString>(function)) {

          std::string name = unquote(str->value());
          std::cerr << "DEPRECATION WARNING: ";
          std::cerr << "Passing a string to call() is deprecated and will be illegal" << std::endl;
          std::cerr << "in Sass 4.0. Use call(get-function(" + quote(name) + ")) instead." << std::endl;
          std::cerr << std::endl;

          InterpolationObj itpl = SASS_MEMORY_NEW(Interpolation, pstate);
          itpl->append(SASS_MEMORY_NEW(StringLiteral, pstate, str->value()));
          FunctionExpression2* expression = SASS_MEMORY_NEW(
            FunctionExpression2, pstate, itpl, invocation);

          return expression->perform(&eval);

        }

        Callable* callable = function->assertFunction("function")->callable();
        return eval._runFunctionCallable(invocation, callable, pstate);


      }

    }

  }

}
