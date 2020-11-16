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


  Value* Eval::visitAssignRule(AssignRule* a)
  {


    // We always must have at least one variable
    SASS_ASSERT(a->vidxs().empty(), "Invalid VIDX");

    // Check if we can overwrite an existing variable
    // We create it locally if none other there yet.
    // for (auto vidx : a->vidxs()) {
    //   // Get the value reference (now guaranteed to exist)
    //   ValueObj& value(compiler.varRoot.getVariable(vidx));
    //   // Skip if nothing is stored yet, keep looking
    //   // for another variable with a value set
    //   if (value == nullptr) continue;
    //   // Set variable if default flag is not set or
    //   // if no value has been set yet or that value
    //   // was explicitly set to a SassNull value.
    // 
    // 
    //   if (!a->is_default() || value->isNull()) {
    // 
    //     if (vidx.isPrivate(compiler.varRoot.privateVarOffset)) {
    //       compiler.addFinalStackTrace(a->pstate());
    //       throw Exception::RuntimeException(compiler,
    //         "Cannot modify built-in variable.");
    //     }
    // 
    //     compiler.varRoot.setVariable(vidx,
    //        a->value()->accept(this));
    //   }
    //   // Finished
    //   return nullptr;
    // }

    // Emit deprecation for new var with global flag
    if (a->is_global()) {

      auto rframe = compiler.varRoot.stack[0];
      auto it = rframe->varIdxs.find(a->variable());

      bool hasVar = false;

      if (it != rframe->varIdxs.end()) {
        VarRef vidx(rframe->varFrame, it->second);
        auto value = compiler.varRoot.getVariable(vidx);
        if (value != nullptr) hasVar = true;
      }

      if (hasVar == false) {
        // libsass/variable-scoping/defaults-global-null
        // This check may not be needed, but we create a
        // superfluous variable slot in the scope
        for (auto fwds : rframe->fwdGlobal55) {
          auto it = fwds.first->varIdxs.find(a->variable());
          if (it != fwds.first->varIdxs.end()) {
            VarRef vidx(0xFFFFFFFF, it->second);
            auto value = compiler.varRoot.getVariable(vidx);
            if (value != nullptr) hasVar = true;
          }

          auto fwd = fwds.second->mergedFwdVar.find(a->variable());
          if (fwd != fwds.second->mergedFwdVar.end()) {
            VarRef vidx(0xFFFFFFFF, fwd->second);
            auto value = compiler.varRoot.getVariable(vidx);
            if (value != nullptr) hasVar = true;
          }
        }
      }

      if (hasVar == false) {

        // Check if we are at the global scope
        if (compiler.varRoot.isGlobal()) {
          logger456.addDeprecation(
            "As of LibSass 4.1, !global assignments won't be able to declare new"
            " variables. Since this assignment is at the root of the stylesheet,"
            " the !global flag is unnecessary and can safely be removed.",
            a->pstate());
        }
        else {
          logger456.addDeprecation(
            "As of LibSass 4.1, !global assignments won't be able to declare new variables."
            " Consider adding `$" + a->variable().orig() + ": null` at the root of the stylesheet.",
            a->pstate());
        }

      }

    }


    if (a->ns().empty()) {

      if (!compiler.varRoot.setVariable(
        a->variable(),
        a->value()->accept(this),
        a->is_default(),
        a->is_global()))
      {
        if (!a->vidxs().empty()) {
          compiler.varRoot.setVariable(
            a->vidxs().front(),
            a->value()->accept(this),
            false);
        }
      }
    }
    else {

      if (!compiler.varRoot.setModVar(
        a->variable(), a->ns(),
        a->value()->accept(this),
        a->is_default(),
        a->pstate()))
      {
        if (compiler.varRoot.stack.back()->hasNameSpace(a->ns(), a->variable())) {
          callStackFrame frame(traces, a->pstate());
          throw Exception::RuntimeException(traces, "Undefined variable.");
        }
        else {
          callStackFrame frame(traces, a->pstate());
          throw Exception::ModuleUnknown(traces, a->ns());
        }
      }

      return nullptr;

    }

    // Get the main (local) variable
    // VarRef vidx(a->vidxs().front());

    // if (vidx.isPrivate(compiler.varRoot.privateVarOffset)) {
    //   compiler.addFinalStackTrace(a->pstate());
    //   throw Exception::RuntimeException(compiler,
    //     "Cannot modify built-in variable.");
    // }

    // Now create the new variable
    //compiler.varRoot.setVariable(vidx,
    //  a->value()->accept(this));

    return nullptr;
  }


  AssignRule* StylesheetParser::readVariableDeclarationWithoutNamespace(
    const sass::string& ns, Offset start)
  {

    // LocalOption<SilentCommentObj>
    //   scomp(lastSilentComment, nullptr);

    sass::string vname(variableName());

    if (!ns.empty()) {
      assertPublicIdentifier(vname, start);
    }

    EnvKey name(vname);

    if (plainCss()) {
      error("Sass variables aren't allowed in plain CSS.",
        scanner.relevantSpanFrom(start));
    }

    scanWhitespace();
    scanner.expectChar($colon);
    scanWhitespace();

    ExpressionObj value = readExpression();

    bool guarded = false;
    bool global = false;

    Offset flagStart(scanner.offset);
    while (scanner.scanChar($exclamation)) {
      sass::string flag = readIdentifier();
      if (flag == "default") {
        guarded = true;
      }
      else if (flag == "global") {
        if (!ns.empty()) {
          error("!global isn't allowed for variables in other modules.",
            scanner.relevantSpanFrom(flagStart));
        }
        global = true;
      }
      else {
        error("Invalid flag name.",
          scanner.relevantSpanFrom(flagStart));
      }

      scanWhitespace();
      flagStart = scanner.offset;
    }

    expectStatementSeparator("variable declaration");

    bool has_local = false;
    // Skip to optional global scope
    VarRefs* frame = global ?
      context.varRoot.stack.front() :
      context.varRoot.stack.back();
    // As long as we are not in a loop construct, we
    // can utilize full static variable optimizations.
    VarRef vidx(inLoopDirective ?
      frame->getLocalVariableIdx(name) :
      frame->getVariableIdx(name));
    // Check if we found a local variable to use
    if (vidx.isValid()) has_local = true;
    // Otherwise we must create a new local variable

    // JIT self assignments
#ifdef SASS_OPTIMIZE_SELF_ASSIGN
// Optimization for cases where functions manipulate same variable
// We detect these cases here in order for the function to optimize
// self-assignment case (e.g. map-merge). It can then manipulate
// the value passed as the first argument directly in-place.
    if (has_local && !global) {
      // Certainly looks a bit like some poor man's JIT 
      if (auto fn = value->isaFunctionExpression()) {
        auto& pos(fn->arguments()->positional());
        if (pos.size() > 0) {
          if (auto var = pos[0]->isaVariableExpression()) {
            if (var->name() == name) { // same name
              fn->selfAssign(true); // Up to 15% faster
            }
          }
        }
      }
    }
#endif

    sass::vector<VarRef> vidxs;

    if (vidx.isValid()) vidxs.push_back(vidx);
    VarRefs* module(context.varRoot.stack.back()->getModule23());
    SourceSpan pstate(scanner.relevantSpanFrom(start));

    if (!ns.empty()) {
      // auto pstate(scanner.relevantSpanFrom(start));
      // context.addFinalStackTrace(pstate);
      // throw Exception::ParserException(context,
      //   "Variable namespaces not supported yet!");

      auto it = module->fwdModule55.find(ns);
      if (it != module->fwdModule55.end()) {
        VarRefs* refs = it->second.first;
        auto in = refs->varIdxs.find(name);
        if (in != refs->varIdxs.end()) {

          if (name.isPrivate()) {
            SourceSpan state(scanner.relevantSpanFrom(start));
            context.addFinalStackTrace(state);
            throw Exception::ParserException(context,
              "Private members can't be accessed "
              "from outside their modules.");
          }

          uint32_t offset = in->second;
          vidxs.push_back({ refs->varFrame, offset });
        }
      }
      //else {
      //  SourceSpan state(scanner.relevantSpanFrom(start));
      //  context.addFinalStackTrace(state);
      //  throw Exception::RuntimeException(context, "There is no "
      //    "module with the namespace \"" + ns + "\".");
      //}

      if (vidxs.empty()) {
        VarRef vidx(module->getVariableIdx(name, true));
        if (!vidxs.empty()) vidxs.push_back(vidx);
      }

      // if (vidxs.empty()) {
      //   context.addFinalStackTrace(pstate);
      //   auto asd = context.callStack;
      //   throw Exception::RuntimeException(context,
      //     "Undefined variable.");
      // }

    }
    else {

      // This only checks within the local frame
      // Don't assign global if in another scope!
      for (auto fwd : frame->fwdGlobal55) {
        VarRefs* refs = fwd.first;
        auto in = refs->varIdxs.find(name);
        if (in != refs->varIdxs.end()) {
          if (name.isPrivate()) {
            context.addFinalStackTrace(pstate);
            throw Exception::ParserException(context,
              "Private members can't be accessed "
              "from outside their modules.");
          }
          uint32_t offset = in->second;
          vidxs.push_back({ refs->varFrame, offset });
        }
      }

    }

    auto pr = frame;
    while (pr->isImport) pr = pr->pscope;

    if (pr->varFrame == 0xFFFFFFFF) {

      // Assign a default
      if (guarded && ns.empty()) {
        auto cfgvar = context.getCfgVar(name, true, false);
        if (!cfgvar || cfgvar->isNull) cfgvar = context.getCfgVar(name, false, false);

        if (cfgvar) {
          if (cfgvar->value) {
            value = SASS_MEMORY_NEW(ValueExpression,
              cfgvar->value->pstate(), cfgvar->value);
          }
          else {
            value = cfgvar->expression;
          }
        }
      }

    }

    // Check if we have a configuration

    bool hasVar = false;

    if (vidxs.empty() && ns.empty()) {

      // IF we are semi-global and parent is root
      // And if that root also contains that variable
      // We assign to that instead of a new local one!
      auto chroot = frame;

      if (global) {
        while (chroot->pscope) {
          chroot = chroot->pscope;
        }
        pr = chroot;
      }

      while (chroot->permeable && chroot->pscope) {
        // Check if this frame contains the variable
        if (chroot->pscope->varIdxs.count(name)) {
          pr = chroot->pscope;
          hasVar = true;
          break;
        }
        chroot = chroot->pscope;
      }

      // Also skip if on global scope?
      // Not if we have one forwarded!
      if (!hasVar) {
        vidxs.push_back(pr->createVariable(name));
      }
    }

    AssignRule* declaration = SASS_MEMORY_NEW(AssignRule,
      scanner.relevantSpanFrom(start), name, ns, vidxs, value, guarded, global);
    // if (inLoopDirective) frame->assignments.push_back(declaration);
    return declaration;
  }
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
