/*****************************************************************************/
/* Part of LibSass, released under the MIT license (See LICENSE.txt).        */
/*****************************************************************************/
#include "environment_stack.hpp"

#include "ast_expressions.hpp"
#include "ast_statements.hpp"
#include "exceptions.hpp"
#include "compiler.hpp"

namespace Sass {

  /////////////////////////////////////////////////////////////////////////
  // Each parsed scope gets its own environment frame
  /////////////////////////////////////////////////////////////////////////

  // The root is used for all runtime state
  // Also contains parsed root scope stack
  EnvRoot::EnvRoot(
    Compiler& compiler) :
    compiler(compiler),
    stack(compiler.varStack3312),
    idxs(new VarRefs(
      *this,
      nullptr,
      0xFFFFFFFF,
      0xFFFFFFFF,
      0xFFFFFFFF,
      false,
      false,
      true))
  {
    // Push onto our stack
    stack.push_back(this->idxs);
  }
  // EO EnvRoot ctor

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Value constructor
  EnvFrame::EnvFrame(
    Compiler& compiler,
    bool permeable,
    bool isModule,
    bool isImport) :
    stack(compiler.varRoot.stack),
    idxs(new VarRefs(
      compiler.varRoot,
      compiler.varRoot.stack.back(),
      uint32_t(compiler.varRoot.varFramePtr.size()),
      uint32_t(compiler.varRoot.mixFramePtr.size()),
      uint32_t(compiler.varRoot.fnFramePtr.size()),
      permeable, isImport, isModule))
  {
    if (isModule) {
      // Lives in built-in scope
      idxs->varFrame = 0xFFFFFFFF;
      idxs->mixFrame = 0xFFFFFFFF;
      idxs->fnFrame = 0xFFFFFFFF;
    }
    else {
      // Initialize stacks as not active yet
      idxs->root.varFramePtr.push_back(0xFFFFFFFF);
      idxs->root.mixFramePtr.push_back(0xFFFFFFFF);
      idxs->root.fnFramePtr.push_back(0xFFFFFFFF);
      // Account for allocated memory
      idxs->root.scopes.push_back(idxs);
    }
    // Check and prevent stack smashing
    if (stack.size() > MAX_NESTING) {
      throw Exception::RecursionLimitError();
    }
    // Push onto our stack
    stack.push_back(this->idxs);
  }
  // EO EnvFrame ctor
  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Remove frame from stack on destruction
  EnvFrame::~EnvFrame()
  {
    // Pop from stack
    stack.pop_back();
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Register new variable on local stack
  // Invoked mostly by stylesheet parser
  VarRef VarRefs::createVariable(
    const EnvKey& name)
  {
    // Check for existing function
    // auto it = varIdxs.find(name);
    // if (it != varIdxs.end()) {
    //   return { idxs->varFrame, it->second };
    // }
    if (varFrame == 0xFFFFFFFF) {
      // std::cerr << "Create global variable " << name.orig() << "\n";
      uint32_t offset = (uint32_t)root.intVariables.size();
      root.intVariables.resize(offset + 1);
      varIdxs[name] = offset;
      return { 0xFFFFFFFF, offset };
    }

    if (isImport) {
      return pscope->createVariable(name);
    }

    // Get local offset to new variable
    uint32_t offset = (uint32_t)varIdxs.size();
    // Remember the variable name
    varIdxs[name] = offset;
    // Return stack index reference
    return { varFrame, offset };
  }
  // EO createVariable

  // Register new function on local stack
  // Mostly invoked by built-in functions
  // Then invoked for custom C-API function
  // Finally for every parsed function rule
  VarRef VarRefs::createFunction(
    const EnvKey& name)
  {
    // Check for existing function
    auto it = fnIdxs.find(name);
    if (it != fnIdxs.end()) {
      if (it->second > root.privateFnOffset)
        return { fnFrame, it->second };
    }
    if (fnFrame == 0xFFFFFFFF) {
      uint32_t offset = (uint32_t)root.intFunction.size();
      root.intFunction.resize(offset + 1);
      fnIdxs[name] = offset;
      return { 0xFFFFFFFF, offset };
    }
    // Get local offset to new function
    uint32_t offset = (uint32_t)fnIdxs.size();
    // Remember the function name
    fnIdxs[name] = offset;
    // Return stack index reference
    return { fnFrame, offset };
  }
  // EO createFunction

  // Register new mixin on local stack
  // Only invoked for mixin rules
  // But also for content blocks
  VarRef VarRefs::createMixin(
    const EnvKey& name)
  {
    // Check for existing mixin
    auto it = mixIdxs.find(name);
    if (it != mixIdxs.end()) {
      if (it->second > root.privateMixOffset)
        return { mixFrame, it->second };
    }
    if (mixFrame == 0xFFFFFFFF) {
      uint32_t offset = (uint32_t)root.intMixin.size();
      root.intMixin.resize(offset + 1);
      mixIdxs[name] = offset;
      return { 0xFFFFFFFF, offset };
    }
    // Get local offset to new mixin
    uint32_t offset = (uint32_t)mixIdxs.size();
    // Remember the mixin name
    mixIdxs[name] = offset;
    // Return stack index reference
    return { mixFrame, offset };
  }
  // EO createMixin

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Get local variable by name, needed for most simplistic case
  // for static variable optimization in loops. When we know that
  // there is an existing local variable, we can always use that!
  VarRef VarRefs::getLocalVariableIdx(const EnvKey& name)
  {
    auto it = varIdxs.find(name);
    if (it == varIdxs.end()) return {};
    return { varFrame, it->second };
  }
  // EO getLocalVariableIdx

  // Return lookups in lexical manner. If [passThrough] is false,
  // we abort the lexical lookup on any non-permeable scope frame.
  VarRef VarRefs::getMixinIdx(const EnvKey& name, bool passThrough)
  {
    VarRefs* current = this;
    while (current != nullptr) {
      // Check if we already have this var
      auto it = current->mixIdxs.find(name);
      if (it != current->mixIdxs.end()) {
        return { current->mixFrame, it->second };
      }
      current = current->getParent(passThrough);
    }
    // Not found
    return VarRef{};
  }
  // EO getMixinIdx

  // Return lookups in lexical manner. If [passThrough] is false,
  // we abort the lexical lookup on any non-permeable scope frame.
  VarRef VarRefs::getFunctionIdx(const EnvKey& name, bool passThrough)
  {
    VarRefs* current = this;
    while (current != nullptr) {
      // Check if we already have this var
      auto it = current->fnIdxs.find(name);
      if (it != current->fnIdxs.end()) {
        return { current->fnFrame, it->second };
      }
      current = current->getParent(passThrough);
    }
    // Not found
    return VarRef{};
  }
  // EO getFunctionIdx

  // Return lookups in lexical manner. If [passThrough] is false,
  // we abort the lexical lookup on any non-permeable scope frame.
  VarRef VarRefs::getVariableIdx(const EnvKey& name, bool passThrough)
  {
    VarRefs* current = this;
    while (current != nullptr) {
      auto it = current->varIdxs.find(name);
      if (it != current->varIdxs.end()) {
        return { current->fnFrame, it->second };
      }
      current = current->getParent(passThrough);
    }
    // Not found
    return {};
  }

  // Test if we are top frame

  bool VarRefs::isRoot() const {
    // Check if raw pointers are equal
    return this == root.idxs;
  }
  // EO getVariableIdx
  
  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Update variable references and assignments
  void EnvRoot::finalizeScopes()
  {
    return; // Disabled for now
    // Process every scope ever seen
    for (VarRefs* scope : scopes) {

      // Process all variable expression (e.g. variables used in sass-scripts).
      // Move up the tree to find possible parent scopes also containing this
      // variable. On runtime we will return the first item that has a value set.
      for (VariableExpression* variable : scope->variables)
      {
        VarRefs* current = scope;
        const EnvKey& name(variable->name());
        while (current != nullptr) {
          // Find the variable name in current scope
          auto found = current->varIdxs.find(name);
          if (found != current->varIdxs.end()) {
            // Append alternative
            variable->vidxs()
              .emplace_back(VarRef{
                current->varFrame,
                found->second
              });
          }
          // Process the parent scope
          if (current->isModule) break;
          current = current->pscope;
        }
      }
      // EO variables

      // Process all variable assignment rules. Assignments can bleed up to the
      // parent scope under certain conditions. We bleed up regular style rules,
      // but not into the root scope itself (until it is semi global scope).
      for (AssignRule* assignment : scope->assignments)
      {
        VarRefs* current = scope;
        while (current != nullptr) {
          // If current scope is rooted we don't look further, as we
          // already created the local variable and assigned reference.
          if (!current->permeable) break;
          if (current->isModule) break;
          // Start with the parent
          current = current->pscope;
          // Find the variable name in current scope
          auto found = current->varIdxs
            .find(assignment->variable());
          if (found != current->varIdxs.end()) {
            // Append another alternative
            assignment->vidxs()
              .emplace_back(VarRef{
                current->varFrame,
                found->second
              });
          }
          // EO found current var
        }
        // EO while parent
      }
      // EO assignments

    }
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Get value instance by stack index reference
  // Just converting and returning reference to array offset
  ValueObj& EnvRoot::getVariable(const VarRef& vidx)
  {
    if (vidx.frame == 0xFFFFFFFF) {
      // std::cerr << "Get global variable " << vidx.offset << "\n";
      return intVariables[vidx.offset];
    }
    else {
      // std::cerr << "Get variable " << vidx.toString() << "\n";
      return variables[size_t(varFramePtr[vidx.frame]) + vidx.offset];
    }
  }
  // EO getVariable

  // Get function instance by stack index reference
  // Just converting and returning reference to array offset
  CallableObj& EnvRoot::getFunction(const VarRef& fidx)
  {
    if (fidx.frame == 0xFFFFFFFF) {
      return intFunction[fidx.offset];
    }
    else {
      return functions[size_t(fnFramePtr[fidx.frame]) + fidx.offset];
    }
  }
  // EO findFunction

  // Get mixin instance by stack index reference
  // Just converting and returning reference to array offset
  CallableObj& EnvRoot::getMixin(const VarRef& midx)
  {
    if (midx.frame == 0xFFFFFFFF) {
      return intMixin[midx.offset];
    }
    else {
      return mixins[size_t(mixFramePtr[midx.frame]) + midx.offset];
    }
  }
  // EO getMixin

  void EnvRoot::setModVar(const uint32_t offset, Value* value, bool guarded, const SourceSpan& pstate)
  {
    if (offset < privateVarOffset) {
      callStackFrame frame(compiler, pstate);
      throw Exception::RuntimeException(compiler,
        "Cannot modify built-in variable.");
    }
    // std::cerr << "Set global variable " << offset
    //   << " - " << value->inspect() << "\n";
    ValueObj& slot(intVariables[offset]);
    if (!guarded || !slot || slot->isaNull()) slot = value;
  }
  void EnvRoot::setModMix(const uint32_t offset, Callable* callable, bool guarded)
  {
    // std::cerr << "Set global mixin " << offset
    //   << " - " << callable->name() << "\n";
    CallableObj& slot(intMixin[offset]);
    if (!guarded || !slot) slot = callable;
  }
  void EnvRoot::setModFn(const uint32_t offset, Callable* callable, bool guarded)
  {
    // std::cerr << "Set global function " << offset
    //   << " - " << callable->name() << "\n";
    CallableObj& slot(intFunction[offset]);
    if (!guarded || !slot) slot = callable;
  }


  // Set items on runtime/evaluation phase via references
  // Just converting reference to array offset and assigning
  void EnvRoot::setVariable(const VarRef& vidx, ValueObj value, bool guarded)
  {
    if (vidx.frame == 0xFFFFFFFF) {
      // std::cerr << "Set global variable " << vidx.offset << " - " << value->inspect() << "\n";
      ValueObj& slot(intVariables[vidx.offset]);
      if (!guarded || !slot || slot->isaNull())
        intVariables[vidx.offset] = value;
    }
    else {
      // std::cerr << "Set variable " << vidx.toString() << " - " << value->inspect() << "\n";
      ValueObj& slot(variables[size_t(varFramePtr[vidx.frame]) + vidx.offset]);
      if (slot == nullptr || guarded == false) slot = value;
    }
  }
  // EO setVariable

  // Set items on runtime/evaluation phase via references
  // Just converting reference to array offset and assigning
  void EnvRoot::setVariable(uint32_t frame, uint32_t offset, ValueObj value, bool guarded)
  {
    if (frame == 0xFFFFFFFF) {
      ValueObj& slot(intVariables[offset]);
      if (!guarded || !slot || slot->isaNull())
        intVariables[offset] = value;
    }
    else {
      ValueObj& slot(variables[size_t(varFramePtr[frame]) + offset]);
      if (!guarded || !slot || slot->isaNull()) slot = value;
    }
  }
  // EO setVariable

  // Set items on runtime/evaluation phase via references
  // Just converting reference to array offset and assigning
  void EnvRoot::setFunction(const VarRef& fidx, UserDefinedCallableObj value, bool guarded)
  {
    if (fidx.frame == 0xFFFFFFFF) {
      if (!guarded || intFunction[fidx.offset] == nullptr)
        intFunction[fidx.offset] = value;
    }
    else {
      CallableObj& slot(functions[size_t(fnFramePtr[fidx.frame]) + fidx.offset]);
      if (!guarded || !slot) slot = value;
    }
  }
  // EO setFunction

  // Set items on runtime/evaluation phase via references
  // Just converting reference to array offset and assigning
  void EnvRoot::setMixin(const VarRef& midx, UserDefinedCallableObj value, bool guarded)
  {
    if (midx.frame == 0xFFFFFFFF) {
      if (!guarded || intMixin[midx.offset] == nullptr)
        intMixin[midx.offset] = value;
    }
    else {
      CallableObj& slot(mixins[size_t(mixFramePtr[midx.frame]) + midx.offset]);
      if (!guarded || !slot) slot = value;
    }
  }
  // EO setMixin

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////


  // Get a mixin associated with the under [name].
  // Will lookup from the last runtime stack scope.
  // We will move up the runtime stack until we either
  // find a defined mixin or run out of parent scopes.
  Callable* EnvRoot::findMixin(const EnvKey& name) const
  {
    if (stack.empty()) return nullptr;
    return stack.back()->findMixin(name);
  }
  // EO getMixin

  // Get a value associated with the variable under [name].
  // If [global] flag is given, the lookup will be in the root.
  // Otherwise lookup will be from the last runtime stack scope.
  // We will move up the runtime stack until we either find a 
  // defined variable with a value or run out of parent scopes.
  Value* VarRefs::findVariable(const EnvKey& name) const
  {
    const VarRefs* current = this;
    while (current) {
      auto it = current->varIdxs.find(name);
      if (it != current->varIdxs.end()) {
        const VarRef vidx{ current->varFrame, it->second };
        ValueObj& value = root.getVariable(vidx);
        if (value != nullptr) { return value; }
      }
      for (auto fwds : current->fwdGlobal55) {
        auto fwd = fwds.first->varIdxs.find(name);
        if (fwd != fwds.first->varIdxs.end()) {
          const VarRef vidx{ fwds.first->varFrame, fwd->second };
          Value* value = root.getVariable(vidx);
          if (value && name.isPrivate()) continue;
          if (value != nullptr) return value;
        }
        if (Moduled* mod = fwds.second) {
          auto fwd = mod->mergedFwdVar.find(name);
          if (fwd != mod->mergedFwdVar.end()) {
            Value* val = root.getVariable(
              { 0xFFFFFFFF, fwd->second });
            if (val && name.isPrivate()) continue;
            if (val != nullptr) return val;
          }
        }
      }
      if (current->pscope == nullptr) break;
      else current = current->pscope;
    }
    return {};
  }
  // EO getVariable

  // Get a function associated with the under [name].
  // Will lookup from the last runtime stack scope.
  // We will move up the runtime stack until we either
  // find a defined function or run out of parent scopes.
  Callable* VarRefs::findFunction(const EnvKey& name) const
  {
    const VarRefs* current = this;
    while (current) {
      auto it = current->fnIdxs.find(name);
      if (it != current->fnIdxs.end()) {
        const VarRef vidx{ current->fnFrame, it->second };
        CallableObj& value = root.getFunction(vidx);
        if (!value.isNull()) return value;
      }
      for (auto fwds : current->fwdGlobal55) {
        auto fwd = fwds.first->fnIdxs.find(name);
        if (fwd != fwds.first->fnIdxs.end()) {
          const VarRef vidx{ fwds.first->fnFrame, fwd->second };
          CallableObj& value = root.getFunction(vidx);
          if (value && name.isPrivate()) continue;
          if (!value.isNull()) return value;
        }
        if (Moduled* mod = fwds.second) {
          auto fwd = mod->mergedFwdFn.find(name);
          if (fwd != mod->mergedFwdFn.end()) {
            Callable* fn = root.getFunction(
              { 0xFFFFFFFF, fwd->second });
            if (fn && name.isPrivate()) continue;
            if (fn != nullptr) return fn;
          }
        }
      }
      if (current->pscope == nullptr) break;
      else current = current->pscope;
    }
    return nullptr;
  }
  // EO findFunction

  // Get a function associated with the under [name].
  // Will lookup from the last runtime stack scope.
  // We will move up the runtime stack until we either
  // find a defined function or run out of parent scopes.
  Callable* VarRefs::findMixin(const EnvKey& name) const
  {
    const VarRefs* current = this;
    while (current) {
      auto it = current->mixIdxs.find(name);
      if (it != current->mixIdxs.end()) {
        const VarRef midx{ current->mixFrame, it->second };
        Callable* mixin = root.getMixin(midx);
        if (mixin != nullptr) return mixin;
      }
      for (auto fwds : current->fwdGlobal55) {
        auto fwd = fwds.first->mixIdxs.find(name);
        if (fwd != fwds.first->mixIdxs.end()) {
          const VarRef vidx{ fwds.first->mixFrame, fwd->second };
          Callable* mixin = root.getMixin(vidx);
          if (mixin && name.isPrivate()) continue;
          if (mixin != nullptr) return mixin;
        }
        if (Moduled* mod = fwds.second) {
          auto fwd = mod->mergedFwdMix.find(name);
          if (fwd != mod->mergedFwdMix.end()) {
            Callable* mix = root.getMixin(
              { 0xFFFFFFFF, fwd->second });
            if (mix && name.isPrivate()) continue;
            if (mix != nullptr) return mix;
          }
        }
      }
      if (current->pscope == nullptr) break;
      else current = current->pscope;
    }
    return nullptr;
  }
  // EO findFunction

  // Get a mixin associated with the under [name].
  // Will lookup from the last runtime stack scope.
  // We will move up the runtime stack until we either
  // find a defined mixin or run out of parent scopes.
  Callable* VarRefs::getMixin(const EnvKey& name) const
  {
    auto it = mixIdxs.find(name);
    if (it != mixIdxs.end()) {
      const VarRef vidx{ mixFrame, it->second };
      Callable* value = root.getMixin(vidx);
      if (value != nullptr) return value;
    }
    for (auto fwds : fwdGlobal55) {
      auto it = fwds.first->mixIdxs.find(name);
      if (it != fwds.first->mixIdxs.end()) {
        const VarRef vidx{ fwds.first->mixFrame, it->second };
        Callable* value = root.getMixin(vidx);
        if (value != nullptr) return value;
      }
    }
    return nullptr;
  }
  // EO getMixin

  Callable* VarRefs::getFunction(const EnvKey& name) const
  {
    auto it = fnIdxs.find(name);
    if (it != fnIdxs.end()) {
      const VarRef vidx{ fnFrame, it->second };
      Callable* value = root.getFunction(vidx);
      if (value != nullptr) return value;
    }
    for (auto fwds : fwdGlobal55) {
      auto it = fwds.first->fnIdxs.find(name);
      if (it != fwds.first->fnIdxs.end()) {
        const VarRef vidx{ fwds.first->fnFrame, it->second };
        Callable* value = root.getFunction(vidx);
        if (value != nullptr) return value;
      }
    }
    return nullptr;
  }

  //Callable* VarRefs::getMixin(const EnvKey& name) const
  //{
  //  auto it = mixIdxs.find(name);
  //  if (it != mixIdxs.end()) {
  //    const VarRef vidx{ mixFrame, it->second };
  //    Callable* mixin = root.getFunction(vidx);
  //    if (mixin != nullptr) return mixin;
  //  }
  //  return nullptr;
  //}

  Value* VarRefs::getVariable(const EnvKey& name) const
  {
    auto it = varIdxs.find(name);
    if (it != varIdxs.end()) {
      const VarRef vidx{ varFrame, it->second };
      Value* value = root.getVariable(vidx);
      if (value != nullptr) return value;
    }
    for (auto fwds : fwdGlobal55) {
      auto it = fwds.first->varIdxs.find(name);
      if (it != fwds.first->varIdxs.end()) {
        const VarRef vidx{ fwds.first->varFrame, it->second };
        Value* value = root.getVariable(vidx);
        if (value != nullptr) return value;
      }
    }
    return nullptr;
  }

  bool VarRefs::setModVar(const EnvKey& name, Value* value, bool guarded, const SourceSpan& pstate) const
  {
    auto it = varIdxs.find(name);
    if (it != varIdxs.end()) {
      root.setModVar(it->second, value, guarded, pstate);
      return true;
    }
    for (auto fwds : fwdGlobal55) {
      auto it = fwds.first->varIdxs.find(name);
      if (it != fwds.first->varIdxs.end()) {
        root.setModVar(it->second, value, guarded, pstate);
        return true;
      }
    }
    return false;
  }

  bool VarRefs::setModMix(const EnvKey& name, Callable* fn, bool guarded) const
  {
    auto it = mixIdxs.find(name);
    if (it != mixIdxs.end()) {
      root.setModMix(it->second, fn, guarded);
      return true;
    }
    for (auto fwds : fwdGlobal55) {
      auto it = fwds.first->mixIdxs.find(name);
      if (it != fwds.first->mixIdxs.end()) {
        root.setModFn(it->second, fn, guarded);
        return true;
      }
    }
    return false;
  }

  bool VarRefs::setModFn(const EnvKey& name, Callable* fn, bool guarded) const
  {
    auto it = fnIdxs.find(name);
    if (it != fnIdxs.end()) {
      root.setModFn(it->second, fn, guarded);
      return true;
    }
    for (auto fwds : fwdGlobal55) {
      auto it = fwds.first->fnIdxs.find(name);
      if (it != fwds.first->fnIdxs.end()) {
        root.setModFn(it->second, fn, guarded);
        return true;
      }
    }
    return false;
  }


  bool VarRefs::hasNameSpace(const sass::string& ns, const EnvKey& name) const
  {
    const VarRefs* current = this;
    while (current) {
      auto it = current->fwdModule55.find(ns);
      if (it != current->fwdModule55.end()) {
        auto fwd = it->second.first->varIdxs.find(name);
        if (fwd != it->second.first->varIdxs.end()) {
          ValueObj& slot(root.getVariable({ 0xFFFFFFFF, fwd->second }));
          return slot != nullptr;
        }
        return true;
      }
      if (current->pscope == nullptr) break;
      else current = current->pscope;
    }
    return false;
  }

  Value* VarRefs::findVariable(const EnvKey& name, const sass::string& ns) const
  {
    const VarRefs* current = this;
    while (current) {
      // Check if the namespace was registered
      auto it = current->fwdModule55.find(ns);
      if (it != current->fwdModule55.end()) {
        if (VarRefs* idxs = it->second.first) {
          Value* val = idxs->getVariable(name);
          if (val != nullptr) return val;
        }
        if (Moduled* mod = it->second.second) {
          auto fwd = mod->mergedFwdVar.find(name);
          if (fwd != mod->mergedFwdVar.end()) {
            Value* val = root.getVariable(
              { 0xFFFFFFFF, fwd->second });
            if (val != nullptr) return val;
          }
        }
      }
      if (current->pscope == nullptr) break;
      else current = current->pscope;
    }
    return nullptr;
  }

  Callable* VarRefs::findFunction(const EnvKey& name, const sass::string& ns) const
  {
    const VarRefs* current = this;
    while (current) {
      // Check if the namespace was registered
      auto it = current->fwdModule55.find(ns);
      if (it != current->fwdModule55.end()) {
        if (VarRefs* idxs = it->second.first) {
          Callable* fn = idxs->getFunction(name);
          if (fn != nullptr) return fn;
        }
        if (Moduled* mod = it->second.second) {
          auto fwd = mod->mergedFwdFn.find(name);
          if (fwd != mod->mergedFwdFn.end()) {
            Callable* fn = root.getFunction(
              { 0xFFFFFFFF, fwd->second });
            if (fn != nullptr) return fn;
          }
        }
      }
      if (current->pscope == nullptr) break;
      else current = current->pscope;
    }
    return nullptr;
  }

  Callable* VarRefs::findMixin(const EnvKey& name, const sass::string& ns) const
  {
    const VarRefs* current = this;
    while (current) {
      // Check if the namespace was registered
      auto it = current->fwdModule55.find(ns);
      if (it != current->fwdModule55.end()) {
        if (VarRefs* idxs = it->second.first) {
          Callable* mixin = idxs->getMixin(name);
          if (mixin != nullptr) return mixin;
        }
        if (Moduled* mod = it->second.second) {
          auto fwd = mod->mergedFwdMix.find(name);
          if (fwd != mod->mergedFwdMix.end()) {
            Callable* mixin = root.getMixin(
              { 0xFFFFFFFF, fwd->second });
            if (mixin != nullptr) return mixin;
          }
        }
      }
      if (current->pscope == nullptr) break;
      else current = current->pscope;
    }
    return nullptr;
  }

  // Get a function associated with the under [name].
  // Will lookup from the last runtime stack scope.
  // We will move up the runtime stack until we either
  // find a defined function or run out of parent scopes.
  Callable* EnvRoot::findFunction(const EnvKey& name) const
  {
    if (stack.empty()) return nullptr;
    return stack.back()->findFunction(name);
  }
  // EO findFunction

  Callable* EnvRoot::findMixin(const EnvKey& name, const sass::string& ns) const
  {
    if (stack.empty()) return nullptr;
    return stack.back()->findMixin(name, ns);
  }

  Callable* EnvRoot::findFunction(const EnvKey& name, const sass::string& ns) const
  {
    if (stack.empty()) return nullptr;
    return stack.back()->findFunction(name, ns);
  }

  Value* EnvRoot::findVariable(const EnvKey& name, const sass::string& ns) const
  {
    if (stack.empty()) return nullptr;
    return stack.back()->findVariable(name, ns);
  }


  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  bool EnvRoot::setModVar(const EnvKey& name, const sass::string& ns, Value* value, bool guarded, const SourceSpan& pstate)
  {
    if (stack.empty()) return false;
    return stack.back()->setModVar(name, ns, value, guarded, pstate);
  }

  bool EnvRoot::setModMix(const EnvKey& name, const sass::string& ns, Callable* fn, bool guarded)
  {
    if (stack.empty()) return false;
    return stack.back()->setModMix(name, ns, fn, guarded);
  }

  bool EnvRoot::setModFn(const EnvKey& name, const sass::string& ns, Callable* fn, bool guarded)
  {
    if (stack.empty()) return false;
    return stack.back()->setModFn(name, ns, fn, guarded);
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  bool VarRefs::setModVar(const EnvKey& name, const sass::string& ns, Value* value, bool guarded, const SourceSpan& pstate)
  {
    const VarRefs* current = this;
    while (current) {
      // Check if the namespace was registered
      auto it = current->fwdModule55.find(ns);
      if (it != current->fwdModule55.end()) {
        if (VarRefs* idxs = it->second.first) {
          if (idxs->setModVar(name, value, guarded, pstate))
            return true;
        }
        if (Moduled* mod = it->second.second) {
          auto fwd = mod->mergedFwdVar.find(name);
          if (fwd != mod->mergedFwdVar.end()) {
            root.setModVar(fwd->second, value, guarded, pstate);
            return true;
          }
        }
      }
      if (current->pscope == nullptr) break;
      else current = current->pscope;
    }
    return false;
  }

  bool VarRefs::setModMix(const EnvKey& name, const sass::string& ns, Callable* fn, bool guarded)
  {
    const VarRefs* current = this;
    while (current) {
      // Check if the namespace was registered
      auto it = current->fwdModule55.find(ns);
      if (it != current->fwdModule55.end()) {
        if (VarRefs* idxs = it->second.first) {
          if (idxs->setModMix(name, fn, guarded))
            return true;
        }
        if (Moduled* mod = it->second.second) {
          auto fwd = mod->mergedFwdMix.find(name);
          if (fwd != mod->mergedFwdMix.end()) {
            root.setModMix(fwd->second, fn, guarded);
            return true;
          }
        }
      }
      if (current->pscope == nullptr) break;
      else current = current->pscope;
    }
    return false;
  }

  bool VarRefs::setModFn(const EnvKey& name, const sass::string& ns, Callable* fn, bool guarded)
  {
    const VarRefs* current = this;
    while (current) {
      // Check if the namespace was registered
      auto it = current->fwdModule55.find(ns);
      if (it != current->fwdModule55.end()) {
        if (VarRefs* idxs = it->second.first) {
          if (idxs->setModFn(name, fn, guarded))
            return true;
        }
        if (Moduled* mod = it->second.second) {
          auto fwd = mod->mergedFwdFn.find(name);
          if (fwd != mod->mergedFwdFn.end()) {
            root.setModFn(fwd->second, fn, guarded);
            return true;
          }
        }
      }
      if (current->pscope == nullptr) break;
      else current = current->pscope;
    }
    return false;
  }

  // Get a value associated with the variable under [name].
  // If [global] flag is given, the lookup will be in the root.
  // Otherwise lookup will be from the last runtime stack scope.
  // We will move up the runtime stack until we either find a 
  // defined variable with a value or run out of parent scopes.
  Value* EnvRoot::findVariable(const EnvKey& name, bool global) const
  {
    if (stack.empty()) return {};
    auto idxs = global ? stack.front() : stack.back();
    return idxs->findVariable(name);
  }
  // EO getVariable

  // Set a value associated with the variable under [name].
  // If [global] flag is given, the lookup will be in the root.
  // Otherwise lookup will be from the last runtime stack scope.
  // We will move up the runtime stack until we either find a 
  // defined variable with a value or run out of parent scopes.
  bool EnvRoot::setVariable(const EnvKey& name, ValueObj val, bool guarded, bool global)
  {
    if (stack.empty()) return false;
    uint32_t idx = global ? 0 :
      (uint32_t)stack.size() - 1;
    const VarRefs* current = stack[idx];
    while (current) {
      for (auto fwd : current->fwdGlobal55) {
        VarRefs* refs = fwd.first;
        auto in = refs->varIdxs.find(name);
        if (in != refs->varIdxs.end()) {
          if (name.isPrivate()) {
            throw Exception::ParserException(compiler,
              "Private members can't be accessed "
              "from outside their modules.");
          }
          const VarRef vidx{ 0xFFFFFFFF, in->second };
          idxs->root.setVariable(vidx, val, guarded);
          return true;
        }
/*
        if (Moduled* mod = fwd.second) {
          auto fwd = mod->mergedFwdVar.find(name);
          if (fwd != mod->mergedFwdVar.end()) {
            std::cerr << "Got in merged fwd\n";
            // Value* val = getVariable(
            //   { 0xFFFFFFFF, fwd->second });
            // if (val && name.isPrivate()) continue;
            // if (val != nullptr) return val;
          }
        }
*/
      }

      auto it = current->varIdxs.find(name);
      if (it != current->varIdxs.end()) {
        const VarRef vidx{ current->varFrame, it->second };
        idxs->root.setVariable(vidx, val, guarded);
        return true;
      }

      if (current->pscope == nullptr) break;
      else current = current->pscope;
    }
    return false;
  }
  // EO setVariable

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Very small helper for debugging
  sass::string VarRef::toString() const
  {
    sass::sstream strm;
    strm << frame << ":" << offset;
    return strm.str();
  }
  // EO toString

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

}
