/*****************************************************************************/
/* Part of LibSass, released under the MIT license (See LICENSE.txt).        */
/*****************************************************************************/
#ifndef SASS_VAR_STACK_HPP
#define SASS_VAR_STACK_HPP

// sass.hpp must go before all system headers
// to get the __EXTENSIONS__ fix on Solaris.
#include "capi_sass.hpp"

#include "ast_fwd_decl.hpp"
#include "environment_cnt.hpp"

namespace Sass {

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Forward declare
  class VarRef;
  class VarRefs;
  class EnvRoot;
  class EnvFrame;

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Helper typedef for our frame stack type
  typedef sass::vector<VarRefs*> EnvFrameVector;

  extern const VarRef nullidx;

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Base class/struct for variable references. Each variable belongs to an
  // environment frame (determined as we see variables during parsing). This
  // is similar to how C organizes local stack variables via frame offsets.
  class VarRef {

  public:

    uint32_t frame;
    uint32_t offset;

    VarRef() :
      frame(0xFFFFFFFF),
      offset(0xFFFFFFFF)
    {}

    VarRef(
      uint32_t frame,
      uint32_t offset,
      bool overwrites = false) :
      frame(frame),
      offset(offset)
    {}

    bool operator==(const VarRef& rhs) const {
      return frame == rhs.frame
        && offset == rhs.offset;
    }

    bool operator!=(const VarRef& rhs) const {
      return frame != rhs.frame
        || offset != rhs.offset;
    }

    bool operator<(const VarRef& rhs) const {
      if (frame < rhs.frame) return true;
      return offset < rhs.offset;
    }

    bool isValid() const { // 3%
      return offset != 0xFFFFFFFF;
    }

    bool isPrivate(uint32_t privateOffset) {
      return frame == 0xFFFFFFFF &&
        offset <= privateOffset;
    }

    //operator bool() const {
    //  return isValid();
    //}

    // Very small helper for debugging
    sass::string toString() const;

  };

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Runtime query structure
  // Created for every EnvFrame
  // Survives the actual EnvFrame
  class VarRefs {
  public:

    // EnvRoot reference
    EnvRoot& root;

    // Parent is needed during runtime for
    // dynamic setter and getter by EnvKey.
    VarRefs* pscope;

    // Global scope pointers
    uint32_t varFrame;
    uint32_t mixFrame;
    uint32_t fnFrame;

    // Lexical scope entries
    VidxEnvKeyMap varIdxs;
    MidxEnvKeyMap mixIdxs;
    FidxEnvKeyMap fnIdxs;

    // Special set with global assignments
    // Needed for imports within style-rules
    // ToDo: not really tested via specs yet?
    // EnvKeySet globals;

    // Any import may add forwarded entities to current scope
    // Since those scopes are dynamic and not global, we can't
    // simply insert our references. Therefore we must have the
    // possibility to hoist forwarded entities at any lexical scope.
    // All @use as "*" do not get exposed to the parent scope though.
    sass::vector<VarRefs*> fwdGlobal55;

    // ModuleMap<std::pair<VarRefs*, Module*>> fwdModule55;

    // Some scopes are connected to a module
    // Those expose some additional exports
    // Modules are global so we just link them
    Module* module = nullptr;

    // Rules like `@if`, `@for` etc. are semi-global (permeable).
    // Assignments directly in those can bleed to the root scope.
    bool isPermeable = false;

    // Imports are transparent for variables, functions and mixins
    // We always need to create entities inside the parent scope
    bool isImport = false;

    // Set to true once we are compiled via use or forward
    // An import does load the sheet, but does not compile it
    // Compiling it means hard-baking the config vars into it
    bool isCompiled = false;

