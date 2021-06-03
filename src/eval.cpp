/*****************************************************************************/
/* Part of LibSass, released under the MIT license (See LICENSE.txt).        */
/*****************************************************************************/
#include "eval.hpp"

#include "cssize.hpp"
#include "sources.hpp"
#include "compiler.hpp"
#include "stylesheet.hpp"
#include "exceptions.hpp"
#include "ast_values.hpp"
#include "ast_imports.hpp"
#include "ast_selectors.hpp"
#include "ast_callables.hpp"
#include "ast_statements.hpp"
#include "ast_expressions.hpp"
#include "parser_selector.hpp"
#include "parser_media_query.hpp"
#include "parser_keyframe_selector.hpp"

#include "preloader.hpp"

namespace Sass {

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Eval::Eval(Compiler& compiler, Logger& logger, bool plainCss) :
    logger(logger),
    compiler(compiler),
    traces(logger),
    modctx(compiler.modctx),
    wconfig(compiler.wconfig),
//    extender(
//      Extender::NORMAL,
//      logger),
    plainCss(plainCss),
    inMixin(false),
    inFunction(false),
    inUnknownAtRule(false),
    atRootExcludingStyleRule(false),
    inKeyframes(false)
  {

    mediaStack.push_back({});
    selectorStack.push_back({});
    originalStack.push_back({});

    bool_true = SASS_MEMORY_NEW(Boolean, SourceSpan::internal("[TRUE]"), true);
    bool_false = SASS_MEMORY_NEW(Boolean, SourceSpan::internal("[FALSE]"), false);
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Helper function for the division
  Value* doDivision(Value* left, Value* right,
    BinaryOpExpression* node, Logger& logger, SourceSpan pstate)
  {
    bool allowSlash = node->allowsSlash();
    ValueObj result = left->dividedBy(right, logger, pstate);
    if (Number* rv = result->isaNumber()) {
      if (allowSlash && left && right) {
        rv->lhsAsSlash(left->isaNumber());
        rv->rhsAsSlash(right->isaNumber());
      }
      else {
        if (!node->warned() && left && right) {
          logger.addDeprecation("Using / for division is deprecated and will be removed "
            "in LibSass 4.1.0.\n\nRecommendation: math.div(" + node->left()->toString() +
            ", " + node->right()->toString() + ")\n\nMore info and automated migrator: "
            "https://sass-lang.com/d/slash-div", pstate, Logger::WARN_MATH_DIV);
          node->warned(true);
        }
        rv->lhsAsSlash({}); // reset
        rv->lhsAsSlash({}); // reset
      }
    }
    return result.detach();
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Value* Eval::withoutSlash(ValueObj value) {
    if (value == nullptr) return value;
    Number* number = value->isaNumber();
    if (number && number->hasAsSlash()) {
      logger.addDeprecation("Using / for division is deprecated and will be removed " 
        "in LibSass 4.1.0.\n\nRecommendation: math.div(" + number->lhsAsSlash()->inspect() +
        ", " + number->rhsAsSlash()->inspect() + ")\n\nMore info and automated migrator: "
        "https://sass-lang.com/d/slash-div", value->pstate(), Logger::WARN_MATH_DIV);
    }
    // Make sure to collect all memory
    ValueObj result = value->withoutSlash();
    return result.detach();
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Fetch unevaluated positional argument (optionally by name)
  // Will error if argument is missing or available both ways
  // Note: only needed for lazy evaluation in if expressions
  Expression* Eval::getArgument(
    ExpressionVector& positional,
    ExpressionFlatMap& named,
    size_t idx, const EnvKey& name)
  {
    // Try to find the argument by name
    auto it = named.find(name);
    // Check if requested index is available
    if (positional.size() > idx) {
      // Check if argument is also known by name
      if (it != named.end()) {
        // Raise error since it's ambiguous
        throw Exception::ArgumentGivenTwice(
          logger, name);
      }
      // Return the positional value
      return positional[idx];
    }
    else if (it != named.end()) {
      // Return the expression
      return it->second;
    }
    // Raise error since nothing was found
    throw Exception::MissingArgument(
      logger, name);
  }

  // Fetch evaluated positional argument (optionally by name)
  // Will error if argument is missing or available both ways
  // Named arguments are consumed and removed from the hash
  Value* Eval::getParameter(
    ArgumentResults& results,
    size_t idx, const Argument* arg)
  {
    // Try to find the argument by name
    auto it = results.named().find(arg->name());
    // Check if requested index is available
    if (results.positional().size() > idx) {
      // Check if argument is also known by name
      if (it != results.named().end()) {
        // Raise error since it's ambiguous
        throw Exception::ArgumentGivenTwice(
          logger, arg->name());
      }
      // Return the positional value
      return results.positional()[idx];
    }
    // Check if argument was found be name
    else if (it != results.named().end()) {
      // Get value object from hash
      // Need to hold onto the object
      ValueObj val = it->second;
      // Item has been consumed
      // Would destroy the value
      results.named().erase(it);
      // Detach to survive
      return val.detach();
    }
    // Check if we have default values
    else if (!arg->defval().isNull()) {
      // Return evaluated expression
      return arg->defval()->accept(this);
    }
    // Raise error since nothing was found
    throw Exception::MissingArgument(
      logger, arg->name());
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  //*************************************************//
  // Call built-in function with no overloads
  //*************************************************//
  Value* Eval::_runBuiltInCallable(
    CallableArguments* arguments,
    BuiltInCallable* callable,
    const SourceSpan& pstate)
  {
    ArgumentResults results(_evaluateArguments(arguments));
    const SassFnPair& tuple(callable->callbackFor(results));
    return _callBuiltInCallable(results, tuple, pstate);
  }
  // EO _runBuiltInCallable

  //*************************************************//
  // Call built-in function with overloads
  //*************************************************//
  Value* Eval::_runBuiltInCallables(
    CallableArguments* arguments,
    BuiltInCallables* callable,
    const SourceSpan& pstate)
  {
    ArgumentResults results(_evaluateArguments(arguments));
    const SassFnPair& tuple(callable->callbackFor(results));
    return _callBuiltInCallable(results, tuple, pstate);
  }
  // EO _runBuiltInCallables

  //*************************************************//
  // Helper for _runBuiltInCallable(s)
  //*************************************************//
  Value* Eval::_callBuiltInCallable(
    ArgumentResults& results,
    const SassFnPair& function,
    const SourceSpan& pstate)
  {

    // Here the strategy is to re-use the positional arguments if possible
    // In the end we need one continuous array to pass to the built-in callable
    // So we need to split out restargs into it's own array, where as in other
    // implementations we can re-use positional array for this purpose!

    // Get some items from passed parameters
    const SassFnSig& callback(function.second);
    const CallableSignature* prototype(function.first);
    if (!callback) throw std::runtime_error("Mixin declaration has no callback");
    if (!prototype) throw std::runtime_error("Mixin declaration has no prototype");
    const sass::vector<ArgumentObj>& parameters(prototype->arguments());

    // Get reference to positional arguments in the result object
    // Multiple calls to the same function may re-use the object
    ValueVector& positional(results.positional());

    // Needed here for a specific edge case: restargs must be consumed
    // Those can be consumed e.g. by passing them to other functions
    // Or simply by calling `keywords` on the rest arguments
    ArgumentListObj restargs;

    // If the callable accepts rest argument we can pass all unknown args
    // Also if we must pass rest args we must pass only the remaining parts
    if (prototype->restArg().empty() == false) {

      // Superfluous function arguments
      ValueVector superflous;

      // Check if more arguments provided than parameters
      if (positional.size() > parameters.size()) {
        // Move superfluous arguments into the array
        std::move(positional.begin() + parameters.size(),
          positional.end(), back_inserter(superflous));
        // Remove the consumed positional arguments
        positional.resize(parameters.size());
      }

      // Try to get named function parameters from argument results
      for (size_t i = positional.size(); i < parameters.size(); i += 1) {
        positional.push_back(getParameter(results, i, parameters[i]));
      }

      // Inherit separator from argument results
      SassSeparator separator(results.separator());
      // But make the default a comma instead of spaces
      if (separator == SASS_UNDEF) separator = SASS_COMMA;
      // Create the rest arguments (move remaining stuff)
      restargs = SASS_MEMORY_NEW(ArgumentList, pstate, separator,
        std::move(superflous), std::move(results.named()));
      // Append last parameter (rest arguments)
      positional.emplace_back(restargs);

    }
    // Function takes rest arguments, so superfluous arguments must
    // be passed to the function via the rest argument array
    else {

      // Check that all positional arguments are consumed
      if (positional.size() > parameters.size()) {
        throw Exception::TooManyArguments(logger,
          positional.size(), prototype->maxArgs());
      }

      // Try to get needed function parameters from argument results
      for (size_t i = positional.size(); i < parameters.size(); i += 1) {
        positional.push_back(getParameter(results, i, parameters[i]));
      }

      // Check that all named arguments are consumed
      if (results.named().empty() == false) {
        throw Exception::TooManyArguments(
          logger, results.named());
      }

    }

    for (ValueObj& arg : positional) {
      arg = withoutSlash(arg);
    }

    // Now execute the built-in function
    ValueObj result = callback(pstate,
      positional, compiler,
      *this); // 7%

    // If we had no rest arguments, this will be true
    if (restargs == nullptr) return result.detach();
    // Check if all keywords have been marked consumed, meaning we
    // either don't have any or somebody called `keywords` method
    if (restargs->hasAllKeywordsConsumed()) return result.detach();

    // Throw error since not all named arguments were consumed
    throw Exception::TooManyArguments(logger, restargs->keywords());

  }
  // EO _callBuiltInCallable

  //*************************************************//
  // Used for user functions and also by
  // mixin includes and content includes.
  //*************************************************//
  Value* Eval::_runUserDefinedCallable(
    CallableArguments* arguments,
    UserDefinedCallable* callable,
    const SourceSpan& pstate)
  {

    // Here the strategy is to put variables on the current function scope
    // Therefore we do not really need to results anymore once we set them
    // Therefore we can re-use the positional array for our restargs

    // Get some items from passed parameters
    CallableDeclaration* declaration(callable->declaration());
    CallableSignature* prototype(declaration->arguments());
    if (!prototype) throw std::runtime_error("Mixin declaration has no prototype");
    const sass::vector<ArgumentObj>& parameters(prototype->arguments());

    ArgumentResults results(_evaluateArguments(arguments));

    // Get reference to positional arguments in the result object
    // Multiple calls to the same function may re-use the object
    ValueVector& positional(results.positional());

    // Create the variable scope to pass args
    auto idxs = callable->declaration()->idxs;
    EnvScope scoped(compiler.varRoot, idxs);

    // Try to fetch arguments for all parameters
    for (uint32_t i = 0; i < parameters.size(); i += 1) {
      // Errors if argument is missing or given twice
      ValueObj value = getParameter(results, i, parameters[i]);
      // Set lexical variable on scope
      compiler.varRoot.setVariable({ idxs, i },
        value->withoutSlash(), false);
    }

    // Needed here for a specific edge case: restargs must be consumed
    // Those can be consumed e.g. by passing them to other functions
    // Or simply by calling `keywords` on the rest arguments
    ArgumentListObj restargs;

    // If the callable accepts rest argument we can pass all unknown args
    // Also if we must pass rest args we must pass only the remaining parts
    if (prototype->restArg().empty() == false) {

      // Remove consumed items (vars already set)
      // This will leave the rest arguments behind
      if (positional.size() > parameters.size()) {
        positional.erase(positional.begin(),
          positional.begin() + parameters.size());
      }
      else {
        positional.clear();
      }

      // Inherit separator from argument results
      SassSeparator separator(results.separator());
      // But make the default a comma instead of spaces
      if (separator == SASS_UNDEF) separator = SASS_COMMA;
      // Create the rest arguments (move remaining stuff)
      restargs = SASS_MEMORY_NEW(ArgumentList, pstate, separator,
        std::move(positional), std::move(results.named()));
      // Set last lexical variable on scope
      compiler.varRoot.setVariable(
        { idxs, (uint32_t)parameters.size() },
        restargs.ptr(), false);

    }
    else {

      // Check that all positional arguments are consumed
      if (positional.size() > parameters.size()) {
        throw Exception::TooManyArguments(logger,
          positional.size(), parameters.size());
      }

      // Check that all named arguments are consumed
      if (results.named().empty() == false) {
        throw Exception::TooManyArguments(
          logger, results.named());
      }

    }

    ValueObj result;
    // Process all statements within user defined function
    // Only the `@return` statement must return something!
    for (Statement* statement : declaration->elements()) {
      result = statement->accept(this);
      if (result != nullptr) break;
    }

    // If we had no rest arguments, this will be true
    if (restargs == nullptr) return result.detach();
    // Check if all keywords have been marked consumed, meaning we
    // either don't have any or somebody called `keywords` method
    if (restargs->hasAllKeywordsConsumed()) return result.detach();

    // Throw error since not all named arguments were consumed
    throw Exception::TooManyArguments(logger, restargs->keywords());

  }
  // EO _runUserDefinedCallable

  //*************************************************//
  // Call external C-API function
  //*************************************************//
  Value* Eval::_runExternalCallable(
    CallableArguments* arguments,
    ExternalCallable* callable,
    const SourceSpan& pstate)
  {

    // Here the strategy is to put variables into a sass list of Values

    // Get some items from passed parameters
    const EnvKey& name(callable->envkey());
    SassFunctionLambda lambda(callable->lambda());
    CallableSignature* prototype(callable->declaration());
    if (!lambda) throw std::runtime_error("C-API declaration has no callback");
    if (!prototype) throw std::runtime_error("C-API declaration has no prototype");
    const sass::vector<ArgumentObj>& parameters(prototype->arguments());

    ArgumentResults results(_evaluateArguments(arguments));
    ValueFlatMap& named(results.named());
    ValueVector& positional(results.positional());

    // Verify that the passed arguments are valid for this function
    prototype->verify(positional.size(), named, pstate, traces);

    // Process all prototype items which are not positional
    for (size_t i = positional.size(); i < parameters.size(); i++) {
      // Try to find name in passed arguments
      Argument* argument = parameters[i];
      const auto& name(argument->name());
      const auto& it(named.find(name));
      // Check if we found the name
      if (it != named.end()) {
        // Append it to our positional args
        positional.emplace_back(named[name]);
        named.erase(it); // consume argument
      }
      // Otherwise check if argument has a default value
      else if (!argument->defval().isNull()) {
        // Evaluate the expression into final value
        Value* defval(argument->defval()->accept(this));
        // Append it to our positional args
        positional.emplace_back(defval);
      }
      else {
        // This case should never happen due to verification
        throw std::runtime_error("Verify did not protect us!");
      }
    }

    // Needed here for a specific edge case: restargs must be consumed
    // Those can be consumed e.g. by passing them to other functions
    // Or simply by calling `keywords` on the rest arguments
    ArgumentListObj restargs;

    // If the callable accepts rest argument we can pass all unknown args
    // Also if we must pass rest args we must pass only the remaining parts
    if (prototype->restArg().empty() == false) {
      // Superfluous function arguments
      ValueVector superflous;
      // Check if more arguments provided than parameters
      if (positional.size() > parameters.size()) {
        // Move superfluous arguments into the array
        std::move(positional.begin() + parameters.size(),
          positional.end(), back_inserter(superflous));
        // Remove the consumed positional arguments
        positional.resize(parameters.size());
      }

      SassSeparator separator = results.separator();
      if (separator == SASS_UNDEF) separator = SASS_COMMA;
      restargs = SASS_MEMORY_NEW(ArgumentList,
        prototype->pstate(), separator,
        std::move(superflous), std::move(named));
      positional.emplace_back(restargs);
    }

    // Create a new sass list holding parameters to pass to function
    struct SassValue* c_args = sass_make_list(SASS_COMMA, false);
    // First append all positional parameters to it
    for (size_t i = 0; i < positional.size(); i++) {
      sass_list_push(c_args, Value::wrap(positional[i]));
    }

    // Now invoke the function of the callback object
    struct SassValue* c_val = (*lambda)(
      c_args, compiler.wrap(), callable->cookie());
    // It may not return anything at all
    if (c_val == nullptr) return nullptr;
    // Unwrap the result into C++ object
    ValueObj value(&Value::unwrap(c_val));

    // Check for some specific return types to handle
    // Can't use throw in C code, so this has to do it
    if (CustomError* err = value->isaCustomError()) {
      sass::string message("C-API function " +
        name.orig() + ": " + err->message());
      sass_delete_value(c_args);
      sass_delete_value(c_val);
      throw Exception::ParserException(traces, message);
    }
    // This will simply invoke the warning handler
    // ToDo: we should have another way to call this
    // We might want to warn beside returning a value
    else if (CustomWarning* warn = value->isaCustomWarning()) {
      sass::string message("C-API function "
        + name.orig() + ": " + warn->message());
      // warn->pstate(pstate);
      sass_delete_value(c_args);
      sass_delete_value(c_val);
      logger.addWarning(message,
        Logger::WARN_CAPI_FN);
    }
    sass_delete_value(c_val);
    sass_delete_value(c_args);

    // If we had no rest arguments, this will be true
    if (restargs == nullptr) return value.detach();
    // Check if all keywords have been marked consumed, meaning we
    // either don't have any or somebody called `keywords` method
    if (restargs->hasAllKeywordsConsumed()) return value.detach();

    // Throw error since not all named arguments were consumed
    throw Exception::TooManyArguments(logger, restargs->keywords());

  }
  // EO _runExternalCallable

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  //*************************************************//
  // Call built-in function with no overloads
  //*************************************************//
  Value* Eval::execute(
    BuiltInCallable* callable,
    CallableArguments* arguments,
    const SourceSpan& pstate)
  {
    const EnvKey& key(callable->envkey());
    BackTrace trace(pstate, key.orig(), true);
    callStackFrame frame(logger, trace);
    ValueObj rv = _runBuiltInCallable(
      arguments, callable, pstate);
    if (rv.isNull()) {
      throw Exception::RuntimeException(logger,
        "Function finished without @return.");
    }
    rv = rv->withoutSlash();
    return rv.detach();
  }

  //*************************************************//
  // Call built-in function with overloads
  //*************************************************//
  Value* Eval::execute(
    BuiltInCallables* callable,
    CallableArguments* arguments,
    const SourceSpan& pstate)
  {
    const EnvKey& key(callable->envkey());
    BackTrace trace(pstate, key.orig(), true);
    callStackFrame frame(logger, trace);
    ValueObj rv = _runBuiltInCallables(arguments,
      callable, pstate);
    if (rv.isNull()) {
      throw Exception::RuntimeException(logger,
        "Function finished without @return.");
    }
    rv = rv->withoutSlash();
    return rv.detach();
  }

  //*************************************************//
  // Used for user functions and also by
  // mixin includes and content includes.
  //*************************************************//
  Value* Eval::execute(
    UserDefinedCallable* callable,
    CallableArguments* arguments,
    const SourceSpan& pstate)
  {
    RAII_FLAG(inMixin, false);
    const EnvKey& key(callable->envkey());
    BackTrace trace(pstate, key.orig(), true);
    callStackFrame frame(logger, trace);
    ValueObj rv = _runUserDefinedCallable(
      arguments, callable, pstate);
    if (rv.isNull()) {
      throw Exception::RuntimeException(logger,
        "Function finished without @return.");
    }
    rv = rv->withoutSlash();
    return rv.detach();
  }

  //*************************************************//
  // Call external C-API function
  //*************************************************//
  Value* Eval::execute(
    ExternalCallable* callable,
    CallableArguments* arguments,
    const SourceSpan& pstate)
  {
    const EnvKey& key(callable->envkey());
    BackTrace trace(pstate, key.orig(), true);
    callStackFrame frame(logger, trace);
    ValueObj rv = _runExternalCallable(
      arguments, callable, pstate);
    if (rv.isNull()) {
      throw Exception::RuntimeException(logger,
        "Function finished without @return.");
    }
    rv = rv->withoutSlash();
    return rv.detach();
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  ArgumentResults Eval::_evaluateArguments(
    CallableArguments* arguments)
  {
    ArgumentResults results;
    // Get some items from passed parameters
    ValueFlatMap& named(results.named());
    ValueVector& positional(results.positional());

    // Collect positional args by evaluating input arguments
    positional.reserve(arguments->positional().size() + 1);
    for (const auto& arg : arguments->positional())
    {
      ValueObj result(arg->accept(this));
      positional.emplace_back(withoutSlash(result));
    }

    // Collect named args by evaluating input arguments
    for (const auto& kv : arguments->named()) {
      ValueObj result(kv.second->accept(this));
      named.insert(std::make_pair(kv.first,
        withoutSlash(result)));
    }

    // Abort if we don't take any restargs
    if (arguments->restArg() == nullptr) {
      // ToDo: no test case for this!?
      results.separator(SASS_UNDEF);
      return results;
    }

    // Evaluate the variable expression (
    ValueObj result = arguments->restArg()->accept(this);
    ValueObj rest = withoutSlash(result);

    SassSeparator separator = SASS_UNDEF;

    if (Map* restMap = rest->isaMap()) {
      _addRestValueMap(named, restMap,
        arguments->restArg()->pstate());
    }
    else if (List* list = rest->isaList()) {
      std::copy(list->begin(), list->end(),
        std::back_inserter(positional));
      separator = list->separator();
      if (ArgumentList* args = rest->isaArgumentList()) {
        auto& kwds = args->keywords();
        for (const auto& kv : kwds) {
          named[kv.first] = kv.second;
        }
      }
    }
    else {
      positional.emplace_back(std::move(rest));
    }

    if (arguments->kwdRest() == nullptr) {
      results.separator(separator);
      return results;
    }

    // kwdRest already poisoned
    ValueObj keywordRest = arguments->kwdRest()->accept(this);

    if (Map* restMap = keywordRest->isaMap()) {
      _addRestValueMap(named, restMap, arguments->kwdRest()->pstate());
      results.separator(separator);
      return results;
    }

    callStackFrame csf(logger, keywordRest->pstate());
    throw Exception::RuntimeException(traces,
      "Variable keyword arguments must be a map (was $keywordRest).");

  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  /// Evaluates [expression] and calls `toCss()`.
  sass::string Eval::toCss(Expression* expression, bool quote)
  {
    ValueObj value = expression->accept(this);
    return value->toCss(quote);
  }

  /// Evaluates [interpolation] into a serialized string.
  ///
  /// If [trim] is `true`, removes whitespace around the result.
  /// If [warnForColor] is `true`, this will emit a warning for
  /// any named color values passed into the interpolation.
  sass::string Eval::acceptInterpolation(InterpolationObj interpolation, bool warnForColor, bool trim)
  {
    // Needed in loop
    ValueObj value;
    // Create CSS output options
    OutputOptions out(
      SASS_STYLE_TO_CSS,
      compiler.precision);
    // Create the emitter
    Cssize cssize(out);
    // Don't quote strings
    cssize.quotes = false;
    // Process all interpolants in the interpolation
    // Items in interpolations are only of three types
    // Performance optimized since it's used quite a lot
    for (Interpolant* itpl : interpolation->elements()) {
      if (itpl == nullptr) continue;
      switch (itpl->getType()) {
      case Interpolant::LiteralInterpolant:
        cssize.append_token(
          static_cast<ItplString*>(itpl)->text(),
          static_cast<ItplString*>(itpl));
        break;
      case Interpolant::ValueInterpolant:
        static_cast<Value*>(itpl)
          ->accept(&cssize);
        break;
      case Interpolant::ExpressionInterpolant:
        value = static_cast<Expression*>(itpl)->accept(this);
        if (warnForColor) {
          if (Color* color = value->isaColor()) {
            ColorRgbaObj rgba = color->toRGBA();
            double numval = rgba->r() * 0x10000
              + rgba->g() * 0x100 + rgba->b();
            if (const char* disp = color_to_name((int)numval)) {
              sass::sstream msg;
              msg << "You probably don't mean to use the color value ";
              msg << disp << " in interpolation here.\nIt may end up represented ";
              msg << "as " << rgba->inspect() <<", which will likely produce invalid ";
              msg << "CSS. Always quote color names when using them as strings or map ";
              msg << "keys (for example, \"" << disp << "\"). If you really want to ";
              msg << "use the color value, append it to an empty string first to avoid ";
              msg << "this warning (for example, '\"\" + " << disp << "').";
              logger.addWarning(msg.str(), itpl->pstate(), Logger::WARN_COLOR_ITPL);
            }
          }
        }
        value->accept(&cssize);
        break;
      }
    }
    // ToDo: check it's using RVO
    return cssize.get_buffer(trim);
  }
  // EO acceptInterpolation

  /// Evaluates [interpolation] and wraps the result in a [SourceData].
  ///
  /// If [trim] is `true`, removes whitespace around the result.
  /// If [warnForColor] is `true`, this will emit a warning for
  /// any named color values passed into the interpolation.
  SourceData* Eval::interpolationToSource(InterpolationObj interpolation, bool warnForColor, bool trim)
  {
    if (interpolation.isNull()) return nullptr;
    sass::string result = acceptInterpolation(interpolation, warnForColor, trim);
    return SASS_MEMORY_NEW(SourceItpl, interpolation->pstate(), std::move(result));
  }

  /// Evaluates [interpolation] and wraps the result in a [CssValue].
  ///
  /// If [trim] is `true`, removes whitespace around the result.
  /// If [warnForColor] is `true`, this will emit a warning for
  /// any named color values passed into the interpolation.
  CssString* Eval::interpolationToCssString(InterpolationObj interpolation,
    bool warnForColor, bool trim)
  {
    if (interpolation.isNull()) return nullptr;
    sass::string result = acceptInterpolation(interpolation, warnForColor, trim);
    return SASS_MEMORY_NEW(CssString, interpolation->pstate(), std::move(result));
  }

  /// Evaluates [interpolation] and parses the result into a [SelectorList].
  SelectorListObj Eval::interpolationToSelector(Interpolation* itpl, bool plainCss, bool allowParent)
  {
    // Create a new source data object from the evaluated interpolation
    SourceDataObj synthetic = interpolationToSource(itpl, true, true);
    // Everything parsed, will be parsed from perspective of local content
    // Pass the source-map in for the interpolation, so the scanner can
    // update the positions according to previous source-positions
    // Is a parser state solely represented by a source map or do we
    // need an intermediate format for them?
    SelectorParser parser(compiler, synthetic);
    parser.allowPlaceholder = plainCss == false;
    parser.allowParent = allowParent && plainCss == false;
    return parser.parseSelectorList(); // comes detached!
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  void Eval::_evaluateMacroArguments(
    CallableArguments* arguments,
    ExpressionVector& positional,
    ExpressionFlatMap& named)
  {

    if (arguments->restArg()) {

      ValueObj rest = arguments->restArg()->accept(this);

      if (Map* restMap = rest->isaMap()) {
        _addRestExpressionMap(named, restMap, arguments->restArg()->pstate());
      }
      else if (List* restList = rest->isaList()) {
        for (const ValueObj& value : restList->elements()) {
          positional.emplace_back(SASS_MEMORY_NEW(
            ValueExpression, value->pstate(), value));
        }
        // separator = list->separator();
        if (ArgumentList* args = rest->isaArgumentList()) {
          for (auto& kv : args->keywords()) {
            named[kv.first] = SASS_MEMORY_NEW(ValueExpression,
              kv.second->pstate(), kv.second);
          }
        }
      }
      else {
        positional.emplace_back(SASS_MEMORY_NEW(
          ValueExpression, rest->pstate(), rest));
      }

    }

    if (arguments->kwdRest() == nullptr) {
      return;
    }

    ValueObj keywordRest = arguments->kwdRest()->accept(this);

    if (Map* restMap = keywordRest->isaMap()) {
      _addRestExpressionMap(named, restMap, arguments->restArg()->pstate());
      return;
    }

    throw Exception::RuntimeException(logger,
      "Variable keyword arguments must be a map (was $keywordRest).");

  }
  // EO _evaluateMacroArguments

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Value* Eval::visitBooleanExpression(BooleanExpression* ex)
  {
    #ifdef SASS_ELIDE_COPIES
    return ex->value();
    #else
    return SASS_MEMORY_COPY(ex->value());
    #endif
  }

  Value* Eval::visitColorExpression(ColorExpression* ex)
  {
    #ifdef SASS_ELIDE_COPIES
    return ex->value();
    #else
    ColorObj color = ex->value();
    ColorObj copy = SASS_MEMORY_COPY(color);
    copy->disp(color->disp());
    return copy.detach();
#endif
  }

  Value* Eval::visitNumberExpression(NumberExpression* ex)
  {
    #ifdef SASS_ELIDE_COPIES
    return ex->value();
    #else
    return SASS_MEMORY_COPY(ex->value());
    #endif
  }

  Value* Eval::visitNullExpression(NullExpression* ex)
  {
    #ifdef SASS_ELIDE_COPIES
    return ex->value();
    #else
    return SASS_MEMORY_COPY(ex->value());
    #endif
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  //*************************************************//
  //*************************************************//
  Value* Eval::visitListExpression(ListExpression* l)
  {
    // regular case for unevaluated lists
    ListObj ll = SASS_MEMORY_NEW(List, l->pstate(),
      ValueVector(), l->separator());
    ll->hasBrackets(l->hasBrackets());
    for (size_t i = 0, L = l->size(); i < L; ++i) {
      ll->append(l->get(i)->accept(this));
    }
    return ll.detach();
  }
  // EO visitListExpression

  //*************************************************//
  //*************************************************//
  Value* Eval::visitMapExpression(MapExpression* m)
  {
    ValueObj key;
    MapObj map(SASS_MEMORY_NEW(Map, m->pstate()));
    const ExpressionVector& kvlist(m->kvlist());
    for (size_t i = 0, L = kvlist.size(); i < L; i += 2)
    {
      // First evaluate the key
      key = kvlist[i]->accept(this);
      // Check for key duplication
      if (map->has(key)) {
        traces.emplace_back(kvlist[i]->pstate());
        throw Exception::DuplicateKeyError(traces, *map, *key);
      }
      // Second insert the evaluated value for key
      map->insertOrSet(key, kvlist[i + 1]->accept(this));
    }
    return map.detach();
  }
  // EO visitMapExpression

  //*************************************************//
  //*************************************************//
  Value* Eval::visitStringExpression(StringExpression* node)
  {
    // Don't use [performInterpolation] here because we need to get
    // the raw text from strings, rather than the semantic value.
    InterpolationObj itpl = node->text();
    sass::vector<sass::string> strings;
    for (const auto& item : itpl->elements()) {
      if (ItplString* lit = item->isaItplString()) {
        strings.emplace_back(lit->text());
      }
      else {
        ValueObj result = item->isaValue();
        if (Expression* ex = item->isaExpression()) {
          result = ex->accept(this);
        }
        if (String* lit = result->isaString()) {
          strings.emplace_back(lit->value());
        }
        else if (!result->isNull()) {
          strings.emplace_back(result->toCss(false));
        }
      }
    }

    return SASS_MEMORY_NEW(String, node->pstate(),
      StringUtils::join(strings, ""), node->hasQuotes());
  }
  // EO visitStringExpression

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  //*************************************************//
  //*************************************************//
  Value* Eval::visitBinaryOpExpression(BinaryOpExpression* node)
  {
    ValueObj left, right;
    Expression* lhs = node->left();
    Expression* rhs = node->right();
    left = lhs->accept(this);
    switch (node->operand()) {
    case SassOperator::IESEQ:
      right = rhs->accept(this);
      return left->singleEquals(
        right, logger, node->pstate());
    case SassOperator::OR:
      if (left->isTruthy()) {
        return left.detach();
      }
      return rhs->accept(this);
    case SassOperator::AND:
      if (!left->isTruthy()) {
        return left.detach();
      }
      return rhs->accept(this);
    case SassOperator::EQ:
      right = rhs->accept(this);
      return ObjEqualityFn(left, right)
        ? bool_true : bool_false;
    case SassOperator::NEQ:
      right = rhs->accept(this);
      return ObjEqualityFn(left, right)
        ? bool_false : bool_true;
    case SassOperator::GT:
      right = rhs->accept(this);
      return left->greaterThan(right,
        logger, node->pstate())
        ? bool_true : bool_false;
    case SassOperator::GTE:
      right = rhs->accept(this);
      return left->greaterThanOrEquals(right,
        logger, node->pstate())
        ? bool_true : bool_false;
    case SassOperator::LT:
      right = rhs->accept(this);
      return left->lessThan(right,
        logger, node->pstate())
        ? bool_true : bool_false;
    case SassOperator::LTE:
      right = rhs->accept(this);
      return left->lessThanOrEquals(right,
        logger, node->pstate())
        ? bool_true : bool_false;
    case SassOperator::ADD:
      right = rhs->accept(this);
      return left->plus(right,
        logger, node->pstate());
    case SassOperator::SUB:
      right = rhs->accept(this);
      return left->minus(right,
        logger, node->pstate());
    case SassOperator::MUL:
      right = rhs->accept(this);
      return left->times(right,
        logger, node->pstate());
    case SassOperator::DIV:
      right = rhs->accept(this);
      return doDivision(left, right,
        node, logger, node->pstate());
    case SassOperator::MOD:
      right = rhs->accept(this);
      return left->modulo(right,
        logger, node->pstate());
    }
    // Satisfy compiler
    return nullptr;
  }
  // visitBinaryOpExpression

  //*************************************************//
  //*************************************************//
  Value* Eval::visitUnaryOpExpression(UnaryOpExpression* node)
  {
    ValueObj operand = node->operand()->accept(this);
    switch (node->optype()) {
    case UnaryOpType::PLUS:
      return operand->unaryPlus(logger, node->pstate());
    case UnaryOpType::MINUS:
      return operand->unaryMinus(logger, node->pstate());
    case UnaryOpType::NOT:
      return operand->unaryNot(logger, node->pstate());
    case UnaryOpType::SLASH:
      return operand->unaryDivide(logger, node->pstate());
    }
    // Satisfy compiler
    return nullptr;
  }
  // EO visitUnaryOpExpression

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // This operates similar to a function call
  Value* Eval::visitIfExpression(IfExpression* node)
  {
    CallableArguments* arguments = node->arguments();
    callStackFrame frame(logger, node->pstate());
    // We need to make copies here to preserve originals
    // We could optimize this further, but impact is slim
    ExpressionFlatMap named(arguments->named());
    ExpressionVector positional(arguments->positional());
    // Rest arguments must be evaluated in all cases
    // evaluateMacroArguments is only used for this
    _evaluateMacroArguments(node->arguments(), positional, named);
    ExpressionObj condition = getArgument(positional, named, 0, Keys::condition);
    ExpressionObj ifTrue = getArgument(positional, named, 1, Keys::ifTrue);
    ExpressionObj ifFalse = getArgument(positional, named, 2, Keys::ifFalse);
    if (positional.size() > 3) {
      throw Exception::TooManyArguments(
        logger, positional.size(), 3);
    }
    if (positional.size() + named.size() > 3) {
      throw Exception::TooManyArguments(logger, named,
        { Keys::condition, Keys::ifTrue, Keys::ifFalse });
    }

    ValueObj rv = condition ? condition->accept(this) : nullptr;
    Expression* ex = rv && rv->isTruthy() ? ifTrue : ifFalse;
    if (ex == nullptr) return nullptr;
    ValueObj result(ex->accept(this));
    return withoutSlash(result);
  }

  Value* Eval::visitParenthesizedExpression(ParenthesizedExpression* ex)
  {
    // return ex->expression();
    if (ex->expression()) {
      return ex->expression()->accept(this);
    }
    return nullptr;
  }

  Value* Eval::visitParentExpression(ParentExpression* p)
  {
    if (SelectorListObj& parents = original()) {
      return parents->toValue();
    }
    else {
      return SASS_MEMORY_NEW(Null, p->pstate());
    }
  }

  void Eval::renderArgumentInvocation(sass::string& strm, CallableArguments* args)
  {
    if (!args->named().empty()) {
      callStackFrame frame(traces,
        args->pstate());
      throw Exception::RuntimeException(logger,
        "Plain CSS functions don't support keyword arguments.");
    }
    if (args->kwdRest() != nullptr) {
      callStackFrame frame(traces,
        args->kwdRest()->pstate());
      throw Exception::RuntimeException(logger,
        "Plain CSS functions don't support keyword arguments.");
    }
    bool addComma = false;
    strm += "(";
    for (Expression* argument : args->positional()) {
      if (addComma) { strm += ", "; }
      else { addComma = true; }
      strm += toCss(argument);
    }
    if (ExpressionObj rest = args->restArg()) {
      if (addComma) { strm += ", "; }
      else { addComma = true; }
      strm += toCss(rest);
    }
    strm += ")";
  }

  Value* Eval::visitCssFunction(CssFnExpression* cssfn)
  {
    // return ex->expression();
    if (cssfn->itpl()) {
      sass::string strm;
      strm += acceptInterpolation(cssfn->itpl(), false);
      renderArgumentInvocation(strm, cssfn->arguments());
      return SASS_MEMORY_NEW(
        String, cssfn->pstate(),
        std::move(strm));
    }
    return nullptr;
  }

  Value* Eval::visitValueExpression(ValueExpression* node)
  {
    // We have a bug lurking somewhere
    // without detach it gets deleted?
    ValueObj value = node->value();
    return value.detach();
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Value* Eval::visitMixinRule(MixinRule* rule)
  {
    UserDefinedCallableObj callable =
      SASS_MEMORY_NEW(UserDefinedCallable,
        rule->pstate(), rule->name(), rule, nullptr);
    rule->midx(compiler.varRoot.findMixIdx(
      rule->name(), Strings::empty));
    compiler.varRoot.setMixin(
      rule->midx(), callable, false);
    return nullptr;
  }


  Value* Eval::visitFunctionRule(FunctionRule* rule)
  {
    UserDefinedCallableObj callable =
      SASS_MEMORY_NEW(UserDefinedCallable,
        rule->pstate(), rule->name(), rule, nullptr);
    rule->fidx(compiler.varRoot.findFnIdx(
      rule->name(), Strings::empty));
    compiler.varRoot.setFunction(
      rule->fidx(), callable, false);
    return nullptr;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  //*************************************************//
  // Evaluate and return lexical variable with `name`
  // Cache dynamic lookup results in `vidxs` member
  //*************************************************//
  Value* Eval::visitVariableExpression(VariableExpression* variable)
  {

    // Check if variable expression was already resolved
    if (variable->vidxs().empty()) {
      // Is variable on the local scope?
      if (variable->isLexical()) {
        // Find all idxs and fill vidxs
        compiler.varRoot.findVarIdxs(
          variable->vidxs(),
          variable->name());
      }
      // Variable is on module (root) scope
      else {
        EnvRef vidx = compiler.varRoot.findVarIdx(
          variable->name(), variable->ns());
        if (vidx.isValid()) variable->vidxs().push_back(vidx);
      }

    }

    // Variables must be resolved from top to bottom
    // This has to do with the way how Sass handles scopes
    // E.g. in loops, the variable can first point to the outer
    // variable and later to the inner variable, if an assignment
    // exists after the first reference in that loop scope:
    // $a: 0; @for $i from 1 through 3 { @debug $a; $a: $i; } @debug $a
    // $b: 0; a { @for $i from 1 through 3 { @debug $b; $b: $i; } @debug $b }
    for (const EnvRef& vidx : variable->vidxs()) {
      auto& value = compiler.varRoot.getVariable(vidx);
      if (value != nullptr) return value->withoutSlash();
    }

    // If we reach this point we have an error
    // Mixin wasn't found and couldn't be executed
    callStackFrame frame(traces, variable->pstate());

    // Check if variable was requested from a module and if that module actually exists
    if (variable->ns().empty() || compiler.varRoot.stack.back()->hasNameSpace(variable->ns())) {
      throw Exception::RuntimeException(traces, "Undefined variable.");
    }

    // Otherwise the module simply wasn't imported
    throw Exception::ModuleUnknown(traces, variable->ns());

  }
  // EO visitVariableExpression

  //*************************************************//
  // Execute function with `name` and return a Value
  // Cache dynamic lookup results in `fidx` member
  //*************************************************//
  Value* Eval::visitFunctionExpression(FunctionExpression* function)
  {

    // Check if function expression was already resolved
    if (!function->fidx().isValid()) {
      // Try to fetch the function by finding it by name
      // This may fail, as function expressions can also be
      // css functions if the function by name is not declared.
      function->fidx(compiler.varRoot.findFnIdx(function->name(), function->ns()));
    }

    // Check if function expressions is resolved now
    // If not the expression is a regular css function
    if (function->fidx().isValid()) {
      // Check if function is already defined on the frame/scope
      // Can fail if the function definition comes after the usage
      if (Callable* callable = compiler.varRoot.getFunction(function->fidx())) {
        RAII_FLAG(inFunction, true);
        return callable->execute(*this,
          function->arguments(), function->pstate());
      }
    }

    // Only functions without namespace can be css-functions
    // Functions with namespace must be executed or fail
    if (function->ns().empty()) {
      sass::string strm;
      strm += function->name();
      renderArgumentInvocation(
        strm, function->arguments());
      return SASS_MEMORY_NEW(
        String, function->pstate(),
        std::move(strm));
    }

    // If we reach this point we have an error
    // Mixin wasn't found and couldn't be executed
    callStackFrame frame(traces, function->pstate());

    // Check if function was requested from a module and if that module actually exists
    if (function->ns().empty() || compiler.varRoot.stack.back()->hasNameSpace(function->ns())) {
      throw Exception::RuntimeException(traces, "Undefined function.");
    }

    // Otherwise the module simply wasn't imported
    throw Exception::ModuleUnknown(traces, function->ns());

  }
  // EO visitFunctionExpression

  //*************************************************//
  // Execute/include a mixin (return value must be collected)
  // Cache dynamic lookup results in `midx` member
  //*************************************************//
  Value* Eval::visitIncludeRule(IncludeRule* include)
  {

    // Check if mixin expression was already resolved
    if (!include->midx().isValid()) {
      // Try to fetch the mixin by finding it by name
      include->midx(compiler.varRoot.findMixIdx(include->name(), include->ns()));
    }

    // Check if function expressions is resolved now
    // If not the expression is a regular css function
    if (include->midx().isValid()) {
      // Check if mixin is already defined on the frame/scope
      // Can fail if the mixin definition comes after the usage
      if (Callable* callable = compiler.varRoot.getMixin(include->midx())) {

        // 99% of all mixins are user defined (expect `load-css`)
        if (auto mixin = callable->isaUserDefinedCallable()) {

          // An include expression must reference a mixin rule
          MixinRule* rule = mixin->declaration()->isaMixinRule();

          // Sanity assertion
          if (rule == nullptr) {
            throw Exception::RuntimeException(traces,
              "Include doesn't reference a mixin!");
          }

          // Create new mixin for content block
          // Prepares the content block to be called later
          // Content blocks of includes are like mixins themselves
          UserDefinedCallableObj cmixin;

          // Check if a content block was passed to include
          if (include->content() != nullptr) {
            // Create a new temporary mixin
            // Attach current content block to it in order
            // for it to being restored when it is invoked.
            cmixin = SASS_MEMORY_NEW(
              UserDefinedCallable,
              include->pstate(), include->name(),
              include->content().ptr(), content);
            // Check if invoked mixin accepts a content block
            if (!rule->hasContent()) {
              callStackFrame frame(logger,
                include->content()->pstate());
              throw Exception::RuntimeException(logger,
                "Mixin doesn't accept a content block.");
            }
          }

          // Change lexical status (RAII)
          // Influences e.g. `content-exists`
          RAII_FLAG(inMixin, true);

          // Add a special backtrace for include invocation
          callStackFrame frame(logger, BackTrace(
            include->pstate(), mixin->envkey().orig(), true));

          // Overwrite current content block mixin with new one
          // Even overwrite it if no new content block was given
          RAII_PTR(UserDefinedCallable, content, cmixin);

          // Return value can be ignored, but memory must still be collected
          return _runUserDefinedCallable(include->arguments(), mixin, include->pstate());

        }
        // This is currently only used for `load-css` mixin
        else if (auto builtin = callable->isaBuiltInCallable()) {

          // Return value can be ignored, but memory must still be collected
          return builtin->execute(*this, include->arguments(), include->pstate());

        }

      }
    }

    // If we reach this point we have an error
    // Mixin wasn't found and couldn't be executed
    callStackFrame frame(traces, include->pstate());

    // Check if function was requested from a module and if that module actually exists
    if (include->ns().empty() || compiler.varRoot.stack.back()->hasNameSpace(include->ns())) {
      throw Exception::RuntimeException(traces, "Undefined mixin.");
    }

    // Otherwise the module simply wasn't imported
    throw Exception::ModuleUnknown(traces, include->ns());

  }
  // EO visitIncludeRule

  //*************************************************//
  // See visitContentRule and visitIncludeRule
  //*************************************************//
  Value* Eval::visitContentBlock(ContentBlock* rule)
  {
    throw std::runtime_error("Evaluation handles "
      "@include and its content block together.");
  }

  //*************************************************//
  // Invoke the current block mixin (if available)
  //*************************************************//
  Value* Eval::visitContentRule(ContentRule* c)
  {
    // Check if no content block can be called
    // This is no error by design, just ignore
    if (content == nullptr) return nullptr;

    // Get local reference to current content block
    UserDefinedCallable* current = content;

    // Reset lexical status (RAII)
    // Influences e.g. `content-exists`
    RAII_FLAG(inMixin, false);

    // Add a special backtrace for include invocation
    callStackFrame frame(logger, BackTrace(
      c->pstate(), Strings::contentRule));

    // Reset lexical pointer for current content block to the
    // content block where the now invoked mixin was seen. This
    // allows the content calls to be "wrapped recursively".
    // Note: could have been implemented with a regular stack.
    RAII_PTR(UserDefinedCallable, content, current->content());

    // Execute the callable and return value which must be collected
    return _runUserDefinedCallable(c->arguments(), current, c->pstate());

  }
  // EO visitContentRule

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  //*************************************************//
  //*************************************************//
  void Eval::callExternalMessageOverloadFunction(Callable* fn, Value* message)
  {
    // We know that warn override function can only be external
    SASS_ASSERT(fn->isaExternalCallable(), "Custom callable must be external");
    ExternalCallable* def = static_cast<ExternalCallable*>(fn);
    SassFunctionLambda lambda = def->lambda();
    struct SassValue* c_args = sass_make_list(SASS_COMMA, false);
    sass_list_push(c_args, Value::wrap(message));
    struct SassValue* c_val = lambda(
      c_args, compiler.wrap(), def->cookie());
    sass_delete_value(c_args);
    sass_delete_value(c_val);
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Value* Eval::visitDebugRule(DebugRule* node)
  {
    ValueObj message = node->expression()->accept(this);
    EnvRef fidx = compiler.varRoot.findFnIdx(Keys::debugRule, "");
    if (fidx.isValid()) {
      CallableObj& fn = compiler.varRoot.getFunction(fidx);
      callExternalMessageOverloadFunction(fn, message);
    }
    else {
      logger.addDebug(message->
        inspect(compiler.precision, false),
        node->pstate());
    }
    return nullptr;
  }

  Value* Eval::visitWarnRule(WarnRule* node)
  {
    ValueObj message = node->expression()->accept(this);
    EnvRef fidx = compiler.varRoot.findFnIdx(Keys::warnRule, "");
    if (fidx.isValid()) {
      CallableObj& fn = compiler.varRoot.getFunction(fidx);
      callExternalMessageOverloadFunction(fn, message);
    }
    else {
      sass::string result(message->toCss(false));
      callStackFrame frame(logger, BackTrace(node->pstate()));
      logger.addWarning(result, Logger::WARN_RULE);
    }
    return nullptr;
  }

  Value* Eval::visitErrorRule(ErrorRule* node)
  {
    ValueObj message = node->expression()->accept(this);
    EnvRef fidx = compiler.varRoot.findFnIdx(Keys::errorRule, "");
    if (fidx.isValid()) {

      CallableObj& fn = compiler.varRoot.getFunction(fidx);
      callExternalMessageOverloadFunction(fn, message);
    }
    else {
      sass::string result(message->inspect());
      traces.push_back(BackTrace(node->pstate()));
      throw Exception::RuntimeException(traces, result);
    }
    return nullptr;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Value* Eval::visitStyleRule(StyleRule* node)
  {
    // Create a scope for lexical block variables
    EnvScope scope(compiler.varRoot, node->idxs);
    // Keyframe blocks have a specific syntax inside them
    // Therefore style rules render a bit different inside them
    if (inKeyframes) {
      // Find the parent we should append to (bubble up)
      auto chroot = current->bubbleThrough(true);
      // Create a new keyframe parser from the evaluated interpolation
      KeyframeSelectorParser parser(compiler, SASS_MEMORY_NEW(SourceItpl,
        node->interpolation()->pstate(),
        acceptInterpolation(node->interpolation(), true, true)));
      // Invoke the keyframe parser and create a new CssKeyframeBlock
      CssKeyframeBlockObj child = SASS_MEMORY_NEW(CssKeyframeBlock, node->pstate(),
        chroot, SASS_MEMORY_NEW(CssStringList, node->pstate(), parser.parse()));
      // Add child to our parent
      chroot->addChildAt(child, false);
      // addChildAt(chroot, child);
      // Visit the remaining items at new child
      acceptChildrenAt(child, node);
    }
    // Regular style rule
    else if (node->interpolation()) {
      // Check current importer context
      Import* imp = compiler.import_stack.back();
      bool plainCss = imp->syntax == SASS_IMPORT_CSS;
      // Evaluate the interpolation and try to parse a selector list
      SelectorListObj slist = interpolationToSelector(node->interpolation(), plainCss)->
        resolveParentSelectors(selector(), traces, !atRootExcludingStyleRule);
      // Append new selector list to the stack
      RAII_SELECTOR(selectorStack, slist);
      // The copy is needed for parent reference evaluation
      // dart-sass stores it as `originalSelector` member
      RAII_SELECTOR(originalStack, slist->hasExplicitParent() ?
        slist.ptr() : SASS_MEMORY_COPY(slist)); // Avoid copy if possible
      // Make the new selectors known for the extender
      // If previous extend rules match this selector it will
      // immediately do the extending, extend rules that occur
      // later will apply the extending to the existing ones.
      if (extender2) extender2->addSelector(slist, mediaStack.back());
      // Find the parent we should append to (bubble up)
      auto chroot = current->bubbleThrough(true);
      // Create a new style rule at the correct parent
      CssStyleRuleObj child = SASS_MEMORY_NEW(CssStyleRule,
        node->pstate(), chroot, slist);
      // Add child to our parent
      chroot->addChildAt(child, true);
      // Register new child as style rule
      RAII_PTR(CssStyleRule, readStyleRule, child);
      // Reset specific flag (not in an at-rule)
      RAII_FLAG(atRootExcludingStyleRule, false);
      // Visit the remaining items at child
      return acceptChildrenAt(child, node);
    }
    // Consumed node
    return nullptr;
  }
  // EO visitStyleRule

  CssRoot* Eval::acceptRoot(Root* root)
  {

    Preloader preloader(*this, root);
    preloader.process();

    CssRootObj css = SASS_MEMORY_NEW(CssRoot, root->pstate());

    RAII_PTR(Extender, extender2, root->extender);
    RAII_PTR(CssParentNode, current, css);
    root->isCompiled = true;

    RAII_PTR(Root, modctx, root);

    ImportStackFrame iframe(compiler, root->import);

    for (const StatementObj& item : root->elements()) {
      Value* child = item->accept(this);
      if (child) delete child;
    }
    return css.detach();
  }

  CssParentNode* Eval::hoistStyleRule(CssParentNode* node)
  {
    if (isInStyleRule()) {
      auto outer = SASS_MEMORY_RESECT(readStyleRule);
      node->addChildAt(outer, false);
      return outer;
    }
    else {
      return node;
    }
  }

  Value* Eval::visitSupportsRule(SupportsRule* node)
  {
    ValueObj condition = SASS_MEMORY_NEW(
      String, node->condition()->pstate(),
      _visitSupportsCondition(node->condition()));
    EnvScope scoped(compiler.varRoot, node->idxs);
    auto chroot = current->bubbleThrough(true);
    CssSupportsRuleObj css = SASS_MEMORY_NEW(CssSupportsRule,
      node->pstate(), chroot, condition);
    chroot->addChildAt(css, false);
    acceptChildrenAt(
      hoistStyleRule(css),
      node->elements());
    return nullptr;
  }

  CssParentNode* Eval::_trimIncluded(CssParentVector& nodes)
  {

    auto _root = getRoot();
    if (nodes.empty()) return _root;

    auto parent = current;
    size_t innermostContiguous = sass::string::npos;
    for (size_t i = 0; i < nodes.size(); i++) {
      while (parent != nodes[i]) {
        innermostContiguous = sass::string::npos;
        parent = parent->parent();
      }
      if (innermostContiguous == sass::string::npos) {
        innermostContiguous = i;
      }
      parent = parent->parent();
    }

    if (parent != _root) return _root;
    auto& root = nodes[innermostContiguous];
    nodes.resize(innermostContiguous);
    return root;

  }

  Value* Eval::visitAtRootRule(AtRootRule* node)
  {
    EnvScope scoped(compiler.varRoot, node->idxs);
    InterpolationObj itpl = node->query();
    AtRootQueryObj query;

    if (node->query()) {
      query = AtRootQuery::parse(
        interpolationToSource(
          node->query(), true),
        compiler);
    }
    else {
      query = AtRootQuery::defaultQuery(
        SourceSpan{ node->pstate() });
    }

    RAII_FLAG(inKeyframes, false);
    RAII_FLAG(inUnknownAtRule, false);
    RAII_FLAG(atRootExcludingStyleRule,
      query && query->excludesStyleRules());

    CssParentNode* parent = current;
    CssParentNode* orgParent = current;
    CssParentVector included;

    while (parent && parent->parent()) {
      // is!CssStylesheet (is!CssRootNode)
      if (!query->excludes(parent)) {
        included.emplace_back(parent);
      }
      parent = parent->parent();
    }
    auto root = _trimIncluded(included);

    if (root == orgParent) {
      acceptChildrenAt(root, node);
    }
    else {
      CssParentNode* innerCopy = included.empty() ?
        nullptr : SASS_MEMORY_RESECT(included.front());
      // if (innerCopy) innerCopy->clear();
      CssParentNode* outerCopy = innerCopy;
      auto it = included.begin();
      // Included is not empty
      if (it != included.end()) {
        if (++it != included.end()) {
          auto copy = SASS_MEMORY_RESECT(*it);
          copy->addChildAt(outerCopy, false);
          outerCopy = copy;
        }
      }

      if (outerCopy != nullptr) {
        root->addChildAt(outerCopy, false);
      }

      auto newParent = innerCopy == nullptr ? root : innerCopy;

      RAII_FLAG(inKeyframes, inKeyframes);
      RAII_FLAG(inUnknownAtRule, inUnknownAtRule);
      RAII_FLAG(atRootExcludingStyleRule, atRootExcludingStyleRule);
      CssMediaQueryVector oldQueries = mediaQueries;

      if (query->excludesStyleRules()) {
        atRootExcludingStyleRule = true;
      }

      if (query->excludesMedia()) {
        mediaQueries.clear();
      }

      if (inKeyframes && query->excludesName("keyframes")) {
        inKeyframes = false;
      }

      if (inUnknownAtRule) {
        bool hasAtRuleInIncluded = false;
        for (auto& include : included) {
          // A flag on parent could save 1%
          if (include->isaCssAtRule()) {
            hasAtRuleInIncluded = true;
            break;
          }
        }
        if (!hasAtRuleInIncluded) {
          inUnknownAtRule = false;
        }
      }

      acceptChildrenAt(newParent, node);

      mediaQueries = oldQueries;

    }

    return nullptr;
  }

  Value* Eval::visitAtRule(AtRule* node)
  {
    CssStringObj name = interpolationToCssString(node->name(), true, false);
    CssStringObj value = interpolationToCssString(node->value(), true, true);

    if (node->empty()) {
      CssAtRuleObj css = SASS_MEMORY_NEW(CssAtRule,
        node->pstate(), current, name, value, node->isChildless());
      current->addChildAt(css, false);
      return nullptr;
    }

    EnvScope scoped(compiler.varRoot, node->idxs);

    sass::string normalized(StringUtils::unvendor(name->text()));
    bool isKeyframe = normalized == "keyframes";
    RAII_FLAG(inUnknownAtRule, !isKeyframe);
    RAII_FLAG(inKeyframes, isKeyframe);


    auto pu = current->bubbleThrough(true);

    // ModifiableCssKeyframeBlock
    CssAtRuleObj css = SASS_MEMORY_NEW(CssAtRule,
      node->pstate(), pu, name, value, node->isChildless());

    // Adds new empty atRule to Root!
    pu->addChildAt(css, false);

    auto oldParent = current;
    current = css;

    if (!(!atRootExcludingStyleRule && readStyleRule != nullptr) || inKeyframes) {

      for (const auto& child : node->elements()) {
        ValueObj val = child->accept(this);
      }


    }
    else {

      // If we're in a style rule, copy it into the at-rule so that
      // declarations immediately inside it have somewhere to go.
      // For example, "a {@foo {b: c}}" should produce "@foo {a {b: c}}".
      CssStyleRule* qwe = SASS_MEMORY_RESECT(readStyleRule);
      css->addChildAt(qwe, false);
      acceptChildrenAt(qwe, node->elements());

    }
    current = oldParent;


    return nullptr;
  }

  Value* Eval::visitMediaRule(MediaRule* node)
  {

    ExpressionObj mq;
    sass::string str_mq;
    const SourceSpan& state = node->query() ?
      node->query()->pstate() : node->pstate();
    EnvScope scoped(compiler.varRoot, node->idxs);
    if (node->query()) {
      str_mq = acceptInterpolation(node->query(), false);
    }


    MediaQueryParser parser(compiler, SASS_MEMORY_NEW(
      SourceItpl, state, std::move(str_mq)));
    CssMediaQueryVector parsed(parser.parse());

    CssMediaQueryVector mergedQueries
    (mergeMediaQueries(mediaQueries, parsed));

    if (mergedQueries.empty()) {
      if (!mediaQueries.empty()) {
        return nullptr;
      }
      mergedQueries = parsed;
    }

    // Create a new CSS only representation of the media rule
    CssMediaRuleObj css = SASS_MEMORY_NEW(CssMediaRule,
      node->pstate(), current, mergedQueries);
    auto chroot = current->bubbleThrough(false);
    // addChildAt(chroot, css);
    chroot->addChildAt(css, false);

    RAII_PTR(CssParentNode, current, css);
    auto oldMediaQueries(std::move(mediaQueries));
    mediaQueries = mergedQueries;
    mediaStack.emplace_back(css);

    if (isInStyleRule()) {
      CssStyleRule* copy = SASS_MEMORY_RESECT(readStyleRule);
      css->addChildAt(copy, false);
      acceptChildrenAt(copy, node->elements());
    }
    else {
      for (auto& child : node->elements()) {
        ValueObj rv = child->accept(this);
      }
    }

    mediaQueries = std::move(oldMediaQueries);

    mediaStack.pop_back();

    return nullptr;
  }

  Value* Eval::acceptChildren(const Vectorized<Statement>& children)
  {
    for (const auto& child : children) {
      ValueObj val = child->accept(this);
      if (val) return val.detach();
    }
    return nullptr;
  }

  Value* Eval::acceptChildrenAt(CssParentNode* parent,
    const Vectorized<Statement>& children)
  {
    RAII_PTR(CssParentNode, current, parent);
    for (const auto& child : children) {
      ValueObj val = child->accept(this);
      if (val) return val.detach();
    }
    return nullptr;
  }


  /// Add parentheses if necessary.
  ///
  /// If [operator] is passed, it's the operator for the surrounding
  /// [SupportsOperation], and is used to determine whether parentheses are
  /// necessary if [condition] is also a [SupportsOperation].
  sass::string Eval::_parenthesize(SupportsCondition* condition) {
    SupportsNegation* negation = condition->isaSupportsNegation();
    SupportsOperation* operation = condition->isaSupportsOperation();
    SupportsAnything* anything = condition->isaSupportsAnything();
    if (negation != nullptr || operation != nullptr || anything != nullptr) {
      return "(" + _visitSupportsCondition(condition) + ")";
    }
    else {
      return _visitSupportsCondition(condition);
    }
  }

  sass::string Eval::_parenthesize(SupportsCondition* condition, SupportsOperation::Operand operand) {
    SupportsNegation* negation = condition->isaSupportsNegation();
    SupportsOperation* operation = condition->isaSupportsOperation();
    if (negation || (operation && operand != operation->operand())) {
      return "(" + _visitSupportsCondition(condition) + ")";
    }
    else {
      return _visitSupportsCondition(condition);
    }
  }

  /// Evaluates [condition] and converts it to a plain CSS string, with
  sass::string Eval::_visitSupportsCondition(SupportsCondition* condition)
  {
    if (SupportsOperation* operation = condition->isaSupportsOperation()) {
      sass::string strm;
      SupportsOperation::Operand operand = operation->operand();
      strm += _parenthesize(operation->left(), operand);
      strm += (operand == SupportsOperation::AND ? " and " : " or ");
      strm += _parenthesize(operation->right(), operand);
      return strm;
    }
    else if (SupportsNegation* negation = condition->isaSupportsNegation()) {
      return "not " + _parenthesize(negation->condition());
    }
    else if (SupportsInterpolation* interpolation = condition->isaSupportsInterpolation()) {
      return toCss(interpolation->value(), false);
    }
    else if (SupportsDeclaration* declaration = condition->isaSupportsDeclaration()) {
      return "(" + toCss(declaration->feature()) + ": "
             + toCss(declaration->value()) + ")";
    }
    else if (SupportsFunction* function = condition->isaSupportsFunction()) {
      return acceptInterpolation(function->name(), false)
        + "(" + acceptInterpolation(function->args(), false) + ")";
    }
    else if (SupportsAnything* anything = condition->isaSupportsAnything()) {
      return "(" + acceptInterpolation(anything->contents(), false) + ")";
    }
    else {
      return Strings::empty;
    }

  }

  /// Adds the values in [map] to [values].
  ///
  /// Throws a [RuntimeException] associated with [nodeForSpan]'s source
  /// span if any [map] keys aren't strings.
  ///
  /// If [convert] is passed, that's used to convert the map values to the value
  /// type for [values]. Otherwise, the [Value]s are used as-is.
  ///
  /// This takes an [AstNode] rather than a [FileSpan] so it can avoid calling
  /// [AstNode.span] if the span isn't required, since some nodes need to do
  /// real work to manufacture a source span.
  void Eval::_addRestValueMap(ValueFlatMap& values, Map* map, const SourceSpan& pstate) {
    // convert ??= (value) = > value as T;

    for(const auto& kv : map->elements()) {
      if (String* str = kv.first->isaString()) {
        values.insert(std::make_pair(str->value(), kv.second));
      }
      else {
        callStackFrame frame(logger, pstate);
        throw Exception::RuntimeException(logger,
          "Variable keyword argument map must have string keys.\n" +
          kv.first->inspect() + " is not a string in " +
          map->inspect() + ".");
      }
    }
  }

  /// Adds the values in [map] to [values].
  void Eval::_addRestExpressionMap(ExpressionFlatMap& values, Map* map, const SourceSpan& pstate) {
    // convert ??= (value) = > value as T;

    for (const auto& kv : map->elements()) {
      if (String* str = kv.first->isaString()) {
        values.insert(std::make_pair(str->value(), SASS_MEMORY_NEW(
          ValueExpression, map->pstate(), kv.second)));
      }
      else {
        callStackFrame frame(logger, pstate);
        throw Exception::RuntimeException(logger,
          "Variable keyword argument map must have string keys.\n" +
          kv.first->inspect() + " is not a string in " +
          map->inspect() + ".");
      }
    }
  }


  CssMediaQueryVector Eval::mergeMediaQueries(
    const CssMediaQueryVector& lhs,
    const CssMediaQueryVector& rhs)
  {
    CssMediaQueryVector queries;
    for (const CssMediaQueryObj& query1 : lhs) {
      for (const CssMediaQueryObj& query2 : rhs) {
        CssMediaQueryObj result(query1->merge(query2));
        if (result && !result->empty()) {
          queries.emplace_back(result);
        }
      }
    }
    return queries;
  }

  Value* Eval::visitDeclaration(Declaration* node)
  {

    if (!isInStyleRule() && !inUnknownAtRule && !inKeyframes) {
      callStackFrame csf(logger, node->pstate());
      throw Exception::RuntimeException(traces,
        "Declarations may only be used within style rules.");
    }

    CssStringObj name = interpolationToCssString(node->name(), true, false);

    bool is_custom_property = node->is_custom_property();

    if (!declarationName.empty()) {
      name->text(declarationName + "-" + name->text());
    }

    ValueObj cssValue;
    if (node->value()) {
      cssValue = node->value()->accept(this);
    }

    // The parent to add declarations too

    // If the value is an empty list, preserve it, because converting it to CSS
    // will throw an error that we want the user to see.
    if (cssValue != nullptr && (!cssValue->isBlank()
      || cssValue->lengthAsList() == 0)) {
      current->append(SASS_MEMORY_NEW(CssDeclaration,
        node->pstate(), name, cssValue, is_custom_property));
    }
    else if (is_custom_property) {
      callStackFrame frame(logger, node->value()->pstate());
      throw Exception::RuntimeException(logger,
        "Custom property values may not be empty.");
    }

    if (!node->empty()) {
      LocalOption<sass::string> ll1(declarationName, name->text());
      for (Statement* child : node->elements()) {
        ValueObj result = child->accept(this);
      }
    }
    return nullptr;
  }

  Value* Eval::visitLoudComment(LoudComment* c)
  {
    if (inFunction) return nullptr;
    sass::string text(acceptInterpolation(c->text(), false));
    bool preserve = text[2] == '!';
    current->append(SASS_MEMORY_NEW(CssComment, c->pstate(), text, preserve));
    return nullptr;
  }

  Value* Eval::visitIfRule(IfRule* i)
  {
    ValueObj rv;
    // Has a condition?
    if (i->predicate()) {
      // Execute the condition statement
      ValueObj condition = i->predicate()->accept(this);
      // If true append all children of this clause
      if (condition->isTruthy()) {
        // Create local variable scope for children
        EnvScope scoped(compiler.varRoot, i->idxs);
        rv = acceptChildren(i);
      }
      else if (i->alternative()) {
        // If condition is falsy, execute else blocks
        rv = visitIfRule(i->alternative());
      }
    }
    else {
      EnvScope scoped(compiler.varRoot, i->idxs);
      rv = acceptChildren(i);
    }
    // Is probably nullptr!?
    return rv.detach();
  }

  // For does not create a new env scope
  // But iteration vars are reset afterwards
  Value* Eval::visitForRule(ForRule* f)
  {
    BackTrace trace(f->pstate(), Strings::forRule);
    EnvScope scoped(compiler.varRoot, f->idxs);
    ValueObj low = f->lower_bound()->accept(this);
    ValueObj high = f->upper_bound()->accept(this);
    NumberObj sass_start = low->assertNumber(logger, "");
    NumberObj sass_end = high->assertNumber(logger, "");
    // Support compatible unit types (e.g. cm to mm)
    sass_end = sass_end->coerce(logger, sass_start);
    // Can only use integer ranges
    sass_start->assertInt(logger);
    sass_end->assertInt(logger);
    // check if units are valid for sequence
    if (sass_start->unit() != sass_end->unit()) {
      callStackFrame csf(logger, f->pstate());
      throw Exception::UnitMismatch(
        logger, sass_start, sass_end);
    }
    double start = sass_start->value();
    double end = sass_end->value();
    // only create iterator once in this environment
    ValueObj val;
    if (start < end) {
      if (f->is_inclusive()) ++end;
      for (double i = start; i < end; ++i) {
        NumberObj it = SASS_MEMORY_NEW(Number,
          low->pstate(), i, sass_end->unit());
        compiler.varRoot.setVariable(
          { f->idxs, 0 }, it.ptr(), false);
        val = acceptChildren(f);
        if (val) break;
      }
    }
    else {
      if (f->is_inclusive()) --end;
      for (double i = start; i > end; --i) {
        NumberObj it = SASS_MEMORY_NEW(Number,
          low->pstate(), i, sass_end->unit());
        compiler.varRoot.setVariable(
          { f->idxs, 0 }, it.ptr(), false);
        val = acceptChildren(f);
        if (val) break;
      }
    }
    return val.detach();
  }

  Value* Eval::visitExtendRule(ExtendRule* e)
  {

    if (!isInStyleRule() /* || !declarationName.empty() */) {
      callStackFrame csf(logger, e->pstate());
      throw Exception::RuntimeException(traces,
        "@extend may only be used within style rules.");
    }

    SelectorListObj slist = interpolationToSelector(
      e->selector(), plainCss, current == nullptr);

    if (slist) {

      for (const auto& complex : slist->elements()) {

        if (complex->size() != 1) {
          callStackFrame csf(logger, complex->pstate());
          throw Exception::RuntimeException(traces,
            "complex selectors may not be extended.");
        }

        if (const CompoundSelector* compound = complex->first()->isaCompoundSelector()) {

          if (compound->size() != 1) {

            sass::sstream sels; bool addComma = false;
            sels << "compound selectors may no longer be extended.\nConsider `@extend ";
            for (const auto& sel : compound->elements()) {
              if (addComma) sels << ", ";
              sels << sel->inspect();
              addComma = true;
            }
            sels << "` instead.\nSee http://bit.ly/ExtendCompound for details.";
            #if SassRestrictCompoundExtending
            callStackFrame csf(logger, compound->pstate());
            throw Exception::RuntimeException(traces, sels.str());
            #else
            logger.addDeprecation(sels.str(), compound->pstate());
            #endif

            // Make this an error once deprecation is over
            for (SimpleSelectorObj simple : compound->elements()) {
              // Pass every selector we ever see to extender (to make them findable for extend)
              if (extender2) extender2->addExtension(selector(), simple, mediaStack.back(), e->is_optional());
            }

          }
          else {
            // Pass every selector we ever see to extender (to make them findable for extend)
            if (extender2) extender2->addExtension(selector(), compound->first(), mediaStack.back(), e->is_optional());
          }

        }
        else {
          callStackFrame csf(logger, complex->pstate());
          throw Exception::RuntimeException(traces,
            "complex selectors may not be extended.");
        }
      }
    }

    return nullptr;
  }

  Value* Eval::visitEachRule(EachRule* e)
  {
    const EnvRefs* vidx(e->idxs);
    const sass::vector<EnvKey>& variables(e->variables());
    EnvScope scoped(compiler.varRoot, e->idxs);
    ValueObj expr = e->expressions()->accept(this);
    if (MapObj map = expr->isaMap()) {
      Map::ordered_map_type els(map->elements());
      for (const auto& kv : els) {
        ValueObj key = kv.first;
        ValueObj value = kv.second;
        if (variables.size() == 1) {
          List* variable = SASS_MEMORY_NEW(List,
            map->pstate(), { key, value }, SASS_SPACE);
          compiler.varRoot.setVariable({ vidx, 0 }, variable, false);
        }
        else {
          value = withoutSlash(value);
          compiler.varRoot.setVariable({ vidx, 0 }, key, false);
          compiler.varRoot.setVariable({ vidx, 1 }, value, false);
        }
        ValueObj val = acceptChildren(e);
        if (val) return val.detach();
      }
      return nullptr;
    }

    ListObj list;
    if (List* slist = expr->isaList()) {
      list = SASS_MEMORY_NEW(List, expr->pstate(),
        slist->elements(), slist->separator());
      list->hasBrackets(slist->hasBrackets());
    }
    else {
      list = SASS_MEMORY_NEW(List, expr->pstate(),
        { expr }, SASS_COMMA);
    }
    for (size_t i = 0, L = list->size(); i < L; ++i) {
      Value* item = list->get(i);
      // check if we got passed a list of args (investigate)
      if (List* scalars = item->isaList()) { // Ex
        if (variables.size() == 1) {
          compiler.varRoot.setVariable({ vidx, 0 }, scalars, false);
        }
        else {
          for (size_t j = 0, K = variables.size(); j < K; ++j) {
            compiler.varRoot.setVariable({ vidx, (uint32_t)j },
              j < scalars->size() ? scalars->get(j)
              : SASS_MEMORY_NEW(Null, expr->pstate()), false);
          }
        }
      }
      else {
        if (variables.size() > 0) {
          compiler.varRoot.setVariable({ vidx, 0 }, item, false);
          for (size_t j = 1, K = variables.size(); j < K; ++j) {
            Value* res = SASS_MEMORY_NEW(Null, expr->pstate());
            compiler.varRoot.setVariable({ vidx, (uint32_t)j }, res, false);
          }
        }
      }
      ValueObj val = acceptChildren(e);
      if (val) return val.detach();
    }

    return nullptr;
  }

  Value* Eval::visitWhileRule(WhileRule* node)
  {

    // First condition runs outside
    EnvScope scoped(compiler.varRoot, node->idxs);
    Expression* condition = node->condition();
    ValueObj result = condition->accept(this);

    // Evaluate the first run in outer scope
    // All successive runs are from inner scope
    if (result->isTruthy()) {

      while (true) {
        result = acceptChildren(node);
        if (result) {
          return result.detach();
        }
        result = condition->accept(this);
        if (!result->isTruthy()) break;
      }

    }

    return nullptr;

  }

  Value* Eval::visitReturnRule(ReturnRule* rule)
  {
    ValueObj result(rule->value()->accept(this));
    return withoutSlash(result);
  }

  Value* Eval::visitSilentComment(SilentComment* c)
  {
    // current->append(c);
    return nullptr;
  }


  CssMediaQueryVector Eval::evalMediaQueries(Interpolation* itpl)
  {
    SourceDataObj synthetic = interpolationToSource(itpl, true);
    MediaQueryParser parser(compiler, synthetic);
    return parser.parse();
  }

  void Eval::acceptStaticImport(StaticImport* rule)
  {
    // Create new CssImport object
    CssImportObj import = SASS_MEMORY_NEW(CssImport, rule->pstate(),
      interpolationToCssString(rule->url(), false, false));
    import->outOfOrder(rule->outOfOrder());

    if (rule->supports()) {
      if (auto supports = rule->supports()->isaSupportsDeclaration()) {
        sass::string feature(toCss(supports->feature()));
        sass::string value(toCss(supports->value()));
        import->supports(SASS_MEMORY_NEW(CssString,
          rule->supports()->pstate(),
          // Should have a CssSupportsCondition?
          // Nope, spaces are even further down
          feature + ": " + value));
      }
      else {
        import->supports(SASS_MEMORY_NEW(CssString, rule->supports()->pstate(),
          _visitSupportsCondition(rule->supports())));
      }
      // Wrap the resulting condition into a `supports()` clause
      import->supports()->text("supports(" + import->supports()->text() + ")");

    }
    if (rule->media()) {
      import->media(evalMediaQueries(rule->media()));
    }
    // append new css import to result
    current->append(import.ptr());

  }

  // Consume all imports in this rule
  Value* Eval::visitImportRule(ImportRule* rule)
  {
    for (const ImportBaseObj& import : rule->elements()) {
      if (StaticImport* stimp = import->isaStaticImport()) { acceptStaticImport(stimp); }
      else if (IncludeImport* stimp = import->isaIncludeImport()) { acceptIncludeImport(stimp); }
      else throw std::runtime_error("undefined behavior");
    }
    return nullptr;
  }

}
