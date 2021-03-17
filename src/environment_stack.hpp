/*****************************************************************************/
/* Part of LibSass, released under the MIT license (See LICENSE.txt).        */
/*****************************************************************************/
#ifndef SASS_ENV_STACK_HPP
#define SASS_ENV_STACK_HPP

// sass.hpp must go before all system headers
// to get the __EXTENSIONS__ fix on Solaris.
#include "capi_sass.hpp"

#include "ast_fwd_decl.hpp"
#include "environment_cnt.hpp"

namespace Sass {

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Helper typedef for our frame stack type
  typedef sass::vector<EnvRefs*> EnvFrameVector;

  // Constant similar to nullptr
  extern const EnvRef nullidx;

  /////////////////////////////////////////////////////////////////////////
  // Base class/struct for variable references. Each variable belongs to an
  // environment frame (determined as we see variables during parsing). Similar
  // to how C organizes local variables via function stack pointers.
  /////////////////////////////////////////////////////////////////////////
  class EnvRef
  {
  public:

    // The lexical frame pointer
    // Each parsed scope gets its own
    uint32_t frame;

    // Local offset within the frame
    uint32_t offset;

    // Default constructor
    EnvRef() :
      frame(0xFFFFFFFF),
      offset(0xFFFFFFFF)
    {}

    // Value constructor
    EnvRef(
      uint32_t frame,
      uint32_t offset) :
      frame(frame),
      offset(offset)
    {}

    // Implement native equality operator
    bool operator==(const EnvRef& rhs) const {
      return frame == rhs.frame
        && offset == rhs.offset;
    }

    // Implement native inequality operator
    bool operator!=(const EnvRef& rhs) const {
      return frame != rhs.frame
        || offset != rhs.offset;
    }

    // Implement operator to allow use in sets
    bool operator<(const EnvRef& rhs) const {
      if (frame < rhs.frame) return true;
      return offset < rhs.offset;
    }

    // Check if reference is valid
    bool isValid() const { // 3%
      return offset != 0xFFFFFFFF;
    }

    // Check if entity is read-only
    bool isPrivate(uint32_t privateOffset) {
      return frame == 0xFFFFFFFF &&
        offset <= privateOffset;
    }

    // Small helper for debugging
    sass::string toString() const;

  };

  /////////////////////////////////////////////////////////////////////////
  // 
  /////////////////////////////////////////////////////////////////////////

  // Runtime query structure
  // Created for every EnvFrame
  // Survives the actual EnvFrame
  class EnvRefs {
  public:

    // EnvRoot reference
    EnvRoot& root;

    // Parent is needed during runtime for
    // dynamic setter and getter by EnvKey.
    EnvRefs* pscope;

    // Global scope pointers
    uint32_t framePtr;

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
    sass::vector<EnvRefs*> forwards;

    // Some scopes are connected to a module
    // Those expose some additional exports
    // Modules are global so we just link them
    Module* module = nullptr;

    // Imports are transparent for variables, functions and mixins
    // We always need to create entities inside the parent scope
    bool isImport = false;

    // Rules like `@if`, `@for` etc. are semi-global (permeable).
    // Assignments directly in those can bleed to the root scope.
    bool isSemiGlobal = false;

    // Set to true once we are compiled via use or forward
    // An import does load the sheet, but does not compile it
    // Compiling it means hard-baking the config vars into it
    bool isCompiled = false;

    // Value constructor
    EnvRefs(EnvRoot& root,
      EnvRefs* pscope,
      uint32_t framePtr,
      bool isSemiGlobal,
      bool isImport) :
      root(root),
      pscope(pscope),
      framePtr(framePtr),
      isImport(isImport),
      isSemiGlobal(isSemiGlobal)
    {}

    /////////////////////////////////////////////////////////////////////////
    // Register an occurrence during parsing, reserving the offset.
    // Only structures are create when calling this, the real work
    // is done on runtime, where actual stack objects are queried.
    /////////////////////////////////////////////////////////////////////////

    // Register new variable on local stack
    // Invoked mostly by stylesheet parser
    EnvRef createVariable(const EnvKey& name);

    // Register new function on local stack
    // Mostly invoked by built-in functions
    // Then invoked for custom C-API function
    // Finally for every parsed function rule
    EnvRef createFunction(const EnvKey& name);

    // Register new mixin on local stack
    // Only invoked for mixin rules
    // But also for content blocks
    EnvRef createMixin(const EnvKey& name);

    // Get a mixin associated with the under [name].
    // Will lookup from the last runtime stack scope.
    // We will move up the runtime stack until we either
    // find a defined mixin or run out of parent scopes.

