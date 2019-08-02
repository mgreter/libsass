#include "var_stack.hpp"
#include "context.hpp"

namespace Sass {

  EnvRoot::EnvRoot() :
    EnvStack(*this, true),
    contentExists(false)
  {
    // Get our unique frame offset
    varFrameOffset = root.varFrames.size();
    mixFrameOffset = root.mixFrames.size();
    fnFrameOffset = root.fnFrames.size();
    frameOffset = root.idxs.size();
    // Initialize as not active yet
    root.varFrames.push_back(std::string::npos);
    root.mixFrames.push_back(std::string::npos);
    root.fnFrames.push_back(std::string::npos);
    root.idxs.push_back(nullptr);
  }

  // Create new variable if no lexical one exists
  IdxRef EnvStack::hoistVariable(
    const EnvString& name, bool global, bool guarded)
  {

    IdxRef idx;
    // If the value is guarded we check first if
    // we have it already in the full lexical path
    if (guarded) {
      idx = getVariableIdx2(name, global);
    }
    else {
      // Check if the variable already exists
      idx = getVariableIdx(name, global);
    }

    // Return already existing mixin
    if (idx.isValid()) {
      // std::cerr << "return existing " << idx.frame << ":" << idx.offset << "\n";
      return idx;
    }
    // register new stack variable
    auto asd = createVariable(name, global);

    // std::cerr << "hoist variable " << asd.frame << ":" << asd.offset << "\n";

    return asd;

  }
  // hoistVariable

  // Create new variable if no lexical one exists
  IdxRef EnvStack::hoistMixin(
    const EnvString& name)
  {
    // Check if the mixin already exists
    IdxRef idx = getMixinIdx(name);
    // Return already existing mixin
    if (idx.isValid()) return idx;
    // register new stack mixin
    return createMixin(name);
  }
  // EO hoistMixin

  // Create new variable if no lexical one exists
  IdxRef EnvStack::hoistFunction(
    const EnvString& name)
  {
    // Check if the function already exists
    IdxRef idx = getFunctionIdx(name);
    // Return already existing function
    if (idx.isValid()) return idx;
    // register new stack function
    return createFunction(name);
  }
  // EO hoistFunction

  EnvStack::EnvStack(EnvRoot& root, bool chroot) :
    root(root), chroot(chroot),
    varFrameOffset(0),
    mixFrameOffset(0),
    fnFrameOffset(0),
    frameOffset(0)
  {
  }

  IDXS* EnvStack::getIdxs() {

    if (root.idxs[frameOffset] != nullptr) {
      return root.idxs[frameOffset];
    }
    EnvStack* p = getParent();
    size_t parent = std::string::npos;
    if (p) parent = p->frameOffset;
    IDXS* idxs = new IDXS(frameOffset, parent);
    root.idxs[frameOffset] = idxs;

    if (!varIdxs.empty()) {
      idxs->vidxs.frame = varFrameOffset;
      idxs->vidxs.size = varIdxs.size();
      idxs->varFrame = varFrameOffset;
      idxs->varIdxs = varIdxs;
    }

    if (!mixIdxs.empty()) {
      idxs->midxs.frame = mixFrameOffset;
      idxs->midxs.size = mixIdxs.size();
      idxs->mixFrame = mixFrameOffset;
      idxs->mixIdxs = mixIdxs;
    }

    if (!fnIdxs.empty()) {
      idxs->fidxs.frame = fnFrameOffset;
      idxs->fidxs.size = fnIdxs.size();
      idxs->fnFrame = fnFrameOffset;
      idxs->fnIdxs = fnIdxs;
    }

    return idxs;

  }

  // Create new variable if no lexical one exists
  IdxRef EnvStack::createVariable(
    const EnvString& name, bool global)
  {
    // register stack variable
    if (global) {
      // Create in root scope
      size_t offset = root.varIdxs.size();
      root.varIdxs[name] = offset;
      return { root.varFrameOffset, offset };
    }
    else {
      // Create in local scope
      size_t offset = varIdxs.size();
      varIdxs[name] = offset;
      // std::cerr << "create variable " << frameOffset << ":" << offset << "\n";
      return { varFrameOffset, offset };
    }
  }

  // Create new variable if no lexical one exists
  IdxRef EnvStack::createFunction(
    const EnvString& name)
  {
    // Create in local scope
    size_t offset = fnIdxs.size();
    fnIdxs[name] = offset;
    // std::cerr << "create variable " << frameOffset << ":" << offset << "\n";
    return { fnFrameOffset, offset };
  }

  // Create new variable if no lexical one exists
  IdxRef EnvStack::createMixin(
    const EnvString& name)
  {
    // Create in local scope
    size_t offset = mixIdxs.size();
    mixIdxs[name] = offset;
    // std::cerr << "create variable " << frameOffset << ":" << offset << "\n";
    return { mixFrameOffset, offset };
  }

  void EnvRoot::setFunction(const IdxRef& ref, CallableObj obj) {
    //  std::cerr << "set variable " << ref.frameOffset << ":" << ref.offset << "\n";
    // size_t stackBase = fnFrames[ref.frame];
    // if (stackBase == std::string::npos) {
    //   std::cerr << "invalid state1\n";
    // }
    functions[fnFrames[ref.frame] + ref.offset] = obj;
  }

