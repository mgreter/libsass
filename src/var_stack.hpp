#ifndef SASS_VAR_STACK_H
#define SASS_VAR_STACK_H

// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"
#include "allocator.hpp"
#include "ast_fwd_decl.hpp"
#include "environment.hpp"

#include <exception>

// Variable Stacks are created during parser phase.
// We don't need to reset stacks 

// NEW IDEA
// On fn execute create variables for frame
// Appends to global stack

// We also have a global frame pointer array
// Each frame as encountered gets one entry
// Each variable has a pointer/offset for this
// So it knows where its local frame starts

namespace Sass {

  // The stack must be nestable
  // no way around unordered map?
  // Or one root unordered map for index?
  // in the end we want all vars in one vector?
  // means same name can have many instances!
  // all vars linear in order of appearance

  // How to initialize when entering a scope
  // Means we need to attach something to node
  // Basically a list of vidx that need resets
  // Which boils down to all values in varFrame

  // We can give the "caller" the final array offset.
  // To init we only need the start and end position.
  class EnvStack;
  class EnvFrame;
  class EnvRoot;

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  class IdxRef {

  public:

    size_t frame;
    size_t offset;

    IdxRef() :
      frame(std::string::npos),
      offset(std::string::npos)
    {}

    IdxRef(size_t frame, size_t offset)
    : frame(frame), offset(offset) {}

    // Maybe optimize this, 3%
    bool isValid() const {
      return offset != std::string::npos
        && frame != std::string::npos;
    }
  };

  class IdxRefs {

  public:

    size_t frame;
    size_t size;

    IdxRefs() :
      frame(std::string::npos),
      size(0)
    {}

