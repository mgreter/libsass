#include "environment.hpp"

#include "eval.hpp"
#include "cssize.hpp"
#include "sources.hpp"
#include "compiler.hpp"
#include "stylesheet.hpp"
#include "exceptions.hpp"
#include "ast_values.hpp"
#include "ast_imports.hpp"
#include "ast_selectors.hpp"
#include "ast_callables.hpp"
#include "ast_statements.hpp"
#include "ast_expressions.hpp"
#include "parser_selector.hpp"
#include "parser_media_query.hpp"
#include "parser_keyframe_selector.hpp"

#include "parser_stylesheet.hpp"

#include <cstring>
#include "compiler.hpp"
#include "charcode.hpp"
#include "charcode.hpp"
#include "character.hpp"
#include "color_maps.hpp"
#include "exceptions.hpp"
#include "source_span.hpp"
#include "ast_imports.hpp"
#include "ast_supports.hpp"
#include "ast_statements.hpp"
#include "ast_expressions.hpp"
#include "parser_expression.hpp"

namespace Sass {

  // Import some namespaces
  using namespace Charcode;
  using namespace Character;
  using namespace StringUtils;


  void exposeFiltered2(
    EnvKeyFlatMap<uint32_t>& merged,
    EnvKeyFlatMap<uint32_t> expose,
    const sass::string prefix,
    const std::set<EnvKey>& filters,
    const sass::string& errprefix,
    Logger& logger,
    bool show)
  {
    for (auto idx : expose) {
      if (idx.first.isPrivate()) continue;
      EnvKey key(prefix + idx.first.orig());
      if (show == (filters.count(key) == 1)) {
        auto it = merged.find(key);
        if (it == merged.end()) {
          merged.insert({ key, idx.second });
        }
        else if (idx.second != it->second) {
          throw Exception::RuntimeException(logger,
            "Two forwarded modules both define a "
            + errprefix + key.norm() + ".");
        }
      }
    }
  }

  void exposeFiltered2(
    EnvKeyFlatMap<uint32_t>& merged,
    EnvKeyFlatMap<uint32_t> expose,
    const sass::string prefix,
    const sass::string& errprefix,
    Logger& logger)
  {
    for (auto idx : expose) {
      if (idx.first.isPrivate()) continue;
      EnvKey key(prefix + idx.first.orig());
      auto it = merged.find(key);
      if (it == merged.end()) {
        merged.insert({ key, idx.second });
      }
      else if (idx.second != it->second) {
        throw Exception::RuntimeException(logger,
          "Two forwarded modules both define a "
          + errprefix + key.norm() + ".");
      }
    }
  }

  void mergeForwards2(
    VarRefs* idxs,
    Root* currentRoot,
    bool isShown,
    bool isHidden,
    const sass::string prefix,
    const std::set<EnvKey>& toggledVariables,
    const std::set<EnvKey>& toggledCallables,
    Logger& logger)
  {

    if (isShown) {
      exposeFiltered2(currentRoot->mergedFwdVar, idxs->varIdxs, prefix, toggledVariables, "variable named $", logger, true);
      exposeFiltered2(currentRoot->mergedFwdMix, idxs->mixIdxs, prefix, toggledCallables, "mixin named ", logger, true);
      exposeFiltered2(currentRoot->mergedFwdFn, idxs->fnIdxs, prefix, toggledCallables, "function named ", logger, true);
    }
    else if (isHidden) {
      exposeFiltered2(currentRoot->mergedFwdVar, idxs->varIdxs, prefix, toggledVariables, "variable named $", logger, false);
      exposeFiltered2(currentRoot->mergedFwdMix, idxs->mixIdxs, prefix, toggledCallables, "mixin named ", logger, false);
      exposeFiltered2(currentRoot->mergedFwdFn, idxs->fnIdxs, prefix, toggledCallables, "function named ", logger, false);
    }
    else {
      exposeFiltered2(currentRoot->mergedFwdVar, idxs->varIdxs, prefix, "variable named $", logger);
      exposeFiltered2(currentRoot->mergedFwdMix, idxs->mixIdxs, prefix, "mixin named ", logger);
      exposeFiltered2(currentRoot->mergedFwdFn, idxs->fnIdxs, prefix, "function named ", logger);
    }

  }

  ImportRule* StylesheetParser::readImportRule(Offset start)
  {
    ImportRuleObj rule = SASS_MEMORY_NEW(
      ImportRule, scanner.relevantSpanFrom(start));

    do {
      scanWhitespace();
      scanImportArgument(rule);
      scanWhitespace();
    } while (scanner.scanChar($comma));
    // Check for expected finalization token
    expectStatementSeparator("@import rule");
    return rule.detach();

  }

