#ifndef SASS_AST_BASE_H
#define SASS_AST_BASE_H

#include <stdint.h>
#include "sass/values.h"
#include "memory/shared_ptr.hpp"
#include "ast_def_macros.hpp"
#include "source_span.hpp"
#include "operation.hpp"
#include "mapping.hpp"

namespace Sass {

  uint8_t sass_op_to_precedence(enum Sass_OP op);
  const char* sass_op_to_name(enum Sass_OP op);
  const char* sass_op_separator(enum Sass_OP op);

  //////////////////////////////////////////////////////////
  // Abstract base class for all abstract syntax tree nodes.
  //////////////////////////////////////////////////////////
  class AST_Node : public SharedObj {
    ADD_PROPERTY(SourceSpan, pstate);
  public:

    AST_Node(const SourceSpan& pstate)
      : SharedObj(), pstate_(pstate)
    { }

    AST_Node(SourceSpan&& pstate)
      : SharedObj(), pstate_(std::move(pstate))
    { }

    AST_Node(const AST_Node* ptr)
      : pstate_(ptr->pstate_)
      // SharedObj(ptr)
    { }

    // allow implicit conversion to string
    // needed by SharedPtr implementation
    operator sass::string() {
      return to_string();
    }

    virtual ~AST_Node() {};

    virtual sass::string inspect() const { return to_string({ SASS_STYLE_INSPECT, SassDefaultPrecision }); }

    virtual sass::string to_string() const;

    virtual sass::string to_string(Sass_Inspect_Options opt) const;

    virtual sass::string to_css(Sass_Inspect_Options opt, bool quotes = false) const;
    virtual sass::string to_css(Sass_Inspect_Options opt, sass::vector<Mapping>& mappings, bool quotes = false) const;
    virtual sass::string to_css(sass::vector<Mapping>& mappings, bool quotes = false) const;
    virtual sass::string to_css(bool quotes) const;

    virtual void cloneChildren() {};


    // ATTACH_ABSTRACT_EQ_OPERATOR(AST_Node);
    // ATTACH_ABSTRACT_CMP_OPERATORS(AST_Node);

    ATTACH_ABSTRACT_CRTP_PERFORM_METHODS()
  };

};

#endif
