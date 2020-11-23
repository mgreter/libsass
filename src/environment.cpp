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

    // We always must have at least one variable
    // SASS_ASSERT(a->vidxs().empty(), "Invalid VIDX");

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
          auto it = fwds->varIdxs.find(a->variable());
          if (it != fwds->varIdxs.end()) {
            VarRef vidx(0xFFFFFFFF, it->second);
            auto value = compiler.varRoot.getVariable(vidx);
            if (value != nullptr) hasVar = true;
          }

          auto fwd = fwds->module->mergedFwdVar.find(a->variable());
          if (fwd != fwds->module->mergedFwdVar.end()) {
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

      a->vidx2(compiler.varRoot.setVariable(
        a->variable(),
        a->is_default(),
        a->is_global()));
      assigne = &compiler.varRoot.getVariable(a->vidx2());
      ValueObj result = a->value()->accept(this);
      compiler.varRoot.setVariable(
        a->vidx2(),
        result,
        a->is_default());
      assigne = nullptr;

    }
    else {

      ValueObj result = a->value()->accept(this);
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

    VarRefs* module(context.getCurrentModule());
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

    if (chroot->varFrame == 0xFFFFFFFF) {

      // Assign a default
      if (guarded && ns.empty()) {
        auto cfgvar = context.getCfgVar(name, true, false);
        if (!cfgvar || cfgvar->isNull) cfgvar = context.getCfgVar(name, false, false);

        if (cfgvar) {
          guarded = cfgvar->isGuarded;
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

    if (ns.empty()) {

      // IF we are semi-global and parent is root
      // And if that root also contains that variable
      // We assign to that instead of a new local one!
      if (!hasVar) {
        frame->createLexicalVar(name);
      }

    }

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

    VarRef fidx = pr->createLexicalMix(name);
    MixinRule* rule = withChildren<MixinRule>(
      &StylesheetParser::readChildStatement,
      start, name, arguments, local.idxs);
    rule->midx(fidx); // to parent
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

  void exposeFiltered(
    VidxEnvKeyMap& merged,
    VidxEnvKeyMap expose,
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

  void mergeForwards(
    VarRefs* idxs,
    Moduled* module,
    bool isShown,
    bool isHidden,
    const sass::string prefix,
    const std::set<EnvKey>& toggledVariables,
    const std::set<EnvKey>& toggledCallables,
    Logger& logger)
  {

    // Only should happen if forward was found in root stylesheet
    // Doesn't make much sense as there is nowhere to forward to
    if (idxs->module != nullptr) {
      // This is needed to support double forwarding (ToDo - need filter, order?)
      for (auto entry : idxs->module->mergedFwdVar) { module->mergedFwdVar.insert(entry); }
      for (auto entry : idxs->module->mergedFwdMix) { module->mergedFwdMix.insert(entry); }
      for (auto entry : idxs->module->mergedFwdFn) { module->mergedFwdFn.insert(entry); }
    }

    if (isShown) {
      exposeFiltered(module->mergedFwdVar, idxs->varIdxs, prefix, toggledVariables, "variable named $", logger, true);
      exposeFiltered(module->mergedFwdMix, idxs->mixIdxs, prefix, toggledCallables, "mixin named ", logger, true);
      exposeFiltered(module->mergedFwdFn, idxs->fnIdxs, prefix, toggledCallables, "function named ", logger, true);
    }
    else if (isHidden) {
      exposeFiltered(module->mergedFwdVar, idxs->varIdxs, prefix, toggledVariables, "variable named $", logger, false);
      exposeFiltered(module->mergedFwdMix, idxs->mixIdxs, prefix, toggledCallables, "mixin named ", logger, false);
      exposeFiltered(module->mergedFwdFn, idxs->fnIdxs, prefix, toggledCallables, "function named ", logger, false);
    }
    else {
      exposeFiltered(module->mergedFwdVar, idxs->varIdxs, prefix, "variable named $", logger);
      exposeFiltered(module->mergedFwdMix, idxs->mixIdxs, prefix, "mixin named ", logger);
      exposeFiltered(module->mergedFwdFn, idxs->fnIdxs, prefix, "function named ", logger);
    }

  }


  Root* Eval::resolveForwardRule(ForwardRule* rule)
  {

    SourceSpan pstate(rule->pstate());
    const ImportRequest import(rule->url(), rule->prev(), false);

    // Search for valid imports (e.g. partials) on the file-system
    // Returns multiple valid results for ambiguous import path
    const sass::vector<ResolvedImport>& resolved(compiler.findIncludes(import, false));

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
        msg_stream << "  " << resolved[i].imp_path << "\n";
      throw Exception::ParserException(compiler, msg_stream.str());
    }

    // We made sure exactly one entry was found, load its content
    // This only loads the requested source file, does not parse it
    if (ImportObj loaded = compiler.loadImport(resolved[0])) {
      ImportStackFrame iframe(compiler, loaded);
      Root* sheet = nullptr;
      auto cached = compiler.sheets.find(loaded->getAbsPath());
      if (cached != compiler.sheets.end()) {
        // Check if with is given, error
        sheet = cached->second;
        rule->root(sheet);
        return nullptr;
      }
      // Verified this must be permeable for now
      EnvFrame local(compiler, true, true, false); // forward
      sheet = compiler.registerImport(loaded);
      sheet->import = loaded;
      rule->root(sheet);
      return sheet;
    }
    compiler.addFinalStackTrace(pstate);
    throw Exception::ParserException(compiler,
      "Couldn't read stylesheet for import.");
  }
  // EO resolveForwardFule

  Root* Eval::resolveUseRule(UseRule* rule)
  {

    // Deduct namespace from url
    sass::string ns(rule->ns());
    sass::string url(rule->url());

    const ImportRequest import(rule->url(), rule->prev(), false);

    bool hasCached = false;

    // Deduct the namespace from url
    // After last slash before first dot
    if (ns.empty() && !url.empty()) {
      auto start = url.find_last_of("/\\");
      start = (start == NPOS ? 0 : start + 1);
      auto end = url.find_first_of(".", start);
      if (url[start] == '_') start += 1;
      ns = url.substr(start, end);
    }

    VarRefs* modFrame(compiler.varRoot.stack.back()->getModule23());

    // Search for valid imports (e.g. partials) on the file-system
    // Returns multiple valid results for ambiguous import path
    const sass::vector<ResolvedImport>& resolved(
      compiler.findIncludes(import, false));

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

    Root* sheet = nullptr;
    sass::string abspath(loaded->getAbsPath());
    auto cached = compiler.sheets.find(abspath);
    if (cached != compiler.sheets.end()) {

      hasCached = true;

      // Check if with is given, error
      sheet = cached->second;

    }
    else {

      if (!ns.empty()) {
        if (modFrame->fwdModule55.count(ns)) {
          throw Exception::ModuleAlreadyKnown(compiler, ns);
        }
      }
      // Permeable seems to have minor negative impact!?
      EnvFrame local(compiler, false, true); // correct
      sheet = compiler.registerImport(loaded);
      compiler.varRoot.finalizeScopes();
      sheet->import = loaded;
    }


    rule->root(sheet);

    rule->ns(ns == "*" ? "" : ns);

    if (hasCached) return nullptr;
    // wconfig.finalize();
    return sheet;


  }

  VarRefs* Eval::pudding(Moduled* root, bool intoRoot, VarRefs* modFrame)
  {

    VarRefs* idxs = root->idxs;

    if (intoRoot) {

      for (auto var : idxs->varIdxs) {
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
      for (auto mix : idxs->mixIdxs) {
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
      for (auto fn : idxs->fnIdxs) {
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
        for (auto var : idxs->varIdxs) {
          auto it = fwd->varIdxs.find(var.first);
          if (it != fwd->varIdxs.end()) {
            if (var.second == it->second) continue;
            throw Exception::ParserException(compiler,
              "$" + var.first.norm() + " is available "
              "from multiple global modules.");
          }
        }
        for (auto var : idxs->mixIdxs) {
          auto it = fwd->mixIdxs.find(var.first);
          if (it != fwd->mixIdxs.end()) {
            if (var.second == it->second) continue;
            throw Exception::ParserException(compiler,
              "Mixin \"" + var.first.norm() + "(...)\" is "
              "available from multiple global modules.");
          }
        }
        for (auto var : idxs->fnIdxs) {
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
      for (auto var : modFrame->varIdxs) {
        idxs->module->mergedFwdVar.insert(var);
      }
      for (auto var : modFrame->mixIdxs) {
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
    for (auto child : children) {
      auto css = child->isaCssStyleRule();
      auto parent = current->isaCssStyleRule();
      if (css && parent) {
        for (auto inner : css->elements()) {
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
    ImportStackFrame iframe(compiler, loaded);
    Root* sheet = compiler.registerImport(loaded);
    // eval.selectorStack.pop_back();
    sheet->idxs = local.idxs;
    sheet->import = loaded;
    return sheet;
  }

  void Eval::compileModule(Root* root)
  {

    root->isCompiled = true;
    root->loaded = current;
    root->loaded = SASS_MEMORY_NEW(CssStyleRule,
      root->pstate(), nullptr, selectorStack.back());
    auto oldCurrent = current;
    current = root->loaded;

    auto& currentRoot(compiler.currentRoot);
    LOCAL_PTR(Root, currentRoot, root);
    VarRefs* idxs = root->idxs;

    VarRefs* mframe(compiler.varRoot.stack.back()->getModule23());

    EnvScope scoped2(compiler.varRoot, idxs);

    ImportStackFrame iframe(compiler, root->import);

    selectorStack.push_back(nullptr);
    for (auto child : root->elements()) {
      child->accept(this);
    }
    selectorStack.pop_back();

    current = oldCurrent;

    for (auto var : mframe->varIdxs) {
      ValueObj& slot(compiler.varRoot.getModVar(var.second));
      if (slot == nullptr) slot = SASS_MEMORY_NEW(Null, root->pstate());
    }

  }

  Value* Eval::visitUseRule(UseRule* rule)
  {

    BackTrace trace(rule->pstate(), Strings::useRule, false);
    callStackFrame frame(logger456, trace);

    if (udbg) std::cerr << "Visit use rule '" << rule->url() << "' "
      << rule->hasLocalWith() << " -> " << compiler.implicitWithConfig << "\n";

    // The show or hide config also hides these
    WithConfig wconfig(compiler, rule->config(), rule->hasLocalWith());

    LocalOption<bool> scoped(compiler.implicitWithConfig,
      compiler.implicitWithConfig || rule->hasLocalWith());

    VarRefs* mframe(compiler.getCurrentModule());

    if (rule->needsLoading()) {
      if (Root* module = resolveUseRule(rule)) {
        rule->root(module);
        rule->needsLoading(false);
      }
      else {

        if (rule->hasLocalWith() && compiler.implicitWithConfig) {
          throw Exception::ParserException(compiler,
            "This module was already loaded, so it "
            "can't be configured using \"with\".");
        }
      }
    }

    if (rule->root()) {

      Root* root = rule->root();

      if (!root->isCompiled) {

        compileModule(root);
        if (udbg) std::cerr << "Compiled use rule '" << rule->url() << "'\n";
        insertModule(root);
      }

      root->exposing = pudding(root, rule->ns().empty(), mframe);

      if (rule->ns().empty()) {
        root->exposing->module = root;
        mframe->fwdGlobal55.push_back(root->exposing);
      }
      else {
        mframe->fwdModule55[rule->ns()] =
        { root->exposing, root };
      }

    }

    wconfig.finalize();
    return nullptr;
  }

  Value* Eval::visitForwardRule(ForwardRule* rule)
  {

    BackTrace trace(rule->pstate(), Strings::forwardRule, false);
    callStackFrame frame333(logger456, trace);

    if (udbg) std::cerr << "Visit forward rule '" << rule->url() << "' "
      << rule->hasLocalWith() << " -> " << compiler.implicitWithConfig << "\n";

    // The show or hide config also hides these
    WithConfig wconfig(compiler, rule->config(), rule->hasLocalWith(),
      rule->isShown(), rule->isHidden(), rule->toggledVariables(), rule->prefix());

    LocalOption<bool> scoped(compiler.implicitWithConfig,
      compiler.implicitWithConfig || rule->hasLocalWith());

    // Flag for dynamic modules vs builtins
    if (rule->needsLoading()) {
      if (auto sheet = resolveForwardRule(rule)) {
        rule->root(sheet);
        rule->needsLoading(false);
      }
      else {
        if (compiler.implicitWithConfig) {
          throw Exception::ParserException(compiler,
            "This module was already loaded, so it "
            "can't be configured using \"with\".");
        }

        if (udbg) std::cerr << "Cached forward rule '" << rule->url() << "'\n";

      }
    }

    if (rule->root()) {
      Root* root = rule->root();
      if (!root->isCompiled) {
        compileModule(rule->root());
        if (udbg) std::cerr << "Compiled forward rule '" << rule->url() << "'\n";
        insertModule(root);
      }
    }

    // Must release some scope first
    if (rule->root()) {
      mergeForwards(rule->root()->idxs, compiler.currentRoot, rule->isShown(), rule->isHidden(),
        rule->prefix(), rule->toggledVariables(), rule->toggledCallables(), compiler);
    }

    wconfig.finalize();
    return nullptr;
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

      BuiltInMod* module(context.getModule(name));

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

        module->idxs->module = nullptr;
        current->fwdGlobal55.push_back(module->idxs);

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
    rule->needsLoading(true);
    return rule.detach();
  }


  void StylesheetParser::scanImportArgument(ImportRule* rule)
  {
    const char* startpos = scanner.position;
    Offset start(scanner.offset);
    uint8_t next = scanner.peekChar();
    if (next == $u || next == $U) {
      Expression* url = readFunctionOrStringExpression();
      scanWhitespace();
      auto queries = tryImportQueries();
      rule->append(SASS_MEMORY_NEW(StaticImport,
        scanner.relevantSpanFrom(start),
        SASS_MEMORY_NEW(Interpolation,
          url->pstate(), url),
        queries.first, queries.second));
      return;
    }

    sass::string url = string();
    const char* rawUrlPos = scanner.position;
    SourceSpan pstate = scanner.relevantSpanFrom(start);
    scanWhitespace();
    auto queries = tryImportQueries();
    if (isPlainImportUrl(url) || queries.first != nullptr || queries.second != nullptr) {
      // Create static import that is never
      // resolved by libsass (output as is)
      rule->append(SASS_MEMORY_NEW(StaticImport,
        scanner.relevantSpanFrom(start),
        SASS_MEMORY_NEW(Interpolation, pstate,
          SASS_MEMORY_NEW(String, pstate,
            sass::string(startpos, rawUrlPos))),
        queries.first, queries.second));
    }
    // Otherwise return a dynamic import
    // Will resolve during the eval stage
    else {
      // Check for valid dynamic import
      if (inControlDirective || inMixin) {
        throwDisallowedAtRule(rule->pstate().position);
      }

      rule->append(SASS_MEMORY_NEW(IncludeImport,
        scanner.relevantSpanFrom(start), scanner.sourceUrl, url, nullptr));

    }

  }

  // Resolve import of [path] and add imports to [rule]
  Root* Eval::resolveDynamicImport(IncludeImport* rule)
  {
    SourceSpan pstate(rule->pstate());
    const ImportRequest request(rule->url(), rule->prev(), true);
    callStackFrame frame(compiler, { pstate, Strings::importRule });


    // Search for valid imports (e.g. partials) on the file-system
    // Returns multiple valid results for ambiguous import path
    // This should be cached somehow to improve speed by much!

    if (rule->candidates == nullptr) {
      rule->candidates = &compiler.findIncludes(request, true);
    }
    const sass::vector<ResolvedImport>& resolved(*rule->candidates);

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
      // IsModule seems to have minor impacts here
      // IsImport vs permeable seems to be the same!?
      EnvFrame local(compiler, false, true, true); // Verified (import)
      ImportStackFrame iframe(compiler, loaded);
      Root* sheet = compiler.registerImport(loaded);


      // debug_ast(sheet);

      compiler.varRoot.finalizeScopes();
      return sheet;
    }

    compiler.addFinalStackTrace(pstate);
    throw Exception::ParserException(compiler,
      "Couldn't read stylesheet for import.");

  }
  // EO resolveDynamicImport

  void Eval::acceptIncludeImport(IncludeImport* rule)
  {

    if (udbg) std::cerr << "Visit import rule '" << rule->url() << "' "
      << compiler.implicitWithConfig << "\n";

    // Get the include loaded by parser
    // const ResolvedImport& include(rule->include());

    // This will error if the path was not loaded before
    // ToDo: Why don't we attach the sheet to include itself?
    // rule->sheet();

    // Create C-API exposed object to query
    //struct SassImport import{
    //   sheet.syntax, sheet.source, ""
    //};

          // Call custom importers and check if any of them handled the import
      // if (!context.callCustomImporters(url, pstate, rule)) {
        // Try to load url into context.sheets
    RootObj sheet = rule->sheet();

    if (sheet.isNull()) {
      sheet = resolveDynamicImport(rule);
      compiler.varRoot.finalizeScopes();
    }

    // debug_ast(sheet);

        // }
    auto vframe = compiler.getCurrentFrame();

    // Skip over all imports
    // We are doing it out of order
    while (vframe) {
      // Merge it up through all imports
      for (auto& var : sheet->idxs->varIdxs) {
        auto it = vframe->varIdxs.find(var.first);
        if (it == vframe->varIdxs.end()) {
          if (vframe->isCompiled) {
            // throw "Can't create on active frame";
          }
          compiler.varRoot.variables.push_back({});
          // std::cerr << "EXPORT " << var.first.norm() << "\n";
          vframe->createVariable(var.first);
        }
      }

      if (!vframe->isImport) break;
      vframe = vframe->pscope;
    }



    // Add C-API to stack to expose it
    ImportStackFrame iframe(compiler, sheet->import);

    callStackFrame frame(traces,
      BackTrace(rule->pstate(), Strings::importRule));

    VarRefs* pframe = compiler.varRoot.stack.back();
    EnvScope scoped(compiler.varRoot, sheet->idxs);

    // debug_ast(sheet);

    VarRefs* refs = sheet->idxs;

    auto& currentRoot(compiler.currentRoot);
    LOCAL_PTR(Root, currentRoot, sheet);

    // Imports are always executed again
    for (const StatementObj& item : sheet->elements()) {
      item->accept(this);
    }

    if (udbg) std::cerr << "Compiled import rule '" << rule->url() << "' "
      << compiler.implicitWithConfig << "\n";

    while (pframe->isImport) pframe = pframe->pscope;

    if (pframe->varFrame == 0xFFFFFFFF) {

      if (udbg) std::cerr << " import into global frame '" << rule->url() << "'\n";

      // Global can simply be exposed without further ado (same frame)
      for (auto asd : sheet->idxs->varIdxs) {
        if (udbg) std::cerr << "  var " << asd.first.orig() << "\n";
        pframe->varIdxs.insert(asd);
      }
      for (auto asd : sheet->idxs->mixIdxs) {
        if (udbg) std::cerr << "  mix " << asd.first.orig() << "\n";
        pframe->mixIdxs.insert(asd); }
      for (auto asd : sheet->idxs->fnIdxs) {
        if (udbg) std::cerr << "  fn " << asd.first.orig() << "\n";
        pframe->fnIdxs.insert(asd); }

      for (auto asd : sheet->mergedFwdVar) {
        if (udbg) std::cerr << "  merged var " << asd.first.orig() << "\n";
        pframe->varIdxs.insert(asd);
      } // a: 18
      for (auto asd : sheet->mergedFwdMix) {
        if (udbg) std::cerr << "  merged mix " << asd.first.orig() << "\n";
        pframe->mixIdxs[asd.first] = asd.second; }
      for (auto asd : sheet->mergedFwdFn) {
        if (udbg) std::cerr << "  merged fn " << asd.first.orig() << "\n";
        pframe->fnIdxs[asd.first] = asd.second; }

    }
    else {

      if (udbg) std::cerr << "Importing into parent frame '" << rule->url() << "' "
        << compiler.implicitWithConfig << "\n";

      sheet->idxs->module = sheet;
      pframe->fwdGlobal55.insert(
        pframe->fwdGlobal55.begin(),
        sheet->idxs);

    }

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
      if (BuiltInMod* module = context.getModule(name)) {

        mergeForwards(module->idxs, context.currentRoot, isShown, isHidden,
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
    rule->needsLoading(true);
    return rule.detach();
  }

  namespace Functions {

    /////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////

    namespace Meta {


      BUILT_IN_FN(loadCss)
      {
        String* url = arguments[0]->assertStringOrNull(compiler, Strings::url);
        Map* withMap = arguments[1]->assertMapOrNull(compiler, Strings::with);
        // auto name = SASS_MEMORY_NEW(CssString, pstate, "added");
        // auto value = SASS_MEMORY_NEW(String, pstate, "yeppa mixin");
        // auto decl = SASS_MEMORY_NEW(CssDeclaration, pstate, name, value);

        bool hasWith = withMap && !withMap->empty();

        if (udbg) std::cerr << "Visit load-css '" << url->value() << "' "
          << hasWith << " -> " << compiler.implicitWithConfig << "\n";

        LocalOption<bool> scoped(compiler.implicitWithConfig,
          compiler.implicitWithConfig || hasWith);

        EnvKeyFlatMap<ValueObj> config;
        sass::vector<WithConfigVar> withConfigs;
        if (hasWith) {
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

        WithConfig wconfig(compiler, withConfigs, hasWith);

        if (StringUtils::startsWith(url->value(), "sass:", 5)) {

          if (hasWith) {
            throw Exception::RuntimeException(compiler, "Built-in "
              "module " + url->value() + " can't be configured.");
          }

          return SASS_MEMORY_NEW(Null, pstate);
        }

        Import* loaded = compiler.import_stack.back();

        // Loading relative to where the function was included
        const ImportRequest import(url->value(), pstate.getAbsPath(), false);
        // Search for valid imports (e.g. partials) on the file-system
        // Returns multiple valid results for ambiguous import path
        const sass::vector<ResolvedImport> resolved(compiler.findIncludes(import, true)); //XXXXX

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
            msg_stream << "  " << resolved[i].imp_path << "\n";
          throw Exception::ParserException(compiler, msg_stream.str());
        }

        // We made sure exactly one entry was found, load its content
        if (ImportObj loaded = compiler.loadImport(resolved[0])) {
          Root* module = eval.loadModule(compiler, loaded, hasWith);
          if (!module->loaded) eval.compileModule(module);
          eval.insertModule(module);
        }
        else {
          // Probably on access violations?
          throw Exception::ParserException(compiler,
            "Couldn't load it.");
        }

        wconfig.finalize();

        return SASS_MEMORY_NEW(Null, pstate);;
      }

    }
  }
}