    bool isValid() const {
      return size > 0 &&
        frame != std::string::npos;
    }
  };

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  class IDXS {
  public:

    IdxRefs vidxs;
    IdxRefs midxs;
    IdxRefs fidxs;
    size_t varFrame;
    size_t mixFrame;
    size_t fnFrame;
    size_t frame;

    size_t parent;
    bool transparent;

    // Also remember mappings
    NormalizedMap<size_t> varIdxs;
    NormalizedMap<size_t> mixIdxs;
    NormalizedMap<size_t> fnIdxs;

    IDXS(size_t frame, size_t parent) :
      varFrame(std::string::npos),
      mixFrame(std::string::npos),
      fnFrame(std::string::npos),
      frame(frame),
      parent(parent),
      transparent(false)
    {}

  };

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  class EnvStack {

  private:
    friend class EnvRoot;
    friend class EnvFrame;

    // Cache last parent
    EnvRoot& root;

    // The variables off the current frame
    NormalizedMap<size_t> varIdxs;
    NormalizedMap<size_t> mixIdxs;
    NormalizedMap<size_t> fnIdxs;

    // New variables are hoisted at the closest chroot
    // Lookups are still looking at all parents and root.
    bool chroot;

    // Offset to get active frame
    size_t varFrameOffset;
    size_t mixFrameOffset;
    size_t fnFrameOffset;
    size_t frameOffset;

    void setVariable(const EnvString& name, size_t idx)
    {
      varIdxs[name] = idx;
    }

    void setFunction(const EnvString& name, size_t idx)
    {
      fnIdxs[name] = idx;
    }

    void setMixin(const EnvString& name, size_t idx)
    {
      mixIdxs[name] = idx;
    }

  public:

    EnvStack(EnvRoot& root, bool chroot = false);

    // EnvStack& operator=(const EnvStack& other);

    IDXS* getIdxs(bool transparent = false);

    EnvStack(EnvStack& other) :
      root(other.root),
      varIdxs(other.varIdxs),
      mixIdxs(other.mixIdxs),
      fnIdxs(other.fnIdxs)
    {
    }

    EnvStack(EnvStack&& other) :
      root(other.root),
      varIdxs(other.varIdxs),
      mixIdxs(other.mixIdxs),
      fnIdxs(other.fnIdxs)
    {
      std::cerr << "who is moving?\n";
    }

    virtual bool isRoot() const = 0;

    virtual EnvRoot& getRoot() {
      return root;
    };
    virtual EnvStack* getParent() = 0;

    // ToDo: we should find unitialized var access
    // Called whenever we find a variable declaration.
    // Indicates that the locale frame uses this variable.
    // Will check if it already exists on the lexical scope.

    IdxRef createVariable(const EnvString& name, bool global = false);
    IdxRef hoistVariable(const EnvString& name, bool global = false, bool guarded = false);
    virtual IdxRef getVariableIdx(const EnvString& name, bool global = false) = 0;
    virtual IdxRef getVariableIdx2(const EnvString& name, bool global = false) = 0;

    IdxRef createFunction(const EnvString& name);
    IdxRef hoistFunction(const EnvString& name);
    virtual IdxRef getFunctionIdx(const EnvString& name) = 0;
    virtual IdxRef getFunctionIdx2(const EnvString& name) = 0;

    IdxRef createMixin(const EnvString& name);
    IdxRef hoistMixin(const EnvString& name);
    virtual IdxRef getMixinIdx(const EnvString& name) = 0;
    virtual IdxRef getMixinIdx2(const EnvString& name) = 0;

 };

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  class EnvFrame :
    public EnvStack {

    // All classes work together
    friend class EnvRoot;
    friend class EnvStack;

  private:

    // Variable counts
    // size_t start;
    // size_t length;

    // Reference to parent
    EnvStack& parent;

    // return pointer to parent
    EnvStack* getParent() override final {
      return &parent;
    }

  public:

    // Construct a child frame
    EnvFrame(EnvStack* parent, bool chroot = false);

    // EnvFrame& operator=(const EnvFrame& other);

    bool isRoot() const override final {
      return false;
    }

    IdxRef getVariableIdx(const EnvString& name, bool global = false) override final;
    IdxRef getVariableIdx2(const EnvString& name, bool global = false) override final;
    IdxRef getFunctionIdx(const EnvString& name) override final;
    IdxRef getFunctionIdx2(const EnvString& name) override final;
    IdxRef getMixinIdx(const EnvString& name) override final;
    IdxRef getMixinIdx2(const EnvString& name) override final;

  };

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  class EnvRoot :
    public EnvStack {

  private:
    friend class EnvFrame;

    // return nullptr to indicate root
    EnvStack* getParent() override final {
      return nullptr;
    }

  public:

    // All variables the program knows.
    // Same as all stack variables in C.
    // So stack is static after parsing.
    sass::vector<ExpressionObj> variables;
    sass::vector<CallableObj> functions;
    sass::vector<UserDefinedCallableObj> mixins;

    sass::vector<size_t> varFrames;
    sass::vector<size_t> mixFrames;
    sass::vector<size_t> fnFrames;

    sass::vector<IDXS*> idxs;

    sass::vector<const IDXS*> stack;

    bool contentExists;

    bool isGlobal() const {
      return root.stack.size() == 1;
    }

    bool hasGlobalVariable33(const EnvString& name) const {
      auto it = root.varIdxs.find(name);
      if (it != root.varIdxs.end()) {
        size_t offset = it->second;
        ExpressionObj value = root.getVariable({
          0, offset });
        return !value.isNull();
      }
      return false;
    }

    bool hasLexicalVariable33(const EnvString& name) const {
      if (stack.empty()) return false;
      const IDXS* current = stack.back();
      while (current) {
        auto it = current->varIdxs.find(name);
        if (it != current->varIdxs.end()) {
          size_t offset = it->second;
          ExpressionObj value = root.getVariable({
            current->frame, offset });
          return !value.isNull();
        }
        if (current->parent == std::string::npos) break;
        current = root.idxs[current->parent];
      }
      return false;
    }

    bool hasGlobalFunction33(const EnvString& name) const {
      auto it = root.fnIdxs.find(name);
      if (it != root.fnIdxs.end()) return true;
      return false;
    }

    bool hasLexicalMixin33(const EnvString& name) const {
      size_t i = stack.size();
      while (i > 0) {
        --i;
        auto it = stack[i]->mixIdxs.find(name);
        if (it != stack[i]->mixIdxs.end()) return true;
      }
      return false;
    }

    bool hasLexicalFunction33(const EnvString& name) const {
      size_t i = stack.size();
      while (i > 0) {
        --i;
        auto it = stack[i]->fnIdxs.find(name);
        if (it != stack[i]->fnIdxs.end()) return true;
      }
      return false;
    }

    IdxRef getGlobalVariable33(const EnvString& name) const {
      auto it = root.varIdxs.find(name);
      if (it != root.varIdxs.end()) {
        return { 0, it->second };
      }
      return {};
    }

    IdxRef getLexicalVariable33(const EnvString& name) const {
      size_t i = stack.size(); while (i > 0) { --i;
        auto it = stack[i]->varIdxs.find(name);
        if (it != stack[i]->varIdxs.end()) {
          return { stack[i]->varFrame, it->second };
        }
      }
      return {};
    }

    IdxRef getLexicalFunction33(const EnvString& name) const {
      size_t i = stack.size(); while (i > 0) {
        --i;
        auto it = stack[i]->fnIdxs.find(name);
        if (it != stack[i]->fnIdxs.end()) {
          return { stack[i]->fnFrame, it->second };
        }
      }
      return {};
    }

    IdxRef getLexicalMixin33(const EnvString& name) const {
      if (stack.empty()) return IdxRef{};
      const IDXS* current = stack.back();
      while (current) {
        auto it = current->mixIdxs.find(name);
        if (it != current->mixIdxs.end()) {
          return { current->mixFrame, it->second };
        }
        if (current->parent == std::string::npos) break;
        current = root.idxs[current->parent];
      }
      return IdxRef{};
    }

    IdxRef getVariable33(const EnvString& name) const {
      size_t i = stack.size();
      while (i > 0) { --i;
        auto it = stack[i]->varIdxs.find(name);
        if (it != stack[i]->varIdxs.end()) {
          return { stack[i]->varFrame, it->second };
        }
      }
      return {};
    }

    EnvRoot();

    // We are in care of the idxs pointers
    ~EnvRoot() {
      for (IDXS* idxs : root.idxs) {
        delete idxs;
      }
      root.idxs.clear();
    }

    ExpressionObj& getVariable(const IdxRef& ref) {
      // std::cerr << "get variable " << ref.frameOffset << ":" << ref.varOffset << "\n";
      // size_t stackBase = varFrames[ref.frame];

      // if (stackBase == std::string::npos) {
      //   std::cerr << "invalid state2\n";
      // }

     //  std::cerr << "Get variable " << stackBase + ref.offset << "\n";
      // Value might not be initalized yet
      return variables[varFrames[ref.frame] + ref.offset];
    }

    UserDefinedCallableObj& getMixin(const IdxRef& ref) {
      // std::cerr << "get variable " << ref.frameOffset << ":" << ref.varOffset << "\n";
      // size_t stackBase = mixFrames[ref.frame];

      // if (stackBase == std::string::npos) {
      //   std::cerr << "invalid state2\n";
      // }

      //  std::cerr << "Get variable " << stackBase + ref.offset << "\n";
       // Value might not be initalized yet
      return mixins[mixFrames[ref.frame] + ref.offset];
    }

    ExpressionObj getLexicalVariable44(const EnvString& name) {
      if (stack.empty()) return {};
      size_t idx = stack.size() - 1;
      const IDXS* current = stack[idx];
      while (current) {
        auto it = current->varIdxs.find(name);
        if (it != current->varIdxs.end()) {
          ExpressionObj& value = root.getVariable({
            current->frame, it->second });
          if (value) return value;
        }
        if (current->transparent) idx = idx - 1;
        else if (current->parent == std::string::npos) break;
        else idx = current->parent;
        current = root.idxs[idx];
      }
      return {};
    }

    void setLexicalVariable44(const EnvString& name, ExpressionObj val) {
      if (stack.empty()) return;
      size_t idx = stack.size() - 1;
      const IDXS* current = stack[idx];
      while (current) {
        auto it = current->varIdxs.find(name);
        if (it != current->varIdxs.end()) {
          ExpressionObj& value = root.getVariable({
            current->frame, it->second });
          if (value.isNull()) continue;
          value = val;
          return;
        }
        if (current->transparent) {
          idx = idx - 1;
          current = stack[idx];
        }
        else if (current->parent == std::string::npos) return;
        else {
          idx = current->parent;
          current = root.idxs[idx];
        }
      }
    }

    void setLexicalVariable55(const EnvString& name, ExpressionObj val) {
      if (stack.empty()) return;
      const IDXS* current = stack.back();
      auto it = current->varIdxs.find(name);
      if (it != current->varIdxs.end()) {
        ExpressionObj& value = root.getVariable({
          current->frame, it->second });
        value = val;
        return;
      }

      current = stack[stack.size()-2];
      while (current) {
        auto it = current->varIdxs.find(name);
        if (it != current->varIdxs.end()) {
          ExpressionObj& value = root.getVariable({
            current->frame, it->second });
          if (value.isNull()) continue;
          value = val;
          return;
        }
        if (current->parent == std::string::npos) return;
        current = root.idxs[current->parent];
      }

    }

    UserDefinedCallableObj getLexicalMixin44(const EnvString& name) {
      if (stack.empty()) return {};
      const IDXS* current = stack.back();
      while (current) {
        auto it = current->mixIdxs.find(name);
        if (it != current->mixIdxs.end()) {
          UserDefinedCallableObj& value = root.getMixin({
            current->frame, it->second });
          if (value) return value;
        }
        if (current->parent == std::string::npos) break;
        current = root.idxs[current->parent];
      }
      return {};
    }


    const CallableObj& getFunction(const IdxRef& ref) const {
      // std::cerr << "get variable " << ref.frameOffset << ":" << ref.varOffset << "\n";
      size_t stackBase = fnFrames[ref.frame];

      // if (stackBase == std::string::npos) {
      //   std::cerr << "invalid state4\n";
      // }

      const auto& asd = functions[stackBase + ref.offset];

      // if (!asd) {
      //   std::cerr << "invalid state5\n";
      // }

      return asd;
    }

    const UserDefinedCallableObj& getMixin(const IdxRef& ref) const {
      // std::cerr << "get variable " << ref.frameOffset << ":" << ref.varOffset << "\n";
      size_t stackBase = mixFrames[ref.frame];

      // if (stackBase == std::string::npos) {
      //   std::cerr << "invalid state4\n";
      // }

      const auto& asd = mixins[stackBase + ref.offset];

      // if (!asd) {
      //   std::cerr << "invalid state5\n";
      // }

      return asd;
    }

    void setVariable(size_t frame, size_t offset, ExpressionObj obj) {
      // std::cerr << "set variable " << frame << ":" << offset << "\n";
      size_t stackBase = varFrames[frame];
      // if (stackBase == std::string::npos) {
      //   std::cerr << "invalid state6\n";
      // }
      variables[stackBase + offset] = obj;
    }

    void setVariable(const IdxRef& vidx, ExpressionObj obj);

    void setMixin(const IdxRef& ref, UserDefinedCallableObj obj);
    void setFunction(const IdxRef& ref, CallableObj obj);

    IdxRef getVariableIdx(const EnvString& name, bool global = false) override final;
    IdxRef getVariableIdx2(const EnvString& name, bool global = false) override final;
    IdxRef getFunctionIdx(const EnvString& name) override final;
    IdxRef getFunctionIdx2(const EnvString& name) override final;
    IdxRef getMixinIdx(const EnvString& name) override final;
    IdxRef getMixinIdx2(const EnvString& name) override final;

    bool isRoot() const override final {
      return true;
    }

  };

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  class EnvScope {
  private:
    // Cache the root reference
    EnvRoot& root;

    size_t varFrame;
    size_t mixFrame;
    size_t fnFrame;

    size_t oldVarFrame;
    size_t oldVarOffset;
    size_t oldMixFrame;
    size_t oldMixOffset;
    size_t oldFnFrame;
    size_t oldFnOffset;

  public:
    EnvScope(
      EnvRoot& root,
      IDXS* idxs) :
      root(root),
      varFrame(std::string::npos),
      mixFrame(std::string::npos),
      fnFrame(std::string::npos)
    {

      if (idxs->vidxs.size) {
        varFrame = idxs->vidxs.frame;
        oldVarFrame = root.varFrames[varFrame];
        oldVarOffset = root.variables.size();
        root.varFrames[varFrame] = oldVarOffset;
        root.variables.resize(root.variables.size() + idxs->vidxs.size);
      }

      if (idxs->midxs.size) {
        mixFrame = idxs->midxs.frame;
        oldMixFrame = root.mixFrames[mixFrame];
        oldMixOffset = root.mixins.size();
        root.mixFrames[mixFrame] = oldMixOffset;
        root.mixins.resize(root.mixins.size() + idxs->midxs.size);
      }

      if (idxs->fidxs.size) {
        fnFrame = idxs->fidxs.frame;
        oldFnFrame = root.fnFrames[fnFrame];
        oldFnOffset = root.functions.size();
        root.fnFrames[fnFrame] = oldFnOffset;
        root.functions.resize(root.functions.size() + idxs->fidxs.size);
      }

      root.stack.push_back(idxs);

    }
    ~EnvScope()
    {

      if (varFrame != std::string::npos) {
        // Truncate existing vector
        root.variables.resize(
          oldVarOffset);
        // Restore old frame ptr
        root.varFrames[varFrame] =
          oldVarFrame;
      }

      if (mixFrame != std::string::npos) {
        // Truncate existing vector
        root.mixins.resize(
          oldMixOffset);
        // Restore old frame ptr
        root.mixFrames[mixFrame] =
          oldMixFrame;
      }

      if (fnFrame != std::string::npos) {
        // Truncate existing vector
        root.functions.resize(
          oldFnOffset);
        // Restore old frame ptr
        root.fnFrames[fnFrame] =
          oldFnFrame;
      }

      root.stack.pop_back();

    }
  };

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  // Utility class to add a frame onto a stack vector
  // Once this object goes out of scope, it is removed
  // We assume this happens in well defined order, as
  // we do not check if we actually remove ourself!
  template <typename T> class ScopedStackFrame {

  private:

    // Reference to the managed callStack
    sass::vector<T*>& callStack;

    // The current stack frame
    T* frame;

  public:

    // Create object and add frame to stack
    ScopedStackFrame(
      sass::vector<T*>& callStack,
      T* frame) :
      callStack(callStack),
      frame(frame)
    {
      // Push pointer to stack
      callStack.push_back(frame);
    }

    // Remove frame from stack on desctruction
    ~ScopedStackFrame()
    {
      // Just remove ourself
      callStack.pop_back();
    }

  };

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  // Create a snapshot to be used later
  class EnvSnapshot {
  public:
    // The current stack frame pointers
    sass::vector<size_t> varFrames;
    sass::vector<size_t> mixFrames;
    sass::vector<size_t> fnFrames;
    sass::vector<const IDXS*> stack;
    sass::vector<IDXS*> idxs;
    bool isValid;
    bool isInUse;


