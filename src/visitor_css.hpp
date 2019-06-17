#ifndef SASS_VISITOR_CSS_H
#define SASS_VISITOR_CSS_H

#include "ast_fwd_decl.hpp"

namespace Sass {

  // An interface for [visitors][] that traverse CSS statements.
  // [visitors]: https://en.wikipedia.org/wiki/Visitor_pattern

  template <typename T>
  class CssVisitor {

    // T visitCssAtRule(CssAtRule node);
    // T visitCssComment(CssComment node);
    // T visitCssDeclaration(CssDeclaration node);
    // T visitCssImport(CssImport* node);
    // T visitCssKeyframeBlock(CssKeyframeBlock node);
    virtual T visitCssMediaRule(CssMediaRule* node);
    // T visitCssStyleRule(CssStyleRule node);
    // T visitCssStylesheet(CssStylesheet node);
    // T visitCssSupportsRule(CssSupportsRule* node);

  };

}

#endif
