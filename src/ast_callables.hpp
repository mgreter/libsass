/*****************************************************************************/
/* Part of LibSass, released under the MIT license (See LICENSE.txt).        */
/*****************************************************************************/
#ifndef SASS_AST_CALLABLES_HPP
#define SASS_AST_CALLABLES_HPP

// sass.hpp must go before all system headers
// to get the  __EXTENSIONS__ fix on Solaris.
#include "capi_sass.hpp"

#include "ast_nodes.hpp"
#include "ast_callable.hpp"
#include "ast_statements.hpp"
#include "environment_key.hpp"
#include "environment_stack.hpp"

namespace Sass {

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  class BuiltInCallable final : public Callable {

    // The function name
    ADD_CONSTREF(EnvKey, envkey);

    ADD_CONSTREF(ArgumentDeclarationObj, parameters);

    ADD_REF(SassFnPair, function);

  public:

    // Creates a callable with a single [arguments] declaration
    // and a single [callback]. The argument declaration is parsed
    // from [arguments], which should not include parentheses.
    // Throws a [SassFormatException] if parsing fails.
    BuiltInCallable(
      const EnvKey& fname,
      ArgumentDeclaration* parameters,
      const SassFnSig& callback);

    // Return callback with matching signature
    const SassFnPair& callbackFor(
      const ArgumentResults& evaluated);

    // The main entry point to execute the function (implemented in each specialization)
    Value* execute(Eval& eval, ArgumentInvocation* arguments, const SourceSpan& pstate, bool selfAssign) override final;

    // Return the function name
    const sass::string& name() const override final { return envkey_.norm(); }

    // Equality comparator (needed for `get-function` value)
    bool operator==(const Callable& rhs) const override final;

    IMPLEMENT_ISA_CASTER(BuiltInCallable);
  };

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  class BuiltInCallables final : public Callable {

    // The function name
    ADD_CONSTREF(EnvKey, envkey);

    // The overloads declared for this callable.
    ADD_CONSTREF(SassFnPairs, overloads);

  public:

    // Creates a callable with multiple implementations. Each
    // key/value pair in [overloads] defines the argument declaration
    // for the overload (which should not include parentheses), and
    // the callback to execute if that argument declaration matches.
    // Throws a [SassFormatException] if parsing fails.
    BuiltInCallables(
      const EnvKey& envkey,
      const SassFnPairs& overloads);

    // Return callback with matching signature
    const SassFnPair& callbackFor(
      const ArgumentResults& evaluated);

    // The main entry point to execute the function (implemented in each specialization)
    Value* execute(Eval& eval, ArgumentInvocation* arguments, const SourceSpan& pstate, bool selfAssign) override final;

    // Return the function name
    const sass::string& name() const override final { return envkey_.norm(); }

    // Equality comparator (needed for `get-function` value)
    bool operator==(const Callable& rhs) const override final;

    IMPLEMENT_ISA_CASTER(BuiltInCallables);
  };

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  class PlainCssCallable final : public Callable
  {
  private:

    ADD_CONSTREF(sass::string, fname);

  public:

    // Value constructor
    PlainCssCallable(
      const SourceSpan& pstate,
      const sass::string& fname);

    // The main entry point to execute the function (implemented in each specialization)
    Value* execute(Eval& eval, ArgumentInvocation* arguments, const SourceSpan& pstate, bool selfAssign) override final;

    // Return the function name
    const sass::string& name() const override final { return fname(); }

    // Equality comparator (needed for `get-function` value)
    bool operator==(const Callable& rhs) const override final;

    IMPLEMENT_ISA_CASTER(PlainCssCallable);
  };

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  class UserDefinedCallable final : public Callable
  {
  private:

    // Name of this callable (used for reporting)
    ADD_CONSTREF(EnvKey, envkey);
    // The declaration (parameters this function takes).
    ADD_CONSTREF(CallableDeclarationObj, declaration);
    // The environment in which this callable was declared.
    ADD_PROPERTY(UserDefinedCallable*, content);

  public:

    // Value constructor
    UserDefinedCallable(
      const SourceSpan& pstate,
      const EnvKey& fname,
      CallableDeclarationObj declaration,
      UserDefinedCallable* content);

    // The main entry point to execute the function (implemented in each specialization)
    Value* execute(Eval& eval, ArgumentInvocation* arguments, const SourceSpan& pstate, bool selfAssign) override final;

    // Return the function name
    const sass::string& name() const override final { return envkey_.norm(); }

    // Equality comparator (needed for `get-function` value)
    bool operator==(const Callable& rhs) const override final;

    IMPLEMENT_ISA_CASTER(UserDefinedCallable);
  };

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  class ExternalCallable final : public Callable
  {
  private:

    // Name of this callable (used for reporting)
    ADD_CONSTREF(EnvKey, envkey);
    // The declaration (parameters this function takes).
    ADD_CONSTREF(ArgumentDeclarationObj, declaration);
    // The attached external callback reference
    ADD_PROPERTY(struct SassFunction*, function);

  public:

    // Value constructor
    ExternalCallable(
      const EnvKey& fname,
      ArgumentDeclaration* parameters,
      struct SassFunction* function);

    // Destructor
    ~ExternalCallable() override final {
      sass_delete_function(function_);
    }

    // The main entry point to execute the function (implemented in each specialization)
    Value* execute(Eval& eval, ArgumentInvocation* arguments, const SourceSpan& pstate, bool selfAssign) override final;

    // Return the function name
    const sass::string& name() const override final { return envkey_.norm(); }

    // Equality comparator (needed for `get-function` value)
    bool operator==(const Callable& rhs) const override final;

    IMPLEMENT_ISA_CASTER(ExternalCallable);
  };

}

#endif
