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
#include "sass/functions.h"

namespace Sass {

  struct Backtrace;

  namespace Exception {

    extern sass::string def_msg;
    extern sass::string def_op_msg;
    extern sass::string def_op_null_msg;
    extern sass::string def_nesting_limit;

    extern sass::string msg_recursion_limit;

    class Base : public std::runtime_error {
      protected:
        sass::string msg;
        sass::string prefix;
      public:
        SourceSpan pstate;
        Backtraces traces;
      public:
        Base(SourceSpan pstate, sass::string msg, Backtraces traces);
        virtual const char* errtype() const { return prefix.c_str(); }
        virtual const char* what() const throw() { return msg.c_str(); }
        virtual ~Base() throw() {};
    };

    class InvalidParent : public Base {
      protected:
        Selector* parent;
        Selector* selector;
      public:
        InvalidParent(Selector* parent, Backtraces traces, Selector* selector);
        virtual ~InvalidParent() throw() {};
    };

    class InvalidUnicode : public Base {
    public:
      InvalidUnicode(SourceSpan pstate, Backtraces traces);
      virtual ~InvalidUnicode() throw() {};
    };

    class InvalidSyntax : public Base {
    public:
      InvalidSyntax(SourceSpan pstate, Backtraces traces, sass::string msg);
      virtual ~InvalidSyntax() throw() {};
    };

    class SassScriptException : public std::runtime_error {
      sass::string msg;
    public:
      SassScriptException(sass::string msg, sass::string name = "");
      virtual const char* what() const throw() { return msg.c_str(); }
      ~SassScriptException() throw() {};
    };

    class SassScriptException2 : public Base {
    public:
      SassScriptException2(sass::string msg,
        Backtraces traces, SourceSpan pstate,
        sass::string name = "");
      ~SassScriptException2() throw() {};
    };

    class SassRuntimeException : public std::runtime_error {
      sass::string msg;
      SourceSpan pstate;
    public:
      SassRuntimeException(sass::string msg, SourceSpan pstate);
      virtual const char* what() const throw() { return msg.c_str(); }
      ~SassRuntimeException() throw() {};
    };

    class SassRuntimeException2 : public Base {
    public:
      SassRuntimeException2(sass::string msg,
        Logger& logger, SourceSpan pstate);
      virtual const char* what() const throw() { return msg.c_str(); }
      ~SassRuntimeException2() throw() {};
    };


    class RecursionLimitError : public Base {
      public:
        RecursionLimitError(SourceSpan pstate, Backtraces traces);
        virtual ~RecursionLimitError() throw() {};
    };

    class DuplicateKeyError : public Base {
      protected:
        const Map& dup;
        const Expression& org;
      public:
        DuplicateKeyError(Backtraces traces, const Map& dup, const Expression& org);
        virtual const char* errtype() const { return "Error"; }
        virtual ~DuplicateKeyError() throw() {};
    };

    class TypeMismatch : public Base {
      protected:
        const Expression& var;
        const sass::string type;
      public:
        TypeMismatch(Backtraces traces, const Expression& var, const sass::string type);
        virtual const char* errtype() const { return "Error"; }
        virtual ~TypeMismatch() throw() {};
    };

    class InvalidValue : public Base {
      protected:
        const Expression& val;
      public:
        InvalidValue(Backtraces traces, const Expression& val);
        virtual const char* errtype() const { return "Error"; }
        virtual ~InvalidValue() throw() {};
    };

    /* common virtual base class (has no pstate or trace) */
    class OperationError : public std::runtime_error {
      protected:
        sass::string msg;
      public:
        OperationError(sass::string msg = def_op_msg)
        : std::runtime_error(msg.c_str()), msg(msg)
        {};
      public:
        virtual const char* errtype() const { return "Error"; }
        virtual const char* what() const throw() { return msg.c_str(); }
        virtual ~OperationError() throw() {};
    };

    class ZeroDivisionError : public OperationError {
      protected:
        const Expression& lhs;
        const Expression& rhs;
      public:
        ZeroDivisionError(const Expression& lhs, const Expression& rhs);
        virtual const char* errtype() const { return "ZeroDivisionError"; }
        virtual ~ZeroDivisionError() throw() {};
    };

    class IncompatibleUnits : public OperationError {
      protected:
        // const Sass::UnitType lhs;
        // const Sass::UnitType rhs;
      public:
        IncompatibleUnits(const Units& lhs, const Units& rhs);
        IncompatibleUnits(const UnitType lhs, const UnitType rhs);
        virtual ~IncompatibleUnits() throw() {};
    };

    class UndefinedOperation : public OperationError {
      protected:
        const Expression* lhs;
        const Expression* rhs;
        const Sass_OP op;
      public:
        UndefinedOperation(const Expression* lhs, const Expression* rhs, enum Sass_OP op);
        // virtual const char* errtype() const { return "Error"; }
        virtual ~UndefinedOperation() throw() {};
    };

    class InvalidNullOperation : public UndefinedOperation {
      public:
        InvalidNullOperation(const Expression* lhs, const Expression* rhs, enum Sass_OP op);
        virtual ~InvalidNullOperation() throw() {};
    };

    class AlphaChannelsNotEqual : public OperationError {
      protected:
        const Expression* lhs;
        const Expression* rhs;
        const Sass_OP op;
      public:
        AlphaChannelsNotEqual(const Expression* lhs, const Expression* rhs, enum Sass_OP op);
        // virtual const char* errtype() const { return "Error"; }
        virtual ~AlphaChannelsNotEqual() throw() {};
    };

    class SassValueError : public Base {
    public:
      SassValueError(Backtraces traces, SourceSpan pstate, OperationError& err);
      virtual ~SassValueError() throw() {};
    };

    class TopLevelParent : public Base {
    public:
      TopLevelParent(Backtraces traces, SourceSpan pstate);
      virtual ~TopLevelParent() throw() {};
    };

    class UnsatisfiedExtend : public Base {
    public:
      UnsatisfiedExtend(Backtraces traces, Extension extension);
      virtual ~UnsatisfiedExtend() throw() {};
    };

    class ExtendAcrossMedia : public Base {
    public:
      ExtendAcrossMedia(Backtraces traces, Extension extension);
      virtual ~ExtendAcrossMedia() throw() {};
    };

  }

  void warning(sass::string msg, Logger& logger, SourceSpan pstate);

  void deprecated(sass::string msg, sass::string msg2, bool with_column, SourceSpan pstate);
  // Migrate to new dart sass output
  void deprecatedDart(sass::string msg, size_t showTraces, Logger& logger, SourceSpan pstate);
  void error(const sass::string& msg, SourceSpan pstate, Backtraces& traces);

}

#endif
