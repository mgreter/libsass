#include "fn_meta.hpp"

#include <cstring>

#include "eval.hpp"
#include "compiler.hpp"
#include "exceptions.hpp"
#include "ast_values.hpp"
#include "ast_callables.hpp"
#include "ast_expressions.hpp"
#include "string_utils.hpp"

#include "environment.hpp"
#include "preloader.hpp"

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

    // Optimize case where we know to what variable to assign to
    // This should potentially increase performance, but real-time
    // profiling only show a very minor increase (but keep anyway).
    if (a->vidx().isValid()) {
      assigne = &compiler.varRoot.getVariable(a->vidx());
      compiler.varRoot.setVariable(a->vidx(),
        a->value()->accept(this), a->is_default());
      return nullptr;
    }
    //else {
    //  std::cerr << "FADA\n";
    //}

    ValueObj result;

    const EnvKey& vname(a->variable());

    if (a->is_default()) {

      auto scope = compiler.getCurrentScope();

      // If we have a config and the variable is already set
      // we still overwrite the variable beside being guarded
      WithConfigVar* wconf = nullptr;
      if (compiler.wconfig && scope->isInternal && a->ns().empty()) {
        wconf = wconfig->getCfgVar(vname);
      }
      if (wconf) {
        // Via load-css
        if (wconf->value33) {
          result = wconf->value33;
        }
        // Via regular load
        else if (wconf->expression44) {
          a->value(wconf->expression44);
        }
        a->is_default(wconf->needsAssignment);
      }
    }

    // Emit deprecation for new var with global flag
    if (a->is_global()) {

      auto rframe = compiler.varRoot.stack[0];
      auto it = rframe->varIdxs.find(a->variable());

      bool hasVar = false;

      if (it != rframe->varIdxs.end()) {
        EnvRef vidx(rframe, it->second);
        auto& value = compiler.varRoot.getVariable(vidx);
        if (value != nullptr) hasVar = true;
      }

      if (hasVar == false) {
        // libsass/variable-scoping/defaults-global-null
        // This check may not be needed, but we create a
        // superfluous variable slot in the scope
        for (auto fwds : rframe->forwards) {
          auto it = fwds->varIdxs.find(a->variable());
          if (it != fwds->varIdxs.end()) {
            EnvRef vidx(it->second);
            auto& value = compiler.varRoot.getVariable(vidx);
            if (value != nullptr) hasVar = true;
          }

          auto fwd = fwds->module->mergedFwdVar.find(a->variable());
          if (fwd != fwds->module->mergedFwdVar.end()) {
            EnvRef vidx(fwd->second);
            auto& value = compiler.varRoot.getVariable(vidx);
            if (value != nullptr) hasVar = true;
          }
        }
      }

      if (hasVar == false) {

        // Check if we are at the global scope
        if (compiler.varRoot.isGlobal()) {
          logger.addDeprecation(
            "As of LibSass 4.1, !global assignments won't be able to declare new variables.\n"
            "Since this assignment is at the root of the stylesheet, the !global"
            " flag is unnecessary and can safely be removed.",
            a->pstate());
        }
        else {
          logger.addDeprecation(
            "As of LibSass 4.1, !global assignments won't be able to declare new variables.\n"
            "Consider adding `$" + a->variable().orig() + ": null` at the root of the stylesheet.",
            a->pstate());
        }

      }

    }

    if (a->ns().empty()) {

      a->vidx(compiler.varRoot.findVarIdx(
        a->variable(), a->ns(), a->is_global()));
      assigne = &compiler.varRoot.getVariable(a->vidx());
      if (!result) result = a->value()->accept(this);
      compiler.varRoot.setVariable(
        a->vidx(),
        result,
        a->is_default());
      assigne = nullptr;

    }
    else {

      EnvRefs* mod = compiler.getCurrentModule();

      auto it = mod->module->moduse.find(a->ns());
      /* if (it == mod->module->moduse.end()) {
        callStackFrame csf(compiler, a->pstate());
        throw Exception::ModuleUnknown(compiler, a->ns());
      }
      else */ if (it->second.second && !it->second.second->isCompiled) {
        callStackFrame csf(compiler, a->pstate());
        throw Exception::ModuleUnknown(compiler, a->ns());
      }

      if (!result) result = a->value()->accept(this);


      if (auto frame = compiler.getCurrentScope()) {
        a->vidx(frame->setModVar(
          a->variable(), a->ns(),
          result,
          a->is_default(),
          a->pstate()));
      }

    }

    if (!a->vidx().isValid())
    {
      if (compiler.varRoot.stack.back()->hasNameSpace(a->ns())) {
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




  AssignRule* StylesheetParser::readVariableDeclarationWithoutNamespace(
    const sass::string& ns, Offset start)
  {

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

    // Skip to optional global scope
    EnvRefs* frame = global ?
      compiler.varRoot.stack.front() :
      compiler.varRoot.stack.back();

    SourceSpan pstate(scanner.relevantSpanFrom(start));

    bool hasVar = false;
    auto chroot = frame;
    while (chroot) {
      if (ns.empty()) {
        if (chroot->varIdxs.count(name)) {
          hasVar = true;
          break;
        }
      }
      if (chroot->isImport || chroot->isSemiGlobal) {
        chroot = chroot->pscope;
      }
      else {
        break;
      }
    }

    AssignRule* declaration = SASS_MEMORY_NEW(AssignRule,
      scanner.relevantSpanFrom(start),
      name, ns,
      {}, value, guarded, global);

    if (ns.empty() && !hasVar) {
      frame->createVariable(name);
    }

    return declaration;
  }
  // EO readVariableDeclarationWithoutNamespace

  // Consumes a mixin declaration.
  // [start] should point before the `@`.
  MixinRule* StylesheetParser::readMixinRule(Offset start)
  {

    EnvRefs* frame = compiler.getCurrentScope();

    EnvFrame local(compiler, false);
    // Create space for optional content callable
    // ToDo: check if this can be conditionally done?
    local.idxs->createMixin(Keys::contentRule);
    // var precedingComment = lastSilentComment;
    // lastSilentComment = null;
    sass::string name = readIdentifier();
    scanWhitespace();

    CallableSignatureObj arguments;
    if (scanner.peekChar() == $lparen) {
      arguments = parseArgumentDeclaration();
    }
    else {
      // Dart-sass creates this one too
      arguments = SASS_MEMORY_NEW(CallableSignature,
        scanner.relevantSpan(), sass::vector<ArgumentObj>()); // empty declaration
    }

    if (inMixin || inContentBlock) {
      error("Mixins may not contain mixin declarations.",
        scanner.relevantSpanFrom(start));
    }
    else if (inControlDirective) {
      error("Mixins may not be declared in control directives.",
        scanner.relevantSpanFrom(start));
    }

    scanWhitespace();
    RAII_FLAG(inMixin, true);
    RAII_FLAG(mixinHasContent, false);

    while (frame->isImport) frame = frame->pscope;
    EnvRef midx = frame->createMixin(name);
    MixinRule* rule = withChildren<MixinRule>(
      &StylesheetParser::readChildStatement,
      start, name, arguments, local.idxs);
    // Mixins can't be created in loops
    // Must be on root, not even in @if
    // Therefore this optimization is safe
    rule->midx(midx);
    // rule->cidx(cidx);
    return rule;
  }
  // EO _mixinRule

  // Consumes a function declaration.
  // [start] should point before the `@`.
  FunctionRule* StylesheetParser::readFunctionRule(Offset start)
  {
    // Variables should not be hoisted through
    EnvRefs* parent = compiler.varRoot.stack.back();
    EnvFrame local(compiler, false);

    // var precedingComment = lastSilentComment;
    // lastSilentComment = null;
    sass::string name = readIdentifier();
    sass::string normalized(name);

    scanWhitespace();

    CallableSignatureObj arguments = parseArgumentDeclaration();

    if (inMixin || inContentBlock) {
      error("Mixins may not contain function declarations.",
        scanner.relevantSpanFrom(start));
    }
    else if (inControlDirective) {
      error("Functions may not be declared in control directives.",
        scanner.relevantSpanFrom(start));
    }

    sass::string fname(StringUtils::unvendor(name));
    if (fname == "calc" || fname == "element" || fname == "expression" || fname == "clamp" ||
      fname == "url" || fname == "and" || fname == "or" || fname == "not") {
      error("Invalid function name.",
        scanner.relevantSpanFrom(start));
    }

    scanWhitespace();
    FunctionRule* rule = withChildren<FunctionRule>(
      &StylesheetParser::readFunctionRuleChild,
      start, name, arguments, local.idxs);
    rule->fidx(parent->createFunction(name));
    return rule;
  }
  // EO readFunctionRule

  void exposeFiltered(
    VidxEnvKeyMap& merged,
    VidxEnvKeyMap expose,
    const sass::string prefix,
    const std::set<EnvKey>& filters,
    const sass::string& errprefix,
    Logger& logger,
    bool show)
  {
    for (auto& idx : expose) {
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

  void exposeFiltered(
    VidxEnvKeyMap& merged,
    VidxEnvKeyMap expose,
    const sass::string prefix,
    const sass::string& errprefix,
    Logger& logger)
  {

    for (auto& idx : expose) {
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

  void mergeForwards(
    EnvRefs* idxs,
    Module* module,
    WithConfig* wconfig,
    Logger& logger)
  {

    // Only should happen if forward was found in root stylesheet
    // Doesn't make much sense as there is nowhere to forward to
    if (idxs->module != nullptr) {
      // This is needed to support double forwarding (ToDo - need filter, order?)
      for (auto& entry : idxs->module->mergedFwdVar) { module->mergedFwdVar.insert(entry); }
      for (auto& entry : idxs->module->mergedFwdMix) { module->mergedFwdMix.insert(entry); }
      for (auto& entry : idxs->module->mergedFwdFn) { module->mergedFwdFn.insert(entry); }
    }

    if (wconfig->hasShowFilter) {
      exposeFiltered(module->mergedFwdVar, idxs->varIdxs, wconfig->prefix, wconfig->varFilters, "variable named $", logger, true);
      exposeFiltered(module->mergedFwdMix, idxs->mixIdxs, wconfig->prefix, wconfig->callFilters, "mixin named ", logger, true);
      exposeFiltered(module->mergedFwdFn, idxs->fnIdxs, wconfig->prefix, wconfig->callFilters, "function named ", logger, true);
    }
    else if (wconfig->hasHideFilter) {
      exposeFiltered(module->mergedFwdVar, idxs->varIdxs, wconfig->prefix, wconfig->varFilters, "variable named $", logger, false);
      exposeFiltered(module->mergedFwdMix, idxs->mixIdxs, wconfig->prefix, wconfig->callFilters, "mixin named ", logger, false);
      exposeFiltered(module->mergedFwdFn, idxs->fnIdxs, wconfig->prefix, wconfig->callFilters, "function named ", logger, false);
    }
    else {
      exposeFiltered(module->mergedFwdVar, idxs->varIdxs, wconfig->prefix, "variable named $", logger);
      exposeFiltered(module->mergedFwdMix, idxs->mixIdxs, wconfig->prefix, "mixin named ", logger);
      exposeFiltered(module->mergedFwdFn, idxs->fnIdxs, wconfig->prefix, "function named ", logger);
    }

  }


  Root* Eval::loadModRule(ModRule* rule)
  {

    // May not be defined yet
    Module* mod = rule->module32();

    // Nothing to be done for built-ins
    if (mod && mod->isBuiltIn) {
      return nullptr;
    }

    // Seems already loaded?
    if (rule->root47()) {
      return rule->root47();
    }

    RAII_PTR(WithConfig, wconfig, rule);

    auto sheet = loadModule(
      rule->prev51(), rule->url());
    rule->module32(sheet);
    rule->root47(sheet);

    return sheet;

  }

  Root* Eval::loadModule(
    const sass::string& prev,
    const sass::string& url,
    bool isImport)
  {

    // Resolve final file to load
    const ImportRequest request(
      url, prev, false);

    // Search for valid imports (e.g. partials) on the file-system
    // Returns multiple valid results for ambiguous import path
    const sass::vector<ResolvedImport>& resolved(
      compiler.findIncludes(request, isImport));

    // Error if no file to import was found
    if (resolved.empty()) {
      throw Exception::UnknownImport(compiler);
    }
    // Error if multiple files to import were found
    else if (resolved.size() > 1) {
      throw Exception::AmbiguousImports(compiler, resolved);
    }

    // This is guaranteed to either load or error out!
    ImportObj loaded = compiler.loadImport(resolved[0]);
    ImportStackFrame iframe(compiler, loaded);

    sass::string abspath(loaded->getAbsPath());
    auto cached = compiler.sheets.find(abspath);
    if (cached != compiler.sheets.end()) {
      return cached->second;
    }

    // Permeable seems to have minor negative impact!?
    EnvFrame local(compiler, false, true, isImport); // correct
    Root* sheet = compiler.registerImport(loaded);
    sheet->idxs = local.idxs;
    sheet->import = loaded;
    return sheet;
  }


  Root* Eval::resolveIncludeImport(IncludeImport* rule)
  {
    // Seems already loaded?
    if (rule->root47()) {
      return rule->root47();
    }

    //if (rule->module32() && rule->module32()->isBuiltIn) {
    //  return nullptr;
    //}

    if (Root* sheet2 = loadModule(
      rule->prev51(),
      rule->url(),
      true
    )) {
      rule->module32(sheet2);
      rule->root47(sheet2);
      return sheet2;
    }

    return nullptr;

  }



  EnvRefs* Eval::pudding(EnvRefs* idxs, bool intoRoot, EnvRefs* modFrame)
  {

    if (intoRoot) {

      // Check if we push the same stuff twice
      for (auto fwd : modFrame->forwards) {

        // if (idxs == fwd) continue;

        // Checked, needed
        for (auto& var : idxs->varIdxs) {
          auto it = fwd->varIdxs.find(var.first);
          if (it != fwd->varIdxs.end()) {
            if (var.second == it->second) continue;
            throw Exception::ParserException(compiler,
              "$" + var.first.norm() + " is available "
              "from multiple global modules.");
          }
        }
        // Checked, needed
        for (auto& var : idxs->mixIdxs) {
          auto it = fwd->mixIdxs.find(var.first);
          if (it != fwd->mixIdxs.end()) {
            if (var.second == it->second) continue;
            throw Exception::ParserException(compiler,
              "Mixin \"" + var.first.norm() + "(...)\" is "
              "available from multiple global modules.");
          }
        }
        // Checked, needed
        for (auto& var : idxs->fnIdxs) {
          auto it = fwd->fnIdxs.find(var.first);
          if (it != fwd->fnIdxs.end()) {
            if (var.second == it->second) continue;
            throw Exception::ParserException(compiler,
              "Function \"" + var.first.norm() + "(...)\" is "
              "available from multiple global modules.");
          }
        }
      }

    }
    else {

      // No idea why this is needed!
      for (auto& var : modFrame->varIdxs) {
        idxs->module->mergedFwdVar.insert(var);
      }
      //for (auto& var : modFrame->mixIdxs) {
      //  idxs->mixIdxs.insert(var);
      //}
      //for (auto var : modFrame->fnIdxs) {
      //  idxs->fnIdxs.insert(var);
      //}

    }

    // Expose what has been forwarded to us
    // for (auto var : root->mergedFwdVar) {
    //   if (!var.first.isPrivate())
    //     idxs->varIdxs.insert(var);
    // }
    // for (auto mix : root->mergedFwdMix) {
    //   if (!mix.first.isPrivate())
    //     idxs->mixIdxs.insert(mix);
    // }
    // for (auto fn : root->mergedFwdFn) {
    //   if (!fn.first.isPrivate())
    //     idxs->fnIdxs.insert(fn);
    // }

    return idxs;

  }

  void Eval::insertModule(Module* module)
  {
    // Nowhere to append to, exit
    if (current == nullptr) return;
    // Nothing to be added yet? Error?
    if (module->compiled == nullptr) return;

    // For imports we always reproduce
//    if (inImport) {
//      // Don't like the recursion, but OK
//      for (auto upstream : module->upstream) {
//        insertModule(upstream);
//      }
//    }

    // The children to be added to the document
    auto& children(module->compiled->elements());
    // Check if we have any parent
    // Meaning we append to the root
    if (!current->parent()) {
      current->concat(children);
      return;
    }
    // Process all children to be added
    // Each one needs to be interweaved
    for (auto& child : children) {
      auto css = child->isaCssStyleRule();
      auto parent = current->isaCssStyleRule();
      if (css && parent) {
        for (auto& inner : css->elements()) {
          // SelectorListObj copy = SASS_MEMORY_COPY(css->selector());
          for (ComplexSelector* selector : css->selector()->elements()) {
            selector->chroots(false);
          }
          SelectorListObj resolved = css->selector()->resolveParentSelectors(
            parent->selector(), compiler, true);
          current->parent()->append(SASS_MEMORY_NEW(CssStyleRule,
            css->pstate(), current, resolved, { inner }));
        }
      }
      else {
        current->append(child);
      }
    }
  }

  void Eval::compileModule(Root* root)
  {

    if (root->isCompiled) return;
    root->isCompiled = true;

    root->compiled = current;
    root->compiled = SASS_MEMORY_NEW(CssStyleRule,
      root->pstate(), nullptr, selectorStack.back());
    auto oldCurrent = current;
    current = root->compiled;

    RAII_PTR(Root, modctx, root);
    EnvRefs* idxs = root->idxs;

    EnvRefs* mframe(compiler.getCurrentModule());

    // Make frame scope active for evaluation
    EnvScope scoped(compiler.varRoot, idxs);

    selectorStack.push_back(nullptr);
    for (auto& child : root->elements()) {
      child->accept(this);
    }
    selectorStack.pop_back();

    current = oldCurrent;

    for (auto& var : mframe->varIdxs) {
      ValueObj& slot(compiler.varRoot.getModVar(var.second));
      if (slot == nullptr) slot = SASS_MEMORY_NEW(Null, root->pstate());
    }

   }

  void Eval::exposeFwdRule(ForwardRule* rule)
  {
    if (rule->wasExposed()) return;
    rule->wasExposed(true);
    // if (!rule->module32()) return;
    mergeForwards(rule->module32()->idxs,
      modctx, rule, compiler);

  }

  void Eval::exposeUseRule(UseRule* rule)
  {

    if (rule->wasExposed()) return;
    rule->wasExposed(true);
    // if (!rule->module32()) return;

    EnvRefs* frame(compiler.getCurrentScope());

    if (rule->module32()->isBuiltIn) {

      if (rule->ns().empty()) {
        frame->forwards.push_back(rule->module32()->idxs);
      }
      else if (frame->module->moduse.count(rule->ns())) {
        throw Exception::ModuleAlreadyKnown(compiler, rule->ns());
      }
      else {
        frame->module->moduse.insert({ rule->ns(),
          { rule->module32()->idxs, nullptr } });
      }

    }
    else if (rule->root47()) {

      pudding(rule->root47()->idxs, rule->ns().empty(), frame);

      if (rule->ns().empty()) {
        // We should pudding when accessing!?
        frame->forwards.push_back(rule->root47()->idxs);
      }
      else {
        // Refactor to only fetch once!
        if (frame->module->moduse.count(rule->ns())) {
          throw Exception::ModuleAlreadyKnown(compiler, rule->ns());
        }

        frame->module->moduse[rule->ns()] =
        { rule->root47()->idxs, rule->root47() };
      }

    }
    else {

      throw "Invalid state!";

    }


  }

  void Eval::exposeImpRule(IncludeImport* rule)
  {

    EnvRefs* pframe = compiler.getCurrentScope();

    while (pframe->isImport) {
      pframe = pframe->pscope;
    }

    EnvRefs* cidxs = rule->root47()->idxs;

    if (!pframe->isInternal) {

      cidxs->module = rule->root47();
      pframe->forwards.insert(
        pframe->forwards.begin(),
        rule->root47()->idxs);

    }
    else {

      // Merge it up through all imports
      for (auto& var : cidxs->varIdxs) {
        if (pframe->varIdxs.count(var.first) == 0) {
          pframe->createVariable(var.first);
        }
      }

      // Merge it up through all imports
      for (auto& fn : cidxs->fnIdxs) {
        if (pframe->fnIdxs.count(fn.first) == 0) {
          pframe->createFunction(fn.first);
        }
      }

      // Merge it up through all imports
      // for (auto& mix : cidxs->mixIdxs) {
      //   if (pframe->mixIdxs.count(mix.first) == 0) {
      //     pframe->createMixin(mix.first);
      //   }
      // }

      // Import to forward
      for (auto& var : rule->root47()->mergedFwdVar) {
        pframe->varIdxs[var.first] = var.second;
      } // a: 18
      for (auto& fn : rule->root47()->mergedFwdFn) {
        pframe->fnIdxs[fn.first] = fn.second;
      }
      for (auto& mix : rule->root47()->mergedFwdMix) {
        pframe->mixIdxs[mix.first] = mix.second;
      }

    }

  }
  // EO exposeImpRule

  void Eval::acceptIncludeImport(IncludeImport* rule)
  {
    BackTrace trace(rule->pstate(), Strings::importRule);
    callStackFrame cframe(logger, trace);
    if (Root* root = loadModRule(rule)) {
      ImportStackFrame iframe(compiler, root->import);
      EnvScope scoped(compiler.varRoot, root->idxs);
      RAII_PTR(Root, modctx, root);
      RAII_FLAG(inImport, true);
      exposeImpRule(rule);
      // Imports are always executed again
      for (const StatementObj& item : root->elements()) {
        item->accept(this);
      }
    }
  }

  Value* Eval::visitUseRule(UseRule* rule)
  {

    BackTrace trace(rule->pstate(), Strings::useRule);
    callStackFrame cframe(logger, trace);

    if (Root* root = loadModRule(rule)) {
      compiler.modctx->upstream.push_back(root);
      if (!root->isCompiled) {
        ImportStackFrame iframe(compiler, root->import);
        LocalOption<bool> scoped(compiler.hasWithConfig,
          compiler.hasWithConfig || rule->hasConfig);
        RAII_PTR(WithConfig, wconfig, rule);
        compileModule(root);
        rule->finalize(compiler);
        insertModule(root);
      }
      else if (inImport) {
        // We must also produce inner modules somehow
        insertModule(root);
      }
      else if (rule->hasConfig) {
        throw Exception::ParserException(compiler,
          "This module was already loaded, so it "
          "can't be configured using \"with\".");
      }
    }

    exposeUseRule(rule);
    return nullptr;
  }

  Value* Eval::visitForwardRule(ForwardRule* rule)
  {

    BackTrace trace(rule->pstate(), Strings::forwardRule, false);
    callStackFrame frame333(logger, trace);

    if (Root* root = loadModRule(rule)) {
      if (!root->isCompiled) {
        ImportStackFrame iframe(compiler, root->import);
        LocalOption<bool> scoped(compiler.hasWithConfig,
          compiler.hasWithConfig || rule->hasConfig);
        RAII_PTR(WithConfig, wconfig, rule);
        compileModule(root);
        rule->finalize(compiler);
        insertModule(root);
      }
      else if (compiler.hasWithConfig || rule->hasConfig) {
        throw Exception::ParserException(compiler,
          "This module was already loaded, so it "
          "can't be configured using \"with\".");
      }
    }

    exposeFwdRule(rule);
    return nullptr;
  }

}
