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
        return SASS_MEMORY_NEW(String_Constant, pstate, arguments[0]->type());
        // Sorted by probability of occurence (arglist must come before list)
        // if (Cast<String_Constant>(arguments[0])) return SASS_MEMORY_NEW(String_Constant, pstate, "string");
        // if (Cast<SassColor>(arguments[0])) return SASS_MEMORY_NEW(String_Constant, pstate, "color");
        // if (Cast<SassArgumentList>(arguments[0])) return SASS_MEMORY_NEW(String_Constant, pstate, "arglist");
        // if (Cast<SassList>(arguments[0])) return SASS_MEMORY_NEW(String_Constant, pstate, "list");
        // if (Cast<SassNumber>(arguments[0])) return SASS_MEMORY_NEW(String_Constant, pstate, "number");
        // if (Cast<SassBoolean>(arguments[0])) return SASS_MEMORY_NEW(String_Constant, pstate, "bool");
        // if (Cast<SassMap>(arguments[0])) return SASS_MEMORY_NEW(String_Constant, pstate, "map");
        // if (Cast<SassFunction>(arguments[0])) return SASS_MEMORY_NEW(String_Constant, pstate, "function");
        // if (Cast<SassNull>(arguments[0])) return SASS_MEMORY_NEW(String_Constant, pstate, "null");
        // if (Cast<Color_HSLA>(arguments[0])) return SASS_MEMORY_NEW(String_Constant, pstate, "color");
        // throw Exception::SassRuntimeException2("Invalid type for type-of.", *ctx.logger, pstate);
      }

      BUILT_IN_FN(inspect)
      {
        if (arguments[0] == nullptr) {
          return SASS_MEMORY_NEW(
            String_Constant, pstate, "null");
        }
        return SASS_MEMORY_NEW(String_Constant,
          pstate, arguments[0]->to_string({
            TO_SASS, ctx.c_options.precision
          }));
      }

      BUILT_IN_FN(keywords)
      {
        SassArgumentList* argumentList = arguments[0]->assertArgumentList(*ctx.logger, "args");
        const KeywordMap<ValueObj>& keywords = argumentList->keywords();
        SassMapObj map = SASS_MEMORY_NEW(SassMap, arguments[0]->pstate());
        for (auto kv : keywords) {
          sass::string key = kv.first.norm().substr(1);
          // Util::ascii_normalize_underscore(key);
          // Wrap string key into a sass value
          map->insert(SASS_MEMORY_NEW(String_Constant,
            kv.second->pstate(), key), kv.second);
        }
        return map.detach();
      }

      BUILT_IN_FN(featureExists)
      {
        String_Constant* feature = arguments[0]->assertString(*ctx.logger, pstate, "feature");
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
        String_Constant* variable = arguments[0]->assertString(*ctx.logger, pstate, "name");
        String_Constant* plugin = arguments[1]->assertStringOrNull(*ctx.logger, pstate, "module");
        if (plugin != nullptr) {
          throw Exception::SassRuntimeException2(
            "Modules are not supported yet",
            *ctx.logger, pstate);
        }

        // return SASS_MEMORY_NEW(SassBoolean, pstate,
        //   closure.has_global("$" + variable->value()));

        return SASS_MEMORY_NEW(SassBoolean, pstate,
          ctx.varRoot.hasGlobalVariable33("$" + variable->value()));

      }

      BUILT_IN_FN(variableExists)
      {
        String_Constant* variable = arguments[0]->assertString(*ctx.logger, pstate, "name");
        return SASS_MEMORY_NEW(SassBoolean, pstate,
        //  closure.has("$" + variable->value()));
        ctx.varRoot.hasLexicalVariable33("$" + variable->value()));
      }

      BUILT_IN_FN(functionExists)
      {
        String_Constant* variable = arguments[0]->assertString(*ctx.logger, pstate, "name");
        String_Constant* plugin = arguments[1]->assertStringOrNull(*ctx.logger, pstate, "module");
        if (plugin != nullptr) {
          throw Exception::SassRuntimeException2(
            "Modules are not supported yet",
            *ctx.logger, pstate);
        }
        return SASS_MEMORY_NEW(SassBoolean, pstate,
          ctx.functions.count(variable->value()) == 1
          || ctx.varRoot.hasLexicalFunction33(variable->value()));
      }

      BUILT_IN_FN(mixinExists)
      {
        String_Constant* variable = arguments[0]->assertString(*ctx.logger, pstate, "name");
        String_Constant* plugin = arguments[1]->assertStringOrNull(*ctx.logger, pstate, "module");
        if (plugin != nullptr) {
          throw Exception::SassRuntimeException2(
            "Modules are not supported yet",
            *ctx.logger, pstate);
        }
        return SASS_MEMORY_NEW(SassBoolean, pstate,
          ctx.varRoot.hasLexicalMixin33(variable->value()));
      }

      BUILT_IN_FN(contentExists)
      {
        if (!eval.inMixin) {
          throw Exception::SassRuntimeException2(
            "content-exists() may only be called within a mixin.",
            *ctx.logger, pstate);
        }
        return SASS_MEMORY_NEW(Boolean, pstate,
          ctx.varRoot.contentExists);
      }

      BUILT_IN_FN(moduleVariables)
      {
        // String_Constant* variable = arguments[0]->assertString("name");
        // String_Constant* plugin = arguments[1]->assertStringOrNull("module");
        throw Exception::SassRuntimeException2(
          "Modules are not supported yet",
          *ctx.logger, pstate);
      }

      BUILT_IN_FN(moduleFunctions)
      {
        // String_Constant* variable = arguments[0]->assertString("name");
        // String_Constant* plugin = arguments[1]->assertStringOrNull("module");
        throw Exception::SassRuntimeException2(
          "Modules are not supported yet",
          *ctx.logger, pstate);
      }

      /// Like `_environment.getFunction`, but also returns built-in
      /// globally-avaialble functions.
      Callable* _getFunction(const EnvString& name, Context& ctx, const sass::string& ns = "") {
        IdxRef fidx = ctx.varRoot.getLexicalFunction33(name);
        if (fidx.isValid()) return ctx.varRoot.getFunction(fidx);
        return nullptr;
      }

      BUILT_IN_FN(getFunction)
      {

        String_Constant* name = arguments[0]->assertString(*ctx.logger, pstate, "name");
        bool css = arguments[1]->isTruthy(); // supports all values
        String_Constant* plugin = arguments[2]->assertStringOrNull(*ctx.logger, pstate, "module");

        if (css && plugin != nullptr) {
          Exception::SassRuntimeException2(
            "$css and $module may not both be passed at once.",
            *ctx.logger, pstate);
        }

        CallableObj callable = css
          ? SASS_MEMORY_NEW(PlainCssCallable, pstate, name->value())
          : _getFunction(name->value(), ctx);

        if (callable == nullptr) throw
          Exception::SassRuntimeException2(
            "Function not found: " + name->value(),
            *ctx.logger, pstate);

        return SASS_MEMORY_NEW(SassFunction, pstate, callable);

      }

      BUILT_IN_FN(call)
      {

        Value* function = arguments[0]->assertValue(*ctx.logger, "function");
        SassArgumentList* args = arguments[1]->assertArgumentList(*ctx.logger, "args");

        // sass::vector<ExpressionObj> positional,
        //   NormalizedMap<ExpressionObj> named,
        //   Expression* restArgs = nullptr,
        //   Expression* kwdRest = nullptr);

        ValueExpression* restArg = SASS_MEMORY_NEW(
          ValueExpression, args->pstate(), args);

        ValueExpression* kwdRest = nullptr;
        if (!args->keywords().empty()) {
          SassMap* map = args->keywordsAsSassMap();
          kwdRest = SASS_MEMORY_NEW(
            ValueExpression, map->pstate(), map);
        }

        ArgumentInvocationObj invocation = SASS_MEMORY_NEW(
          ArgumentInvocation, pstate, sass::vector<ExpressionObj>{}, KeywordMap<ExpressionObj>{}, restArg, kwdRest);

        if (String_Constant * str = Cast<String_Constant>(function)) {
          sass::string name = unquote(str->value());
          // callStackFrame frame(*ctx.logger, Backtrace(str->pstate()));
          deprecatedDart(
            "Passing a string to call() is deprecated and will be illegal\n"
            "in Dart Sass 2.0.0. Use call(get-function(" + str->to_css(true) + ")) instead.",
            sass::string::npos, *ctx.logger, pstate);
          // std::cerr << "DEPRECATION WARNING: ";
          // std::cerr << "Passing a string to call() is deprecated and will be illegal" << std::endl;
          // std::cerr << "in Sass 4.0. Use call(get-function(" + quote(name) + ")) instead." << std::endl;
          // std::cerr << std::endl;

          InterpolationObj itpl = SASS_MEMORY_NEW(Interpolation, pstate);
          itpl->append(SASS_MEMORY_NEW(StringLiteral, pstate, str->value()));
          FunctionExpressionObj expression = SASS_MEMORY_NEW(
            FunctionExpression, pstate, itpl, invocation);

          return expression->perform(&eval);

        }

        Callable* callable = function->assertFunction(*ctx.logger, "function")->callable();
        return eval._runFunctionCallable(invocation, callable, pstate);


      }

    }

  }

}
