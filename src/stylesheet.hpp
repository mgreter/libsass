#ifndef SASS_STYLESHEET_HPP
#define SASS_STYLESHEET_HPP

// sass.hpp must go before all system headers
// to get the __EXTENSIONS__ fix on Solaris.
#include "capi_sass.hpp"

#include "file.hpp"
#include "ast_nodes.hpp"
#include "ast_css.hpp"

namespace Sass {


  class Moduled
  {
  public:

    // True once loaded
    bool isActive = false;

    // Forwarded items must be on internal scope
    VarRefs* idxs = nullptr;
    VarRefs* exposing = nullptr;

    // All @forward rules get merged into these objects
    // Those are not available on the local scope, they
    // are only used when another module consumes us!
    // On @use those must be merged with local ones!
    VidxEnvKeyMap mergedFwdVar;
    MidxEnvKeyMap mergedFwdMix;
    FidxEnvKeyMap mergedFwdFn;

    // The evaluated css tree
    CssParentNodeObj loaded = nullptr;

  };

  class Module
  {
  public:
    VarRefs* idxs;
    bool isActive3 = false;
    void addFunction(const EnvKey& name, uint32_t offset);
    void addVariable(const EnvKey& name, uint32_t offset);
    void addMixin(const EnvKey& name, uint32_t offset);
    Module(EnvRoot& root);
  };

  // parsed stylesheet from loaded resource
  // this should be a `Module` for sass 4.0
  class Root final : public AstNode,
    public Vectorized<Statement>,
    public Moduled
  {
  public:

    // The canonical URL for this module's source file. This may be `null`
    // if the module was loaded from a string without a URL provided.
    // It also has the input type (css vs sass) attached
    ImportObj import;

    // Modules that this module uses.
    // List<Module> get upstream;

    // The module's variables.
    // Map<String, Value> get variables;

    // The module's functions. Implementations must ensure
    // that each [Callable] is stored under its own name.
    // Map<String, Callable> get functions;

    // The module's mixins. Implementations must ensure that
    // each [Callable] is stored under its own name.
    // Map<String, Callable> get mixins;

    // The extensions defined in this module, which is also able to update
    // [css]'s style rules in-place based on downstream extensions.
    // Extender extender;

    Root(const SourceSpan& pstate, size_t reserve = 0)
      : AstNode(pstate), Vectorized<Statement>(reserve)
    {}

    Root(const SourceSpan& pstate, StatementVector&& vec)
      : AstNode(pstate), Vectorized<Statement>(std::move(vec))
    {}

  };

}

#endif