    EnvSnapshot(const EnvRoot& root, bool isValid = true) :
      isValid(isValid),
      isInUse(false)
    {
      if (isValid) {
        // Only copy if valid, having the check here makes
        // it much simpler for callers/users, since conditional
        // stack variables are only possible with `if` branches.
        varFrames = root.varFrames;
        mixFrames = root.mixFrames;
        fnFrames = root.fnFrames;
        stack = root.stack;
        idxs = root.idxs;
      }
    }

    EnvSnapshot(const EnvSnapshot& other) :
      varFrames(other.varFrames),
      mixFrames(other.mixFrames),
      fnFrames(other.fnFrames),
      stack(other.stack),
      idxs(other.idxs),
      isValid(other.isValid),
      isInUse(false)
    {
      std::cerr << "copy snapshot?\n";
    }

    EnvSnapshot(EnvSnapshot&& other) :
      varFrames(std::move(other.varFrames)),
      mixFrames(std::move(other.mixFrames)),
      fnFrames(std::move(other.fnFrames)),
      stack(std::move(other.stack)),
      idxs(std::move(other.idxs)),
      isValid(other.isValid),
      isInUse(other.isInUse)
    {
      std::cerr << "move snapshot?\n";
    }

  };

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  // 
  class EnvSnapshotView {
    EnvRoot& root;
    EnvSnapshot* snapshot;

