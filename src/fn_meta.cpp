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
        auto parent = compiler.varStack.back()->getModule();
        if (plugin != nullptr) {
          auto pp = parent->fwdModule33.find(plugin->value());
          if (pp != parent->fwdModule33.end()) {
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
        for (auto asd : parent->fwdGlobal33) {
          VarRefs* global = asd.first;
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
          compiler.getVariable(variable->value(), true));

      }

      /*******************************************************************/

      BUILT_IN_FN(variableExists)
      {
        String* variable = arguments[0]->assertString(compiler, Sass::Strings::name);
        ValueObj ex = compiler.getVariable(variable->value());
        bool hasVar = false;
        auto parent = compiler.varStack.back()->getModule();
        for (auto asd : parent->fwdGlobal33) {
          VarRefs* global = asd.first;
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
        auto parent = compiler.varStack.back()->getModule();
        if (plugin != nullptr) {
          auto pp = parent->fwdModule33.find(plugin->value());
          if (pp != parent->fwdModule33.end()) {
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
        for (auto asd : parent->fwdGlobal33) {
          VarRefs* global = asd.first;
          if (global->fnIdxs.count(variable->value()) != 0) {
            if (hasFn) {
              throw Exception::RuntimeException(compiler,
                "This function is available from multiple global modules.");
            }
            hasFn = true;
          }
        }
        if (hasFn) return SASS_MEMORY_NEW(Boolean, pstate, true);
        CallableObj fn = compiler.getFunction(variable->value());
        return SASS_MEMORY_NEW(Boolean, pstate, !fn.isNull());
      }

      BUILT_IN_FN(mixinExists)
      {
        String* variable = arguments[0]->assertString(compiler, Sass::Strings::name);
        String* plugin = arguments[1]->assertStringOrNull(compiler, Sass::Strings::module);

        auto parent = compiler.varStack.back()->getModule();
        if (plugin != nullptr) {
          auto pp = parent->fwdModule33.find(plugin->value());
          if (pp != parent->fwdModule33.end()) {
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
        for (auto asd : parent->fwdGlobal33) {
          VarRefs* global = asd.first;
          if (global->mixIdxs.count(variable->value()) != 0) {
            if (hasFn) {
              throw Exception::RuntimeException(compiler,
                "This function is available from multiple global modules.");
            }
            hasFn = true;
          }
        }
        if (hasFn) return SASS_MEMORY_NEW(Boolean, pstate, true);

        CallableObj fn = compiler.getMixin(variable->value());
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
        Map* list = SASS_MEMORY_NEW(Map, pstate);
        auto module = compiler.varStack.back()->getModule();
        auto it = module->fwdModule33.find(ns->value());
        if (it != module->fwdModule33.end()) {
          VarRefs* refs = it->second.first;
          Root* root = it->second.second;
          if (root && !root->isActive) {
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
        }
        else {
          throw Exception::RuntimeException(compiler, "There is "
            "no module with namespace \"" + ns->value() + "\".");
        }
        return list;
      }

      BUILT_IN_FN(moduleFunctions)
      {
        String* ns = arguments[0]->assertStringOrNull(compiler, Sass::Strings::module);
        Map* list = SASS_MEMORY_NEW(Map, pstate);
        auto module = compiler.varStack.back()->getModule();
        auto it = module->fwdModule33.find(ns->value());
        if (it != module->fwdModule33.end()) {
          VarRefs* refs = it->second.first;
          Root* root = it->second.second;
          if (root && !root->isActive) {
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
        }
        else {
          throw Exception::RuntimeException(compiler, "There is "
            "no module with namespace \"" + ns->value() + "\".");
        }
        return list;
      }

      /// Like `_environment.getFunction`, but also returns built-in
      /// globally-available functions.
      Callable* _getFunction(const EnvKey& name, Compiler& ctx, const sass::string& ns = "") {
        return ctx.getFunction(name); // no detach, is a reference anyway
      }

      BUILT_IN_FN(getFunction)
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

        auto parent = compiler.varStack.back()->getModule();

        if (ns != nullptr) {
          auto pp = parent->fwdModule33.find(ns->value());
          if (pp != parent->fwdModule33.end()) {
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

            for (auto asd : parent->fwdGlobal33) {
              VarRefs* global = asd.first;
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
            FunctionExpression, pstate, itpl, invocation);
          return eval.acceptFunctionExpression(expression);
          // return expression->accept(&eval);

        }

        Callable* callable = function->assertFunction(compiler, "function")->callable();
        return callable->execute(eval, invocation, pstate, false);


      }

      BUILT_IN_FN(loadCss)
      {
        String* url = arguments[0]->assertStringOrNull(compiler, Strings::url);
        Map* withMap = arguments[1]->assertMapOrNull(compiler, Strings::with);
        // auto name = SASS_MEMORY_NEW(CssString, pstate, "added");
        // auto value = SASS_MEMORY_NEW(String, pstate, "yeppa mixin");
        // auto decl = SASS_MEMORY_NEW(CssDeclaration, pstate, name, value);
        LocalOption<bool> scoped(compiler.hasWithConfig,
          compiler.hasWithConfig || withMap);

        EnvKeyFlatMap<ValueObj> config;
        sass::vector<WithConfigVar> withConfigs;
        if (withMap) {
          for (auto kv : withMap->elements()) {
            String* name = kv.first->assertString(compiler, "with key");
            EnvKey kname(name->value());
            WithConfigVar kvar;
            kvar.name = name->value();
            kvar.value = kv.second;
            kvar.isGuarded = false;
            kvar.wasUsed = false;
            kvar.pstate2 = name->pstate();
            kvar.isNull = !kv.second || kv.second->isaNull();
            withConfigs.push_back(kvar);
            if (config.count(kname) == 1) {
              throw Exception::RuntimeException(compiler,
                "The variable $" + kname.norm() + " was configured twice.");
            }
            config[name->value()] = kv.second;
          }
        }

        WithConfig wconfig(compiler, withConfigs, withMap);

        if (StringUtils::startsWith(url->value(), "sass:", 5)) {

          if (withMap) {
            throw Exception::RuntimeException(compiler, "Built-in "
              "module " + url->value() + " can't be configured.");
          }

          return SASS_MEMORY_NEW(Null, pstate);
        }

        sass::string previous(".");
        const ImportRequest import(url->value(), previous);
        // Search for valid imports (e.g. partials) on the file-system
        // Returns multiple valid results for ambiguous import path
        const sass::vector<ResolvedImport> resolved(compiler.findIncludes(import));

        // Error if no file to import was found
        if (resolved.empty()) {
          compiler.addFinalStackTrace(pstate);
          throw Exception::ParserException(compiler,
            "Can't find stylesheet to import.");
        }
        // Error if multiple files to import were found
        else if (resolved.size() > 1) {
          sass::sstream msg_stream;
          msg_stream << "It's not clear which file to import. Found:\n";
          for (size_t i = 0, L = resolved.size(); i < L; ++i)
          {
            msg_stream << "  " << resolved[i].imp_path << "\n";
          }
          throw Exception::ParserException(compiler, msg_stream.str());
        }

        // We made sure exactly one entry was found, load its content
        if (ImportObj loaded = compiler.loadImport(resolved[0])) {

          auto asd = compiler.sheets.find(loaded->getAbsPath());

          StyleSheet* sheet = asd == compiler.sheets.end() ? nullptr : asd->second;

          if (sheet == nullptr) {
            // This is the new barrier!
            EnvFrame local(compiler.varStack, true, true);
            // EnvFrame local(context.varStack, true, true);
            local.idxs->varFrame = 0xFFFFFFFF;
            local.idxs->mixFrame = 0xFFFFFFFF;
            local.idxs->fnFrame = 0xFFFFFFFF;
            // eval.selectorStack.push_back(nullptr);
            sheet = compiler.registerImport(loaded); // @use
            // eval.selectorStack.pop_back();
            sheet->root2->idxs = local.idxs;
          }
          else if (sheet->root2->isActive) {
            if (withMap) {
              throw Exception::ParserException(compiler,
                sass::string(sheet->root2->pstate().getFileName())
                + " was already loaded, so it "
                "can\'t be configured using \"with\".");
            }
            else if (sheet->root2->isLoading) {
              throw Exception::ParserException(compiler,
                "Module loop: " + sass::string(sheet->root2->pstate().getFileName()) + " is already being loaded.");
            }
          }

          if (sheet->root2->loaded) {
            if (withMap) {
              throw Exception::RuntimeException(compiler,
                "Module twice");
            }
            for (auto child : sheet->root2->loaded->elements()) {
              if (auto css = child->isaCssStyleRule()) {
                if (auto pr = eval.current->isaCssStyleRule()) {
                  for (auto inner : css->elements()) {
                    auto copy = SASS_MEMORY_COPY(css->selector());
                    for (ComplexSelector* asd : copy->elements()) {
                      asd->chroots(false);
                    }
                    // auto reduced1 = copy1->resolveParentSelectors(css->selector(), compiler, false);
                    SelectorListObj resolved = copy->resolveParentSelectors(pr->selector(), compiler, true);
                    auto newRule = SASS_MEMORY_NEW(CssStyleRule, css->pstate(), eval.current, resolved, { inner });
                    eval.current->parent()->append(newRule);
                  }
                }
                else {
                  if (eval.current) eval.current->append(child);
                }
              }
              else {
                if (eval.current) eval.current->append(child);
              }
            }
            }
          else {
            Root* root = sheet->root2;
            root->isActive = true;
            root->isLoading = true;
            //root->loaded = eval.current;
            root->loaded = SASS_MEMORY_NEW(CssStyleRule,
              root->pstate(), nullptr, nullptr);
            auto oldCurrent = eval.current;
            eval.current = root->loaded;
            EnvScope scoped(compiler.varRoot, root->idxs);
            eval.selectorStack.push_back(nullptr);
            for (auto child : root->elements()) {
              child->accept(&eval);
            }
            eval.selectorStack.pop_back();
            eval.current = oldCurrent;
            root->isLoading = false;

            for (auto child : sheet->root2->loaded->elements()) {
              if (auto css = child->isaCssStyleRule()) {
                if (auto pr = eval.current->isaCssStyleRule()) {
                  for (auto inner : css->elements()) {
                    auto copy = SASS_MEMORY_COPY(css->selector());
                    for (ComplexSelector* asd : copy->elements()) {
                      asd->chroots(false);
                    }
                    // auto reduced1 = copy1->resolveParentSelectors(css->selector(), compiler, false);
                    SelectorListObj resolved = copy->resolveParentSelectors(pr->selector(), compiler, true);
                    auto newRule = SASS_MEMORY_NEW(CssStyleRule, css->pstate(), eval.current, resolved, { inner });
                    eval.current->parent()->append(newRule);
                  }
                }
                else {
                  if (eval.current) eval.current->append(child);
                }
              }
              else {
                if (eval.current) eval.current->append(child);
              }
            }

          }


        }
        else {
          throw Exception::ParserException(compiler,
            "Couldn't load it.");
        }

        wconfig.finalize();

        return SASS_MEMORY_NEW(Null, pstate);;
      }

      void registerFunctions(Compiler& compiler)
	    {

        Module& module(compiler.createModule("meta"));

        module.addMixin("load-css", compiler.createBuiltInMixin("load-css", "$url, $with: null", loadCss));

        // Meta functions
        module.addFunction("feature-exists", compiler.registerBuiltInFunction("feature-exists", "$feature", featureExists));
        module.addFunction("type-of", compiler.registerBuiltInFunction("type-of", "$value", typeOf));
        module.addFunction("inspect", compiler.registerBuiltInFunction("inspect", "$value", inspect));
        module.addFunction("keywords", compiler.registerBuiltInFunction("keywords", "$args", keywords));

        compiler.registerBuiltInFunction("if", "$condition, $if-true, $if-false", fnIf);
        // ToDo: dart-sass keeps them on the local environment scope, see below:
        // These functions are defined in the context of the evaluator because
        // they need access to the [_environment] or other local state.
        module.addFunction("global-variable-exists", compiler.registerBuiltInFunction("global-variable-exists", "$name, $module: null", globalVariableExists));
        module.addFunction("variable-exists", compiler.registerBuiltInFunction("variable-exists", "$name", variableExists));
        module.addFunction("function-exists", compiler.registerBuiltInFunction("function-exists", "$name, $module: null", functionExists));
        module.addFunction("mixin-exists", compiler.registerBuiltInFunction("mixin-exists", "$name, $module: null", mixinExists));
        module.addFunction("content-exists", compiler.registerBuiltInFunction("content-exists", "", contentExists));
        module.addFunction("module-variables", compiler.createBuiltInFunction("module-variables", "$module", moduleVariables));
        module.addFunction("module-functions", compiler.registerBuiltInFunction("module-functions", "$module", moduleFunctions));
        module.addFunction("get-function", compiler.registerBuiltInFunction("get-function", "$name, $css: false, $module: null", getFunction));
        module.addFunction("call", compiler.registerBuiltInFunction("call", "$function, $args...", call));

	    }

    }

  }

}
