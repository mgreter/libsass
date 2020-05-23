#ifndef SASS_AST_BASE_H
#define SASS_AST_BASE_H

// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include <stdint.h>
#include "ast_def_macros.hpp"
#include "ast_fwd_decl.hpp"
#include "source_span.hpp"
#include "operation.hpp"
#include "mapping.hpp"
#include "logger.hpp"

// #include "sass/values.h"
// #include "memory/shared_ptr.hpp"

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
      return "Fuck ya";
    }

    virtual ~AST_Node() {};

    virtual sass::string inspect(int precision = SassDefaultPrecision) const;

    virtual sass::string to_string() const;

    virtual sass::string to_string(Sass_Inspect_Options opt) const;

    virtual sass::string to_css(Logger& logger, Sass_Inspect_Options opt, bool quotes = false) const;
    // virtual sass::string to_css(Logger& logger, Sass_Inspect_Options opt, sass::vector<Mapping>& mappings, bool quotes = false) const;
    // virtual sass::string to_css(Logger& logger, sass::vector<Mapping>& mappings, bool quotes = false) const;
    virtual sass::string to_css(Logger& logger, bool quotes = false) const;

    virtual void cloneChildren() {};


    // ATTACH_ABSTRACT_EQ_OPERATOR(AST_Node);
    // ATTACH_ABSTRACT_CMP_OPERATORS(AST_Node);

    ATTACH_ABSTRACT_CRTP_PERFORM_METHODS()
  };

};

#endif
