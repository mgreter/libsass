// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include "ast.hpp"
#include "debugger.hpp"
#include "backtrace.hpp"
#include "error_handling.hpp"

#include <iostream>

namespace Sass {

  namespace Exception {

    sass::string def_msg("Invalid sass detected");
    sass::string def_op_msg("Undefined operation");
    sass::string def_op_null_msg("Invalid null operation");
    sass::string def_nesting_limit("Code too deeply neested");

    sass::string msg_recursion_limit =
      "Too deep recursion detected. This can be caused by too deep level nesting.\n"
      "LibSass will abort here in order to avoid a possible stack overflow.\n";

    Base::Base(SourceSpan pstate, sass::string msg, Backtraces traces)
    : std::runtime_error(msg.c_str()), msg(msg),
      prefix("Error"), pstate(pstate), traces(traces)
    { }

    SassScriptException::SassScriptException(sass::string msg, sass::string name)
      : std::runtime_error(msg.c_str()), msg(name.empty() ? msg : "$" + name + ": " + msg) {}

    SassRuntimeException::SassRuntimeException(sass::string msg, SourceSpan pstate)
      : std::runtime_error(msg.c_str()), msg(msg), pstate(pstate) {}

    SassRuntimeException2::SassRuntimeException2(sass::string msg,
      Logger& logger, SourceSpan pstate)
      : Base(pstate, msg, logger) {}

    InvalidParent::InvalidParent(Selector* parent, Backtraces traces, Selector* selector)
    : Base(selector->pstate(), def_msg, traces), parent(parent), selector(selector)
    {
      msg = "Parent "
        // "\"" + selector->to_string(Sass_Inspect_Options()) + "\": "
        "\"" + parent->to_string(Sass_Inspect_Options()) + "\""
        " is incompatible with this selector.";
    }

    InvalidSyntax::InvalidSyntax(SourceSpan pstate, Backtraces traces, sass::string msg)
      : Base(pstate, msg, traces)
    { }

    RecursionLimitError::RecursionLimitError(SourceSpan pstate, Backtraces traces)
      : Base(pstate, msg_recursion_limit, traces) {}

    DuplicateKeyError::DuplicateKeyError(Backtraces traces, const Map& dup, const Expression& org)
    : Base(org.pstate(), def_msg, traces), dup(dup), org(org)
    {
      // msg = "Duplicate key " + dup.get_duplicate_key()->inspect() + " in map (" + org.inspect() + ").";
      msg = "Duplicate key."; // dart-sass keeps it simple ...
    }

    TypeMismatch::TypeMismatch(Backtraces traces, const Expression& var, const sass::string type)
    : Base(var.pstate(), def_msg, traces), var(var), type(type)
    {
      msg = var.to_string() + " is not " + type + ".";
    }

    InvalidValue::InvalidValue(Backtraces traces, const Expression& val)
    : Base(val.pstate(), def_msg, traces), val(val)
    {
      msg = val.inspect() + " isn't a valid CSS value.";
    }

    IncompatibleUnits::IncompatibleUnits(const Units& lhs, const Units& rhs)
    {
      msg = "Incompatible units " + rhs.unit() + " and " + lhs.unit() + ".";
    }

    IncompatibleUnits::IncompatibleUnits(const UnitType lhs, const UnitType rhs)
    {
      msg = sass::string("Incompatible units ") + unit_to_string(rhs) + " and " + unit_to_string(lhs) + ".";
    }

    AlphaChannelsNotEqual::AlphaChannelsNotEqual(const Expression* lhs, const Expression* rhs, enum Sass_OP op)
    : OperationError(), lhs(lhs), rhs(rhs), op(op)
    {
      msg = "Alpha channels must be equal: " +
        lhs->to_string({ NESTED, 5 }) +
        " " + sass_op_to_name(op) + " " +
        rhs->to_string({ NESTED, 5 }) + ".";
    }

    ZeroDivisionError::ZeroDivisionError(const Expression& lhs, const Expression& rhs)
    : OperationError(), lhs(lhs), rhs(rhs)
    {
      msg = "divided by 0";
    }

    UndefinedOperation::UndefinedOperation(const Expression* lhs, const Expression* rhs, enum Sass_OP op)
    : OperationError(), lhs(lhs), rhs(rhs), op(op)
    {
      msg = def_op_msg + " \"" +
        lhs->to_string({ NESTED, 5 }) +
        " " + sass_op_separator(op) + " " +
        rhs->to_string({ TO_SASS, 5 }) +
        "\".";
    }

