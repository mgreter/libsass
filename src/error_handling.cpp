// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include "ast.hpp"
#include "debugger.hpp"
#include "terminal.hpp"
#include "backtrace.hpp"
#include "error_handling.hpp"

#include <iostream>
#include <string.h>

namespace Sass {

  StackTraces convertTraces(BackTraces traces)
  {
    // This will trigger StackTrace constructor
    // Copies necessary stuff from BackTrace
    return { traces.begin(), traces.end() };
  }

  namespace Exception {

    Base::Base(sass::string msg, BackTraces traces)
    : std::runtime_error(msg.c_str()), msg(msg),
      traces(traces.begin(), traces.end())
    { }

    Base::Base(sass::string msg, BackTraces traces, SourceSpan pstate)
      : Base(msg, traces)
    {
      Base::traces.push_back(pstate);
    }

    SassRuntimeException2::SassRuntimeException2(
      sass::string msg, BackTraces traces)
      : Base(msg, traces) {}

    SassRuntimeException2::SassRuntimeException2(
      sass::string msg, BackTraces traces, SourceSpan pstate)
      : Base(msg, traces, pstate) {}

    InvalidParent::InvalidParent(Selector* parent, BackTraces traces, Selector* selector)
    : Base(def_msg, traces, selector->pstate()), parent(parent), selector(selector)
    {
      msg = "Parent "
        "\"" + parent->inspect() + "\""
        " is incompatible with this selector.";
    }

    InvalidSyntax::InvalidSyntax(BackTraces traces, sass::string msg)
      : Base(msg, traces)
    { }

    RecursionLimitError::RecursionLimitError(SourceSpan pstate, BackTraces traces)
      : Base(msg_recursion_limit, traces) {}

    DuplicateKeyError::DuplicateKeyError(BackTraces traces, const Map& dup, const Expression& org)
    : Base(def_msg, traces), dup(dup), org(org)
    {
      // msg = "Duplicate key " + dup.get_duplicate_key()->inspectValue() + " in map (" + org.inspectValue() + ").";
      msg = "Duplicate key."; // dart-sass keeps it simple ...
    }

    sass::string formatUnknownNamedArgument(const EnvKeyFlatMap<ValueObj>& names)
    {
      sass::string msg("No ");
      msg += pluralize(Strings::argument, names.size());
      msg += " named ";
      msg += toSentence(names, Strings::_or_);
      msg += ".";
      return msg;
    }

    sass::string formatTooManyArguments(size_t given, size_t expected) {
      sass::ostream msg;
      msg << "Only ";
      msg << expected;
      msg << " arguments allowed, but ";
      msg << given;
      msg << " were passed.";
      return msg.str();
    }

    sass::string formatTooManyArguments(EnvKeyFlatMap<ExpressionObj> given, Sass::EnvKeySet expected) {
      Sass::EnvKeySet superfluous;
      for (auto pair : given) {
        if (expected.count(pair.first) == 0) {
          superfluous.insert(pair.first);
        }
      }
      return "No argument named " +
        toSentence(superfluous, "or") + ".";
    }

    TooManyArguments::TooManyArguments(BackTraces traces, size_t given, size_t expected)
      : Base(formatTooManyArguments(given, expected), traces)
    {}

    TooManyArguments::TooManyArguments(BackTraces traces, EnvKeyFlatMap<ExpressionObj> given, Sass::EnvKeySet expected)
      : Base(formatTooManyArguments(given, expected), traces)
    {}

    MissingArgument::MissingArgument(BackTraces traces, const sass::string& name)
      : Base("Missing argument " + name + ".", traces)
    {}

    ArgumentGivenTwice::ArgumentGivenTwice(BackTraces traces, const sass::string& name)
      : Base("Argument " + name + " name was passed both by position and by name.", traces)
    {}

    UnknownNamedArgument::UnknownNamedArgument(SourceSpan pstate, BackTraces traces, EnvKeyFlatMap<ValueObj> names)
      : Base(formatUnknownNamedArgument(names), traces, pstate)
    {
    }

