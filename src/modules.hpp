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

  class Module : public Env
  {
  public:
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

    // Modules that this module uses.
    sass::vector<Module*> upstream;
    sass::vector<Module*> forwards2;

    sass::vector<Module*> globuse;

    ModuleMap<Module*> uses;
    ModuleSet loaded;
    

    CssParentNodeObj compiled66;

    bool isBuiltIn = false;

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
  };

}

#endif
