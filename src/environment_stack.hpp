#ifndef SASS_VAR_STACK_H
#define SASS_VAR_STACK_H

// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"
#include "memory.hpp"
#include "ast_fwd_decl.hpp"
#include "environment.hpp"

#include <exception>

// functions know global flag, while mixins/functions don't

// Variable Stacks are created during parser phase.
// We don't need to reset stacks 

// NEW IDEA
// On fn execute create variables for frame
// Appends to global stack

// We also have a global frame pointer array
// Each frame as encountered gets one entry
// Each variable has a pointer/offset for this
// So it knows where its local frame starts

    // All variables the program knows.
    // Same as all stack variables in C.
    // So stack is static after parsing.

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
  class EnvFrame;
  class EnvRoot;

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  class IdxRef {

  public:

    uint32_t frame;
    uint32_t offset;

    IdxRef() :
      frame(0xFFFFFFFF),
      offset(0xFFFFFFFF)
    {}

    IdxRef(uint32_t frame, uint32_t offset)
    : frame(frame), offset(offset) {}

    bool isValid() const { // 3%
      return offset != 0xFFFFFFFF
        && frame != 0xFFFFFFFF;
    }

  };

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  class IdxRefs {

  public:

    uint32_t frame;
    uint32_t size;

    IdxRefs() :
      frame(0xFFFFFFFF),
      size(0)
    {}

    bool isValid() const {
      return size > 0 &&
        frame != 0xFFFFFFFF;
    }
  };

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  class IDXS {
  public:

    uint32_t varFrame;
    uint32_t mixFrame;
    uint32_t fnFrame;

    // uint32_t frame;

    // Parent is needed during runtime for
    // dynamic setter and getter by EnvKey.
    uint32_t parent;

    // Also remember mappings
    EnvKeyMap<uint32_t> varIdxs;
    EnvKeyMap<uint32_t> mixIdxs;
    EnvKeyMap<uint32_t> fnIdxs;

    IDXS(uint32_t frame, uint32_t parent) :
      varFrame(0xFFFFFFFF),
      mixFrame(0xFFFFFFFF),
      fnFrame(0xFFFFFFFF),
      parent(parent)
    {}

    IDXS(uint32_t frame, uint32_t parent,
      uint32_t varFrame,
      uint32_t mixFrame,
      uint32_t fnFrame,
      EnvKeyMap<uint32_t>&& varIdxs,
      EnvKeyMap<uint32_t>&& mixIdxs,
      EnvKeyMap<uint32_t>&& fnIdxs) :
      varFrame(varFrame),
      mixFrame(mixFrame),
      fnFrame(fnFrame),
      parent(parent),
      varIdxs(std::move(varIdxs)),
      mixIdxs(std::move(mixIdxs)),
      fnIdxs(std::move(fnIdxs))
    {}

  };

  /////////////////////////////////////////////////////////////////////////////
  // EnvFrames are created during the parsing phase.
  /////////////////////////////////////////////////////////////////////////////

  class EnvFrame {

  public:
    friend class EnvRoot;

    // Cache last parent
    EnvRoot& root;

    // Reference to parent
    EnvFrame& parent;

    // New variables are hoisted at the closest chroot
    // Lookups are still looking at all parents and root.
    bool chroot;

    // The variables off the current frame
    EnvKeyMap<uint32_t> varIdxs;
    EnvKeyMap<uint32_t> mixIdxs;
    EnvKeyMap<uint32_t> fnIdxs;


    EnvKeyMap<sass::vector<VariableObj>> outsiders;

    // Offset to get active frame
    uint32_t varFrameOffset;
    uint32_t mixFrameOffset;
    uint32_t fnFrameOffset;
    uint32_t frameOffset;

  public:

    EnvFrame(EnvRoot& root);
    EnvFrame(EnvFrame* parent, bool chroot = false);

    IDXS* getIdxs();

    bool isRoot() const {
      // Check if raw pointers are equal
      return this == (EnvFrame*)&root;
    }

    EnvRoot& getRoot() {
      return root;
    }

    EnvFrame* getParent() {
      if (isRoot())
        return nullptr;
      return &parent;
    }

    // ToDo: we should find uninitialized var access
    // Called whenever we find a variable declaration.
    // Indicates that the local frame uses this variable.
    // Will check if it already exists on the lexical scope.

    IdxRef createMixin(const EnvKey& name);
    IdxRef getMixinIdx(const EnvKey& name);

    IdxRef createFunction(const EnvKey& name);
    IdxRef getFunctionIdx(const EnvKey& name);

    IdxRef hoistVariable(const EnvKey& name, bool global = false);
    IdxRef createVariable(const EnvKey& name, bool global = false);
    IdxRef getVariableIdx(const EnvKey& name, bool global = false);
    IdxRef getVariableIdx2(const EnvKey& name, bool global = false);

 };

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////

  class EnvRoot :
    public EnvFrame {

  public:

    // These vectors are the main stacks during runtime
    // When a scope with two variables is executed, two
    // new items are added to the variables stack. If the
    // same scope is called more than once, its variables
    // are added multiple times so we can revert to them.
    sass::vector<ValueObj> variables;
    sass::vector<CallableObj> functions;
    sass::vector<CallableObj> mixins;

    // Every scope we see in sass code gets an entry here.
    // The value stored here is the base address of the
    // active scope, used to calculate the final offset.
    // variables[varFrames[vidx.frame] + vidx.offset]
    sass::vector<uint32_t> varFrames;
    sass::vector<uint32_t> mixFrames;
    sass::vector<uint32_t> fnFrames;

    // The current runtime stack 
    sass::vector<const IDXS*> stack;

    // All parsed index references
    sass::vector<IDXS*> idxs;

    // Runtime check to see if we are currently in global scope
    bool isGlobal() const { return root.stack.size() == 1; }

    // Functions only for evaluation phase
    ValueObj getLocalVariable(const EnvKey& name) const;
    ValueObj getGlobalVariable(const EnvKey& name) const;
    ValueObj getLexicalVariable(const EnvKey& name) const;

    // Functions only for evaluation phase
    void setLocalVariable(const EnvKey& name, ValueObj val);
    void setGlobalVariable(const EnvKey& name, ValueObj val);
    void setLexicalVariable(const EnvKey& name, ValueObj val);

    EnvRoot();

    // Take care of pointers
    ~EnvRoot() {
      for (IDXS* idx : idxs) {
        delete idx;
      }
    }

    // Functions only for evaluation phase
    CallableObj getLexicalMixin(const EnvKey& name) const;
    CallableObj getGlobalFunction(const EnvKey& name) const;
    CallableObj getLexicalFunction(const EnvKey& name) const;

    // Functions only for evaluation phase
    CallableObj& getMixin(const IdxRef& ref);
    CallableObj& getFunction(const IdxRef& ref);
    ValueObj& getVariable(const IdxRef& ref);

    // Functions only for evaluation phase
    void setMixin(const IdxRef& ref, CallableObj obj);
    void setFunction(const IdxRef& ref, CallableObj obj);
    void setVariable(const IdxRef& vidx, ValueObj obj);
    void setVariable(uint32_t frame, uint32_t offset, ValueObj obj);

  };

  class EnvStack : public EnvRoot {
  public:
    EnvStack() :
      EnvRoot()
    {}
  };


  /////////////////////////////////////////////////////////////////////////////
  // EnvScopes are created during evaluation phase. When we enter a parsed
  // scope, e.g. a function, mixin or style-rule, we create a new EnvScope
  // object on the stack and pass it the runtime environment and the current
  // stack frame (in form of an IDXS pointer). We will "allocate" the needed
  // space for scope items and update any offset pointers. Once we go out of
  // scope the previous state is restored by unwinding the runtime stack.
  /////////////////////////////////////////////////////////////////////////////

  class EnvScope {

  public:

    // Runtime environment
    EnvRoot& env;

    // Frame stack index references
    IDXS* idxs;

    // Remember previous "addresses"
    // Restored when we go out of scope
    uint32_t oldVarFrame;
    uint32_t oldVarOffset;
    uint32_t oldMixFrame;
    uint32_t oldMixOffset;
    uint32_t oldFnFrame;
    uint32_t oldFnOffset;

  public:

    // Put frame onto stack
    EnvScope(
      EnvRoot& env,
      IDXS* idxs) :
      env(env),
      idxs(idxs),
      oldVarFrame(0),
      oldVarOffset(0),
      oldMixFrame(0),
      oldMixOffset(0),
      oldFnFrame(0),
      oldFnOffset(0)
    {

      // The frame might be fully empty
      // Meaning it no scoped items at all
      if (idxs == nullptr) return;

      // Check if we have scoped variables
      if (idxs->varIdxs.size() != 0) {
        // Get offset into variable vector
        oldVarOffset = (uint32_t)env.variables.size();
        // Remember previous frame "addresses"
        oldVarFrame = env.varFrames[idxs->varFrame];
        // Update current frame offset address
        env.varFrames[idxs->varFrame] = oldVarOffset;
        // Create space for variables in this frame scope
        env.variables.resize(oldVarOffset + idxs->varIdxs.size());
      }

      // Check if we have scoped mixins
      if (idxs->mixIdxs.size() != 0) {
        // Get offset into mixin vector
        oldMixOffset = (uint32_t)env.mixins.size();
        // Remember previous frame "addresses"
        oldMixFrame = env.mixFrames[idxs->mixFrame];
        // Update current frame offset address
        env.mixFrames[idxs->mixFrame] = oldMixOffset;
        // Create space for mixins in this frame scope
        env.mixins.resize(oldMixOffset + idxs->mixIdxs.size());
      }

      // Check if we have scoped functions
      if (idxs->fnIdxs.size() != 0) {
        // Get offset into function vector
        oldFnOffset = (uint32_t)env.functions.size();
        // Remember previous frame "addresses"
        oldFnFrame = env.fnFrames[idxs->fnFrame];
        // Update current frame offset address
        env.fnFrames[idxs->fnFrame] = oldFnOffset;
        // Create space for functions in this frame scope
        env.functions.resize(oldFnOffset + idxs->fnIdxs.size());
      }

      // Push frame onto stack
      // Mostly for dynamic lookups
      env.stack.push_back(idxs);

    }
    // EO ctor

    // Restore old state on destructor
    ~EnvScope()
    {

      // The frame might be fully empty
      // Meaning it no scoped items at all
      if (idxs == nullptr) return;

      // Check if we had scoped variables
      if (idxs->varIdxs.size() != 0) {
        // Truncate variable vector
        env.variables.resize(
          oldVarOffset);
        // Restore old frame address
        env.varFrames[idxs->varFrame] =
          oldVarFrame;
      }

      // Check if we had scoped mixins
      if (idxs->mixIdxs.size() != 0) {
        // Truncate existing vector
        env.mixins.resize(
          oldMixOffset);
        // Restore old frame address
        env.mixFrames[idxs->mixFrame] =
          oldMixFrame;
      }

      // Check if we had scoped functions
      if (idxs->fnIdxs.size() != 0) {
        // Truncate existing vector
        env.functions.resize(
          oldFnOffset);
        // Restore old frame address
        env.fnFrames[idxs->fnFrame] =
          oldFnFrame;
      }

      // Pop frame from stack
      env.stack.pop_back();

    }
    // EO dtor

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
      // Push frame-ptr to stack
      callStack.push_back(frame);
    }

    // Remove frame from stack on destruction
    ~ScopedStackFrame()
    {
      // Just remove the last item
      // We assume this is ourself
      callStack.pop_back();
    }

  };

}

#endif
