#include "environment_stack.hpp"
#include "context.hpp"

namespace Sass {

  // ##########################################################################
	// Each parsed scope gets its own environment frame
	// ##########################################################################

  EnvFrame::EnvFrame(EnvRoot& root) :
    root(root),
    parent(root),
    chroot(true),
    varFrameOffset(0),
    mixFrameOffset(0),
    fnFrameOffset(0),
    frameOffset(0)
  {
  }
  // EO EnvFrame ctor

  EnvFrame::EnvFrame(EnvFrame* parent, bool chroot) :
    root(parent->root),
    parent(*parent),
    chroot(chroot),
    varFrameOffset(uint32_t(root.varFrames.size())),
    mixFrameOffset(uint32_t(root.mixFrames.size())),
    fnFrameOffset(uint32_t(root.fnFrames.size())),
    frameOffset(uint32_t(root.idxs.size()))
  {
    // Initialize stacks as not active yet
    root.varFrames.push_back(0xFFFFFFFF);
    root.mixFrames.push_back(0xFFFFFFFF);
    root.fnFrames.push_back(0xFFFFFFFF);
    root.idxs.push_back(nullptr);
  }
  // EO EnvFrame ctor

  // The root is used for all runtime state
  // Also contains parsed root scope stack
  EnvRoot::EnvRoot() :
    EnvFrame(*this)
  {
    // Initialize as not active yet
    root.varFrames.push_back(0xFFFFFFFF);
    root.mixFrames.push_back(0xFFFFFFFF);
    root.fnFrames.push_back(0xFFFFFFFF);
    root.idxs.push_back(nullptr);
  }
  // EO EnvRoot ctor

  // ##########################################################################
  // ##########################################################################

  // Create new variable or use lexical
  IdxRef EnvFrame::hoistVariable(
    const EnvKey& name, bool global)
  {
    // Check if the variable already exists
    IdxRef idx = getVariableIdx(name, global);
    // Return already existing mixin
    if (idx.isValid()) return idx;
    // Register new stack variable
    return createVariable(name, global);
  }
  // EO hoistVariable

  // Create new variable on local stack
  IdxRef EnvFrame::createVariable(
    const EnvKey& name, bool global)
  {
    // Dispatch to root if global flag is set
    if (global) return root.createVariable(name);
    // Get local offset to new variable
    uint32_t offset = (uint32_t)varIdxs.size();
    // Remember the variable name
    varIdxs[name] = offset;
    // Return stack index reference
    return { varFrameOffset, offset };
  }
  // EO createVariable

  // Create new function on local stack
  IdxRef EnvFrame::createFunction(
    const EnvKey& name)
  {
    // Check for existing function
    auto it = fnIdxs.find(name);
    if (it != fnIdxs.end()) {
      return { fnFrameOffset, it->second };
    }
    // Get local offset to new function
    uint32_t offset = (uint32_t)fnIdxs.size();
    // Remember the function name
    fnIdxs[name] = offset;
    // Return stack index reference
    return { fnFrameOffset, offset };
  }
  // EO createFunction

  // Create new mixin or use lexical
  IdxRef EnvFrame::createMixin(
    const EnvKey& name)
  {
    // Check for existing mixin
    auto it = mixIdxs.find(name);
    if (it != mixIdxs.end()) {
      return { mixFrameOffset, it->second };
    }
    // Get local offset to new mixin
    uint32_t offset = (uint32_t)mixIdxs.size();
    // Remember the mixin name
    mixIdxs[name] = offset;
    // Return stack index reference
    return { mixFrameOffset, offset };
  }
  // EO createMixin

  // ##########################################################################
  // ##########################################################################

  // Get local variable by `name` of current `stack`
  // Note: this is not local to the environment root!
  ValueObj EnvRoot::getLocalVariable(
    const EnvKey& name) const
  {
    if (stack.empty()) return {};
    uint32_t idx = (uint32_t)stack.size() - 1;
    const IDXS* current = stack[idx];
    auto it = current->varIdxs.find(name);
    if (it != current->varIdxs.end()) {
      const IdxRef vidx{ current->varFrame, it->second };
      ValueObj& value = root.getVariable(vidx);
      if (!value.isNull()) return value;
    }
    return {};
  }
  // EO getLocalVariable

