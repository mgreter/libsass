#ifndef SASS_STYLESHEET_HPP
#define SASS_STYLESHEET_HPP

// sass.hpp must go before all system headers
// to get the __EXTENSIONS__ fix on Solaris.
#include "capi_sass.hpp"

#include "file.hpp"
#include "ast_nodes.hpp"
#include "ast_css.hpp"
#include "modules.hpp"

namespace Sass {


  class Moduled
  {
  public:

    // True once loaded
    bool isCompiled = false;

    // Modules that this module uses.
    // List<BuiltInMod> get upstream;

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

  // parsed stylesheet from loaded resource
  // this should be a `Module` for sass 4.0
  class Root final : public AstNode,
    public Vectorized<Statement>,
    public Moduled
  {
  public:

    // Import object through which this module was loaded.
    // It also has the input type (css vs sass) attached
    ImportObj import;

    Root(const SourceSpan& pstate, size_t reserve = 0)
      : AstNode(pstate), Vectorized<Statement>(reserve)
    {}

    Root(const SourceSpan& pstate, StatementVector&& vec)
      : AstNode(pstate), Vectorized<Statement>(std::move(vec))
    {}

  };

}

#endif