    // Value constructor
    VarRefs(EnvRoot& root,
      VarRefs* pscope,
      uint32_t varFrame,
      uint32_t mixFrame,
      uint32_t fnFrame,
      bool isPermeable,
      bool isImport) :
      root(root),
      pscope(pscope),
      isPermeable(isPermeable),
      isImport(isImport),
      varFrame(varFrame),
      mixFrame(mixFrame),
      fnFrame(fnFrame)
    {}

    /////////////////////////////////////////////////////////////////////////
    // Register an occurrence during parsing, reserving the offset.
    // Only structures are create when calling this, the real work
    // is done on runtime, where actual stack objects are queried.
    /////////////////////////////////////////////////////////////////////////

    // Register new variable on local stack
    // Invoked mostly by stylesheet parser
    VarRef createVariable(const EnvKey& name);
    VarRef createLexicalVar(const EnvKey& name);

    // Register new function on local stack
    // Mostly invoked by built-in functions
    // Then invoked for custom C-API function
    // Finally for every parsed function rule
    VarRef createFunction(const EnvKey& name);
    VarRef createLexicalFn(const EnvKey& name);

    // Register new mixin on local stack
    // Only invoked for mixin rules
    // But also for content blocks
    VarRef createMixin(const EnvKey& name);
    VarRef createLexicalMix(const EnvKey& name);

    // Get local variable by name, needed for most simplistic case
    // for static variable optimization in loops. When we know that
    // there is an existing local variable, we can always use that!
    VarRef getLocalVariableIdx(const EnvKey& name);

    // Return mixin in lexical manner. If [passThrough] is false,
    // we abort the lexical lookup on any non-permeable scope frame.
    VarRef getMixinIdx(const EnvKey& name, bool passThrough = true);

    // Return function in lexical manner. If [passThrough] is false,
    // we abort the lexical lookup on any non-permeable scope frame.
    VarRef getFunctionIdx(const EnvKey& name, bool passThrough = true);

    // Return variable in lexical manner. If [passThrough] is false,
    // we abort the lexical lookup on any non-permeable scope frame.
    VarRef getVariableIdx(const EnvKey& name, bool passThrough = false);

    // Get a mixin associated with the under [name].
    // Will lookup from the last runtime stack scope.
    // We will move up the runtime stack until we either
    // find a defined mixin or run out of parent scopes.
    Callable* findMixin(const EnvKey& name, const sass::string& ns) const;
    Callable* findMixin(const EnvKey& name) const;
    Callable* getMixin(const EnvKey& name) const;

    // Get a function associated with the under [name].
    // Will lookup from the last runtime stack scope.
    // We will move up the runtime stack until we either
    // find a defined function or run out of parent scopes.
    CallableObj* findFunction(const EnvKey& name, const sass::string& ns) const;
    CallableObj* findFunction(const EnvKey& name) const;
    CallableObj* getFunction(const EnvKey& name) const;

    VarRef findFnIdx(const EnvKey& name, const sass::string& ns) const;
    VarRef findFnIdx(const EnvKey& name) const;
    VarRef getFnIdx(const EnvKey& name) const;

    // Get a value associated with the variable under [name].
    // If [global] flag is given, the lookup will be in the root.
    // Otherwise lookup will be from the last runtime stack scope.
    // We will move up the runtime stack until we either find a 
    // defined variable with a value or run out of parent scopes.
    Value* findVariable(const EnvKey& name, const sass::string& ns) const;
    Value* findVariable(const EnvKey& name) const;
    Value* getVariable(const EnvKey& name) const;

    VarRef findVarIdx(const EnvKey& name, const sass::string& ns) const;
    VarRef findVarIdx(const EnvKey& name) const;
    VarRef getVarIdx(const EnvKey& name) const;

    bool hasNameSpace(const sass::string& ns, const EnvKey& name) const;

    // Find function only in local frame


    VarRef setModVar(const EnvKey& name, Value* value, bool guarded, const SourceSpan& pstate) const;
    bool setModMix(const EnvKey& name, Callable* callable, bool guarded) const;
    bool setModFn(const EnvKey& name, Callable* callable, bool guarded) const;


