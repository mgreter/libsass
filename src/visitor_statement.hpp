#ifndef SASS_VISITOR_STATEMENT_H
#define SASS_VISITOR_STATEMENT_H

#include "ast_fwd_decl.hpp"

namespace Sass {

  // An interface for [visitors][] that traverse Sass statements.
  // [visitors]: https://en.wikipedia.org/wiki/Visitor_pattern

  template <typename T>
  class StatementVisitor {

    // T* visitAtRootRule(AtRootRule node);
    // T* visitAtRule(AtRule node);
    // T* visitContentBlock(ContentBlock node);
    // T* visitContentRule(ContentRule node);
    // T* visitDebugRule(DebugRule node);
    // T* visitDeclaration(Declaration* node);
    // T* visitEachRule(EachRule node);
    // T* visitErrorRule(ErrorRule node);
    // T* visitExtendRule(ExtendRule* node);
    // T* visitForRule(ForRule node);
    // T* visitFunctionRule(FunctionRule node);
    // T* visitIfRule(IfRule node);
    // T* visitImportRule(ImportRule* node);
    // T* visitIncludeRule(IncludeRule node);
    // T* visitLoudComment(LoudComment node);
    // T* visitMediaRule(MediaRule* node);
    // T* visitMixinRule(MixinRule node);
    // T* visitReturnRule(ReturnRule node);
    // T* visitSilentComment(SilentComment node);
    // T* visitStyleRule(StyleRule node);
    // T* visitStylesheet(Stylesheet node);
    // T* visitSupportsRule(SupportsRule node);
    // T* visitUseRule(UseRule node);
    // T* visitVariableDeclaration(VariableDeclaration node);
    // T* visitWarnRule(WarnRule node);
    // T* visitWhileRule(WhileRule node);

  };

}

#endif
