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

  const EnvRef nullidx{ 0xFFFFFFFF, 0xFFFFFFFF };

  // The root is used for all runtime state
  // Also contains parsed root scope stack
  EnvRoot::EnvRoot(
    Compiler& compiler) :
    compiler(compiler),
    stack(compiler.varStack3312),
    idxs(new EnvRefs(
      *this,
      nullptr,
      0xFFFFFFFF,
      false,
      false))
  {
    varStack.reserve(256);
    fnStack.reserve(256);
    mixStack.reserve(128);
    varStackPtr.reserve(256);
    mixStackPtr.reserve(128);
    fnStackPtr.reserve(256);
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
    bool isSemiGlobal,
    bool isModule,
    bool isImport) :
    stack(compiler.varRoot.stack),
    idxs(new EnvRefs(
      compiler.varRoot,
      compiler.varRoot.stack.back(),
      uint32_t(compiler.varRoot.varStackPtr.size()),
      isSemiGlobal, isImport))
  {
    if (isModule) {
      // Lives in built-in scope
      idxs->framePtr = 0xFFFFFFFF;
    }
    else {
      // Initialize stacks as not active yet
      idxs->root.varStackPtr.push_back(0xFFFFFFFF);
      idxs->root.mixStackPtr.push_back(0xFFFFFFFF);
      idxs->root.fnStackPtr.push_back(0xFFFFFFFF);
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
  EnvRef EnvRefs::createVariable(
    const EnvKey& name)
  {
    if (framePtr == 0xFFFFFFFF) {
      uint32_t offset = (uint32_t)root.intVariables.size();
      if (stkdbg) std::cerr << "Create global variable " << name.orig() << " at " << offset << "\n";
      root.intVariables.resize(offset + 1);
      varIdxs[name] = offset;
      return { 0xFFFFFFFF, offset };
    }
    // Get local offset to new variable
    uint32_t offset = (uint32_t)varIdxs.size();
    if (stkdbg) std::cerr << "Create local variable " << name.orig() << " at " << offset << "\n";
    // Remember the variable name
    varIdxs[name] = offset;
    // Return stack index reference
    return { framePtr, offset };
  }
  // EO createVariable

  // Register new function on local stack
  // Mostly invoked by built-in functions
  // Then invoked for custom C-API function
  // Finally for every parsed function rule
  EnvRef EnvRefs::createFunction(
    const EnvKey& name)
  {
    if (framePtr == 0xFFFFFFFF) {
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
    return { framePtr, offset };
  }
  // EO createFunction

  // Register new mixin on local stack
  // Only invoked for mixin rules
  // But also for content blocks
  EnvRef EnvRefs::createMixin(
    const EnvKey& name)
  {
    if (framePtr == 0xFFFFFFFF) {
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
    return { framePtr, offset };
  }
  // EO createMixin

 
  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Get value instance by stack index reference
  // Just converting and returning reference to array offset
  ValueObj& EnvRoot::getVariable(const EnvRef& vidx)
  {
    if (vidx.frame == 0xFFFFFFFF) {
      // if (stkdbg) std::cerr << "Get global variable " << vidx.offset << "\n";
      return intVariables[vidx.offset];
    }
    else {
      // if (stkdbg) std::cerr << "Get variable " << vidx.toString() << "\n";
      return varStack[size_t(varStackPtr[vidx.frame]) + vidx.offset];
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
  CallableObj& EnvRoot::getModFn(const uint32_t offset)
  {
    return intFunction[offset];
  }
  // EO findFunction

  // Get function instance by stack index reference
  // Just converting and returning reference to array offset
  CallableObj& EnvRoot::getModMix(const uint32_t offset)
  {
    return intMixin[offset];
  }
  // EO findFunction

  // Get function instance by stack index reference
  // Just converting and returning reference to array offset
  CallableObj& EnvRoot::getFunction(const EnvRef& fidx)
  {
    if (fidx.frame == 0xFFFFFFFF) {
      return intFunction[fidx.offset];
    }
    else {
      return fnStack[size_t(fnStackPtr[fidx.frame]) + fidx.offset];
    }
  }
  // EO findFunction

  // Get mixin instance by stack index reference
  // Just converting and returning reference to array offset
  CallableObj& EnvRoot::getMixin(const EnvRef& midx)
  {
    if (midx.frame == 0xFFFFFFFF) {
      if (mixdbg) std::cerr << "Get global mixin " << midx.offset << "\n";
      return intMixin[midx.offset];
    }
    else {
      if (mixdbg) std::cerr << "Get mixin " << midx.toString() << "\n";
      return mixStack[size_t(mixStackPtr[midx.frame]) + midx.offset];
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

  // Set items on runtime/evaluation phase via references
  // Just converting reference to array offset and assigning
  void EnvRoot::setVariable(const EnvRef& vidx, Value* value, bool guarded)
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
      ValueObj& slot(varStack[size_t(varStackPtr[vidx.frame]) + vidx.offset]);
      if (slot == nullptr || guarded == false) slot = value;
    }
  }
  // EO setVariable

  // Set items on runtime/evaluation phase via references
  // Just converting reference to array offset and assigning
  void EnvRoot::setVariable(uint32_t frame, uint32_t offset, Value* value, bool guarded)
  {
    if (frame == 0xFFFFFFFF) {
      ValueObj& slot(intVariables[offset]);
      if (stkdbg) std::cerr << "Set global variable " << offset << " - " << value->inspect() << "\n";
      if (!guarded || !slot || slot->isaNull())
        intVariables[offset] = value;
    }
    else {
      if (stkdbg) std::cerr << "Set variable " << frame << ":" << offset << " - " << value->inspect() << "\n";
      ValueObj& slot(varStack[size_t(varStackPtr[frame]) + offset]);
      if (!guarded || !slot || slot->isaNull()) slot = value;
    }
  }
  // EO setVariable

  // Set items on runtime/evaluation phase via references
  // Just converting reference to array offset and assigning
  void EnvRoot::setFunction(const EnvRef& fidx, UserDefinedCallable* value, bool guarded)
  {
    if (fidx.frame == 0xFFFFFFFF) {
      if (!guarded || intFunction[fidx.offset] == nullptr)
        intFunction[fidx.offset] = value;
    }
    else {
      CallableObj& slot(fnStack[size_t(fnStackPtr[fidx.frame]) + fidx.offset]);
      if (!guarded || !slot) slot = value;
    }
  }
  // EO setFunction

  // Set items on runtime/evaluation phase via references
  // Just converting reference to array offset and assigning
  void EnvRoot::setMixin(const EnvRef& midx, UserDefinedCallable* value, bool guarded)
  {
    if (midx.frame == 0xFFFFFFFF) {
      if (!guarded || intMixin[midx.offset] == nullptr)
        intMixin[midx.offset] = value;
    }
    else {
      CallableObj& slot(mixStack[size_t(mixStackPtr[midx.frame]) + midx.offset]);
      if (!guarded || !slot) slot = value;
    }
  }
  // EO setMixin

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Get a function associated with the under [name].
  // Will lookup from the last runtime stack scope.
  // We will move up the runtime stack until we either
  // find a defined function or run out of parent scopes.
  EnvRef EnvRefs::findMixIdx(const EnvKey& name) const
  {
    for (const EnvRefs* current = this; current; current = current->pscope)
    {
      if (!name.isPrivate()) {
        for (auto fwds : current->forwards) {
          auto fwd = fwds->mixIdxs.find(name);
          if (fwd != fwds->mixIdxs.end()) {
            return { fwds->framePtr, fwd->second };
          }
          if (Module* mod = fwds->module) {
            auto fwd = mod->mergedFwdMix.find(name);
            if (fwd != mod->mergedFwdMix.end()) {
              return { 0xFFFFFFFF, fwd->second };
            }
          }
        }
      }
      if (current->isImport) continue;
      auto it = current->mixIdxs.find(name);
      if (it != current->mixIdxs.end()) {
        return { current->framePtr, it->second };
      }
    }
    return nullidx;
  }
  // EO findFunction


  // Get a function associated with the under [name].
  // Will lookup from the last runtime stack scope.
  // We will move up the runtime stack until we either
  // find a defined function or run out of parent scopes.
  EnvRef EnvRefs::findFnIdx(const EnvKey& name) const
  {
    for (const EnvRefs* current = this; current; current = current->pscope)
    {
      if (!name.isPrivate()) {
        for (auto fwds : current->forwards) {
          auto fwd = fwds->fnIdxs.find(name);
          if (fwd != fwds->fnIdxs.end()) {
            return { fwds->framePtr, fwd->second };
          }
          if (Module* mod = fwds->module) {
            auto fwd = mod->mergedFwdFn.find(name);
            if (fwd != mod->mergedFwdFn.end()) {
              return { 0xFFFFFFFF, fwd->second };
            }
          }
        }
      }
      if (current->isImport) continue;
      auto it = current->fnIdxs.find(name);
      if (it != current->fnIdxs.end()) {
        return { current->framePtr, it->second };
      }
    }
    return nullidx;
  }
  // EO findFunction

  // Get a value associated with the variable under [name].
  // If [global] flag is given, the lookup will be in the root.
  // Otherwise lookup will be from the last runtime stack scope.
  // We will move up the runtime stack until we either find a 
  // defined variable with a value or run out of parent scopes.
  EnvRef EnvRefs::findVarIdx(const EnvKey& name) const
  {
    for (const EnvRefs* current = this; current; current = current->pscope)
    {
      for (auto fwds : current->forwards) {
        auto fwd = fwds->varIdxs.find(name);
        if (fwd != fwds->varIdxs.end()) {
          if (name.isPrivate()) {
            throw Exception::ParserException(root.compiler,
              "Private members can't be accessed "
              "from outside their modules.");
          }
          return { fwds->framePtr, fwd->second };
        }
        if (Module* mod = fwds->module) {
          auto fwd = mod->mergedFwdVar.find(name);
          if (fwd != mod->mergedFwdVar.end()) {
            if (name.isPrivate()) {
              throw Exception::ParserException(root.compiler,
                "Private members can't be accessed "
                "from outside their modules.");
            }
            return { 0xFFFFFFFF, fwd->second };
          }
        }
      }
      if (current->isImport) continue;
      auto it = current->varIdxs.find(name);
      if (it != current->varIdxs.end()) {
        return { current->framePtr, it->second };
      }
    }
    return nullidx;
  }
  // EO findVarIdx

  // Get a value associated with the variable under [name].
  // If [global] flag is given, the lookup will be in the root.
  // Otherwise lookup will be from the last runtime stack scope.
  // We will move up the runtime stack until we either find a 
  // defined variable with a value or run out of parent scopes.
  void EnvRefs::findVarIdxs(sass::vector<EnvRef>& vidxs, const EnvKey& name) const
  {
    for (const EnvRefs* current = this; current; current = current->pscope)
    {
      if (current->isImport == false) {
        auto it = current->varIdxs.find(name);
        if (it != current->varIdxs.end()) {
          vidxs.emplace_back(EnvRef{
            current->framePtr, it->second });
        }
      }
      if (name.isPrivate()) continue;
      for (auto fwds : current->forwards) {
        auto fwd = fwds->varIdxs.find(name);
        if (fwd != fwds->varIdxs.end()) {
          vidxs.emplace_back(EnvRef{
            fwds->framePtr, fwd->second });
        }
        if (Module* mod = fwds->module) {
          auto fwd = mod->mergedFwdVar.find(name);
          if (fwd != mod->mergedFwdVar.end()) {
            vidxs.emplace_back(EnvRef{
              0xFFFFFFFF, fwd->second });
          }
        }
      }
    }
  }
  // EO getVariable

  EnvRef EnvRefs::setModVar(const EnvKey& name, Value* value, bool guarded, const SourceSpan& pstate) const
  {
    auto it = varIdxs.find(name);
    if (it != varIdxs.end()) {
      root.setModVar(it->second, value, guarded, pstate);
      return { 0xFFFFFFFF, it->second };
    }
    for (auto fwds : forwards) {
      auto it = fwds->varIdxs.find(name);
      if (it != fwds->varIdxs.end()) {
        root.setModVar(it->second, value, guarded, pstate);
        return { 0xFFFFFFFF, it->second };
      }
    }
    return nullidx;
  }


  bool EnvRefs::hasNameSpace(const sass::string& ns, const EnvKey& name) const
  {
    for (const EnvRefs* current = this; current; current = current->pscope)
    {
      if (current->isImport) continue;
      Module* mod = current->module;
      if (mod == nullptr) continue;
      // Check if the namespace was registered
      auto it = mod->moduse.find(ns);
      if (it == mod->moduse.end()) continue;
      auto fwd = it->second.first->varIdxs.find(name);
      // if (fwd != it->second.first->varIdxs.end()) {
      //   ValueObj& slot(root.getVariable({ 0xFFFFFFFF, fwd->second }));
      //   return slot != nullptr;
      // }
      if (it->second.second) {
        return it->second.second->isCompiled;
      }
      else {
        return true;
      }
    }
    return false;
  }

  EnvRef EnvRefs::findVarIdx(const EnvKey& name, const sass::string& ns) const
  {
    for (const EnvRefs* current = this; current; current = current->pscope)
    {
      if (current->isImport) continue;
      Module* mod = current->module;
      if (mod == nullptr) continue;
      auto it = mod->moduse.find(ns);
      if (it == mod->moduse.end()) continue;
      if (EnvRefs* idxs = it->second.first) {
        auto it = idxs->varIdxs.find(name);
        if (it != idxs->varIdxs.end()) {
          return { idxs->framePtr, it->second };
        }
      }
      if (Module* mod = it->second.second) {
        auto fwd = mod->mergedFwdVar.find(name);
        if (fwd != mod->mergedFwdVar.end()) {
          EnvRef vidx{ 0xFFFFFFFF, fwd->second };
          ValueObj& val = root.getVariable(vidx);
          if (val != nullptr) return vidx;
        }
      }
    }
    return nullidx;
  }


  EnvRef EnvRefs::findMixIdx22(const EnvKey& name, const sass::string& ns) const
  {
    for (const EnvRefs* current = this; current; current = current->pscope)
    {
      if (current->isImport) continue;
      Module* mod = current->module;
      if (mod == nullptr) continue;
      auto it = mod->moduse.find(ns);
      if (it == mod->moduse.end()) continue;
      if (EnvRefs* idxs = it->second.first) {
        auto it = idxs->mixIdxs.find(name);
        if (it != idxs->mixIdxs.end()) {
          return { idxs->framePtr, it->second };
        }
      }
      if (Module* mod = it->second.second) {
        auto fwd = mod->mergedFwdMix.find(name);
        if (fwd != mod->mergedFwdMix.end()) {
          return { 0xFFFFFFFF, fwd->second };
        }
      }
    }
    return nullidx;
  }

  EnvRef EnvRefs::findFnIdx22(const EnvKey& name, const sass::string& ns) const
  {
    for (const EnvRefs* current = this; current; current = current->pscope)
    {
      if (current->isImport) continue;
      Module* mod = current->module;
      if (mod == nullptr) continue;
      auto it = mod->moduse.find(ns);
      if (it == mod->moduse.end()) continue;
      if (EnvRefs* idxs = it->second.first) {
        auto it = idxs->fnIdxs.find(name);
        if (it != idxs->fnIdxs.end()) {
          return { idxs->framePtr, it->second };
        }
      }
      if (Module* mod = it->second.second) {
        auto fwd = mod->mergedFwdFn.find(name);
        if (fwd != mod->mergedFwdFn.end()) {
          return { 0xFFFFFFFF, fwd->second };
        }
      }
    }
    return nullidx;
  }

  EnvRef EnvRoot::findVarIdx(const EnvKey& name, const sass::string& ns, bool global) const
  {
    if (stack.empty()) return nullidx;
    auto& frame = global ? stack.front() : stack.back();
    if (ns.empty()) return frame->findVarIdx(name);
    else return frame->findVarIdx(name, ns);
  }

  // Find a function reference for [name] within the current scope stack.
  // If [ns] is not empty, we will only look within loaded modules.
  EnvRef EnvRoot::findFnIdx(const EnvKey& name, const sass::string& ns) const
  {
    if (stack.empty()) return nullidx;
    if (ns.empty()) return stack.back()->findFnIdx(name);
    else return stack.back()->findFnIdx22(name, ns);
  }

  // Find a function reference for [name] within the current scope stack.
  // If [ns] is not empty, we will only look within loaded modules.
  EnvRef EnvRoot::findMixIdx(const EnvKey& name, const sass::string& ns) const
  {
    if (stack.empty()) return nullidx;
    if (ns.empty()) return stack.back()->findMixIdx(name);
    else return stack.back()->findMixIdx22(name, ns);
  }

  void EnvRoot::findVarIdxs(sass::vector<EnvRef>& vidxs, const EnvKey& name) const
  {
    if (stack.empty()) return;
    stack.back()->findVarIdxs(vidxs, name);
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  EnvRef EnvRoot::setModVar2(const EnvKey& name, const sass::string& ns, Value* value, bool guarded, const SourceSpan& pstate)
  {
    if (stack.empty()) return nullidx;
    return stack.back()->setModVar(name, ns, value, guarded, pstate);
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  EnvRef EnvRefs::setModVar(const EnvKey& name, const sass::string& ns, Value* value, bool guarded, const SourceSpan& pstate)
  {
    for (const EnvRefs* current = this; current; current = current->pscope)
    {
      if (current->isImport) continue;
      Module* mod = current->module;
      if (mod == nullptr) continue;
      // Check if the namespace was registered
      auto it = mod->moduse.find(ns);
      if (it == mod->moduse.end()) continue;
      // We set forwarded vars first!
      if (Module* mod = it->second.second) {
        auto fwd = mod->mergedFwdVar.find(name);
        if (fwd != mod->mergedFwdVar.end()) {
          root.setModVar(fwd->second, value, guarded, pstate);
          return { 0xFFFFFFFF, fwd->second };
        }
      }
      if (EnvRefs* idxs = it->second.first) {
        EnvRef vidx = idxs->setModVar(name, value, guarded, pstate);
        if (vidx.isValid()) return vidx;
      }
    }
    return nullidx;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Very small helper for debugging
  sass::string EnvRef::toString() const
  {
    sass::sstream strm;
    strm << frame << ":" << offset;
    return strm.str();
  }
  // EO toString

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

}