    VarRef setModVar(const EnvKey& name, const sass::string& ns, Value* value, bool guarded, const SourceSpan& pstate);
    bool setModMix(const EnvKey& name, const sass::string& ns, Callable* fn, bool guarded);
    bool setModFn(const EnvKey& name, const sass::string& ns, Callable* fn, bool guarded);

    // Test if we are top frame
    bool isRoot() const;

    // Get next parent, but break on root
    VarRefs* getParent(bool passThrough = false) {
      if (isRoot())
        return nullptr;
      if (module)
        return nullptr;
      if (!passThrough)
        if (!isPermeable)
          return nullptr;
      return pscope;
    }

  };

  /////////////////////////////////////////////////////////////////////////
  // EnvFrames are created during the parsing phase.
  /////////////////////////////////////////////////////////////////////////

  class EnvFrame {

  public:

    // Reference to stack
    // We manage it ourself
    EnvFrameVector& stack;

    // Our runtime object
    VarRefs* idxs;

  private:

  public:

    // Value constructor
    EnvFrame(
      Compiler& compiler,
      bool isPermeable,
      bool isModule = false,
      bool isImport = false);

    // Destructor
    ~EnvFrame();
  
 };

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  class EnvRoot {

  public:

    Compiler& compiler;

    // Reference to stack
    // We manage it ourself
    EnvFrameVector& stack;

    // Our runtime object
    VarRefs* idxs;

  private:

    // We work together
    friend class Compiler;
    friend class EnvScope;
    friend class EnvFrame;
    friend class Preloader;
    friend class VarRefs;
    friend class Eval;

    // Growable runtime stack (get offset by xxxFramePtr).
    // These vectors are the main stacks during runtime.
    // When a scope with two variables is executed, two
    // new items are added to the variables stack. If the
    // same scope is called more than once, its variables
    // are added multiple times so we can revert to them.
    // variables[varFramePtr[vidx.frame] + vidx.offset]
    sass::vector<ValueObj> variables;
    sass::vector<CallableObj> mixins;
    sass::vector<CallableObj> functions;

    // Every scope we execute in sass gets an entry here.
    // The value stored here is the base address of the
    // active scope, used to calculate the final offset.
    // Gives current offset into growable runtime stack.
    // Old values are restored when scopes are exited.
    // Access it by absolute `frameOffset`
    sass::vector<uint32_t> varFramePtr;
    sass::vector<uint32_t> mixFramePtr;
    sass::vector<uint32_t> fnFramePtr;

    // Internal functions are stored here
    sass::vector<CallableObj> intFunction;
    sass::vector<CallableObj> intMixin;
    sass::vector<ValueObj> intVariables;
  public:
    // Last private accessible item
    uint32_t privateVarOffset = 0;
    uint32_t privateMixOffset = 0;
    uint32_t privateFnOffset = 0;
  private:
    // All created runtime variable objects.
    // Needed to track the memory allocations
    // And useful to resolve parents indirectly
    // Access it by absolute `frameOffset`
    sass::vector<VarRefs*> scopes3;

  public:

    // Value constructor
    EnvRoot(Compiler& compiler);

    // Destructor
    ~EnvRoot() {
      // Pop from stack
      stack.pop_back();
      // Take care of scope pointers
      for (VarRefs* idx : scopes3) {
        delete idx;
      }
      // Delete our env
      delete idxs;
    }

    // Update variable references and assignments
    // Process all variable expression (e.g. variables used in sass-scripts).
    // Move up the tree to find possible parent scopes also containing this
    // variable. On runtime we will return the first item that has a value set.
    // Process all variable assignment rules. Assignments can bleed up to the
    // parent scope under certain conditions. We bleed up regular style rules,
    // but not into the root scope itself (until it is semi global scope).
    void finalizeScopes();

    // Runtime check to see if we are currently in global scope
    bool isGlobal() const { return idxs->root.stack.size() == 1; }

    // Get value instance by stack index reference
    // Just converting and returning reference to array offset
    ValueObj& getVariable(const VarRef& vidx);
    ValueObj& getModVar(const uint32_t offset);

    // Get function instance by stack index reference
    // Just converting and returning reference to array offset
    CallableObj& getFunction(const VarRef& vidx);

    // Get mixin instance by stack index reference
    // Just converting and returning reference to array offset
    CallableObj& getMixin(const VarRef& midx);

    // Set items on runtime/evaluation phase via references
    // Just converting reference to array offset and assigning
    void setVariable(const VarRef& vidx, ValueObj value, bool guarded);

    void setModVar(const uint32_t offset, Value* value, bool guarded, const SourceSpan& pstate);
    void setModMix(const uint32_t offset, Callable* callable, bool guarded);
    void setModFn(const uint32_t offset, Callable* callable, bool guarded);

    // Set items on runtime/evaluation phase via references
    // Just converting reference to array offset and assigning
    void setVariable(uint32_t frame, uint32_t offset, ValueObj value, bool guarded);

    // Set items on runtime/evaluation phase via references
    // Just converting reference to array offset and assigning
    void setFunction(const VarRef& fidx, UserDefinedCallableObj value, bool guarded);

    // Set items on runtime/evaluation phase via references
    // Just converting reference to array offset and assigning
    void setMixin(const VarRef& midx, UserDefinedCallableObj value, bool guarded);

    // Get a mixin associated with the under [name].
    // Will lookup from the last runtime stack scope.
    // We will move up the runtime stack until we either
    // find a defined mixin or run out of parent scopes.
    Callable* findMixin(const EnvKey& name, const sass::string& ns) const;
    Callable* findMixin(const EnvKey& name) const;

    // Get a function associated with the under [name].
    // Will lookup from the last runtime stack scope.
    // We will move up the runtime stack until we either
    // find a defined function or run out of parent scopes.
    CallableObj* findFunction(const EnvKey& name, const sass::string& ns) const;
    CallableObj* findFunction(const EnvKey& name) const;
    VarRef findFnIdx(const EnvKey& name, const sass::string& ns) const;
    VarRef findFnIdx(const EnvKey& name) const;

    // Get a value associated with the variable under [name].
    // If [global] flag is given, the lookup will be in the root.
    // Otherwise lookup will be from the last runtime stack scope.
    // We will move up the runtime stack until we either find a 
    // defined variable with a value or run out of parent scopes.
    Value* findVariable(const EnvKey& name, const sass::string& ns) const;
    Value* findVariable(const EnvKey& name, bool global = false) const;
    VarRef findVarIdx(const EnvKey& name, const sass::string& ns) const;
    VarRef findVarIdx(const EnvKey& name) const;

    VarRef setModVar(const EnvKey& name, const sass::string& ns, Value* value, bool guraded, const SourceSpan& pstate);
    bool setModMix(const EnvKey& name, const sass::string& ns, Callable* fn, bool guraded);
    bool setModFn(const EnvKey& name, const sass::string& ns, Callable* fn, bool guraded);

    // Set a value associated with the variable under [name].
    // If [global] flag is given, the lookup will be in the root.
    // Otherwise lookup will be from the last runtime stack scope.
    // We will move up the runtime stack until we either find a 
    // defined variable with a value or run out of parent scopes.
    VarRef setVariable(const EnvKey& name, bool guarded, bool global);
    VarRef setFunction(const EnvKey& name, bool guarded, bool global);
    VarRef setMixin(const EnvKey& name, bool guarded, bool global);

  };