    sass::string formatUnknownNamedArgument2(const EnvKeyFlatMap<ExpressionObj>& names)
    {
      sass::string msg("No ");
      msg += pluralize(Strings::argument, names.size());
      msg += " named ";
      msg += toSentence(names, Strings::_or_);
      msg += ".";
      return msg;
    }

    InvalidCssValue::InvalidCssValue(BackTraces traces, const Value& val)
      : Base(val.inspectValue() + " isn't a valid CSS value.", traces, val.pstate())
    {}

    InvalidCssValue2::InvalidCssValue2(BackTraces traces, const Value& val)
      : Base(val.inspectValue() + " isn't a valid CSS value.", traces, val.pstate())
    {}

    // Thrown when a parent selector is used without any parent
    TopLevelParent::TopLevelParent(BackTraces traces, SourceSpan pstate)
      : Base("Top-level selectors may not contain the parent selector \"&\".", traces, pstate)
    {}

    // Thrown when a non-optional extend found nothing to extend
    UnsatisfiedExtend::UnsatisfiedExtend(BackTraces traces, Extension extension)
      : Base("The target selector was not found.\n"
        // Calling inspect to the placeholder is visible
        "Use \"@extend " + extension.target->inspect() +
        " !optional\" to avoid this error.",
        traces, extension.target->pstate())
    {}

    // Thrown when we extend across incompatible media contexts
    ExtendAcrossMedia::ExtendAcrossMedia(BackTraces traces, Extension extension)
      : Base("You may not @extend selectors across media queries.", traces)
    {}
    
    // Thrown when we find an unexpected UTF8 sequence
    InvalidUnicode::InvalidUnicode(SourceSpan pstate, BackTraces traces)
      : Base("Invalid UTF-8.", traces, pstate)
    {}

    SassScriptException2::SassScriptException2(sass::string msg,
      BackTraces traces, SourceSpan pstate, sass::string name) :
      Base(name.empty() ? msg : "$" + name + ": " + msg, traces)
    {}























    /////////////////////////////////////////////////////////////////////////
    // Various value operation errors
    /////////////////////////////////////////////////////////////////////////

    ZeroDivisionError::ZeroDivisionError(const Expression& lhs, const Expression& rhs)
      : OperationError("divided by 0")
    {}

    IncompatibleUnits::IncompatibleUnits(const Units& lhs, const Units& rhs)
      : OperationError("Incompatible units "
        + rhs.unit() + " and "
        + lhs.unit() + ".")
    {}

    AlphaChannelsNotEqual::AlphaChannelsNotEqual(const Color_RGBA* lhs, const Color_RGBA* rhs, enum Sass_OP op)
      : OperationError("Alpha channels must be equal: "
        + lhs->inspectValue() + " "
        + sass_op_to_name(op) + " "
        + rhs->inspectValue() + ".")
    {}


    InvalidNullOperation::InvalidNullOperation(const Value* lhs, const Value* rhs, enum Sass_OP op)
      : OperationError("Invalid null operation: \""
        + lhs->inspectValue() + " "
        + sass_op_to_name(op) + " "
        + rhs->inspectValue() + "\".")
    {}

    UndefinedOperation::UndefinedOperation(const Value* lhs, const Value* rhs, enum Sass_OP op)
      : OperationError("Undefined operation: \""
        + lhs->inspectValue() + " "
        + sass_op_separator(op) + " "
        + rhs->inspectValue() + "\".")
    {}


















}

  void deprecated(sass::string msg, sass::string msg2, bool with_column, SourceSpan pstate)
  {
    sass::string output_path(pstate.getDebugPath());
    std::cerr << "DEPRECATION WARNING on line " << pstate.getLine();
    if (with_column) std::cerr << ", column " << pstate.getColumn();
    if (output_path.length()) std::cerr << " of " << output_path;
    std::cerr << ':' << STRMLF;
    std::cerr << msg << STRMLF;
    if (msg2.length()) std::cerr << msg2 << STRMLF;
    std::cerr << STRMLF;
  }


  void error(const sass::string& msg, SourceSpan pstate, BackTraces& traces)
  {
    traces.push_back(BackTrace(pstate));
    throw Exception::InvalidSyntax(traces, msg);
  }

}
