/*****************************************************************************/
/* Part of LibSass, released under the MIT license (See LICENSE.txt).        */
/*****************************************************************************/
#include "preloader.hpp"

#include "eval.hpp"
#include "compiler.hpp"
#include "exceptions.hpp"
#include "environment.hpp"
#include "ast_imports.hpp"

namespace Sass {

  Preloader::Preloader(Eval& eval, Root* root) :
    eval(eval),
    root(root),
    chroot77(eval.chroot77),
    wconfig(eval.wconfig),
    idxs(root->idxs)
  {}

  // During the whole parsing we should keep a big map of
  // Variable name to VarRef vector with all alternatives


  void Preloader::process()
  {
    acceptRoot(root);
  }


  void Preloader::acceptRoot(Root* sheet)
  {
    if (sheet && !sheet->empty()) {
      LOCAL_PTR(Root, chroot77, sheet);
      LOCAL_PTR(VarRefs, idxs, sheet->idxs);
      ImportStackFrame isf(eval.compiler, sheet->import);
      eval.compiler.varRoot.stack.push_back(sheet->idxs);
      for (auto& it : sheet->elements()) it->accept(this);
      eval.compiler.varRoot.stack.pop_back();
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


  void Preloader::visitParentStatement(ParentStatement* rule)
  {
    if (rule->empty()) return;
    LOCAL_PTR(VarRefs, idxs, rule->idxs);
    eval.compiler.varRoot.stack.push_back(rule->idxs);
    for (auto& it : rule->elements()) it->accept(this);
    eval.compiler.varRoot.stack.pop_back();
  }

  void Preloader::visitUseRule(UseRule* rule)
  {
    callStackFrame frame(eval.compiler, {
      rule->pstate(), Strings::useRule });

    Root* sheet = eval.resolveUseRule(rule);

    // return;

    if (sheet && !sheet->empty()) {
      LOCAL_PTR(Root, chroot77, sheet);
      LOCAL_PTR(VarRefs, idxs, sheet->idxs);
      ImportStackFrame iframe(eval.compiler, rule->import());
      eval.compiler.varRoot.stack.push_back(sheet->idxs);
      for (auto& it : sheet->elements()) it->accept(this);
      eval.compiler.varRoot.stack.pop_back();
    }

    return;

    eval.exposeUseRule(rule);

  }

  void Preloader::visitForwardRule(ForwardRule* rule)
  {
    callStackFrame frame(eval.compiler, {
      rule->pstate(), Strings::forwardRule });

    Root* sheet = eval.resolveForwardRule(rule);

    // return;

    if (sheet && !sheet->empty()) {
      LOCAL_PTR(Root, chroot77, sheet);
      LOCAL_PTR(VarRefs, idxs, sheet->idxs);
      ImportStackFrame iframe(eval.compiler, rule->import());
      eval.compiler.varRoot.stack.push_back(sheet->idxs);
      for (auto& it : sheet->elements()) it->accept(this);
      eval.compiler.varRoot.stack.pop_back();
    }

    return;

    eval.exposeFwdRule(rule);
  }


  void Preloader::visitIncludeImport(IncludeImport* rule)
  {

    callStackFrame frame(eval.compiler, {
      rule->pstate(), Strings::importRule });

    Root* root = eval.resolveIncludeImport(rule);

    return;

    if (root && !root->empty()) {
      LOCAL_PTR(Root, chroot77, root);
      LOCAL_PTR(VarRefs, idxs, root->idxs);
      ImportStackFrame iframe(eval.compiler, rule->import());
      eval.compiler.varRoot.stack.push_back(root->idxs);
      for (auto& it : root->elements()) it->accept(this);
      eval.compiler.varRoot.stack.pop_back();
    }

    return;

    eval.exposeImpRule(rule);

  }

  void Preloader::visitAssignRule(AssignRule* rule)
  {
    if (!rule->ns().empty()) return;
  }

  void Preloader::visitFunctionRule(FunctionRule* rule)
  {
    const EnvKey& fname(rule->name());
    LOCAL_PTR(VarRefs, idxs, rule->idxs);
    eval.compiler.varRoot.stack.push_back(rule->idxs);
    for (auto& it : rule->elements()) it->accept(this);
    eval.compiler.varRoot.stack.pop_back();
  }

  void Preloader::visitMixinRule(MixinRule* rule)
  {
    const EnvKey& mname(rule->name());
    LOCAL_PTR(VarRefs, idxs, rule->idxs);
    eval.compiler.varRoot.stack.push_back(rule->idxs);
    for (auto& it : rule->elements()) it->accept(this);
    eval.compiler.varRoot.stack.pop_back();
  }

  void Preloader::visitImportRule(ImportRule* rule)
  {
    for (const ImportBaseObj& import : rule->elements()) {
      if (IncludeImport* include = import->isaIncludeImport()) {
        visitIncludeImport(include);
      }
    }
  }


  void Preloader::visitAtRootRule(AtRootRule* rule)
  {
    visitParentStatement(rule);
  }

  void Preloader::visitAtRule(AtRule* rule)
  {
    visitParentStatement(rule);
  }

  void Preloader::visitContentBlock(ContentBlock* rule)
  {
    visitParentStatement(rule);
  }

  void Preloader::visitContentRule(ContentRule* rule)
  {
  }

  void Preloader::visitDebugRule(DebugRule* rule)
  {
  }

  void Preloader::visitDeclaration(Declaration* rule)
  {
    visitParentStatement(rule);
  }

  void Preloader::visitErrorRule(ErrorRule* rule)
  {
  }

  void Preloader::visitExtendRule(ExtendRule* rule)
  {
  }

  void Preloader::visitIfRule(IfRule* rule)
  {
    visitParentStatement(rule);
    if (rule->alternative() == nullptr) return;
    visitIfRule(rule->alternative());
  }


  void Preloader::visitIncludeRule(IncludeRule* rule)
  {
    if (ContentBlock* content = rule->content()) {
      LOCAL_PTR(VarRefs, idxs, content->idxs);
      eval.compiler.varRoot.stack.push_back(content->idxs);
      for (auto& it : content->elements()) it->accept(this);
      eval.compiler.varRoot.stack.pop_back();
    }
  }

  void Preloader::visitLoudComment(LoudComment* rule)
  {
  }

  void Preloader::visitMediaRule(MediaRule* rule)
  {
    visitParentStatement(rule);
  }

  void Preloader::visitEachRule(EachRule* rule)
  {
    LOCAL_PTR(VarRefs, idxs, rule->idxs);
    auto& vars(rule->variables());
    for (size_t i = 0; i < vars.size(); i += 1) {
      idxs->varIdxs.insert({ vars[i], (uint32_t)i });
    }
    eval.compiler.varRoot.stack.push_back(rule->idxs);
    for (auto& it : rule->elements()) it->accept(this);
    eval.compiler.varRoot.stack.pop_back();
  }

  void Preloader::visitForRule(ForRule* rule)
  {
    LOCAL_PTR(VarRefs, idxs, rule->idxs);
    idxs->varIdxs.insert({ rule->varname(), 0 });
    eval.compiler.varRoot.stack.push_back(rule->idxs);
    for (auto& it : rule->elements()) it->accept(this);
    eval.compiler.varRoot.stack.pop_back();
  }

  void Preloader::visitReturnRule(ReturnRule* rule)
  {
  }

  void Preloader::visitSilentComment(SilentComment* rule)
  {
  }

  void Preloader::visitStyleRule(StyleRule* rule)
  {
    visitParentStatement(rule);
  }

  void Preloader::visitSupportsRule(SupportsRule* rule)
  {
    visitParentStatement(rule);
  }

  void Preloader::visitWarnRule(WarnRule* rule)
  {
  }

  void Preloader::visitWhileRule(WhileRule* rule)
  {
    visitParentStatement(rule);
  }


  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

};