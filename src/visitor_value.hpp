#ifndef SASS_VISITOR_VALUE_H
#define SASS_VISITOR_VALUE_H

#include "ast_fwd_decl.hpp"

namespace Sass {

  // An interface for [visitors][] that traverse SassScript values.
  // [visitors]: https://en.wikipedia.org/wiki/Visitor_pattern

  template <typename T>
  class ValueVisitor {

    // T visitBoolean(Boolean value);
    virtual T visitBoolean(Boolean* value) = 0;
    // T visitColor(SassColor value);
    virtual T visitColor(Color* value) = 0;
    // T visitFunction(Function value);
    // T visitList(SassList value);
    // virtual T visitList(List* value) = 0;
    // T visitMap(Map value);
    virtual T visitMap(Map* value) = 0;
    // T visitNull(SassNull value);
    virtual T visitNull(Null* value) = 0;
    // T visitNumber(Number value);
    virtual T visitNumber(Number* value) = 0;
    // T visitString(SassString value);

  };

}

#endif