  void EnvRoot::setMixin(const IdxRef& ref, UserDefinedCallableObj obj) {
    //  std::cerr << "set variable " << ref.frameOffset << ":" << ref.offset << "\n";
    // size_t stackBase = mixFrames[ref.frame];
    // if (stackBase == std::string::npos) {
    //   std::cerr << "invalid state1\n";
    // }
    mixins[mixFrames[ref.frame] + ref.offset] = obj;
  }

  IdxRef EnvRoot::getVariableIdx(const EnvString& name, bool global)
  {
    // Check if we already have this var
    auto it = varIdxs.find(name);
    if (it != varIdxs.end()) {
      return {
        varFrameOffset,
        it->second
      };
    }
    // Return error
    return IdxRef{};
  }

  IdxRef EnvFrame::getVariableIdx(const EnvString& name, bool global) {

    if (global) {
      return root.getVariableIdx(name, global);
    }

    // Check if we already have this var
    auto it = varIdxs.find(name);
    if (it != varIdxs.end()) {
      return {
        varFrameOffset,
        it->second
      };
    }
    // Return error if chroot is enforced
    if (chroot) return IdxRef{};

    // Dispatch to parent or root frame
    // Last parent must be a variable root!
    return parent.getVariableIdx(name, global);
  }

  IdxRef EnvRoot::getVariableIdx2(const EnvString& name, bool global)
  {
    // Check if we already have this var
    auto it = varIdxs.find(name);
    if (it != varIdxs.end()) {
      return {
        varFrameOffset,
        it->second
      };
    }
    // Return error
    return IdxRef{};
  }

  IdxRef EnvFrame::getVariableIdx2(const EnvString& name, bool global) {

    if (global) {
      return root.getVariableIdx2(name, global);
    }

    // Check if we already have this var
    auto it = varIdxs.find(name);
    if (it != varIdxs.end()) {
      return {
        varFrameOffset,
        it->second
      };
    }

    // Dispatch to parent or root frame
    // Last parent must be a variable root!
    return parent.getVariableIdx2(name, global);
  }

  IdxRef EnvRoot::getFunctionIdx(const EnvString& name)
  {
    // Check if we already have this var
    auto it = fnIdxs.find(name);
    if (it != fnIdxs.end()) {
      return {
        fnFrameOffset,
        it->second
      };
    }
    // Return error
    return IdxRef{};
  }

  IdxRef EnvFrame::getFunctionIdx(const EnvString& name) {

    // Check if we already have this var
    auto it = fnIdxs.find(name);
    if (it != fnIdxs.end()) {
      return {
        fnFrameOffset,
        it->second
      };
    }
    // Return error if chroot is enforced
    if (chroot) return IdxRef{};

    // Dispatch to parent or root frame
    // Last parent must be a variable root!
    return parent.getFunctionIdx(name);
  }

  IdxRef EnvRoot::getFunctionIdx2(const EnvString& name)
  {
    // Check if we already have this var
    auto it = fnIdxs.find(name);
    if (it != fnIdxs.end()) {
      return {
        fnFrameOffset,
        it->second
      };
    }
    // Return error
    return IdxRef{};
  }

  IdxRef EnvFrame::getFunctionIdx2(const EnvString& name) {

    // Check if we already have this var
    auto it = fnIdxs.find(name);
    if (it != fnIdxs.end()) {
      return {
        fnFrameOffset,
        it->second
      };
    }
    // Dispatch to parent or root frame
    // Last parent must be a variable root!
    return parent.getFunctionIdx2(name);
  }



  IdxRef EnvRoot::getMixinIdx2(const EnvString& name)
  {
    // Check if we already have this var
    auto it = mixIdxs.find(name);
    if (it != mixIdxs.end()) {
      return {
        mixFrameOffset,
        it->second
      };
    }
    // Return error
    return IdxRef{};
  }

  IdxRef EnvFrame::getMixinIdx2(const EnvString& name) {

    // Check if we already have this var
    auto it = mixIdxs.find(name);
    if (it != mixIdxs.end()) {
      return {
        mixFrameOffset,
        it->second
      };
    }

    // Dispatch to parent or root frame
    // Last parent must be a variable root!
    return parent.getMixinIdx2(name);
  }

  IdxRef EnvRoot::getMixinIdx(const EnvString& name)
  {
    // Check if we already have this var
    auto it = mixIdxs.find(name);
    if (it != mixIdxs.end()) {
      return {
        mixFrameOffset,
        it->second
      };
    }
    // Return error
    return IdxRef{};
  }

  IdxRef EnvFrame::getMixinIdx(const EnvString& name) {

    // Check if we already have this var
    auto it = mixIdxs.find(name);
    if (it != mixIdxs.end()) {
      return {
        mixFrameOffset,
        it->second
      };
    }
    // Return error if chroot is enforced
    if (chroot) return IdxRef{};

    // Dispatch to parent or root frame
    // Last parent must be a variable root!
    return parent.getMixinIdx(name);
  }

  EnvFrame::EnvFrame(EnvStack* parent, bool chroot) :
    EnvStack(parent->root, chroot), parent(*parent)
  {
    // Get our unique frame offset
    varFrameOffset = root.varFrames.size();
    mixFrameOffset = root.mixFrames.size();
    fnFrameOffset = root.fnFrames.size();
    frameOffset = root.idxs.size();
    // Initialize as not active yet
    root.varFrames.push_back(std::string::npos);
    root.mixFrames.push_back(std::string::npos);
    root.fnFrames.push_back(std::string::npos);
    root.idxs.push_back(nullptr);
  }

}
