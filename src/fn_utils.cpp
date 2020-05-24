// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"
#include "ast.hpp"

#include "eval.hpp"
#include "fn_utils.hpp"
#include "util_string.hpp"
#include "parser_scss.hpp"
#include "parser_selector.hpp"

namespace Sass {

  Callable::Callable(
    const SourceSpan& pstate) :
    AST_Node(pstate) {}

  PlainCssCallable::PlainCssCallable(
    const SourceSpan& pstate, const sass::string& name) :
    Callable(pstate), name_(name) {}

  bool PlainCssCallable::operator==(const Callable& rhs) const
  {
    if (const PlainCssCallable * plain = Cast<PlainCssCallable>(&rhs)) {
      return this == plain;
    }
    return false;
  }

  BuiltInCallable::BuiltInCallable(
    const EnvKey& name,
    ArgumentDeclaration* parameters,
    const SassFnSig& callback) :
    Callable(SourceSpan::tmp("[BUILTIN]")),
    name_(name),
    // Create a single entry in overloaded function
    function_(SassFnPair{ parameters, callback })
  {
  }

  const SassFnPair& BuiltInCallable::callbackFor(
    const ArgumentResults& evaluated)
  {
    return function_;
  }

  BuiltInCallables::BuiltInCallables(
    const EnvKey& name,
    const SassFnPairs& overloads) :
    Callable(SourceSpan::tmp("[BUILTIN]")),
    name_(name),
    overloads_(overloads)
  {
  }

  const SassFnPair& BuiltInCallables::callbackFor(
    const ArgumentResults& evaluated)
  {
    for (SassFnPair& pair : overloads_) {
      if (pair.first->matches(evaluated)) {
        return pair;
      }
    }
    return overloads_.back();
  }

  bool BuiltInCallable::operator==(const Callable& rhs) const
  {
    if (const BuiltInCallable* builtin = Cast<BuiltInCallable>(&rhs)) {
      return this == builtin;
    }
    return false;
  }

  bool BuiltInCallables::operator==(const Callable& rhs) const
  {
    if (const BuiltInCallables * builtin = Cast<BuiltInCallables>(&rhs)) {
      return this == builtin;
    }
    return false;
  }

}
