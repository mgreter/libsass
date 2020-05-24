#ifndef SASS_ERROR_HANDLING_H
#define SASS_ERROR_HANDLING_H

// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include <string>
#include <sstream>
#include <stdexcept>
#include "logger.hpp"
#include "units.hpp"
#include "position.hpp"
#include "backtrace.hpp"
#include "ast_fwd_decl.hpp"
#include "environment_cnt.hpp"
#include "sass/functions.h"

namespace Sass {

  struct BackTrace;

  namespace Exception {

    const sass::string def_msg("Invalid sass detected");
    const sass::string def_op_null_msg("Invalid null operation");
    const sass::string def_nesting_limit("Code too deeply nested");

    const sass::string msg_recursion_limit =
      "Too deep recursion detected. This can be caused by too deep level nesting.\n"
      "LibSass will abort here in order to avoid a possible stack overflow.\n";

    class Base : public std::runtime_error {
      protected:
        sass::string msg;
      public:
        StackTraces traces;
      public:
        Base(sass::string msg, BackTraces traces);
        Base(sass::string msg, BackTraces traces, SourceSpan pstate);
        virtual const char* what() const throw() { return msg.c_str(); }
        virtual ~Base() noexcept {};
    };

    class InvalidParent : public Base {
      protected:
        Selector* parent;
        Selector* selector;
      public:
        InvalidParent(Selector* parent, BackTraces traces, Selector* selector);
        virtual ~InvalidParent() noexcept {};
    };

    class InvalidUnicode : public Base {
    public:
      InvalidUnicode(SourceSpan pstate, BackTraces traces);
      virtual ~InvalidUnicode() noexcept {};
    };

    class InvalidSyntax : public Base {
    public:
      InvalidSyntax(BackTraces traces, sass::string msg);
      virtual ~InvalidSyntax() noexcept {};
    };

    class SassScriptException2 : public Base {
    public:
      SassScriptException2(sass::string msg,
        BackTraces traces, SourceSpan pstate,
        sass::string name = "");
      ~SassScriptException2() noexcept {};
    };

    class SassRuntimeException2 : public Base {
    public:
      SassRuntimeException2(sass::string msg, BackTraces traces);
      SassRuntimeException2(sass::string msg, BackTraces traces, SourceSpan pstate);
      virtual const char* what() const throw() { return msg.c_str(); }
      ~SassRuntimeException2() noexcept {};
    };


    class RecursionLimitError : public Base {
      public:
        RecursionLimitError(SourceSpan pstate, BackTraces traces);
        virtual ~RecursionLimitError() noexcept {};
    };

    class DuplicateKeyError : public Base {
      protected:
        const Map& dup;
        const Expression& org;
      public:
        DuplicateKeyError(BackTraces traces, const Map& dup, const Expression& org);
        virtual const char* errtype() const { return "Error"; }
        virtual ~DuplicateKeyError() noexcept {};
    };

    class ArgumentGivenTwice : public Base {
    public:
      ArgumentGivenTwice(BackTraces traces, const sass::string& name);
      virtual const char* errtype() const { return "Error"; }
    };

    class UnknownNamedArgument : public Base {
    public:
      UnknownNamedArgument(SourceSpan pstate, BackTraces traces, EnvKeyFlatMap<ValueObj> names);
      virtual const char* errtype() const { return "Error"; }
    };

    class UnknownNamedArgument2 : public Base {
    public:
      UnknownNamedArgument2(SourceSpan pstate, BackTraces traces, EnvKeyFlatMap<ExpressionObj> names);
      virtual const char* errtype() const { return "Error"; }
    };

    class InvalidCssValue : public Base {
      public:
        InvalidCssValue(BackTraces traces, const Value& val);
        virtual const char* errtype() const { return "Error"; }
        virtual ~InvalidCssValue() noexcept {};
    };


    class InvalidCssValue2 : public Base {
    public:
      InvalidCssValue2(BackTraces traces, const Value& val);
      virtual const char* errtype() const { return "Error"; }
      virtual ~InvalidCssValue2() noexcept {};
    };

    /* common virtual base class (has no pstate or trace) */
    class OperationError : public std::runtime_error {
      protected:
        sass::string msg;
      public:
        OperationError(sass::string msg = sass::string("Undefined operation"))
        : std::runtime_error(msg.c_str()), msg(msg)
        {};
      public:
        virtual const char* errtype() const { return "Error"; }
        virtual const char* what() const throw() { return msg.c_str(); }
        virtual ~OperationError() noexcept {};
    };

    class ZeroDivisionError : public OperationError {
      public:
        ZeroDivisionError(const Expression& lhs, const Expression& rhs);
        virtual const char* errtype() const { return "ZeroDivisionError"; }
        virtual ~ZeroDivisionError() noexcept {};
    };

    class IncompatibleUnits : public OperationError {
      protected:
        // const Sass::UnitType lhs;
        // const Sass::UnitType rhs;
      public:
        IncompatibleUnits(const Units& lhs, const Units& rhs);
        virtual ~IncompatibleUnits() noexcept {};
    };


    class UndefinedOperation : public OperationError {
    public:
      UndefinedOperation(const Value* lhs, const Value* rhs, enum Sass_OP op);
      // virtual const char* errtype() const { return "Error"; }
      virtual ~UndefinedOperation() noexcept {};
    };

    class InvalidNullOperation : public OperationError {
    public:
      InvalidNullOperation(const Value* lhs, const Value* rhs, enum Sass_OP op);
      virtual ~InvalidNullOperation() noexcept {};
    };

    class AlphaChannelsNotEqual : public OperationError {
      public:
        AlphaChannelsNotEqual(const Color_RGBA* lhs, const Color_RGBA* rhs, enum Sass_OP op);
        // virtual const char* errtype() const { return "Error"; }
        virtual ~AlphaChannelsNotEqual() noexcept {};
    };

    class TopLevelParent : public Base {
    public:
      TopLevelParent(BackTraces traces, SourceSpan pstate);
      virtual ~TopLevelParent() noexcept {};
    };

    class UnsatisfiedExtend : public Base {
    public:
      UnsatisfiedExtend(BackTraces traces, Extension extension);
      virtual ~UnsatisfiedExtend() noexcept {};
    };

    class ExtendAcrossMedia : public Base {
    public:
      ExtendAcrossMedia(BackTraces traces, Extension extension);
      virtual ~ExtendAcrossMedia() noexcept {};
    };

  }

  void deprecated(sass::string msg, sass::string msg2, bool with_column, SourceSpan pstate);
  // Migrate to new dart sass output
  void error(const sass::string& msg, SourceSpan pstate, BackTraces& traces);

}

#endif