  /////////////////////////////////////////////////////////////////////////
  // EnvScopes are created during evaluation phase. When we enter a parsed
  // scope, e.g. a function, mixin or style-rule, we create a new EnvScope
  // object on the stack and pass it the runtime environment and the current
  // stack frame (in form of a VarRefs pointer). We will "allocate" the needed
  // space for scope items and update any offset pointers. Once we go out of
  // scope the previous state is restored by unwinding the runtime stack.
  /////////////////////////////////////////////////////////////////////////

  class EnvScope {

  private:

    // Runtime environment
    EnvRoot& env;

    // Frame stack index references
    VarRefs* idxs;

    // Remember previous "addresses"
    // Restored when we go out of scope
    uint32_t oldVarFrame;
    uint32_t oldVarOffset;
    uint32_t oldMixFrame;
    uint32_t oldMixOffset;
    uint32_t oldFnFrame;
    uint32_t oldFnOffset;

    bool wasActive;

  public:

    // Put frame onto stack
    EnvScope(
      EnvRoot& env,
      VarRefs* idxs) :
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

      wasActive = idxs->isCompiled;
      idxs->isCompiled = true;

      if (idxs->varFrame != 0xFFFFFFFF) {

        // Check if we have scoped variables
        if (idxs->varIdxs.size() != 0) {
          // Get offset into variable vector
          oldVarOffset = (uint32_t)env.variables.size();
          // Remember previous frame "addresses"
          oldVarFrame = env.varFramePtr[idxs->varFrame];
          // Update current frame offset address
          env.varFramePtr[idxs->varFrame] = oldVarOffset;
          // Create space for variables in this frame scope
          env.variables.resize(oldVarOffset + idxs->varIdxs.size());
        }

        // Check if we have scoped mixins
        if (idxs->mixIdxs.size() != 0) {
          // Get offset into mixin vector
          oldMixOffset = (uint32_t)env.mixins.size();
          // Remember previous frame "addresses"
          oldMixFrame = env.mixFramePtr[idxs->mixFrame];
          // Update current frame offset address
          env.mixFramePtr[idxs->mixFrame] = oldMixOffset;
          // Create space for mixins in this frame scope
          env.mixins.resize(oldMixOffset + idxs->mixIdxs.size());
        }

        // Check if we have scoped functions
        if (idxs->fnIdxs.size() != 0) {
          // Get offset into function vector
          oldFnOffset = (uint32_t)env.functions.size();
          // Remember previous frame "addresses"
          oldFnFrame = env.fnFramePtr[idxs->fnFrame];
          // Update current frame offset address
          env.fnFramePtr[idxs->fnFrame] = oldFnOffset;
          // Create space for functions in this frame scope
          env.functions.resize(oldFnOffset + idxs->fnIdxs.size());
        }

      }


