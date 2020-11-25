/*****************************************************************************/
/* Part of LibSass, released under the MIT license (See LICENSE.txt).        */
/*****************************************************************************/
#ifndef SASS_MODULES_HPP
#define SASS_MODULES_HPP

// sass.hpp must go before all system headers
// to get the __EXTENSIONS__ fix on Solaris.
#include "capi_sass.hpp"

#include "environment_cnt.hpp"

namespace Sass {

  class VarRefs;
  class EnvRoot;
  class EnvKey;

  class Env
  {
  public:
    VarRefs* idxs = nullptr;
  public:
    Env(VarRefs* idxs)
      : idxs(idxs) {}
  };

  /////////////////////////////////////////////////////////////////////////
  // A module is first a foremost a unit that provides variables,
  // functions and mixins. We know built-in modules, which don't
  // have any content ever are always loaded and available. Custom
  // modules are loaded from any given url. They are related and
  // linked to regular @imports, as those are also some kind of
  // special modules. An @import will load as a module, but that
  // module with not be compiled until used by @forward or @use.
  /////////////////////////////////////////////////////////////////////////
  // When a module is @imported, all its local root variables
  // are "exposed" to the caller's scope. Those are actual new
  // instances of the variables, as e.g. the same file might be
  // imported into different style-rules. The variables will then
  // really exist in the env of those style-rules. Similarly when
  // importing on the root scope, the variables are not shared with
  // the internal ones (the ones we reference when being @used).
  /////////////////////////////////////////////////////////////////////////
  // To support this, we can't statically optimize variable accesses
  // across module boundaries. We need to mark environments to remember
  // in which context a module was brought into the tree. For imports
  // we simply skip the whole frame. Instead the lookup should find
  // the variable we created in the caller's scope.
  /////////////////////////////////////////////////////////////////////////

  class Module : public Env
  {
  public:

    // Flag for internal modules
    // They don't have any content
    bool isBuiltIn = false;

    // Only makes sense for non built-ins
    // True once the content has been loaded
    bool isLoaded = false;

    // Only makes sense for non built-ins
    // True once the module is compiled and ready
    bool isCompiled = false;

    // The loaded AST-Tree
    RootObj loaded83;

    // The compiled AST-Tree
    CssParentNodeObj loaded;

    // All @forward rules get merged into these objects
    // Those are not available on the local scope, they
    // are only used when another module consumes us!
    // On @use those must be merged with local ones!
    VidxEnvKeyMap mergedFwdVar;
    MidxEnvKeyMap mergedFwdMix;
    FidxEnvKeyMap mergedFwdFn;

    // Special set with global assignments
    // Needed for imports within style-rules
    EnvKeySet globals;

    // Uses with namespace
    ModuleMap<Module*> moduse;

    // Uses without namespace (as *)
    sass::vector<Module*> globuse;

  public:

    Module(VarRefs* idxs);

  };

  class BuiltInMod : public Module
  {
  public:

    void addFunction(const EnvKey& name, uint32_t offset);
    void addVariable(const EnvKey& name, uint32_t offset);
    void addMixin(const EnvKey& name, uint32_t offset);
    BuiltInMod(EnvRoot& root);
    ~BuiltInMod();
  };

}

#endif
