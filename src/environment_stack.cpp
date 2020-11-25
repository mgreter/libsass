bool stkdbg = false;
bool mixdbg = false;
bool fndbg = false;

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

  const VarRef nullidx;

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
    variables.reserve(256);
    functions.reserve(256);
    mixins.reserve(128);
    varFramePtr.reserve(256);
    mixFramePtr.reserve(128);
    fnFramePtr.reserve(256);
    intVariables.reserve(256);
    intFunction.reserve(256);
    intMixin.reserve(128);
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
    }
    // Check and prevent stack smashing
    if (stack.size() > MAX_NESTING) {
      throw Exception::RecursionLimitError();
    }
    // Push onto our stack
    stack.push_back(this->idxs);
    // Account for allocated memory
    idxs->root.scopes3.push_back(idxs);
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
      uint32_t offset = (uint32_t)root.intVariables.size();
      if (stkdbg) std::cerr << "Create global variable " << name.orig() << " at " << offset << "\n";
      root.intVariables.resize(offset + 1);
      //root.intVariables[offset] = SASS_MEMORY_NEW(Null,
      //  SourceSpan::tmp("null"));
      varIdxs[name] = offset;
      return { 0xFFFFFFFF, offset };
    }

    //if (isImport) {
    //  return pscope->createVariable(name);
    //}

    // Get local offset to new variable
    uint32_t offset = (uint32_t)varIdxs.size();
    if (stkdbg) std::cerr << "Create local variable " << name.orig() << " at " << offset << "\n";
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
      if (fndbg) std::cerr << "Create global function " << name.orig() << " at " << offset << "\n";
      root.intFunction.resize(offset + 1);
      fnIdxs[name] = offset;
      return { 0xFFFFFFFF, offset };
    }
    // Get local offset to new function
    uint32_t offset = (uint32_t)fnIdxs.size();
    if (fndbg) std::cerr << "Create local function " << name.orig() << " at " << offset << "\n";
    // Remember the function name
    fnIdxs[name] = offset;
    // Return stack index reference
    return { fnFrame, offset };
  }
  // EO createFunction

  VarRef VarRefs::createLexicalVar(
    const EnvKey& name)
  {
    auto current = this;
    // Skip over all imports
    //while (current->isImport) {
    //  current = current->pscope;
    //}
    //if (current->isCompiled) {
    //  // throw "Can't create on active frame";
    //  root.variables.push_back({});
    //}
    return current->createVariable(name);
  }
  // EO createFunction


  VarRef VarRefs::createLexicalFn(
    const EnvKey& name)
  {
    auto current = this;
    // Skip over all imports
    // while (current->isImport) {
    //   current = current->pscope;
    // }
    // if (current->isCompiled) {
    //   // throw "Can't create on active frame";
    //   root.functions.push_back({});
    // }
    return current->createFunction(name);
  }
  // EO createFunction

  VarRef VarRefs::createLexicalMix(
    const EnvKey& name)
  {
    auto current = this;
    // Skip over all imports
    // while (current->isImport) {
    //   current = current->pscope;
    // }
    // if (current->isCompiled) {
    //   // throw "Can't create on active frame";
    //   root.mixins.push_back({});
    // }
    return current->createMixin(name);
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
      if (mixdbg) std::cerr << "Create global mixin " << name.orig() << " at " << offset << "\n";
      mixIdxs[name] = offset;
      return { 0xFFFFFFFF, offset };
    }
    // Get local offset to new mixin
    uint32_t offset = (uint32_t)mixIdxs.size();
    if (mixdbg) std::cerr << "Create local mixin " << name.orig() << " at " << offset << "\n";
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
      if (!current->isImport) {
        // Check if we already have this var
        auto it = current->mixIdxs.find(name);
        if (it != current->mixIdxs.end()) {
          return { current->mixFrame, it->second };
        }
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
      if (!current->isImport) {
        // Check if we already have this var
        auto it = current->fnIdxs.find(name);
        if (it != current->fnIdxs.end()) {
          return { current->fnFrame, it->second };
        }
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
      if (!current->isImport) {
        auto it = current->varIdxs.find(name);
        if (it != current->varIdxs.end()) {
          return { current->fnFrame, it->second };
        }
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
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Get value instance by stack index reference
  // Just converting and returning reference to array offset
  ValueObj& EnvRoot::getVariable(const VarRef& vidx)
  {
    if (vidx.frame == 0xFFFFFFFF) {
      // if (stkdbg) std::cerr << "Get global variable " << vidx.offset << "\n";
      return intVariables[vidx.offset];
    }
    else {
      // if (stkdbg) std::cerr << "Get variable " << vidx.toString() << "\n";
      return variables[size_t(varFramePtr[vidx.frame]) + vidx.offset];
    }
  }
  // EO getVariable

  // Get function instance by stack index reference
  // Just converting and returning reference to array offset
  ValueObj& EnvRoot::getModVar(const uint32_t offset)
  {
    return intVariables[offset];
  }
  // EO findFunction

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
      if (mixdbg) std::cerr << "Get global mixin " << midx.offset << "\n";
      return intMixin[midx.offset];
    }
    else {
      if (mixdbg) std::cerr << "Get mixin " << midx.toString() << "\n";
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
    ValueObj& slot(intVariables[offset]);
    if (!guarded || !slot || slot->isaNull()) {
      if (stkdbg) std::cerr << "Set global variable " << offset
        << " - " << value->inspect() << "\n";
      slot = value;
    }
    else {
      if (stkdbg) std::cerr << "Guarded global variable " << offset
        << " - " << value->inspect() << "\n";
    }
  }
  void EnvRoot::setModMix(const uint32_t offset, Callable* callable, bool guarded)
  {
    if (stkdbg) std::cerr << "Set global mixin " << offset
       << " - " << callable->name() << "\n";
    CallableObj& slot(intMixin[offset]);
    if (!guarded || !slot) slot = callable;
  }
  void EnvRoot::setModFn(const uint32_t offset, Callable* callable, bool guarded)
  {
    if (stkdbg) std::cerr << "Set global function " << offset
       << " - " << callable->name() << "\n";
    CallableObj& slot(intFunction[offset]);
    if (!guarded || !slot) slot = callable;
  }


  // Set items on runtime/evaluation phase via references
  // Just converting reference to array offset and assigning
  void EnvRoot::setVariable(const VarRef& vidx, ValueObj value, bool guarded)
  {
    if (vidx.frame == 0xFFFFFFFF) {
      ValueObj& slot(intVariables[vidx.offset]);
      if (!guarded || !slot || slot->isaNull()) {
        if (stkdbg) std::cerr << "Set global variable " << vidx.offset << " - " << value->inspect() << "\n";
        slot = value;
      }
      else {
        if (stkdbg) std::cerr << "Guarded global variable " << vidx.offset << " - " << value->inspect() << "\n";
      }
    }
    else {
      if (stkdbg) std::cerr << "Set variable " << vidx.toString() << " - " << value->inspect() << "\n";
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
      if (stkdbg) std::cerr << "Set global variable " << offset << " - " << value->inspect() << "\n";
      if (!guarded || !slot || slot->isaNull())
        intVariables[offset] = value;
    }
    else {
      if (stkdbg) std::cerr << "Set variable " << frame << ":" << offset << " - " << value->inspect() << "\n";
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
      if (!current->isImport) {
        auto it = current->varIdxs.find(name);
        if (it != current->varIdxs.end()) {
          const VarRef vidx{ current->varFrame, it->second };
          ValueObj& value = root.getVariable(vidx);
          if (value != nullptr) { return value; }
        }
      }
      for (auto fwds : current->fwdGlobal55) {
        auto fwd = fwds->varIdxs.find(name);
        if (fwd != fwds->varIdxs.end()) {
          const VarRef vidx{ fwds->varFrame, fwd->second };
          Value* value = root.getVariable(vidx);
          if (value && name.isPrivate()) continue;
          if (value != nullptr) return value;
        }
        if (Moduled* mod = fwds->module) {
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


  // Get a value associated with the variable under [name].
  // If [global] flag is given, the lookup will be in the root.
  // Otherwise lookup will be from the last runtime stack scope.
  // We will move up the runtime stack until we either find a 
  // defined variable with a value or run out of parent scopes.
  VarRef VarRefs::findVarIdx(const EnvKey& name) const
  {
    const VarRefs* current = this;
    while (current) {
      if (!current->isImport) {
        auto it = current->varIdxs.find(name);
        if (it != current->varIdxs.end()) {
          const VarRef vidx{ current->varFrame, it->second };
          ValueObj& value = root.getVariable(vidx);
          if (!value.isNull()) return vidx;
        }
      }
      for (auto fwds : current->fwdGlobal55) {
        auto fwd = fwds->varIdxs.find(name);
        if (fwd != fwds->varIdxs.end()) {
          const VarRef vidx{ fwds->varFrame, fwd->second };
          Value* value = root.getVariable(vidx);
          if (value && name.isPrivate()) continue;
          if (value != nullptr) return vidx;
        }
        if (Moduled* mod = fwds->module) {
          auto fwd = mod->mergedFwdVar.find(name);
          if (fwd != mod->mergedFwdVar.end()) {
            Value* value = root.getModVar(fwd->second);
            if (value && name.isPrivate()) continue;
            VarRef vidx{ 0xFFFFFFFF, fwd->second };
            if (value != nullptr) return vidx;
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
  CallableObj* VarRefs::findFunction(const EnvKey& name) const
  {
    const VarRefs* current = this;
    while (current) {
      if (!current->isImport) {
        auto it = current->fnIdxs.find(name);
        if (it != current->fnIdxs.end()) {
          const VarRef vidx{ current->fnFrame, it->second };
          CallableObj& value = root.getFunction(vidx);
          if (!value.isNull()) return &value;
        }
      }
      for (auto fwds : current->fwdGlobal55) {
        auto fwd = fwds->fnIdxs.find(name);
        if (fwd != fwds->fnIdxs.end()) {
          const VarRef vidx{ fwds->fnFrame, fwd->second };
          CallableObj& value = root.getFunction(vidx);
          if (value && name.isPrivate()) continue;
          if (!value.isNull()) return &value;
        }
        if (Moduled* mod = fwds->module) {
          auto fwd = mod->mergedFwdFn.find(name);
          if (fwd != mod->mergedFwdFn.end()) {
            CallableObj& fn = root.getFunction(
              { 0xFFFFFFFF, fwd->second });
            if (fn && name.isPrivate()) continue;
            if (!fn.isNull()) return &fn;
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
  VarRef VarRefs::findFnIdx(const EnvKey& name) const
  {
    const VarRefs* current = this;
    while (current) {
      if (!current->isImport) {
        auto it = current->fnIdxs.find(name);
        if (it != current->fnIdxs.end()) {
          const VarRef vidx{ current->fnFrame, it->second };
          CallableObj& value = root.getFunction(vidx);
          if (!value.isNull()) return vidx;
        }
      }
      for (auto fwds : current->fwdGlobal55) {
        auto fwd = fwds->fnIdxs.find(name);
        if (fwd != fwds->fnIdxs.end()) {
          const VarRef vidx{ fwds->fnFrame, fwd->second };
          CallableObj& value = root.getFunction(vidx);
          if (value && name.isPrivate()) continue;
          if (!value.isNull()) return vidx;
        }
        if (Moduled* mod = fwds->module) {
          auto fwd = mod->mergedFwdFn.find(name);
          if (fwd != mod->mergedFwdFn.end()) {
            const VarRef fidx{ 0xFFFFFFFF, fwd->second };
            CallableObj& fn = root.getFunction(fidx);
            if (fn && name.isPrivate()) continue;
            if (!fn.isNull()) return fidx;
          }
        }
      }
      if (current->pscope == nullptr) break;
      else current = current->pscope;
    }
    return nullidx;
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
      if (!current->isImport) {
        auto it = current->mixIdxs.find(name);
        if (it != current->mixIdxs.end()) {
          const VarRef midx{ current->mixFrame, it->second };
          Callable* mixin = root.getMixin(midx);
          if (mixin != nullptr) return mixin;
        }
      }
      for (auto fwds : current->fwdGlobal55) {
        auto fwd = fwds->mixIdxs.find(name);
        if (fwd != fwds->mixIdxs.end()) {
          const VarRef vidx{ fwds->mixFrame, fwd->second };
          Callable* mixin = root.getMixin(vidx);
          if (mixin && name.isPrivate()) continue;
          if (mixin != nullptr) return mixin;
        }
        if (Moduled* mod = fwds->module) {
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
    if (isImport) return nullptr;
    auto it = mixIdxs.find(name);
    if (it != mixIdxs.end()) {
      const VarRef vidx{ mixFrame, it->second };
      Callable* value = root.getMixin(vidx);
      if (value != nullptr) return value;
    }
    for (auto fwds : fwdGlobal55) {
      auto it = fwds->mixIdxs.find(name);
      if (it != fwds->mixIdxs.end()) {
        const VarRef vidx{ fwds->mixFrame, it->second };
        Callable* value = root.getMixin(vidx);
        if (value != nullptr) return value;
      }
      if (Moduled* mod = fwds->module) {
        auto fwd = mod->mergedFwdMix.find(name);
        if (fwd != mod->mergedFwdMix.end()) {
          const VarRef vidx{ fwds->mixFrame, it->second };
          Callable* value = root.getMixin(vidx);
          if (value != nullptr) return value;
        }
      }
    }
    return nullptr;
  }
  // EO getMixin

  VarRef VarRefs::getFnIdx(const EnvKey& name) const
  {
    if (isImport) return nullidx;
    auto it = fnIdxs.find(name);
    if (it != fnIdxs.end()) {
      const VarRef fidx{ fnFrame, it->second };
      CallableObj& value = root.getFunction(fidx);
      if (value != nullptr) return fidx;
    }
    for (auto fwds : fwdGlobal55) {
      auto it = fwds->fnIdxs.find(name);
      if (it != fwds->fnIdxs.end()) {
        const VarRef fidx{ fwds->fnFrame, it->second };
        CallableObj& value = root.getFunction(fidx);
        if (value != nullptr) return fidx;
      }
      if (Moduled* mod = fwds->module) {
        auto fwd = mod->mergedFwdFn.find(name);
        if (fwd != mod->mergedFwdFn.end()) {
          const VarRef fidx{ fwds->fnFrame, it->second };
          CallableObj& value = root.getFunction(fidx);
          if (value != nullptr) return fidx;
        }
      }
    }
    return nullidx;
  }

  CallableObj* VarRefs::getFunction(const EnvKey& name) const
  {
    if (isImport) return nullptr;
    auto it = fnIdxs.find(name);
    if (it != fnIdxs.end()) {
      const VarRef vidx{ fnFrame, it->second };
      CallableObj& value = root.getFunction(vidx);
      if (value != nullptr) return &value;
    }
    for (auto fwds : fwdGlobal55) {
      auto it = fwds->fnIdxs.find(name);
      if (it != fwds->fnIdxs.end()) {
        const VarRef vidx{ fwds->fnFrame, it->second };
        CallableObj& value = root.getFunction(vidx);
        if (value != nullptr) return &value;
      }
      if (Moduled* mod = fwds->module) {
        auto fwd = mod->mergedFwdFn.find(name);
        if (fwd != mod->mergedFwdFn.end()) {
          const VarRef vidx{ fwds->fnFrame, it->second };
          CallableObj& value = root.getFunction(vidx);
          if (value != nullptr) return &value;
        }
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
    if (isImport) return nullptr;
    auto it = varIdxs.find(name);
    if (it != varIdxs.end()) {
      const VarRef vidx{ varFrame, it->second };
      Value* value = root.getVariable(vidx);
      if (value != nullptr) return value;
    }
    for (auto fwds : fwdGlobal55) {
      auto it = fwds->varIdxs.find(name);
      if (it != fwds->varIdxs.end()) {
        const VarRef vidx{ fwds->varFrame, it->second };
        Value* value = root.getVariable(vidx);
        if (value != nullptr) return value;
      }
      if (Moduled* mod = fwds->module) {
        auto fwd = mod->mergedFwdFn.find(name);
        if (fwd != mod->mergedFwdFn.end()) {
          const VarRef vidx{ fwds->varFrame, it->second };
          Value* value = root.getVariable(vidx);
          if (value != nullptr) return value;
        }
      }
    }
    return nullptr;
  }


  VarRef VarRefs::getVarIdx(const EnvKey& name) const
  {
    if (isImport) return nullidx;
    auto it = varIdxs.find(name);
    if (it != varIdxs.end()) {
      const VarRef vidx{ varFrame, it->second };
      Value* value = root.getVariable(vidx);
      if (value != nullptr) return vidx;
    }
    for (auto fwds : fwdGlobal55) {
      auto it = fwds->varIdxs.find(name);
      if (it != fwds->varIdxs.end()) {
        const VarRef vidx{ fwds->varFrame, it->second };
        Value* value = root.getVariable(vidx);
        if (value != nullptr) return vidx;
      }
      if (Moduled* mod = fwds->module) {
        auto fwd = mod->mergedFwdFn.find(name);
        if (fwd != mod->mergedFwdFn.end()) {
          const VarRef vidx{ fwds->varFrame, it->second };
          Value* value = root.getVariable(vidx);
          if (value != nullptr) return vidx;
        }
      }
    }
    return nullidx;
  }

  VarRef VarRefs::setModVar(const EnvKey& name, Value* value, bool guarded, const SourceSpan& pstate) const
  {
    auto it = varIdxs.find(name);
    if (it != varIdxs.end()) {
      root.setModVar(it->second, value, guarded, pstate);
      return { 0xFFFFFFFF, it->second };
    }
    for (auto fwds : fwdGlobal55) {
      auto it = fwds->varIdxs.find(name);
      if (it != fwds->varIdxs.end()) {
        root.setModVar(it->second, value, guarded, pstate);
        return { 0xFFFFFFFF, it->second };
      }
    }
    return nullidx;
  }

  bool VarRefs::setModMix(const EnvKey& name, Callable* fn, bool guarded) const
  {
    auto it = mixIdxs.find(name);
    if (it != mixIdxs.end()) {
      root.setModMix(it->second, fn, guarded);
      return true;
    }
    for (auto fwds : fwdGlobal55) {
      auto it = fwds->mixIdxs.find(name);
      if (it != fwds->mixIdxs.end()) {
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
      auto it = fwds->fnIdxs.find(name);
      if (it != fwds->fnIdxs.end()) {
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
        if (it->second.second) {
          return it->second.second->isCompiled;
        }
        else {
          return true;
        }
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


  VarRef VarRefs::findVarIdx(const EnvKey& name, const sass::string& ns) const
  {
    const VarRefs* current = this;
    while (current) {
      // Check if the namespace was registered
      auto it = current->fwdModule55.find(ns);
      if (it != current->fwdModule55.end()) {
        if (VarRefs* idxs = it->second.first) {
          VarRef vidx = idxs->getVarIdx(name);
          if (vidx != nullidx) return vidx;
        }
        if (Moduled* mod = it->second.second) {
          auto fwd = mod->mergedFwdVar.find(name);
          if (fwd != mod->mergedFwdVar.end()) {
            VarRef vidx{ 0xFFFFFFFF, fwd->second };
            ValueObj& val = root.getVariable(vidx);
            if (val != nullptr) return vidx;
          }
        }
      }
      if (current->pscope == nullptr) break;
      else current = current->pscope;
    }
    return nullidx;
  }

  VarRef VarRefs::findFnIdx(const EnvKey& name, const sass::string& ns) const
  {
    const VarRefs* current = this;
    while (current) {
      if (!current->isImport) {
        // Check if the namespace was registered
        auto it = current->fwdModule55.find(ns);
        if (it != current->fwdModule55.end()) {
          if (VarRefs* idxs = it->second.first) {
            VarRef fidx = idxs->getFnIdx(name);
            if (fidx != nullidx) return fidx;
          }
          if (Moduled* mod = it->second.second) {
            auto fwd = mod->mergedFwdFn.find(name);
            if (fwd != mod->mergedFwdFn.end()) {
              VarRef fidx{ 0xFFFFFFFF, fwd->second };
              CallableObj& fn = root.getFunction(fidx);
              if (fn != nullptr) return fidx;
            }
          }
        }
      }
      if (current->pscope == nullptr) break;
      else current = current->pscope;
    }
    return nullidx;
  }

  CallableObj* VarRefs::findFunction(const EnvKey& name, const sass::string& ns) const
  {
    const VarRefs* current = this;
    while (current) {
      // Check if the namespace was registered
      auto it = current->fwdModule55.find(ns);
      if (it != current->fwdModule55.end()) {
        if (VarRefs* idxs = it->second.first) {
          CallableObj* fn = idxs->getFunction(name);
          if (fn != nullptr) return fn;
        }
        if (Moduled* mod = it->second.second) {
          auto fwd = mod->mergedFwdFn.find(name);
          if (fwd != mod->mergedFwdFn.end()) {
            CallableObj& fn = root.getFunction(
              { 0xFFFFFFFF, fwd->second });
            if (fn != nullptr) return &fn;
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
  CallableObj* EnvRoot::findFunction(const EnvKey& name) const
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

  CallableObj* EnvRoot::findFunction(const EnvKey& name, const sass::string& ns) const
  {
    if (stack.empty()) return nullptr;
    return stack.back()->findFunction(name, ns);
  }

  Value* EnvRoot::findVariable(const EnvKey& name, const sass::string& ns) const
  {
    if (stack.empty()) return nullptr;
    return stack.back()->findVariable(name, ns);
  }

  VarRef EnvRoot::findVarIdx(const EnvKey& name, const sass::string& ns) const
  {
    if (stack.empty()) return nullidx;
    return stack.back()->findVarIdx(name, ns);
  }

  VarRef EnvRoot::findVarIdx(const EnvKey& name) const
  {
    if (stack.empty()) return nullidx;
    return stack.back()->findVarIdx(name);
  }

  VarRef EnvRoot::findFnIdx(const EnvKey& name, const sass::string& ns) const
  {
    if (stack.empty()) return nullidx;
    return stack.back()->findFnIdx(name, ns);
  }

  VarRef EnvRoot::findFnIdx(const EnvKey& name) const
  {
    if (stack.empty()) return nullidx;
    return stack.back()->findFnIdx(name);
  }



  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  VarRef EnvRoot::setModVar(const EnvKey& name, const sass::string& ns, Value* value, bool guarded, const SourceSpan& pstate)
  {
    if (stack.empty()) return nullidx;
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

  VarRef VarRefs::setModVar(const EnvKey& name, const sass::string& ns, Value* value, bool guarded, const SourceSpan& pstate)
  {
    const VarRefs* current = this;
    while (current) {
      // Check if the namespace was registered
      auto it = current->fwdModule55.find(ns);
      if (it != current->fwdModule55.end()) {
        // We set forwarded vars first!
        if (Moduled* mod = it->second.second) {
          auto fwd = mod->mergedFwdVar.find(name);
          if (fwd != mod->mergedFwdVar.end()) {
            root.setModVar(fwd->second, value, guarded, pstate);
            return { 0xFFFFFFFF, fwd->second };
          }
        }
        if (VarRefs* idxs = it->second.first) {
          VarRef vidx = idxs->setModVar(name, value, guarded, pstate);
          if (vidx.isValid()) return vidx;
        }
      }
      if (current->pscope == nullptr) break;
      else current = current->pscope;
    }
    return nullidx;
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
  VarRef EnvRoot::setVariable(const EnvKey& name, bool guarded, bool global)
  {
    if (stack.empty()) return nullidx;
    uint32_t idx = global ? 0 :
      (uint32_t)stack.size() - 1;
    const VarRefs* current = stack[idx];
    while (current) {

      for (auto refs : current->fwdGlobal55) {
        auto in = refs->varIdxs.find(name);
        if (in != refs->varIdxs.end()) {
          if (name.isPrivate()) {
            throw Exception::ParserException(compiler,
              "Private members can't be accessed "
              "from outside their modules.");
          }
          return { 0xFFFFFFFF, in->second };
          // idxs->root.setVariable(vidx, val, guarded);
          // return vidx;
        }
        // Modules inserted by import
        if (Moduled* mod = refs->module) {
          auto fwd = mod->mergedFwdVar.find(name);
          if (fwd != mod->mergedFwdVar.end()) {
            if (name.isPrivate()) {
              throw Exception::ParserException(compiler,
                "Private members can't be accessed "
                "from outside their modules.");
            }
            return { 0xFFFFFFFF, fwd->second };
            // idxs->root.setVariable(vidx, val, guarded);
            // return vidx;
          }
        }
      }

      if (current->isImport) {
        current = current->pscope;
        continue;
      }

      auto it = current->varIdxs.find(name);
      if (it != current->varIdxs.end()) {
        return { current->varFrame, it->second };
        // idxs->root.setVariable(vidx, val, guarded);
        // return vidx;
      }

      if (current->pscope == nullptr) break;
      else current = current->pscope;
    }
    return nullidx;
  }
  // EO setVariable




  // Set a value associated with the variable under [name].
  // If [global] flag is given, the lookup will be in the root.
  // Otherwise lookup will be from the last runtime stack scope.
  // We will move up the runtime stack until we either find a 
  // defined variable with a value or run out of parent scopes.
  VarRef EnvRoot::setFunction(const EnvKey& name, bool guarded, bool global)
  {
    if (stack.empty()) return nullidx;
    uint32_t idx = global ? 0 :
      (uint32_t)stack.size() - 1;
    const VarRefs* current = stack[idx];
    while (current) {

      for (auto refs : current->fwdGlobal55) {
        auto in = refs->fnIdxs.find(name);
        if (in != refs->fnIdxs.end()) {
          if (name.isPrivate()) {
            throw Exception::ParserException(compiler,
              "Private members can't be accessed "
              "from outside their modules.");
          }
          return { 0xFFFFFFFF, in->second };
          // idxs->root.setVariable(vidx, val, guarded);
          // return vidx;
        }
        // Modules inserted by import
        if (Moduled* mod = refs->module) {
          auto fwd = mod->mergedFwdFn.find(name);
          if (fwd != mod->mergedFwdFn.end()) {
            if (name.isPrivate()) {
              throw Exception::ParserException(compiler,
                "Private members can't be accessed "
                "from outside their modules.");
            }
            return { 0xFFFFFFFF, fwd->second };
            // idxs->root.setFunction(vidx, val, guarded);
            // return vidx;
          }
        }
      }

      if (current->isImport) {
        current = current->pscope;
        continue;
      }

      auto it = current->fnIdxs.find(name);
      if (it != current->fnIdxs.end()) {
        return { current->fnFrame, it->second };
        // idxs->root.setVariable(vidx, val, guarded);
        // return vidx;
      }

      if (current->pscope == nullptr) break;
      else current = current->pscope;
    }
    return nullidx;
  }
  // EO setVariable




  // Set a value associated with the variable under [name].
  // If [global] flag is given, the lookup will be in the root.
  // Otherwise lookup will be from the last runtime stack scope.
  // We will move up the runtime stack until we either find a 
  // defined variable with a value or run out of parent scopes.
  VarRef EnvRoot::setMixin(const EnvKey& name, bool guarded, bool global)
  {
    if (stack.empty()) return nullidx;
    uint32_t idx = global ? 0 :
      (uint32_t)stack.size() - 1;
    const VarRefs* current = stack[idx];
    while (current) {

      for (auto refs : current->fwdGlobal55) {
        auto in = refs->mixIdxs.find(name);
        if (in != refs->mixIdxs.end()) {
          if (name.isPrivate()) {
            throw Exception::ParserException(compiler,
              "Private members can't be accessed "
              "from outside their modules.");
          }
          return { 0xFFFFFFFF, in->second };
          // idxs->root.setVariable(vidx, val, guarded);
          // return vidx;
        }
        // Modules inserted by import
        if (Moduled* mod = refs->module) {
          auto fwd = mod->mergedFwdMix.find(name);
          if (fwd != mod->mergedFwdMix.end()) {
            if (name.isPrivate()) {
              throw Exception::ParserException(compiler,
                "Private members can't be accessed "
                "from outside their modules.");
            }
            return { 0xFFFFFFFF, fwd->second };
            // idxs->root.setFunction(vidx, val, guarded);
            // return vidx;
          }
        }
      }

      if (current->isImport) {
        current = current->pscope;
        continue;
      }

      auto it = current->mixIdxs.find(name);
      if (it != current->mixIdxs.end()) {
        return { current->mixFrame, it->second };
        // idxs->root.setVariable(vidx, val, guarded);
        // return vidx;
      }

      if (current->pscope == nullptr) break;
      else current = current->pscope;
    }
    return nullidx;
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
