// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"
#include "ast.hpp"

#include "fn_utils.hpp"
#include "util_string.hpp"
#include "parser_scss.hpp"
#include "parser_selector.hpp"

namespace Sass {

  ExternalCallable* make_c_function2(Sass_Function_Entry c_func, Context& ctx)
  {
    const char* sig = sass_function_get_signature(c_func);
    auto qwe = SASS_MEMORY_NEW(SourceFile,
      "sass://signature", sig, -1);
    ScssParser p2(ctx, qwe);
    sass::string name;
    if (p2.scanner.scanChar(Character::$at)) {
      name = "@"; // allow @warn etc.
    }
    name += p2.identifier();
    ArgumentDeclarationObj params = p2.parseArgumentDeclaration2();
    return SASS_MEMORY_NEW(ExternalCallable,
      // SourceSpan("[c function]"),
      name, params, c_func);
  }

  Callable::Callable(
    const SourceSpan& pstate) :
    SassNode(pstate) {}

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
    const EnvString& name,
    ArgumentDeclaration* parameters,
    const SassFnSig& callback) :
    Callable(SourceSpan::fake("[BUILTIN]")),
    name_(name),
    // Create a single entry in overloaded function
    function_(SassFnPair{ parameters, callback })
  {
  }

  const SassFnPair& BuiltInCallable::callbackFor(
    size_t positional, const KeywordMap<ValueObj>& names)
  {
    return function_;
  }

  BuiltInCallables::BuiltInCallables(
    const EnvString& name,
    const SassFnPairs& overloads) :
    Callable(SourceSpan::fake("[BUILTIN]")),
    name_(name),
    overloads_(overloads)
  {
  }

  const SassFnPair& BuiltInCallables::callbackFor(
    size_t positional, const KeywordMap<ValueObj>& names)
  {
    for (SassFnPair& pair : overloads_) {
      if (pair.first->matches(positional, names)) {
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