    // Get a function associated with the under [name].
    // Will lookup from the last runtime stack scope.
    // We will move up the runtime stack until we either
    // find a defined function or run out of parent scopes.
    

    // Get a value associated with the variable under [name].
    // If [global] flag is given, the lookup will be in the root.
    // Otherwise lookup will be from the last runtime stack scope.
    // We will move up the runtime stack until we either find a 
    // defined variable with a value or run out of parent scopes.

    void findVarIdxs(sass::vector<EnvRef>& vidxs, const EnvKey& name) const;

    EnvRef findVarIdx(const EnvKey& name, const sass::string& ns) const;
    EnvRef findFnIdx22(const EnvKey& name, const sass::string& ns) const;
    EnvRef findMixIdx22(const EnvKey& name, const sass::string& ns) const;

    EnvRef findVarIdx(const EnvKey& name) const;
    EnvRef findFnIdx(const EnvKey& name) const;
    EnvRef findMixIdx(const EnvKey& name) const;

    bool hasNameSpace(const sass::string& ns, const EnvKey& name) const;

    // Find function only in local frame


    EnvRef setModVar(const EnvKey& name, Value* value, bool guarded, const SourceSpan& pstate) const;


    EnvRef setModVar(const EnvKey& name, const sass::string& ns, Value* value, bool guarded, const SourceSpan& pstate);

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
    EnvRefs* idxs;

  private:

  public:

    // Value constructor
    EnvFrame(
      Compiler& compiler,
      bool isSemiGlobal,
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

    // Root runtime env
    EnvRefs* idxs;

  private:

    // We work together
    friend class Compiler;
    friend class EnvScope;
    friend class EnvFrame;
    friend class EnvRefs;

    // Growable runtime stack (get offset by xxxStackPtr).
    // These vectors are the main stacks during runtime.
    // When a scope with two variables is executed, two
    // new items are added to the variables stack. If the
    // same scope is called more than once, its variables
    // are added multiple times so we can revert to them.
    // variables[varStackPtr[vidx.frame] + vidx.offset]
    sass::vector<ValueObj> varStack;
    sass::vector<CallableObj> mixStack;
    sass::vector<CallableObj> fnStack;

    // Every scope we execute in sass gets an entry here.
    // The value stored here is the base address of the
    // active scope, used to calculate the final offset.
    // Gives current offset into growable runtime stack.
    // Old values are restored when scopes are exited.
    // Access it by absolute `frameOffset`
    sass::vector<uint32_t> varStackPtr;
    sass::vector<uint32_t> mixStackPtr;
    sass::vector<uint32_t> fnStackPtr;

    // Internal functions are stored here
    sass::vector<CallableObj> intFunction;
    sass::vector<CallableObj> intMixin;
    sass::vector<ValueObj> intVariables;

    // Last private accessible item
    uint32_t privateVarOffset = 0;
    uint32_t privateMixOffset = 0;
    uint32_t privateFnOffset = 0;

    // All created runtime variable objects.
    // Needed to track the memory allocations
    // And useful to resolve parents indirectly
    // Access it by absolute `frameOffset`
    sass::vector<EnvRefs*> scopes;

  public:

    // Value constructor
    EnvRoot(Compiler& compiler);

    // Destructor
    ~EnvRoot() {
      // Pop from stack
      stack.pop_back();
      // Take care of scope pointers
      for (EnvRefs* idx : scopes) {
        delete idx;
      }
      // Delete our env
      delete idxs;
    }

    // Runtime check to see if we are currently in global scope
    bool isGlobal() const { return idxs->root.stack.size() == 1; }

    // Get value instance by stack index reference
    // Just converting and returning reference to array offset
    ValueObj& getVariable(const EnvRef& vidx);
    ValueObj& getModVar(const uint32_t offset);

    // Get function instance by stack index reference
    // Just converting and returning reference to array offset
    CallableObj& getFunction(const EnvRef& vidx);
    CallableObj& getModFn(const uint32_t offset);

    // Get mixin instance by stack index reference
    // Just converting and returning reference to array offset
    CallableObj& getMixin(const EnvRef& midx);
    CallableObj& getModMix(const uint32_t offset);

    // Set items on runtime/evaluation phase via references
    // Just converting reference to array offset and assigning
    void setVariable(const EnvRef& vidx, Value* value, bool guarded);


    void setModVar(const uint32_t offset, Value* value, bool guarded, const SourceSpan& pstate);

    // Set items on runtime/evaluation phase via references
    // Just converting reference to array offset and assigning
    void setVariable(uint32_t frame, uint32_t offset, Value* value, bool guarded);

    // Set items on runtime/evaluation phase via references
    // Just converting reference to array offset and assigning
    void setFunction(const EnvRef& fidx, UserDefinedCallable* value, bool guarded);

    // Set items on runtime/evaluation phase via references
    // Just converting reference to array offset and assigning
    void setMixin(const EnvRef& midx, UserDefinedCallable* value, bool guarded);

    // Get a value associated with the variable under [name].
    // If [global] flag is given, the lookup will be in the root.
    // Otherwise lookup will be from the last runtime stack scope.
    // We will move up the runtime stack until we either find a 
    // defined variable with a value or run out of parent scopes.

    EnvRef findFnIdx(
      const EnvKey& name,
      const sass::string& ns) const;

    EnvRef findMixIdx(
      const EnvKey& name,
      const sass::string& ns) const;

    EnvRef findVarIdx(
      const EnvKey& name,
      const sass::string& ns,
      bool global = false) const;

    void findVarIdxs(
      sass::vector<EnvRef>& vidxs,
      const EnvKey& name) const;

  };