      // Push frame onto stack
      // Mostly for dynamic lookups
      env.stack.push_back(idxs);

    }
    // EO ctor

    // Restore old state on destruction
    ~EnvScope()
    {

      // The frame might be fully empty
      // Meaning it no scoped items at all
      if (idxs == nullptr) return;

      if (idxs->varFrame != 0xFFFFFFFF) {

        // Check if we had scoped variables
        if (idxs->varIdxs.size() != 0) {
          // Truncate variable vector
          env.variables.resize(
            oldVarOffset);
          // Restore old frame address
          env.varFramePtr[idxs->varFrame] =
            oldVarFrame;
        }

        // Check if we had scoped mixins
        if (idxs->mixIdxs.size() != 0) {
          // Truncate existing vector
          env.mixins.resize(
            oldMixOffset);
          // Restore old frame address
          env.mixFramePtr[idxs->mixFrame] =
            oldMixFrame;
        }

        // Check if we had scoped functions
        if (idxs->fnIdxs.size() != 0) {
          // Truncate existing vector
          env.functions.resize(
            oldFnOffset);
          // Restore old frame address
          env.fnFramePtr[idxs->fnFrame] =
            oldFnFrame;
        }

      }

      idxs->isCompiled = wasActive;

      // Pop frame from stack
      env.stack.pop_back();

    }
    // EO dtor

  };

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

}

#endif