  public:


    EnvSnapshotView(EnvRoot& root, EnvSnapshot* snapshot) :
      root(root), snapshot(snapshot)
    {
      // Using a pointer makes usage easier, since snapshots
      // are often used conditionally, therefore we don't want
      // to use `if` scopes to create the views conditionally.
      if (snapshot != nullptr && snapshot->isValid) {
        SASS_ASSERT(!snapshot->isInUse, "Occupied");
        std::swap(root.varFrames, snapshot->varFrames);
        std::swap(root.mixFrames, snapshot->mixFrames);
        std::swap(root.fnFrames, snapshot->fnFrames);
        std::swap(root.stack, snapshot->stack);
        std::swap(root.idxs, snapshot->idxs);
        snapshot->isInUse = true;
      }
    }

    ~EnvSnapshotView()
    {
      if (snapshot != nullptr && snapshot->isValid) {
        SASS_ASSERT(snapshot->isInUse, "Unsued");
        std::swap(snapshot->varFrames, root.varFrames);
        std::swap(snapshot->mixFrames, root.mixFrames);
        std::swap(snapshot->fnFrames, root.fnFrames);
        std::swap(snapshot->stack, root.stack);
        std::swap(snapshot->idxs, root.idxs);
        snapshot->isInUse = false;
      }
    }

  };

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

}


#endif