  /////////////////////////////////////////////////////////////////////////
  // EnvScopes are created during evaluation phase. When we enter a parsed
  // scope, e.g. a function, mixin or style-rule, we create a new EnvScope
  // object on the stack and pass it the runtime environment and the current
  // stack frame (in form of a EnvRefs pointer). We will "allocate" the needed
  // space for scope items and update any offset pointers. Once we go out of
  // scope the previous state is restored by unwinding the runtime stack.
  /////////////////////////////////////////////////////////////////////////

  class EnvScope {

  private:

    // Runtime environment
    EnvRoot& env;

    // Frame stack index references
    EnvRefs* idxs;

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
      EnvRefs* idxs) :
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

      if (idxs->framePtr != 0xFFFFFFFF) {

        // Check if we have scoped variables
        if (idxs->varIdxs.size() != 0) {
          // Get offset into variable vector
          oldVarOffset = (uint32_t)env.varStack.size();
          // Remember previous frame "addresses"
          oldVarFrame = env.varStackPtr[idxs->framePtr];
          // Update current frame offset address
          env.varStackPtr[idxs->framePtr] = oldVarOffset;
          // Create space for variables in this frame scope
          env.varStack.resize(oldVarOffset + idxs->varIdxs.size());
        }

        // Check if we have scoped mixins
        if (idxs->mixIdxs.size() != 0) {
          // Get offset into mixin vector
          oldMixOffset = (uint32_t)env.mixStack.size();
          // Remember previous frame "addresses"
          oldMixFrame = env.mixStackPtr[idxs->framePtr];
          // Update current frame offset address
          env.mixStackPtr[idxs->framePtr] = oldMixOffset;
          // Create space for mixins in this frame scope
          env.mixStack.resize(oldMixOffset + idxs->mixIdxs.size());
        }

        // Check if we have scoped functions
        if (idxs->fnIdxs.size() != 0) {
          // Get offset into function vector
          oldFnOffset = (uint32_t)env.fnStack.size();
          // Remember previous frame "addresses"
          oldFnFrame = env.fnStackPtr[idxs->framePtr];
          // Update current frame offset address
          env.fnStackPtr[idxs->framePtr] = oldFnOffset;
          // Create space for functions in this frame scope
          env.fnStack.resize(oldFnOffset + idxs->fnIdxs.size());
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

      if (idxs->framePtr != 0xFFFFFFFF) {

        // Check if we had scoped variables
        if (idxs->varIdxs.size() != 0) {
          // Truncate variable vector
          env.varStack.resize(
            oldVarOffset);
          // Restore old frame address
          env.varStackPtr[idxs->framePtr] =
            oldVarFrame;
        }

        // Check if we had scoped mixins
        if (idxs->mixIdxs.size() != 0) {
          // Truncate existing vector
          env.mixStack.resize(
            oldMixOffset);
          // Restore old frame address
          env.mixStackPtr[idxs->framePtr] =
            oldMixFrame;
        }

        // Check if we had scoped functions
        if (idxs->fnIdxs.size() != 0) {
          // Truncate existing vector
          env.fnStack.resize(
            oldFnOffset);
          // Restore old frame address
          env.fnStackPtr[idxs->framePtr] =
            oldFnFrame;
        }

      }

      // Pop frame from stack
      env.stack.pop_back();

    }
    // EO dtor

  };

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

}

#endif
