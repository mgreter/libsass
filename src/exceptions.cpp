/*****************************************************************************/
/* Part of LibSass, released under the MIT license (See LICENSE.txt).        */
/*****************************************************************************/
#include "exceptions.hpp"

#include "ast_selectors.hpp"
#include "ast_values.hpp"
#include "extension.hpp"

namespace Sass {

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  StackTraces convertTraces(BackTraces traces)
  {
    // This will trigger StackTrace constructor
    // Copies necessary stuff from BackTrace
    return { traces.begin(), traces.end() };
  }

  StringVector getKeyVector(
    const ValueFlatMap& names)
  {
    StringVector keys;
    for (auto it : names) {
      keys.push_back(it.first.orig());
    }
    return keys;
  }

  StringVector getKeyVector(
    const ExpressionFlatMap& names)
  {
    StringVector keys;
    for (auto it : names) {
      keys.push_back(it.first.orig());
    }
    return keys;
  }

  sass::string pluralize(
    const sass::string& singular,
    size_t size, const sass::string& plural)
  {
    if (size == 1) return singular;
    else if (!plural.empty()) return plural;
    else return singular + "s";
  }

  sass::string toSentence(
    const StringVector& names,
    const sass::string& conjunction,
    const uint8_t prefix)
  {
    sass::string buffer;
    size_t L = names.size(), i = 0;
    auto it = names.begin();
    while (i < L) {
      // add conjugation
      if (i > 0) {
        if (i < L - 1) {
          buffer += ", ";
        }
        else {
          buffer += " ";
          buffer += conjunction;
          buffer += " ";
        }
      }
      if (prefix) buffer += prefix;
      buffer += *it;
      it++; i++;
    }
    return buffer;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

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

    ParserException::ParserException(BackTraces traces, sass::string msg)
      : Base(msg, traces)
    { }

    RuntimeException::RuntimeException(
      BackTraces traces, sass::string msg)
      : Base(msg, traces)
    {}

    RuntimeException::RuntimeException(
      sass::string msg, BackTraces traces, SourceSpan pstate)
      : Base(msg, traces, pstate) {}

    UnitMismatch::UnitMismatch(BackTraces traces, const Number& lhs, const Number& rhs)
      : RuntimeException(traces, "Incompatible units "
        + rhs.unit() + " and "
        + lhs.unit() + ".")
    {
      this->traces.push_back(lhs.pstate());
      this->traces.push_back(rhs.pstate());
    }

    InvalidParent::InvalidParent(Selector* parent, BackTraces traces, Selector* selector)
    : Base(def_msg, traces, selector->pstate()), parent(parent), selector(selector)
    {
      msg = "Parent "
        "\"" + parent->inspect() + "\""
        " is incompatible with this selector.";
    }


    CustomImportError::CustomImportError(BackTraces traces, sass::string msg)
      : Base(msg, traces)
    { }

    CustomImportNotFound::CustomImportNotFound(BackTraces traces, sass::string file)
      : RuntimeException(traces, def_msg)
    {
      msg = "Can't find stylesheet \"" + file + "\".";
      msg += "\nAs requested by custom importer.";
    }

    CustomImportAmbigous::CustomImportAmbigous(BackTraces traces, sass::string file)
      : RuntimeException(traces, def_msg)
    {
      msg = "CustomImportAmbigous \"" + file + "\".";
      msg += "\nAs requested by custom importer.";
    }

    CustomImportLoadError::CustomImportLoadError(BackTraces traces, sass::string file)
      : RuntimeException(traces, def_msg)
    {
      msg = "CustomImportLoadError \"" + file + "\".";
      msg += "\nAs requested by custom importer.";
    }
    

    RecursionLimitError::RecursionLimitError()
      : Base(msg_recursion_limit, {}) {}

    DuplicateKeyError::DuplicateKeyError(BackTraces traces, const Map& dup, const Value& org)
    : Base(def_msg, traces), dup(dup), org(org)
    {
      // msg = "Duplicate key " + dup.get_duplicate_key()->inspect() + " in map (" + org.inspect() + ").";
      msg = "Duplicate key."; // dart-sass keeps it simple ...
    }

    sass::string formatMixedParamGroups(const sass::string& first, const StringVector& others)
    {
      // RGB HWB
      sass::string msg(first);
      msg += " parameters may not be passed along with ";
      msg += toSentence(others, Strings::_or_);
      msg += " parameters.";
      return msg;
    }

    sass::string formatUnknownNamedArgument(const StringVector& names)
    {
      sass::string msg("No ");
      msg += pluralize(Strings::argument, names.size());
      msg += " named ";
      msg += toSentence(names, Strings::_or_, '$');
      msg += ".";
      return msg;
    }

    sass::string formatTooManyArguments(size_t given, size_t expected) {
      sass::ostream msg;
      msg << "Only " << expected << " ";
      msg << pluralize("argument", expected);
      msg << " allowed, but " << given << " ";
      msg << pluralize("was", given, "were");
      msg << " passed.";
      return msg.str();
    }

    sass::string formatTooManyArguments(const ExpressionFlatMap& given, const Sass::EnvKeySet& expected) {
      StringVector superfluous;
      for (auto pair : given) {
        if (expected.count(pair.first) == 0) {
          superfluous.emplace_back(pair.first.orig());
        }
      }
      return "No argument named " +
        toSentence(superfluous, "or", '$') + ".";
    }

    sass::string formatTooManyArguments(const ValueFlatMap& given, const Sass::EnvKeySet& expected) {
      StringVector superfluous;
      for (auto pair : given) {
        if (expected.count(pair.first) == 0) {
          superfluous.emplace_back(pair.first.orig());
        }
      }
      return "No argument named " +
        toSentence(superfluous, "or", '$') + ".";
    }

    sass::string formatTooManyArguments(const ValueFlatMap& superfluous) {
      return "No argument named " +
        toSentence(getKeyVector(superfluous), "or", '$') + ".";
    }

    TooManyArguments::TooManyArguments(BackTraces traces, size_t given, size_t expected)
      : RuntimeException(traces, formatTooManyArguments(given, expected))
    {}

    TooManyArguments::TooManyArguments(BackTraces traces, const ExpressionFlatMap& given, const Sass::EnvKeySet& expected)
      : RuntimeException(traces, formatTooManyArguments(given, expected))
    {}

    TooManyArguments::TooManyArguments(BackTraces traces, const ValueFlatMap& superflous)
      : RuntimeException(traces, formatTooManyArguments(superflous))
    {}

    MissingArgument::MissingArgument(BackTraces traces, const EnvKey& name)
      : RuntimeException(traces, "Missing argument $" + name.norm() + ".")
    {}

    ArgumentGivenTwice::ArgumentGivenTwice(BackTraces traces, const EnvKey& name)
      : RuntimeException(traces, "Argument $" + name.norm() + " name was passed both by position and by name.")
    {}

    UnknownNamedArgument::UnknownNamedArgument(BackTraces traces, ValueFlatMap names)
      : RuntimeException(traces, formatUnknownNamedArgument(getKeyVector(names)))
    {
    }

    MixedParamGroups::MixedParamGroups(BackTraces traces, const sass::string& first, const StringVector others)
      : RuntimeException(traces, formatMixedParamGroups(first, others))
    {}

    InvalidCssValue::InvalidCssValue(BackTraces traces, const Value& val)
      : Base(val.inspect() + " isn't a valid CSS value.", traces, val.pstate())
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

    SassScriptException::SassScriptException(sass::string msg,
      BackTraces traces, SourceSpan pstate, sass::string name) :
      Base(name.empty() ? msg : "$" + name + ": " + msg, traces)
    {}























    /////////////////////////////////////////////////////////////////////////
    // Various value operation errors
    /////////////////////////////////////////////////////////////////////////

    ZeroDivisionError::ZeroDivisionError(const Value& lhs, const Value& rhs)
      : OperationError("divided by 0")
    {}

    //IncompatibleUnits::IncompatibleUnits(const Units& lhs, const Units& rhs)
    //  : OperationError("Incompatible units "
    //    + rhs.unit() + " and "
    //    + lhs.unit() + ".")
    //{}



















	DeprecatedColorAdjustFn::DeprecatedColorAdjustFn(
    Logger& logger,
    const ValueVector& arguments,
    sass::string name,
    sass::string prefix,
    sass::string secondarg) :
    RuntimeException(logger, "")
	{
    msg += "The function " + name + "() isn't in the sass:color module.\n\n";
    msg += "Recommendation: color.adjust(" + arguments[0]->inspect() +
      ", " + prefix + arguments[1]->inspect() + ")\n\n";
    msg += "More info: https://sass-lang.com/documentation/functions/color#" + name;
	}

  ModuleUnknown::ModuleUnknown(
    BackTraces traces,
    sass::string name) :
    RuntimeException(traces,
      "There is no module with the"
      " namespace \"" + name + "\".")
  {}

  ModuleAlreadyKnown::ModuleAlreadyKnown(
    BackTraces traces,
    sass::string name) :
    RuntimeException(traces,
      "There's already a module with"
      " namespace \"" + name + "\".")
	{}

  TardyAtRule::TardyAtRule(
    BackTraces traces,
    sass::string name) :
    RuntimeException(traces,
      name + " rules must be written"
      " before any other rules.")
  {}

  InvalidSassIdentifier::InvalidSassIdentifier(
    BackTraces traces,
    sass::string name) :
    RuntimeException(traces,
      "Invalid Sass identifier"
      " \"" + name + "\"")
  {}

  UnknwonImport::UnknwonImport(
    BackTraces traces) :
    RuntimeException(traces,
      "Can't find stylesheet to import.")
  {}

  AmbiguousImports::AmbiguousImports(BackTraces traces,
    sass::vector<ResolvedImport> imports) :
    RuntimeException(traces,
      "It's not clear which file to import.")
    {
      msg += " Found:\n";
      for (size_t i = 0, L = imports.size(); i < L; ++i)
      { msg += "  " + imports[i].imp_path + "\n"; }
    }


  
}

}