  // Get global variable by `name` of current `stack`
  ValueObj EnvRoot::getGlobalVariable(
    const EnvKey& name) const
  {
    if (stack.empty()) return {};
    const IDXS* current = stack[0]; 
    auto it = current->varIdxs.find(name);
    if (it != current->varIdxs.end()) {
      const IdxRef vidx{ current->varFrame, it->second };
      ValueObj value = root.getVariable(vidx);
      if (!value.isNull()) return value;
    }
    return {};
  }
  // EO getGlobalVariable

  // Get lexical variable by `name` of current `stack`
  ValueObj EnvRoot::getLexicalVariable(
    const EnvKey& name) const
  {
    if (stack.empty()) return {};
    uint32_t idx = (uint32_t)stack.size() - 1;
    const IDXS* current = stack[idx];
    while (current) {
      auto it = current->varIdxs.find(name);
      if (it != current->varIdxs.end()) {
        const IdxRef vidx{ current->varFrame, it->second };
        ValueObj value = root.getVariable(vidx);
        if (!value.isNull()) return value;
      }
      if (current->parent == 0xFFFFFFFF) break;
      else current = root.idxs[idx = current->parent];
    }
    return {};
  }
  // EO getLexicalVariable

  // ##########################################################################
  // ##########################################################################

  // Set local variable by `name` of current `stack`
  // Note: this is not local to the environment root!
  void EnvRoot::setLocalVariable(
    const EnvKey& name, ValueObj val)
  {
    if (stack.empty()) return;
    uint32_t idx = (uint32_t)stack.size() - 1;
    const IDXS* current = stack[idx];
    auto it = current->varIdxs.find(name);
    if (it != current->varIdxs.end()) {
      const IdxRef vidx{ current->varFrame, it->second };
      ValueObj& value = root.getVariable(vidx);
      if (!value.isNull()) { value = val; return; }
    }
  }
  // EO setLocalVariable

  // Set local variable by `name` of current `stack`
  // Note: this is not local to the environment root!
  void EnvRoot::setGlobalVariable(
    const EnvKey& name, ValueObj val)
  {
    if (stack.empty()) return;
    const IDXS* current = stack[0];
    auto it = current->varIdxs.find(name);
    if (it != current->varIdxs.end()) {
      const IdxRef vidx{ current->varFrame, it->second };
      ValueObj& value = root.getVariable(vidx);
      if (!value.isNull()) { value = val; return; }
    }
  }
  // EO setGlobalVariable

  // Set lexical variable by `name` of current `stack`
  void EnvRoot::setLexicalVariable(
    const EnvKey& name, ValueObj val)
  {
    if (stack.empty()) return;
    uint32_t idx = (uint32_t)stack.size() - 1;
    const IDXS* current = stack[idx];
    while (current) {
      auto it = current->varIdxs.find(name);
      if (it != current->varIdxs.end()) {
        const IdxRef vidx{ current->varFrame, it->second };
        ValueObj& value = root.getVariable(vidx);
        if (!value.isNull()) { value = val; return; }
      }
      if (current->parent == 0xFFFFFFFF) break;
      else current = root.idxs[idx = current->parent];
    }
  }
  // EO setLexicalVariable

  // ##########################################################################
  // ##########################################################################

  // Get variable instance by stack index reference
  ValueObj& EnvRoot::getVariable(const IdxRef& ref)
  {
    return variables[size_t(varFrames[ref.frame]) + ref.offset];
  }
  // EO getVariable

  // Get function instance by stack index reference
  CallableObj& EnvRoot::getFunction(const IdxRef& ref)
  {
    return functions[size_t(fnFrames[ref.frame]) + ref.offset];
  }
  // EO getFunction

  // Get mixin instance by stack index reference
  CallableObj& EnvRoot::getMixin(const IdxRef& ref)
  {
    return mixins[size_t(mixFrames[ref.frame]) + ref.offset];
  }
  // EO getMixin

  // Set variable instance by stack index reference
  void EnvRoot::setVariable(const IdxRef& vidx, ValueObj value)
  {
    variables[size_t(varFrames[vidx.frame]) + vidx.offset] = value;
  }
  // EO setVariable

  // Set variable instance by stack index reference
  void EnvRoot::setVariable(uint32_t frame, uint32_t offset, ValueObj obj)
  {
    variables[size_t(varFrames[frame]) + offset] = obj;
  }
  // EO setVariable

  // Set function instance by stack index reference
  void EnvRoot::setFunction(const IdxRef& vidx, CallableObj value)
  {
    functions[size_t(fnFrames[vidx.frame]) + vidx.offset] = value;
  }
  // EO setFunction

