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
    wconfig(eval.compiler.wconfig),
    module(root),
    idxs(root->idxs)
  {}

  // During the whole parsing we should keep a big map of
  // Variable name to VarRef vector with all alternatives

  void Preloader::process()
  {
    acceptRoot(root);
  }


  void Preloader::acceptRoot(Root* root)
  {
    if (root->empty()) return;
    LOCAL_PTR(Root, module, root);
    LOCAL_PTR(VarRefs, idxs, root->idxs);
    ImportStackFrame isf(eval.compiler, root->import);
    eval.compiler.varRoot.stack.push_back(root->idxs);
    for (auto& it : root->elements()) it->accept(this);
    eval.compiler.varRoot.stack.pop_back();
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

    // {
    //   if (sheet->empty()) return;
    //   LOCAL_PTR(Root, module, sheet);
    //   LOCAL_PTR(VarRefs, idxs, sheet->idxs);
    //   // ImportStackFrame iframe(eval.compiler, rule->import());
    //   eval.compiler.varRoot.stack.push_back(sheet->idxs);
    //   for (auto it : sheet->elements()) it->accept(this);
    //   eval.compiler.varRoot.stack.pop_back();
    // }
    // // 
    // eval.exposeUseRule(rule); // five errs

  }

  void Preloader::visitForwardRule(ForwardRule* rule)
  {
    callStackFrame frame(eval.compiler, {
      rule->pstate(), Strings::forwardRule });

    Root* root = eval.resolveForwardRule(rule);

    {
      if (root->empty()) return;
      LOCAL_PTR(Root, module, root);
      LOCAL_PTR(VarRefs, idxs, root->idxs);
      ImportStackFrame iframe(eval.compiler, rule->import());
      eval.compiler.varRoot.stack.push_back(root->idxs);
      for (auto& it : root->elements()) it->accept(this);
      eval.compiler.varRoot.stack.pop_back();
    }

    // Nothing to be done for built-ins
    if (root && root->isBuiltIn && !rule->wasMerged()) {
      mergeForwards(rule->module()->idxs, module, rule->isShown(), rule->isHidden(),
        rule->prefix(), rule->toggledVariables(), rule->toggledCallables(), eval.compiler);
      rule->wasMerged(true);
    }


  }

  void Preloader::exposeImport(Eval& eval, Root* sheet)
  {
    auto vframe = eval.compiler.getCurrentFrame();

    // Skip over all imports
    // We are doing it out of order
    while (vframe) {

      for (auto& var : sheet->mergedFwdVar) {
        auto it = module->mergedFwdVar.find(var.first);
        if (it == module->mergedFwdVar.end()) {
          module->mergedFwdVar[var.first] = var.second;
          // eval.compiler.varRoot.variables.push_back({});
          // std::cerr << "EXPORT " << var.first.norm() << "\n";
          // vframe->createVariable(var.first);
        }
        else {
          // it->second = var.second;
        }
      }

      // Merge it up through all imports
      for (auto& var : sheet->idxs->varIdxs) {
        auto it = vframe->varIdxs.find(var.first);
        if (it == vframe->varIdxs.end()) {
          if (vframe->isCompiled) {
            // throw "Can't create on active frame";
          }
          // eval.compiler.varRoot.variables.push_back({});
          std::cerr << "EXPORT " << var.first.norm() << "\n";
          vframe->createVariable(var.first);
        }
        else {
          // it->second = var.second;
        }
      }


      // Merge it up through all imports
      for (auto& var : sheet->idxs->varIdxs) {
        auto it = vframe->varIdxs.find(var.first);
        if (it == vframe->varIdxs.end()) {
          if (vframe->isCompiled) {
            throw "Can't create on active frame";
            // eval.compiler.varRoot.variables.push_back({});
          }
          std::cerr << "EXPORT " << var.first.norm() << "\n";
          vframe->createVariable(var.first);
        }
        else {
          it->second = var.second;
        }
      }

      // Merge it up through all imports
      for (auto& fn : sheet->idxs->fnIdxs) {
        auto it = vframe->fnIdxs.find(fn.first);
        if (it == vframe->fnIdxs.end()) {
          if (vframe->isCompiled) {
            throw "Can't create on active frame";
            // eval.compiler.varRoot.functions.push_back({});
          }
          // std::cerr << "EXPORT " << var.first.norm() << "\n";
          vframe->createFunction(fn.first);
        }
      }

      // Merge it up through all imports
      for (auto& mix : sheet->idxs->mixIdxs) {
        auto it = vframe->mixIdxs.find(mix.first);
        if (it == vframe->mixIdxs.end()) {
          if (vframe->isCompiled) {
            throw "Can't create on active frame";
            // eval.compiler.varRoot.mixins.push_back({});
          }
          // std::cerr << "EXPORT " << var.first.norm() << "\n";
          vframe->createMixin(mix.first);
        }
      }

      if (!vframe->isImport) break;
      vframe = vframe->pscope;
      // break;
    }
  }

  void Preloader::visitIncludeImport(IncludeImport* rule)
  {

    callStackFrame frame(eval.compiler, {
      rule->pstate(), Strings::importRule });

    Root* root = eval.resolveIncludeImport(rule);

return;
    {
      if (root->empty()) return;
      LOCAL_PTR(Root, module, root);
      LOCAL_PTR(VarRefs, idxs, root->idxs);
      eval.compiler.varRoot.stack.push_back(root->idxs);
      for (auto& it : root->elements()) it->accept(this);
      eval.compiler.varRoot.stack.pop_back();
    }

  }


  // void Preloader::visitArgumentDeclaration(ArgumentDeclaration* args)
  // {
  //   for (Argument* arg : args->arguments()) {
  //     uint32_t offset = (uint32_t)idxs->varIdxs.size();
  //     idxs->varIdxs.insert({ arg->name(), offset });
  //   }
  //   if (!args->restArg().empty()) {
  //     uint32_t offset = (uint32_t)idxs->varIdxs.size();
  //     idxs->varIdxs.insert({ args->restArg(), offset });
  //   }
  // }

  void Preloader::visitAssignRule(AssignRule* rule)
  {
    if (!rule->ns().empty()) return;
  }

  void Preloader::visitFunctionRule(FunctionRule* rule)
  {
    // VarRefs* idxs(chroot->idxs);
    const EnvKey& fname(rule->name());

    LOCAL_PTR(VarRefs, idxs, rule->idxs);
    // First push argument onto the scope!
    // visitArgumentDeclaration(rule->arguments());

    eval.compiler.varRoot.stack.push_back(rule->idxs);
    for (auto& it : rule->elements()) it->accept(this);
    eval.compiler.varRoot.stack.pop_back();
  }

  void Preloader::visitMixinRule(MixinRule* rule)
  {
    //VarRefs* idxs(chroot->idxs);
    const EnvKey& mname(rule->name());
    LOCAL_PTR(VarRefs, idxs, rule->idxs);
    // First push argument onto the scope!
    // visitArgumentDeclaration(rule->arguments());

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
    auto& content = rule->content();
    if (content == nullptr) return;
    // rule->cidx(idxs->createMixin(Keys::contentRule));
    LOCAL_PTR(VarRefs, idxs, content->idxs);
    // First push argument onto the scope!
    // visitArgumentDeclaration(content->arguments());
    eval.compiler.varRoot.stack.push_back(content->idxs);
    for (auto& it : content->elements()) it->accept(this);
    eval.compiler.varRoot.stack.pop_back();
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