  // Consumes a `@use` rule.
  // [start] should point before the `@`.
  UseRule* StylesheetParser::readUseRule(Offset start)
  {
    scanWhitespace();
    sass::string url(string());
    scanWhitespace();
    sass::string ns(readUseNamespace(url, start));
    scanWhitespace();

    SourceSpan state(scanner.relevantSpanFrom(start));

    // Check if name is valid identifier
    if (url.empty() || isDigit(url[0])) {
      context.addFinalStackTrace(state);
      throw Exception::InvalidSassIdentifier(context, url);
    }

    sass::vector<WithConfigVar> config;
    bool hasWith(readWithConfiguration(config, false));
    LOCAL_FLAG(implicitWithConfig, implicitWithConfig || hasWith);
    expectStatementSeparator("@use rule");

    if (isUseAllowed == false) {
      context.addFinalStackTrace(state);
      throw Exception::TardyAtRule(
        context, Strings::useRule);
    }

    UseRuleObj rule = SASS_MEMORY_NEW(UseRule,
      scanner.relevantSpanFrom(start), url, {});
    rule->hasLocalWith(hasWith);

    VarRefs* current(context.varRoot.stack.back());
    VarRefs* modFrame(context.varRoot.stack.back()->getModule23());

    // Support internal modules first
    if (startsWithIgnoreCase(url, "sass:", 5)) {

      WithConfig wconfig(context, config, hasWith);

      if (hasWith) {
        context.addFinalStackTrace(rule->pstate());
        throw Exception::RuntimeException(context,
          "Built-in modules can't be configured.");
      }

      sass::string name(url.substr(5));
      if (ns.empty()) ns = name;

      Module* module(context.getModule(name));

      if (module == nullptr) {
        context.addFinalStackTrace(rule->pstate());
        throw Exception::RuntimeException(context,
          "Invalid internal module requested.");
      }

      if (ns == "*") {

        for (auto var : module->idxs->varIdxs) {
          if (!var.first.isPrivate())
            current->varIdxs.insert(var);
        }
        for (auto mix : module->idxs->mixIdxs) {
          if (!mix.first.isPrivate())
            current->mixIdxs.insert(mix);
        }
        for (auto fn : module->idxs->fnIdxs) {
          if (!fn.first.isPrivate())
            current->fnIdxs.insert(fn);
        }

        current->fwdGlobal55.push_back(
          { module->idxs, nullptr });

      }
      else if (modFrame->fwdModule55.count(ns)) {
        context.addFinalStackTrace(rule->pstate());
        throw Exception::ModuleAlreadyKnown(context, ns);
      }
      else {
        current->fwdModule55.insert({ ns,
          { module->idxs, nullptr } });
      }

      wconfig.finalize();
      return rule.detach();

    }

    bool hasCached = false;
    rule->ns(ns);
    rule->url(url);
    rule->config(config);
    rule->prev(scanner.sourceUrl);
#ifndef SassEagerUseParsing
    rule->needsLoading(true);
#else
    rule = resolveUseRule(rule);
#endif
    return rule.detach();
  }

  // Consumes a `@forward` rule.
  // [start] should point before the `@`.
  ForwardRule* StylesheetParser::readForwardRule(Offset start)
  {
    scanWhitespace();
    sass::string url = string();

    scanWhitespace();
    sass::string prefix;
    if (scanIdentifier("as")) {
      scanWhitespace();
      prefix = readIdentifier();
      scanner.expectChar($asterisk);
      scanWhitespace();
    }

    bool isShown = false;
    bool isHidden = false;
    std::set<EnvKey> toggledVariables2;
    std::set<EnvKey> toggledCallables2;
    Offset beforeShow(scanner.offset);
    if (scanIdentifier("show")) {
      readForwardMembers(toggledVariables2, toggledCallables2);
      isShown = true;
    }
    else if (scanIdentifier("hide")) {
      readForwardMembers(toggledVariables2, toggledCallables2);
      isHidden = true;
    }

    sass::vector<WithConfigVar> config;
    bool hasWith(readWithConfiguration(config, true));
    LOCAL_FLAG(implicitWithConfig, implicitWithConfig || hasWith);
    expectStatementSeparator("@forward rule");

    if (isUseAllowed == false) {
      SourceSpan state(scanner.relevantSpanFrom(start));
      context.addFinalStackTrace(state);
      throw Exception::ParserException(context,
        "@forward rules must be written before any other rules.");
    }

    ForwardRuleObj rule = SASS_MEMORY_NEW(ForwardRule,
      scanner.relevantSpanFrom(start),
      url, {},
      std::move(toggledVariables2),
      std::move(toggledCallables2),
      isShown);

    rule->hasLocalWith(hasWith);

    std::set<EnvKey>& toggledVariables(rule->toggledVariables());
    std::set<EnvKey>& toggledCallables(rule->toggledCallables());

    // Support internal modules first
    if (startsWithIgnoreCase(url, "sass:", 5)) {

      // The show or hide config also hides these
      WithConfig wconfig(context, config, hasWith,
        isShown, isHidden, rule->toggledVariables(), prefix);

      if (hasWith) {
        context.addFinalStackTrace(rule->pstate());
        throw Exception::RuntimeException(context,
          "Built-in modules can't be configured.");
      }

      sass::string name(url.substr(5));
      // if (prefix.empty()) prefix = name; // Must not happen!
      if (Module* module = context.getModule(name)) {

        mergeForwards2(module->idxs, context.currentRoot, isShown, isHidden,
          prefix, toggledVariables, toggledCallables, context);
        rule->root(nullptr);

      }
      else {
        context.addFinalStackTrace(rule->pstate());
        throw Exception::RuntimeException(context,
          "Invalid internal module requested.");
      }

      wconfig.finalize();
      return rule.detach();
    }

    rule->url(url);
    rule->config(config);
    rule->isShown(isShown);
    rule->isHidden(isHidden);
    rule->prefix(prefix);
    rule->prev(scanner.sourceUrl);
    rule->hasLocalWith(hasWith);

#ifndef SassEagerForwardParsing
    rule->needsLoading(true);
#else
    rule = resolveForwardRule(rule);
#endif

    return rule.detach();
  }

}