  // Set mixin instance by stack index reference
  void EnvRoot::setMixin(const IdxRef& vidx, CallableObj value)
  {
    mixins[size_t(mixFrames[vidx.frame]) + vidx.offset] = value;
  }
  // EO setMixin

  // ##########################################################################
  // ##########################################################################

  IdxRef EnvFrame::getVariableIdx2(const EnvKey& name, bool global)
  {
    EnvFrame* current = this;
    if (global) current = &root;
    while (current != nullptr) {
      // Check if we already have this var
      auto it = current->varIdxs.find(name);
      if (it != current->varIdxs.end()) {
        return {
          current->varFrameOffset,
          it->second
        };
      }
      // Return error if chroot is enforced
      // if (current->chroot) return IdxRef{};
      current = current->getParent();
    }
    return IdxRef{};
  }

  IdxRef EnvFrame::getVariableIdx(const EnvKey& name, bool global)
  {
    EnvFrame* current = this;
    if (global) current = &root;
    while (current != nullptr) {
      // Check if we already have this var
      auto it = current->varIdxs.find(name);
      if (it != current->varIdxs.end()) {
        return {
          current->varFrameOffset,
          it->second
        };
      }
      // Return error if chroot is enforced
      if (current->chroot) return IdxRef{};
      current = current->getParent();
    }
    return IdxRef{};
  }

  IdxRef EnvFrame::getFunctionIdx(const EnvKey& name)
  {
    EnvFrame* current = this;
    while (current != nullptr) {
      // Check if we already have this var
      auto it = current->fnIdxs.find(name);
      if (it != current->fnIdxs.end()) {
        return {
          current->fnFrameOffset,
          it->second
        };
      }
      current = current->getParent();
    }
    // Not found
    return IdxRef{};
  }

  IdxRef EnvFrame::getMixinIdx(const EnvKey& name)
  {
    EnvFrame* current = this;
    while (current != nullptr) {
      // Check if we already have this var
      auto it = current->mixIdxs.find(name);
      if (it != current->mixIdxs.end()) {
        return {
          current->mixFrameOffset,
          it->second
        };
      }
      current = current->getParent();
    }
    // Not found
    return IdxRef{};
  }

  // ##########################################################################
  // ##########################################################################

  CallableObj EnvRoot::getLexicalFunction(const EnvKey& name) const {

    if (stack.empty()) return {};
    uint32_t idx = uint32_t(stack.size() - 1);
    const IDXS* current = stack[idx];
    while (current) {
      auto it = current->fnIdxs.find(name);
      if (it != current->fnIdxs.end()) {
        const IdxRef vidx{ current->fnFrame, it->second };
        CallableObj value = root.getFunction(vidx);
        if (!value.isNull()) return value;
      }
      if (current->parent == 0xFFFFFFFF) break;
      else current = root.idxs[idx = current->parent];
    }
    return {};
  }

  CallableObj EnvRoot::getLexicalMixin(const EnvKey& name) const {

    if (stack.empty()) return {};
    uint32_t idx = uint32_t(stack.size() - 1);
    const IDXS* current = stack[idx];
    while (current) {
      auto it = current->mixIdxs.find(name);
      if (it != current->mixIdxs.end()) {
        const IdxRef vidx{ current->mixFrame, it->second };
        CallableObj value = root.getMixin(vidx);
        if (!value.isNull()) return value;
      }
      if (current->parent == 0xFFFFFFFF) break;
      else current = root.idxs[idx = current->parent];
    }
    return {};
  }

  IDXS* EnvFrame::getIdxs() {

    if (root.idxs[frameOffset] != nullptr) {
      return root.idxs[frameOffset];
    }

    uint32_t offset = 0xFFFFFFFF;
		EnvFrame* parent = getParent();
    if (parent) offset = parent->frameOffset;

    if (outsiders.size()) {
      for (auto var : outsiders) {
        // Check if we now have a local variable for me
        if (varIdxs.count(var.first) == 1) {
          auto loc = varIdxs.at(var.first);
          for (const VariableObj& idx : var.second) {
            idx->lidx({ varFrameOffset, loc });
          }
        }
      }
    }
    outsiders.clear();

    IDXS* idxs = new IDXS(
      frameOffset, offset,
      varFrameOffset,
      mixFrameOffset,
      fnFrameOffset,
      std::move(varIdxs),
      std::move(mixIdxs),
      std::move(fnIdxs));

    root.idxs[frameOffset] = idxs;

    return idxs;

  }

  // ##########################################################################
  // ##########################################################################

}
