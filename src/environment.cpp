bool udbg = false;

#include "fn_meta.hpp"

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

#include "debugger.hpp"

namespace Sass {

  // Import some namespaces
  using namespace Charcode;
  using namespace Character;
  using namespace StringUtils;

  Value* Eval::visitAssignRule(AssignRule* a)
  {

    if (!a->withinLoop() && a->vidx2().isValid()) {
      assigne = &compiler.varRoot.getVariable(a->vidx2());
      ValueObj result = a->value()->accept(this);
      compiler.varRoot.setVariable(
        a->vidx2(),
        result,
        false);
      assigne = nullptr;
      return nullptr;
    }

    ValueObj result;

    const EnvKey& vname(a->variable());

    if (a->is_default()) {

      auto frame = compiler.getCurrentFrame();

      // If we have a config and the variable is already set
      // we still overwrite the variable beside being guarded
      WithConfigVar* wconf = nullptr;
      if (compiler.wconfig && frame->varFrame == 0xFFFFFFFF && a->ns().empty()) {
        wconf = wconfig->getCfgVar(vname);
      }
      if (wconf) {
        // Via load-css
        if (wconf->value) {
          result = wconf->value;
        }
        // Via regular load
        else if (wconf->expression) {
          a->value(wconf->expression);
        }
        a->is_default(wconf->isGuarded);
      }
    }

    // Emit deprecation for new var with global flag
    if (a->is_global()) {

      auto rframe = compiler.varRoot.stack[0];
      auto it = rframe->varIdxs.find(a->variable());

      bool hasVar = false;

      if (it != rframe->varIdxs.end()) {
        VarRef vidx(rframe->varFrame, it->second);
        auto& value = compiler.varRoot.getVariable(vidx);
        if (value != nullptr) hasVar = true;
      }

      if (hasVar == false) {
        // libsass/variable-scoping/defaults-global-null
        // This check may not be needed, but we create a
        // superfluous variable slot in the scope
        for (auto fwds : rframe->fwdGlobal55) {
          auto it = fwds->varIdxs.find(a->variable());
          if (it != fwds->varIdxs.end()) {
            VarRef vidx(0xFFFFFFFF, it->second);
            auto& value = compiler.varRoot.getVariable(vidx);
            if (value != nullptr) hasVar = true;
          }

          auto fwd = fwds->module->mergedFwdVar.find(a->variable());
          if (fwd != fwds->module->mergedFwdVar.end()) {
            VarRef vidx(0xFFFFFFFF, fwd->second);
            auto& value = compiler.varRoot.getVariable(vidx);
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

      a->vidx2(compiler.varRoot.setVariable(
        a->variable(),
        a->is_default(),
        a->is_global()));
      assigne = &compiler.varRoot.getVariable(a->vidx2());
      if (!result) result = a->value()->accept(this);
      compiler.varRoot.setVariable(
        a->vidx2(),
        result,
        a->is_default());
      assigne = nullptr;

    }
    else {

      VarRefs* mod = compiler.getCurrentModule();

      auto it = mod->fwdModule55.find(a->ns());
      if (it == mod->fwdModule55.end()) {
        compiler.addFinalStackTrace(a->pstate());
        throw Exception::ModuleUnknown(compiler, a->ns());
      }
      else if (it->second.second && !it->second.second->isCompiled) {
        compiler.addFinalStackTrace(a->pstate());
        throw Exception::ModuleUnknown(compiler, a->ns());
      }

      if (!result) result = a->value()->accept(this);

      a->vidx2(compiler.varRoot.setModVar(
        a->variable(), a->ns(),
        result,
        a->is_default(),
        a->pstate()));

    }

    if (!a->vidx2().isValid())
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

  Value* Eval::visitMixinRule(MixinRule* rule)
  {
    UserDefinedCallableObj callable =
      SASS_MEMORY_NEW(UserDefinedCallable,
        rule->pstate(), rule->name(), rule, nullptr);
    rule->midx(compiler.varRoot.setMixin(
      rule->name(), false, false));
    compiler.varRoot.setMixin(rule->midx(), callable, false);
    return nullptr;
  }


  Value* Eval::visitFunctionRule(FunctionRule* rule)
  {
    UserDefinedCallableObj callable =
      SASS_MEMORY_NEW(UserDefinedCallable,
        rule->pstate(), rule->name(), rule, nullptr);

    rule->fidx(compiler.varRoot.setFunction(
      rule->name(), false, false));

    compiler.varRoot.setFunction(rule->fidx(), callable, false);
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

    // Skip to optional global scope
    VarRefs* frame = global ?
      context.varRoot.stack.front() :
      context.varRoot.stack.back();

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
      if (chroot->isImport || chroot->permeable) {
        chroot = chroot->pscope;
      }
      else {
        break;
      }
    }

    // Check if we have a configuration

    if (ns.empty()) {

      // IF we are semi-global and parent is root
      // And if that root also contains that variable
      // We assign to that instead of a new local one!
      if (!hasVar) {
        frame->createLexicalVar(name);
      }

    }
    // else {
    //   auto it = frame->fwdModule55.find(ns);
    //   if (it == frame->fwdModule55.end()) {
    //     throw "No modulpo";
    //   }
    // }

    AssignRule* declaration = SASS_MEMORY_NEW(AssignRule,
      scanner.relevantSpanFrom(start),
      name, inLoopDirective, ns,
      {}, value, guarded, global);

    // if (inLoopDirective) frame->assignments.push_back(declaration);
    return declaration;
  }

  // Consumes a mixin declaration.
  // [start] should point before the `@`.
  MixinRule* StylesheetParser::readMixinRule(Offset start)
  {

    VarRefs* parent = context.varRoot.stack.back();
    EnvFrame local(context, false);
    // Create space for optional content callable
    // ToDo: check if this can be conditionally done?
    auto cidx = local.idxs->createMixin(Keys::contentRule);
    // var precedingComment = lastSilentComment;
    // lastSilentComment = null;
    sass::string name = readIdentifier();
    scanWhitespace();

    ArgumentDeclarationObj arguments;
    if (scanner.peekChar() == $lparen) {
      arguments = parseArgumentDeclaration();
    }
    else {
      // Dart-sass creates this one too
      arguments = SASS_MEMORY_NEW(ArgumentDeclaration,
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
    LOCAL_FLAG(inMixin, true);
    LOCAL_FLAG(mixinHasContent, false);

    auto pr = parent;
    while (pr->isImport) pr = pr->pscope;
    // Not if we have one forwarded!

    VarRef midx = pr->createLexicalMix(name);
    MixinRule* rule = withChildren<MixinRule>(
      &StylesheetParser::readChildStatement,
      start, name, arguments, local.idxs);
    rule->midx(midx); // to parent
    rule->cidx(cidx);
    return rule;
  }
  // EO _mixinRule

  // Consumes a function declaration.
  // [start] should point before the `@`.
  FunctionRule* StylesheetParser::readFunctionRule(Offset start)
  {
    // Variables should not be hoisted through
    VarRefs* parent = context.varRoot.stack.back();
    EnvFrame local(context, false);

    // var precedingComment = lastSilentComment;
    // lastSilentComment = null;
    sass::string name = readIdentifier();
    sass::string normalized(name);

    scanWhitespace();

    ArgumentDeclarationObj arguments = parseArgumentDeclaration();

    if (inMixin || inContentBlock) {
      error("Mixins may not contain function declarations.",
        scanner.relevantSpanFrom(start));
    }
    else if (inControlDirective) {
      error("Functions may not be declared in control directives.",
        scanner.relevantSpanFrom(start));
    }

    sass::string fname(Util::unvendor(name));
    if (fname == "calc" || fname == "element" || fname == "expression" ||
      fname == "url" || fname == "and" || fname == "or" || fname == "not") {
      error("Invalid function name.",
        scanner.relevantSpanFrom(start));
    }

    scanWhitespace();
    FunctionRule* rule = withChildren<FunctionRule>(
      &StylesheetParser::readFunctionRuleChild,
      start, name, arguments, local.idxs);
    rule->fidx(parent->createLexicalFn(name));
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
    VarRefs* idxs,
    Moduled* module,
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


  Root* Eval::resolveForwardRule(ForwardRule* rule)
  {

    // May not be defined yet
    Module* mod = rule->module();

    // Nothing to be done for built-ins
    if (mod && mod->isBuiltIn) {
      return nullptr;
    }

    // Seems already loaded?
    if (rule->root()) {
      return rule->root();
    }

    // callStackFrame frame(compiler, {
    //   rule->pstate(), Strings::useRule });

    LOCAL_PTR(WithConfig, wconfig, rule);

    // Resolve final file to load
    const ImportRequest request(
      rule->url(), rule->prev(), false);

    // Search for valid imports (e.g. partials) on the file-system
    // Returns multiple valid results for ambiguous import path
    const sass::vector<ResolvedImport>& resolved(
      compiler.findIncludes(request, false));

    // Error if no file to import was found
    if (resolved.empty()) {
      compiler.addFinalStackTrace(rule->pstate());
      throw Exception::UnknwonImport(compiler);
    }
    // Error if multiple files to import were found
    else if (resolved.size() > 1) {
      compiler.addFinalStackTrace(rule->pstate());
      throw Exception::AmbiguousImports(compiler, resolved);
    }

    // This is guaranteed to either load or error out!
    ImportObj loaded = compiler.loadImport(resolved[0]);
    ImportStackFrame iframe(compiler, loaded);
    rule->import(loaded);

    Root* sheet = nullptr;
    sass::string abspath(loaded->getAbsPath());
    auto cached = compiler.sheets.find(abspath);
    if (cached != compiler.sheets.end()) {
      sheet = cached->second;
    }
    else {
      // Permeable seems to have minor negative impact!?
      EnvFrame local(compiler, true, true, false); // correct
      sheet = compiler.registerImport(loaded);
      sheet->import = loaded;
    }

    rule->module(sheet);
    rule->root(sheet);

    return sheet;

  }
  // EO resolveForwardFule

  Root* Eval::resolveIncludeImport(IncludeImport* rule)
  {
    // Seems already loaded?
    if (rule->sheet()) {
      return rule->sheet();
    }

    if (rule->module() && rule->module()->isBuiltIn) {
      return nullptr;
    }


    if (Root* sheet2 = resolveIncludeImport(
      rule->pstate(),
      rule->prev(),
      rule->url()
    )) {
      rule->import(sheet2->import);
      rule->module(sheet2);
      rule->sheet(sheet2);
      return sheet2;
    }

    return nullptr;

  }


  Root* Eval::resolveIncludeImport(
    const SourceSpan& pstate,
    const sass::string& prev,
    const sass::string& url,
    bool scoped)
  {

    // Nothing to be done for built-ins
    // if (mod && mod->isBuiltIn) {
    //   return nullptr;
    // }

    // Seems already loaded?
    // if (rule->sheet()) {
    //   return rule->sheet();
    // }

    // callStackFrame frame(compiler, {
    //   rule->pstate(), Strings::useRule });

    // Resolve final file to load
    const ImportRequest request(
      url, prev, false);

    // Search for valid imports (e.g. partials) on the file-system
    // Returns multiple valid results for ambiguous import path
    const sass::vector<ResolvedImport>& resolved(
      compiler.findIncludes(request, true));

    // Error if no file to import was found
    if (resolved.empty()) {
      compiler.addFinalStackTrace(pstate);
      throw Exception::UnknwonImport(compiler);
    }
    // Error if multiple files to import were found
    else if (resolved.size() > 1) {
      compiler.addFinalStackTrace(pstate);
      throw Exception::AmbiguousImports(compiler, resolved);
    }

    // This is guaranteed to either load or error out!
    ImportObj loaded = compiler.loadImport(resolved[0]);
    ImportStackFrame iframe(compiler, loaded);
    // rule->import(loaded);

    Root* sheet = nullptr;
    sass::string abspath(loaded->getAbsPath());
    auto cached = compiler.sheets.find(abspath);
    if (cached != compiler.sheets.end()) {
      sheet = cached->second;
    }
    else {
      // Permeable seems to have minor negative impact!?
      EnvFrame local(compiler, false, true, !scoped); // correct
      sheet = compiler.registerImport(loaded);
      sheet->idxs = local.idxs;
      sheet->import = loaded;
    }

    // rule->module(sheet);
    // rule->sheet(sheet);

    // wconfig.finalize();
    return sheet;
  }
  Root* Eval::resolveUseRule(UseRule* rule)
  {

    // May not be defined yet
    Module* mod = rule->module();

    // Nothing to be done for built-ins
    if (mod && mod->isBuiltIn) {
      return nullptr;
    }

    // Seems already loaded?
    if (rule->root()) {
      return rule->root();
    }

    // callStackFrame frame(compiler, {
    //   rule->pstate(), Strings::useRule });

    LOCAL_PTR(WithConfig, wconfig, rule);

    // Resolve final file to load
    const ImportRequest request(
      rule->url(), rule->prev(), false);

    // Deduce namespace from url
    sass::string ns(rule->ns());
    const sass::string& url(rule->url());

    // Deduct the namespace from url
    // After last slash before first dot
    if (ns.empty() && !url.empty()) {
      auto start = url.find_last_of("/\\");
      start = (start == NPOS ? 0 : start + 1);
      auto end = url.find_first_of(".", start);
      if (url[start] == '_') start += 1;
      ns = url.substr(start, end);
    }

    // Search for valid imports (e.g. partials) on the file-system
    // Returns multiple valid results for ambiguous import path
    const sass::vector<ResolvedImport>& resolved(
      compiler.findIncludes(request, false));

    // Error if no file to import was found
    if (resolved.empty()) {
      compiler.addFinalStackTrace(rule->pstate());
      throw Exception::UnknwonImport(compiler);
    }
    // Error if multiple files to import were found
    else if (resolved.size() > 1) {
      compiler.addFinalStackTrace(rule->pstate());
      throw Exception::AmbiguousImports(compiler, resolved);
    }

    // This is guaranteed to either load or error out!
    ImportObj loaded = compiler.loadImport(resolved[0]);
    ImportStackFrame iframe(compiler, loaded);
    rule->import(loaded);

    rule->ns(ns == "*" ? "" : ns);

    Root* sheet = nullptr;
    sass::string abspath(loaded->getAbsPath());
    auto cached = compiler.sheets.find(abspath);
    if (cached != compiler.sheets.end()) {
      sheet = cached->second;
    }
    else {
      if (!ns.empty()) {
        VarRefs* modFrame(compiler.getCurrentModule());
        if (modFrame->fwdModule55.count(ns)) {
          throw Exception::ModuleAlreadyKnown(compiler, ns);
        }
      }
      // Permeable seems to have minor negative impact!?
      EnvFrame local(compiler, false, true); // correct
      sheet = compiler.registerImport(loaded);
      sheet->import = loaded;
    }


    rule->module(sheet);
    rule->root(sheet);

    // wconfig.finalize();
    return sheet;


  }

  VarRefs* Eval::pudding(VarRefs* idxs, bool intoRoot, VarRefs* modFrame)
  {

    if (intoRoot) {

      for (auto& var : idxs->varIdxs) {
        auto it = modFrame->varIdxs.find(var.first);
        if (it != modFrame->varIdxs.end()) {
          if (var.first.isPrivate()) continue;
          if (var.second == it->second) continue;

          ValueObj& slot = compiler.varRoot.getVariable({ 0xFFFFFFFF, it->second });
          if (slot == nullptr || slot->isaNull()) continue;

          throw Exception::ParserException(compiler,
            "This module and the new module both define a "
            "variable named \"$" + var.first.norm() + "\".");
        }
      }
      for (auto& mix : idxs->mixIdxs) {
        auto it = modFrame->mixIdxs.find(mix.first);
        if (it != modFrame->mixIdxs.end()) {
          if (mix.first.isPrivate()) continue;
          if (mix.second == it->second) continue;
          CallableObj& slot = compiler.varRoot.getMixin({ 0xFFFFFFFF, it->second });
          if (slot == nullptr) continue;
          throw Exception::ParserException(compiler,
            "This module and the new module both define a "
            "mixin named \"" + mix.first.norm() + "\".");
        }
      }
      for (auto& fn : idxs->fnIdxs) {
        auto it = modFrame->fnIdxs.find(fn.first);
        if (it != modFrame->fnIdxs.end()) {
          if (fn.first.isPrivate()) continue;
          if (fn.second == it->second) continue;
          CallableObj& slot = compiler.varRoot.getFunction({ 0xFFFFFFFF, it->second });
          if (slot == nullptr) continue;
          throw Exception::ParserException(compiler,
            "This module and the new module both define a "
            "function named \"" + fn.first.norm() + "\".");
        }
      }

      // Check if we push the same stuff twice
      for (auto fwd : modFrame->fwdGlobal55) {
        if (idxs == fwd) continue;
        for (auto& var : idxs->varIdxs) {
          auto it = fwd->varIdxs.find(var.first);
          if (it != fwd->varIdxs.end()) {
            if (var.second == it->second) continue;
            throw Exception::ParserException(compiler,
              "$" + var.first.norm() + " is available "
              "from multiple global modules.");
          }
        }
        for (auto& var : idxs->mixIdxs) {
          auto it = fwd->mixIdxs.find(var.first);
          if (it != fwd->mixIdxs.end()) {
            if (var.second == it->second) continue;
            throw Exception::ParserException(compiler,
              "Mixin \"" + var.first.norm() + "(...)\" is "
              "available from multiple global modules.");
          }
        }
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
      for (auto& var : modFrame->mixIdxs) {
        idxs->mixIdxs.insert(var);
      }
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

  void Eval::insertModule(Moduled* module)
  {
    // Nowhere to append to, exit
    if (current == nullptr) return;
    // Nothing to be added yet? Error?
    if (module->loaded == nullptr) return;
    // The children to be added to the document
    auto& children(module->loaded->elements());
    // Check if we have any parent
    // Meaning we append to the root
    if (!current->parent()) {
      auto& target(current->elements());
      target.insert(target.end(),
        children.begin(),
        children.end());
      return;
    }
    // Process all children to be added
    // Each one needs to be interweaved
    for (auto& child : children) {
      auto css = child->isaCssStyleRule();
      auto parent = current->isaCssStyleRule();
      if (css && parent) {
        for (auto& inner : css->elements()) {
          auto copy = SASS_MEMORY_COPY(css->selector());
          for (ComplexSelector* selector : copy->elements()) {
            selector->chroots(false);
          }
          SelectorListObj resolved = copy->resolveParentSelectors(
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

  Root* Eval::loadModule(Compiler& compiler, Import* loaded, bool hasWith)
  {
    // First check if the module was already loaded
    auto it = compiler.sheets.find(loaded->getAbsPath());
    if (it != compiler.sheets.end()) {
      // Don't allow to reconfigure once loaded
      if (hasWith && compiler.implicitWithConfig) {
        throw Exception::ParserException(compiler,
          sass::string(loaded->getImpPath())
          + " was already loaded, so it "
          "can\'t be configured using \"with\".");
      }
      // Return cached stylesheet
      return it->second;
    }
    // BuiltInMod is created within a new scope
    EnvFrame local(compiler, false, true); 
    // eval.selectorStack.push_back(nullptr);
    // ImportStackFrame iframe(compiler, loaded);
    Root* sheet = compiler.registerImport(loaded);
    // eval.selectorStack.pop_back();
    sheet->idxs = local.idxs;
    sheet->import = loaded;
    return sheet;
  }

  void Eval::compileModule(Root* root)
  {

    if (root->isCompiled) return;
    root->isCompiled = true;

    root->loaded = current;
    root->loaded = SASS_MEMORY_NEW(CssStyleRule,
      root->pstate(), nullptr, selectorStack.back());
    auto oldCurrent = current;
    current = root->loaded;

    LOCAL_PTR(Root, chroot77, root);
    VarRefs* idxs = root->idxs;

    VarRefs* mframe(compiler.varRoot.stack.back()->getModule23());

    EnvScope scoped2(compiler.varRoot, idxs);

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
    if (!rule->module()) return;
    if (rule->wasMerged()) return;
    rule->wasMerged(true);

    mergeForwards(rule->module()->idxs, chroot77, rule, compiler);

  }

  void Eval::exposeUseRule(UseRule* rule)
  {

    if (!rule->module()) return;
    if (rule->wasExported()) return;
    rule->wasExported(true);

    VarRefs* frame(compiler.getCurrentFrame());

    if (rule->module()->isBuiltIn) {

      if (rule->ns().empty()) {
        frame->fwdGlobal55.push_back(rule->module()->idxs);
        rule->wasExported(true);
      }
      else if (frame->fwdModule55.count(rule->ns())) {
        compiler.addFinalStackTrace(rule->pstate());
        throw Exception::ModuleAlreadyKnown(compiler, rule->ns());
      }
      else {
        frame->fwdModule55.insert({ rule->ns(),
          { rule->module()->idxs, nullptr } });
        rule->wasExported(true);
      }

    }
    else if (rule->root()) {

      pudding(rule->root()->idxs, rule->ns().empty(), frame);

      if (rule->ns().empty()) {
        // We should pudding when accessing!?
        frame->fwdGlobal55.push_back(rule->root()->idxs);
      }
      else {
        // Refactor to only fetch once!
        if (frame->fwdModule55.count(rule->ns())) {
          throw Exception::ModuleAlreadyKnown(compiler, rule->ns());
        }

        frame->fwdModule55[rule->ns()] =
        { rule->root()->idxs, rule->root() };
      }

    }
    else {

      throw "Invalid state!";

    }


  }

  void Eval::exposeImpRule(IncludeImport* rule, VarRefs* pframe2)
  {

    VarRefs* pframe = compiler.getCurrentFrame();


    while (pframe->isImport) {
      pframe = pframe->pscope;
    }

    // Merge it up through all imports
    for (auto& var : rule->sheet()->idxs->varIdxs) {
      auto it = pframe->varIdxs.find(var.first);
      if (it == pframe->varIdxs.end()) {
        if (pframe->isCompiled) {
          // throw "Can't create on active frame";
          compiler.varRoot.variables.push_back({});
        }
        // std::cerr << "EXPORT " << var.first.norm() << "\n";
        pframe->createVariable(var.first);
      }
    }

    // Merge it up through all imports
    for (auto& fn : rule->sheet()->idxs->fnIdxs) {
      auto it = pframe->fnIdxs.find(fn.first);
      if (it == pframe->fnIdxs.end()) {
        if (pframe->isCompiled) {
          // throw "Can't create on active frame";
          compiler.varRoot.functions.push_back({});
        }
        // std::cerr << "EXPORT " << var.first.norm() << "\n";
        pframe->createFunction(fn.first);
      }
    }

    // Merge it up through all imports
    for (auto& mix : rule->sheet()->idxs->mixIdxs) {
      auto it = pframe->mixIdxs.find(mix.first);
      if (it == pframe->mixIdxs.end()) {
        if (pframe->isCompiled) {
          // throw "Can't create on active frame";
          compiler.varRoot.mixins.push_back({});
        }
        // std::cerr << "EXPORT " << var.first.norm() << "\n";
        pframe->createMixin(mix.first);
      }
    }


    if (pframe->varFrame != 0xFFFFFFFF) {

      if (udbg) std::cerr << "Importing into parent frame '" << rule->url() << "' "
        << compiler.implicitWithConfig << "\n";

      rule->sheet()->idxs->module = rule->sheet();
      pframe->fwdGlobal55.insert(
        pframe->fwdGlobal55.begin(),
        rule->sheet()->idxs);

    }

  }

  void Eval::acceptIncludeImport(IncludeImport* rule)
  {

    if (udbg) std::cerr << "Visit import rule '" << rule->url() << "' "
      << compiler.implicitWithConfig << "\n";

    callStackFrame cframe(traces, BackTrace(
      rule->pstate(), Strings::importRule));
    Root* sheet = resolveIncludeImport(rule);
    rule->sheet(sheet);

    VarRefs* pframe = compiler.getCurrentFrame();

    // Imports are always executed again
    Preloader preproc(*this, sheet);
    preproc.acceptRoot(sheet);

    // Add C-API to stack to expose it
    ImportStackFrame iframe(compiler, sheet->import);
    EnvScope scoped(compiler.varRoot, sheet->idxs);
    LOCAL_PTR(Root, chroot77, sheet);

    while (pframe->isImport) {
      pframe = pframe->pscope;
    }

    // Skip over all imports
    // We are doing it out of order

    exposeImpRule(rule, pframe);



    // Imports are always executed again
    for (const StatementObj& item : sheet->elements()) {
      item->accept(this);
    }

    if (udbg) std::cerr << "Compiled import rule '" << rule->url() << "' "
      << compiler.implicitWithConfig << "\n";


    if (pframe->varFrame == 0xFFFFFFFF) {

      // Import to forward
      for (auto& asd : sheet->mergedFwdVar) {
        if (udbg) std::cerr << "  merged var " << asd.first.orig() << "\n";
        pframe->varIdxs.insert(asd);
      } // a: 18
      for (auto& asd : sheet->mergedFwdMix) {
        if (udbg) std::cerr << "  merged mix " << asd.first.orig() << "\n";
        pframe->mixIdxs[asd.first] = asd.second; }
      for (auto& asd : sheet->mergedFwdFn) {
        if (udbg) std::cerr << "  merged fn " << asd.first.orig() << "\n";
        pframe->fnIdxs[asd.first] = asd.second; }

    }


  }
  Value* Eval::visitUseRule(UseRule* rule)
  {

    BackTrace trace(rule->pstate(), Strings::useRule, false);
    callStackFrame frame(logger456, trace);

    if (udbg) std::cerr << "Visit use rule '" << rule->url() << "' "
      << rule->hasConfig << " -> " << compiler.implicitWithConfig << "\n";

    if (Root* root = resolveUseRule(rule)) {
      if (!root->isCompiled) {
        ImportStackFrame iframe(compiler, root->import);
        LocalOption<bool> scoped(compiler.implicitWithConfig,
          compiler.implicitWithConfig || rule->hasConfig);
        LOCAL_PTR(WithConfig, wconfig, rule);
        compileModule(root);
        rule->finalize(compiler);
        if (udbg) std::cerr << "Compiled use rule '" << rule->url() << "'\n";
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
    callStackFrame frame333(logger456, trace);

    if (udbg) std::cerr << "Visit forward rule '" << rule->url() << "' "
      << rule->hasConfig << " -> " << compiler.implicitWithConfig << "\n";

    if (Root* root = resolveForwardRule(rule)) {
      if (!root->isCompiled) {
        ImportStackFrame iframe(compiler, root->import);
        LocalOption<bool> scoped(compiler.implicitWithConfig,
          compiler.implicitWithConfig || rule->hasConfig);
        LOCAL_PTR(WithConfig, wconfig, rule);
        compileModule(root);
        rule->finalize(compiler);
        if (udbg) std::cerr << "Compiled forward rule '" << rule->url() << "'\n";
        insertModule(root);
      }
      else if (compiler.implicitWithConfig || rule->hasConfig) {
        throw Exception::ParserException(compiler,
          "This module was already loaded, so it "
          "can't be configured using \"with\".");
      }
    }

    exposeFwdRule(rule);
    return nullptr;
  }

  namespace Functions {

    /////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////

    namespace Meta {


      BUILT_IN_FN(loadCss)
      {
        String* url = arguments[0]->assertStringOrNull(compiler, Strings::url);
        Map* withMap = arguments[1]->assertMapOrNull(compiler, Strings::with);

        bool hasWith = withMap && !withMap->empty();

        if (udbg) std::cerr << "Visit load-css '" << url->value() << "' "
          << hasWith << " -> " << compiler.implicitWithConfig << "\n";

        EnvKeyFlatMap<ValueObj> config;
        sass::vector<WithConfigVar> withConfigs;

        if (hasWith) {
          for (auto& kv : withMap->elements()) {
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


        if (StringUtils::startsWith(url->value(), "sass:", 5)) {

          if (hasWith) {
            throw Exception::RuntimeException(compiler, "Built-in "
              "module " + url->value() + " can't be configured.");
          }

          return SASS_MEMORY_NEW(Null, pstate);
        }

        WithConfig wconfig(compiler.wconfig, withConfigs, hasWith);

        WithConfig*& pwconfig(compiler.wconfig);
        LOCAL_PTR(WithConfig, pwconfig, &wconfig);

        sass::string prev(pstate.getAbsPath());
        if (Root* sheet = eval.resolveIncludeImport(
          pstate, prev, url->value(), true)) {
          if (!sheet->isCompiled) {
            ImportStackFrame iframe(compiler, sheet->import);
            LocalOption<bool> scoped(compiler.implicitWithConfig,
              compiler.implicitWithConfig || hasWith);
            eval.compileModule(sheet);
            wconfig.finalize(compiler);
          }
          else if (compiler.implicitWithConfig || hasWith) {
            throw Exception::ParserException(compiler,
              sass::string(sheet->pstate().getImpPath())
              + " was already loaded, so it "
              "can't be configured using \"with\".");
          }
          eval.insertModule(sheet);
        }

        return SASS_MEMORY_NEW(Null, pstate);
      }

    }
  }
}