    InvalidNullOperation::InvalidNullOperation(const Expression* lhs, const Expression* rhs, enum Sass_OP op)
    : UndefinedOperation(lhs, rhs, op)
    {
      msg = def_op_null_msg + ": \"" + lhs->inspect() + " " + sass_op_to_name(op) + " " + rhs->inspect() + "\".";
    }

    SassValueError::SassValueError(Backtraces traces, SourceSpan pstate, OperationError& err)
    : Base(pstate, err.what(), traces)
    {
      msg = err.what();
      prefix = err.errtype();
    }

    TopLevelParent::TopLevelParent(Backtraces traces, SourceSpan pstate)
      : Base(pstate, "Top-level selectors may not contain the parent selector \"&\".", traces)
    {

    }
      
    UnsatisfiedExtend::UnsatisfiedExtend(Backtraces traces, Extension extension)
      : Base(extension.target->pstate(), sass::string("The target selector was not found.\n")
        + "Use \"@extend " + extension.target->inspect() + " !optional\" to avoid this error.", traces)
    {
    }

    ExtendAcrossMedia::ExtendAcrossMedia(Backtraces traces, Extension extension)
      : Base(extension.target->pstate(), "You may not @extend selectors across media queries.", traces)
    {

    }
    

    InvalidUnicode::InvalidUnicode(SourceSpan pstate, Backtraces traces)
      : Base(pstate, "Invalid UTF-8.", traces)
    {
    }

    SassScriptException2::SassScriptException2(sass::string msg,
      Backtraces traces, SourceSpan pstate, sass::string name) :
      Base(pstate, name.empty() ? msg : "$" + name + ": " + msg, traces)
    {}

}

  void warning(sass::string msg, Logger& logger, SourceSpan pstate)
  {
    sass::string cwd(Sass::File::get_cwd());
    sass::string abs_path(Sass::File::rel2abs(pstate.getPath(), cwd, cwd));
    sass::string rel_path(Sass::File::abs2rel(pstate.getPath(), cwd, cwd));
    sass::string output_path(Sass::File::path_for_console(rel_path, abs_path, pstate.getPath()));

    callStackFrame frame(logger, Backtrace(pstate));
    std::cerr << "WARNING on line " << pstate.getLine() << ", column " <<
      pstate.getColumn() << " of " << output_path << ":" << std::endl;
    std::cerr << msg << std::endl << std::endl;
    std::cerr << traces_to_string(logger, "    ", true);
  }

  void deprecated(sass::string msg, sass::string msg2, bool with_column, SourceSpan pstate)
  {
    sass::string cwd(Sass::File::get_cwd());
    sass::string abs_path(Sass::File::rel2abs(pstate.getPath(), cwd, cwd));
    sass::string rel_path(Sass::File::abs2rel(pstate.getPath(), cwd, cwd));
    sass::string output_path(Sass::File::path_for_console(rel_path, pstate.getPath(), pstate.getPath()));

    std::cerr << "DEPRECATION WARNING on line " << pstate.getLine();
    if (with_column) std::cerr << ", column " << pstate.getColumn();
    if (output_path.length()) std::cerr << " of " << output_path;
    std::cerr << ":" << std::endl;
    std::cerr << msg << std::endl;
    if (msg2.length()) std::cerr << msg2 << std::endl;
    std::cerr << std::endl;
  }

  void deprecatedDart(sass::string msg, size_t showTraces, Logger& logger, SourceSpan pstate)
  {
    sass::string cwd(Sass::File::get_cwd());
    sass::string abs_path(Sass::File::rel2abs(pstate.getPath(), cwd, cwd));
    sass::string rel_path(Sass::File::abs2rel(pstate.getPath(), cwd, cwd));
    sass::string output_path(Sass::File::path_for_console(rel_path, pstate.getPath(), pstate.getPath()));

    std::cerr << "DEPRECATION WARNING: ";
    std::cerr << msg << std::endl;
    std::cerr << std::endl;

    // callStackFrame frame(logger,
    //   Backtrace(pstate));
    std::cerr << traces_to_string(logger, "    ", showTraces);
    std::cerr << std::endl;

    // ToDo: add source indicator

  }

  void error(const sass::string& msg, SourceSpan pstate, Backtraces& traces)
  {
    traces.push_back(Backtrace(pstate));
    throw Exception::InvalidSyntax(pstate, traces, msg);
  }

}
