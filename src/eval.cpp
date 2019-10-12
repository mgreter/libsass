// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include <cstdlib>
#include <cmath>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <typeinfo>

#include "file.hpp"
#include "eval.hpp"
#include "ast.hpp"
#include "util.hpp"
#include "inspect.hpp"
#include "operators.hpp"
#include "environment.hpp"
#include "ast_def_macros.hpp"
#include "position.hpp"
#include "sass/values.h"
#include "ast2c.hpp"
#include "c2ast.hpp"
#include "context.hpp"
#include "backtrace.hpp"
// #include "debugger.hpp"
#include "sass_context.hpp"
#include "color_maps.hpp"
#include "sass_functions.hpp"
#include "error_handling.hpp"
#include "util_string.hpp"
#include "dart_helpers.hpp"
#include "parser_selector.hpp"
#include "parser_media_query.hpp"
#include "parser_keyframe_selector.hpp"
#include "strings.hpp"

namespace Sass {


  Eval::Eval(Context& ctx) :
    inMixin(false),
    blockStack(),
    mediaStack(),
    originalStack(),
    selectorStack(),
    _styleRule(),
    _declarationName(),
    _inFunction(false),
    _inUnknownAtRule(false),
    _atRootExcludingStyleRule(false),
    _inKeyframes(false),
    plainCss(false),
    at_root_without_rule(false),
    ctx(ctx),
    traces(*ctx.logger)
  {

    mediaStack.push_back({});
    blockStack.emplace_back(nullptr);
    selectorStack.push_back({});
    originalStack.push_back({});

    bool_true = SASS_MEMORY_NEW(Boolean, SourceSpan::fake("[NA]"), true);
    bool_false = SASS_MEMORY_NEW(Boolean, SourceSpan::fake("[NA]"), false);
  }
  Eval::~Eval() { }

  void debug_call(
    UserDefinedCallable* callable,
    ArgumentResults2& arguments) {

    std::cerr << "calling <<" << callable->name().orig() << ">> with (";

    bool addComma = false;
    for (auto arg : arguments.positional()) {
      if (addComma) std::cerr << ", ";
      std::cerr << arg->to_string();
      addComma = true;
    }

    for (auto arg : arguments.named()) {
      if (addComma) std::cerr << ", ";
      std::cerr << arg.first.orig();
      std::cerr << ": " << arg.second->to_string();
      addComma = true;
    }

    std::cerr << ")\n";
  }

  Value* Eval::_runUserDefinedCallable(
    ArgumentInvocation* arguments,
    UserDefinedCallable* callable,
    UserDefinedCallable* content,
    bool isMixinCall,
    Value* (Eval::* run)(UserDefinedCallable*, Trace*),
    Trace* trace,
    const SourceSpan& pstate)
  {

    auto idxs = callable->declaration()->idxs();
    // auto fidx = callable->declaration()->fidx();

    // On user defined fn we set the variables on the stack
    ArgumentResults2& evaluated = _evaluateArguments(arguments); // , false

    EnvSnapshotView view(ctx.varRoot, callable->snapshot());

    EnvScope scoped(ctx.varRoot, idxs);

    if (content) {
      auto cidx = content->declaration()->cidx();
      if (cidx.isValid()) {
        ctx.varRoot.setMixin(cidx, content);
      }
      else {
        std::cerr << "Invalid cidx1 on " << content << "\n";
      }
    }
    // debug_call(callable, evaluated);

    KeywordMap<ValueObj>& named = evaluated.named();
    sass::vector<ValueObj>& positional = evaluated.positional();
    CallableDeclaration* declaration = callable->declaration();
    ArgumentDeclaration* declaredArguments = declaration->arguments();
    if (!declaredArguments) throw std::runtime_error("Mixin declaration has no arguments");
    const sass::vector<ArgumentObj>& declared = declaredArguments->arguments();

    // Create a new scope from the callable, outside variables are not visible?
    if (declaredArguments) declaredArguments->verify(positional.size(), named, pstate, traces);
    size_t minLength = std::min(positional.size(), declared.size());

    for (size_t i = 0; i < minLength; i++) {
      ctx.varRoot.setVariable(idxs->vidxs.frame, i,
        positional[i]->withoutSlash());
    }

    size_t i;
    ValueObj value;
    for (i = positional.size(); i < declared.size(); i++) {
      Argument* argument = declared[i];
      auto& name(argument->name());
      if (named.count(name) == 1) {
        value = named[name]->perform(this);
        named.erase(name);
      }
      else {
        // Use the default arguments
        value = argument->value()->perform(this);
      }
      auto result = value->withoutSlash();
      ctx.varRoot.setVariable(idxs->vidxs.frame, i, result);

      // callenv.set_local(
      //   argument->name(),
      //   result);
    }

    bool isNamedEmpty = named.empty();
    SassArgumentListObj argumentList;
    if (!declaredArguments->restArg().empty()) {
      sass::vector<ValueObj> values;
      if (positional.size() > declared.size()) {
        values = sublist(positional, declared.size());
      }
      Sass_Separator separator = evaluated.separator();
      if (separator == SASS_UNDEF) separator = SASS_COMMA;
      argumentList = SASS_MEMORY_NEW(SassArgumentList,
        pstate, std::move(values), separator, std::move(named));
      auto size = declared.size();
      ctx.varRoot.setVariable(idxs->vidxs.frame, size, argumentList);
      // callenv.set_local(declaredArguments->restArg(), argumentList);
    }

    // sass::sstream invocation;
    // ArgumentInvocationObj args =
    //   visitArgumentInvocation(arguments);
    // invocation << callable->name();
    // invocation << args->toString();
    // callStackFrame frame(*ctx.logger,
    //   Backtrace(pstate, callable->name()));

    ValueObj result = (this->*run)(callable, trace);

    if (isNamedEmpty) return result.detach();
    if (argumentList == nullptr) return result.detach();
    if (argumentList->wereKeywordsAccessed()) return result.detach();

    throw Exception::SassScriptException("Nonono");

  }

  Value* Eval::_runBuiltInCallable(
    ArgumentInvocation* arguments,
    BuiltInCallable* callable,
    const SourceSpan& pstate)
  {
    // On builtin we pass it to the function (has only positional args)
    ArgumentResults2& evaluated = _evaluateArguments(arguments); // 34% // , false
    KeywordMap<ValueObj>& named(evaluated.named());
    sass::vector<ValueObj>& positional(evaluated.positional());
    const SassFnPair& tuple(callable->callbackFor(positional.size(), named)); // 4.7%

    ArgumentDeclaration* overload = tuple.first;
    const SassFnSig& callback = tuple.second;
    const sass::vector<ArgumentObj>& declaredArguments(overload->arguments());

    overload->verify(positional.size(), named, pstate, *ctx.logger); // 7.5%

    for (size_t i = positional.size();
      i < declaredArguments.size();
      i++) {
      Argument* argument = declaredArguments[i];
      const auto& name(argument->name());
      if (named.count(name) == 1) {
        positional.emplace_back(named[name]->perform(this));
        named.erase(name); // consume arguments once
      }
      else {
        positional.emplace_back(argument->value()->perform(this));
      }
    }

    bool isNamedEmpty = named.empty();
    SassArgumentListObj argumentList;
    if (!overload->restArg().empty()) {
      sass::vector<ValueObj> rest;
      if (positional.size() > declaredArguments.size()) {
        rest = sublist(positional, declaredArguments.size());
        removeRange(positional, declaredArguments.size(), positional.size());
      }

      Sass_Separator separator = evaluated.separator();
      if (separator == SASS_UNDEF) separator = SASS_COMMA;
      argumentList = SASS_MEMORY_NEW(SassArgumentList,
        pstate, std::move(rest), separator, std::move(named));
      positional.emplace_back(argumentList);
    }

    ValueObj result;
    // try {
//    double epsilon = ctx.logger->epsilon;
    // This one
    // sass::sstream invocation;
    // ArgumentInvocationObj args =
    //   visitArgumentInvocation(arguments);
    // invocation << callable->name();
    // invocation << args->toString();
    result = callback(pstate, positional, ctx, *this); // 13%
      // }

    if (argumentList == nullptr) return result.detach();
    if (isNamedEmpty) return result.detach();
    /* if (argumentList.wereKeywordsAccessed) */ return result.detach();
    sass::sstream strm;
    strm << "No " << pluralize("argument", named.size());
    strm << " named " << toSentence(named, "or") << ".";
    throw Exception::SassRuntimeException2(
      strm.str(), *ctx.logger, pstate);
  }



  Value* Eval::_runBuiltInCallables(
    ArgumentInvocation* arguments,
    BuiltInCallables* callable,
    const SourceSpan& pstate)
  {
    ArgumentResults2& evaluated = _evaluateArguments(arguments); // 34% // , false
    KeywordMap<ValueObj>& named(evaluated.named());
    sass::vector<ValueObj>& positional(evaluated.positional());
    const SassFnPair& tuple(callable->callbackFor(positional.size(), named)); // 4.7%

    ArgumentDeclaration* overload = tuple.first;
    const SassFnSig& callback = tuple.second;
    const sass::vector<ArgumentObj>& declaredArguments(overload->arguments());

    overload->verify(positional.size(), named, pstate, *ctx.logger); // 7.5%

    for (size_t i = positional.size();
      i < declaredArguments.size();
      i++) {
      Argument* argument = declaredArguments[i];
      const auto& name(argument->name());
      if (named.count(name) == 1) {
        positional.emplace_back(named[name]->perform(this));
        named.erase(name); // consume arguments once
      }
      else {
        positional.emplace_back(argument->value()->perform(this));
      }
    }

    bool isNamedEmpty = named.empty();
    SassArgumentListObj argumentList;
    if (!overload->restArg().empty()) {
      sass::vector<ValueObj> rest;
      if (positional.size() > declaredArguments.size()) {
        rest = sublist(positional, declaredArguments.size());
        removeRange(positional, declaredArguments.size(), positional.size());
      }

      Sass_Separator separator = evaluated.separator();
      if (separator == SASS_UNDEF) separator = SASS_COMMA;
      argumentList = SASS_MEMORY_NEW(SassArgumentList,
        pstate, std::move(rest), separator, std::move(named));
      positional.emplace_back(argumentList);
    }

    ValueObj result;
    // try {
//    double epsilon = ctx.logger->epsilon;
    // This one
    // sass::sstream invocation;
    // ArgumentInvocationObj args =
    //   visitArgumentInvocation(arguments);
    // invocation << callable->name();
    // invocation << args->toString();
    result = callback(pstate, positional, ctx, *this); // 13%
      // }

    if (argumentList == nullptr) return result.detach();
    if (isNamedEmpty) return result.detach();
    /* if (argumentList.wereKeywordsAccessed) */ return result.detach();
    sass::sstream strm;
    strm << "No " << pluralize("argument", named.size());
    strm << " named " << toSentence(named, "or") << ".";
    throw Exception::SassRuntimeException2(
      strm.str(), *ctx.logger, pstate);
  }









  Value* Eval::_runExternalCallable(
    ArgumentInvocation* arguments,
    ExternalCallable* callable,
    const SourceSpan& pstate)
  {
    ArgumentResults2& evaluated = _evaluateArguments(arguments); // , false
    KeywordMap<ValueObj>& named = evaluated.named();
    sass::vector<ValueObj>& positional = evaluated.positional();
    ArgumentDeclaration* overload = callable->declaration();

    sass::string name(callable->name());

//    return SASS_MEMORY_NEW(String_Constant, "[asd]", "Hossa");

    overload->verify(positional.size(), named, pstate, traces);

    const sass::vector<ArgumentObj>& declaredArguments = overload->arguments();

    for (size_t i = positional.size();
      i < declaredArguments.size();
      i++) {
      Argument* argument = declaredArguments[i];
      const auto& name(argument->name());
      if (named.count(name) == 1) {
        positional.emplace_back(named[name]->perform(this));
        named.erase(name); // consume arguments once
      }
      else {
        positional.emplace_back(argument->value()->perform(this));
      }
    }

    bool isNamedEmpty = named.empty();
    SassArgumentListObj argumentList;
    if (!overload->restArg().empty()) {
      sass::vector<ValueObj> rest;
      if (positional.size() > declaredArguments.size()) {
        rest = sublist(positional, declaredArguments.size());
        removeRange(positional, declaredArguments.size(), positional.size());
      }

      Sass_Separator separator = evaluated.separator();
      if (separator == SASS_UNDEF) separator = SASS_COMMA;
      argumentList = SASS_MEMORY_NEW(SassArgumentList,
        SourceSpan::fake("[pstate5]"),
        std::move(rest), separator, std::move(named));
      positional.emplace_back(argumentList);
    }

    ValueObj result;
    // try {
    // double epsilon = std::pow(0.1, ctx.c_options.precision + 1);

    Sass_Function_Entry entry = callable->function();

    AST2C ast2c;
    union Sass_Value* c_args = sass_make_list(positional.size(), SASS_COMMA, false);
    for (size_t i = 0; i < positional.size(); i++) {
      sass_list_set_value(c_args, i, positional[i]->perform(&ast2c));
    }

    union Sass_Value* c_val =
      entry->function(c_args, entry, ctx.c_compiler);

    if (sass_value_get_tag(c_val) == SASS_ERROR) {
      sass::string message("error in C function " + name + ": " + sass_error_get_message(c_val));
      sass_delete_value(c_val);
      sass_delete_value(c_args);
      error(message, pstate, traces);
    }
    else if (sass_value_get_tag(c_val) == SASS_WARNING) {
      sass::string message("warning in C function " + name + ": " + sass_warning_get_message(c_val));
      sass_delete_value(c_val);
      sass_delete_value(c_args);
      error(message, pstate, traces);
    }
    result = c2ast(c_val, traces, pstate);
    sass_delete_value(c_val);
    sass_delete_value(c_args);
    if (argumentList == nullptr) return result.detach();
    if (isNamedEmpty) return result.detach();
    /* if (argumentList.wereKeywordsAccessed) */ return result.detach();
    // sass::sstream strm;
    // strm << "No " << pluralize("argument", named.size());
    // strm << " named " << toSentence(named, "or") << ".";
    // throw Exception::SassRuntimeException(strm.str(), pstate);
  }

  sass::string Eval::serialize(AST_Node* node)
  {
    Sass_Inspect_Options serializeOpt(ctx.c_options);
    serializeOpt.output_style = TO_CSS;
    Sass_Output_Options out(serializeOpt);
    Emitter emitter(out);
    Inspect serialize(emitter);
    serialize.in_declaration = true;
    serialize.quotes = false;
    node->perform(&serialize);
    return serialize.get_buffer();
  }

  const sass::string Eval::cwd()
  {
    return ctx.cwd();
  }

  struct Sass_Inspect_Options& Eval::options()
  {
    return ctx.c_options;
  }

  struct Sass_Compiler* Eval::compiler()
  {
    return ctx.c_compiler;
  }

  std::pair<
    sass::vector<ExpressionObj>,
    KeywordMap<ExpressionObj>
  > Eval::_evaluateMacroArguments(
    CallableInvocation& invocation
  )
  {

    if (invocation.arguments()->restArg() == nullptr) {
      return std::make_pair(
        invocation.arguments()->positional(),
        invocation.arguments()->named());
    }

    ArgumentInvocation* arguments = invocation.arguments();
    // var positional = invocation.arguments.positional.toList();
    sass::vector<ExpressionObj> positional = arguments->positional();
    // var named = normalizedMap(invocation.arguments.named);
    // ToDO: why is this chancged?
    KeywordMap<ExpressionObj> named = arguments->named();
    // var rest = invocation.arguments.rest.accept(this);
    ValueObj rest = arguments->restArg()->perform(this);

    if (SassMap* restMap = Cast<SassMap>(rest)) {
      _addRestMap2(named, restMap, arguments->restArg()->pstate());
    }
    else if (SassList * restList = Cast<SassList>(rest)) {
      const sass::vector<ValueObj>& values = restList->asVector();
      for (Value* value : values) {
        positional.emplace_back(SASS_MEMORY_NEW(
          ValueExpression, value->pstate(), value));
      }
      // separator = list->separator();
      if (SassArgumentList * args = Cast<SassArgumentList>(rest)) {
        for (auto kv : args->keywords()) {
          named[kv.first] = SASS_MEMORY_NEW(ValueExpression,
            kv.second->pstate(), kv.second);
        }
      }
    }
    else {
      positional.emplace_back(SASS_MEMORY_NEW(
        ValueExpression, rest->pstate(), rest));
    }

    if (arguments->kwdRest() == nullptr) {
      return std::make_pair(
        std::move(positional),
        std::move(named));
    }

    auto keywordRest = arguments->kwdRest()->perform(this);

    if (Map * restMap = Cast<Map>(keywordRest)) {
      _addRestMap2(named, restMap, arguments->kwdRest()->pstate());
      return std::make_pair(positional, named);
    }

    throw Exception::SassRuntimeException2(
      "Variable keyword arguments must be a map (was $keywordRest).",
      *ctx.logger, keywordRest->pstate());

  }

  sass::vector<Sass_Callee>& Eval::callee_stack()
  {
    return ctx.callee_stack;
  }

  Value* Eval::visitBlock(Block* node)
  {
    BlockObj bb = visitRootBlock99(node);
    blockStack.back()->append(bb);
    return nullptr;
  }

  Value* Eval::operator()(Block* b)
  {
    ValueObj val;
    for (const auto& item : b->elements()) {
      val = item->perform(this);
      if (val) return val.detach();
    }
    return val.detach();
  }

  Value* Eval::operator()(Return* r)
  {
    return Cast<Value>(r->value()->perform(this));
  }

  void Eval::visitWarnRule(WarnRule* node)
  {
    Sass_Output_Style outstyle = options().output_style;
    options().output_style = NESTED;
    Expression_Obj message = node->expression()->perform(this);


    if (ctx.varRoot.hasGlobalFunction33(Keys::warnRule)) {

      IdxRef fidx = ctx.varRoot.getFunctionIdx2(Keys::warnRule);
      ExternalCallable* def = Cast<ExternalCallable>(ctx.varRoot.getFunction(fidx));
      Sass_Function_Entry c_function = def->function();
      Sass_Function_Fn c_func = sass_function_get_function(c_function);
      EnvScope scoped(ctx.varRoot, def->idxs());

      AST2C ast2c;
      union Sass_Value* c_args = sass_make_list(1, SASS_COMMA, false);
      sass_list_set_value(c_args, 0, message->perform(&ast2c));
      union Sass_Value* c_val = c_func(c_args, c_function, compiler());
      options().output_style = outstyle;
      // callee_stack().pop_back();
      sass_delete_value(c_args);
      sass_delete_value(c_val);

    }

    // try to use generic function
    /*if (env->has(kwdWarnFn)) {

      // add call stack entry
      callee_stack().push_back({
        "@warn",
        node->pstate().getPath(),
        node->pstate().getLine(),
        node->pstate().getColumn(),
        SASS_CALLEE_FUNCTION,
        { env }
        });

      ExternalCallable* def = Cast<ExternalCallable>((*env)[kwdWarnFn]);
      Sass_Function_Entry c_function = def->function();
      Sass_Function_Fn c_func = sass_function_get_function(c_function);

      AST2C ast2c;
      union Sass_Value* c_args = sass_make_list(1, SASS_COMMA, false);
      sass_list_set_value(c_args, 0, message->perform(&ast2c));
      union Sass_Value* c_val = c_func(c_args, c_function, compiler());
      options().output_style = outstyle;
      callee_stack().pop_back();
      sass_delete_value(c_args);
      sass_delete_value(c_val);

    }
    */
    else {

      sass::string result(unquote(message->to_sass()));
      std::cerr << "WARNING: " << result << std::endl;
      traces.emplace_back(Backtrace(node->pstate()));
      std::cerr << traces_to_string(traces, "    ", 0);
      std::cerr << std::endl;
      options().output_style = outstyle;
      traces.pop_back();

    }

  }

  Value* Eval::operator()(WarnRule* node)
  {
    visitWarnRule(node);
    return nullptr;
  }

  void Eval::visitErrorRule(ErrorRule* node)
  {
    Sass_Output_Style outstyle = options().output_style;
    options().output_style = NESTED;
    Expression_Obj message = node->expression()->perform(this);

    if (ctx.varRoot.hasGlobalFunction33(Keys::errorRule)) {

      IdxRef fidx = ctx.varRoot.getFunctionIdx2(Keys::errorRule);
      ExternalCallable* def = Cast<ExternalCallable>(ctx.varRoot.getFunction(fidx));
      Sass_Function_Entry c_function = def->function();
      Sass_Function_Fn c_func = sass_function_get_function(c_function);
      EnvScope scoped(ctx.varRoot, def->idxs());

      AST2C ast2c;
      union Sass_Value* c_args = sass_make_list(1, SASS_COMMA, false);
      sass_list_set_value(c_args, 0, message->perform(&ast2c));
      union Sass_Value* c_val = c_func(c_args, c_function, compiler());
      options().output_style = outstyle;
      // callee_stack().pop_back();
      sass_delete_value(c_args);
      sass_delete_value(c_val);

    }

    // try to use generic function
    /* if (env->has(kwdErrorFn)) {

      // add call stack entry
      callee_stack().push_back({
        "@error",
        node->pstate().getPath(),
        node->pstate().getLine(),
        node->pstate().getColumn(),
        SASS_CALLEE_FUNCTION,
        { env }
        });

      ExternalCallable* def = Cast<ExternalCallable>((*env)[kwdErrorFn]);
      Sass_Function_Entry c_function = def->function();
      Sass_Function_Fn c_func = sass_function_get_function(c_function);

      AST2C ast2c;
      union Sass_Value* c_args = sass_make_list(1, SASS_COMMA, false);
      sass_list_set_value(c_args, 0, message->perform(&ast2c));
      union Sass_Value* c_val = c_func(c_args, c_function, compiler());
      options().output_style = outstyle;
      callee_stack().pop_back();
      sass_delete_value(c_args);
      sass_delete_value(c_val);

    }
    */
    else { 

      sass::string result(message->to_sass());
      options().output_style = outstyle;
      error(result, node->pstate(), traces);

    }

  }

  Value* Eval::operator()(ErrorRule* node)
  {
    visitErrorRule(node);
    return nullptr;
  }

  void Eval::visitDebugRule(DebugRule* node)
  {
    Sass_Output_Style outstyle = options().output_style;
    options().output_style = NESTED;
    Expression_Obj message = node->expression()->perform(this);

    if (ctx.varRoot.hasGlobalFunction33(Keys::debugRule)) {

      IdxRef fidx = ctx.varRoot.getFunctionIdx2(Keys::debugRule);
      ExternalCallable* def = Cast<ExternalCallable>(ctx.varRoot.getFunction(fidx));
      Sass_Function_Entry c_function = def->function();
      Sass_Function_Fn c_func = sass_function_get_function(c_function);
      EnvScope scoped(ctx.varRoot, def->idxs());

      AST2C ast2c;
      union Sass_Value* c_args = sass_make_list(1, SASS_COMMA, false);
      sass_list_set_value(c_args, 0, message->perform(&ast2c));
      union Sass_Value* c_val = c_func(c_args, c_function, compiler());
      options().output_style = outstyle;
      // callee_stack().pop_back();
      sass_delete_value(c_args);
      sass_delete_value(c_val);

    }

    // try to use generic function
    /* if (env->has(kwdDebugFn)) {

      // add call stack entry
      callee_stack().push_back({
        "@debug",
        node->pstate().getPath(),
        node->pstate().getLine(),
        node->pstate().getColumn(),
        SASS_CALLEE_FUNCTION,
        { env }
        });

      ExternalCallable* def = Cast<ExternalCallable>((*env)[kwdDebugFn]);
      // Block_Obj          body   = def->block();
      // Native_Function func   = def->native_function();
      Sass_Function_Entry c_function = def->function();
      Sass_Function_Fn c_func = sass_function_get_function(c_function);

      AST2C ast2c;
      union Sass_Value* c_args = sass_make_list(1, SASS_COMMA, false);
      sass_list_set_value(c_args, 0, message->perform(&ast2c));
      union Sass_Value* c_val = c_func(c_args, c_function, compiler());
      options().output_style = outstyle;
      callee_stack().pop_back();
      sass_delete_value(c_args);
      sass_delete_value(c_val);

    }
     */
    else {

      sass::string result(unquote(message->to_sass()));
      sass::string abs_path(Sass::File::rel2abs(node->pstate().getPath(), cwd(), cwd()));
      sass::string rel_path(Sass::File::abs2rel(node->pstate().getPath(), cwd(), cwd()));
      sass::string output_path(Sass::File::path_for_console(rel_path, abs_path, node->pstate().getPath()));
      options().output_style = outstyle;

      std::cerr << output_path << ":" << node->pstate().getLine() << " DEBUG: " << result;
      std::cerr << std::endl;

    }

  }

  Value* Eval::operator()(DebugRule* node)
  {
    visitDebugRule(node);
    return nullptr;
  }

  SassList* Eval::operator()(ListExpression* l)
  {
    // debug_ast(l, "ListExp IN: ");
    // regular case for unevaluated lists
    SassListObj ll = SASS_MEMORY_NEW(SassList,
      l->pstate(), sass::vector<ValueObj>(), l->separator());
    ll->hasBrackets(l->hasBrackets());
    for (size_t i = 0, L = l->size(); i < L; ++i) {
      ll->append(l->get(i)->perform(this));
    }
    // debug_ast(ll, "ListExp OF: ");
    return ll.detach();
  }

  Value* Eval::operator()(ValueExpression* node)
  {
    // We have a bug lurcking here
    // without detach it gets deleted?
    ValueObj value = node->value();
    return value.detach();
  }

  Map* Eval::operator()(MapExpression* m)
  {
    const sass::vector<ExpressionObj>& kvlist = m->kvlist();
    MapObj map = SASS_MEMORY_NEW(Map, m->pstate());
    // map->reserve(kvlist.size() / 2);
    for (size_t i = 0, L = kvlist.size(); i < L; i += 2)
    {
      ExpressionObj key = kvlist[i + 0]->perform(this);
      ExpressionObj val = kvlist[i + 1]->perform(this);
      if (map->has(key)) {
        traces.emplace_back(Backtrace(kvlist[i + 0]->pstate()));
        throw Exception::DuplicateKeyError(traces, *map, *key);
      }
      map->insert(key, val);
    }

    return map.detach();
  }

  SassMap* Eval::operator()(SassMap* m)
  {
    return m;
  }

  SassList* Eval::operator()(SassList* m)
  {
    return m;
  }

  Value* Eval::operator()(ParenthesizedExpression* ex)
  {
    // return ex->expression();
    if (ex->expression()) {
      return ex->expression()->perform(this);
    }
    return nullptr;
  }

  Value* doDivision(Value* left, Value* right,
    bool allowSlash, Logger& logger, SourceSpan pstate)
  {
    ValueObj result = left->dividedBy(right, logger, pstate);
    SassNumber* rv = Cast<SassNumber>(result);
    SassNumber* lnr = Cast<SassNumber>(left);
    SassNumber* rnr = Cast<SassNumber>(right);
    if (rv) {
      rv->lhsAsSlash({}); rv->lhsAsSlash({});
      if (allowSlash && left && right) {
        rv->lhsAsSlash(lnr); rv->rhsAsSlash(rnr);
      }
    }
    return result.detach();
  }

  Value* Eval::operator()(Binary_Expression* node)
  {
    ValueObj left, right;
    Expression* lhs = node->left();
    Expression* rhs = node->right();
    left = lhs->perform(this);
    switch (node->optype()) {
    case Sass_OP::IESEQ:
      right = rhs->perform(this);
      return left->singleEquals(
        right, *ctx.logger, node->pstate());
    case Sass_OP::OR:
      if (left->isTruthy()) {
        return left.detach();
      }
      return rhs->perform(this);
    case Sass_OP::AND:
      if (!left->isTruthy()) {
        return left.detach();
      }
      return rhs->perform(this);
    case Sass_OP::EQ:
      right = rhs->perform(this);
      return ObjEqualityFn(left, right)
        ? bool_true : bool_false;
    case Sass_OP::NEQ:
      right = rhs->perform(this);
      return ObjEqualityFn(left, right)
        ? bool_false : bool_true;
    case Sass_OP::GT:
      right = rhs->perform(this);
      return left->greaterThan(right, *ctx.logger, node->pstate())
        ? bool_true : bool_false;
    case Sass_OP::GTE:
      right = rhs->perform(this);
      return left->greaterThanOrEquals(right, *ctx.logger, node->pstate())
        ? bool_true : bool_false;
    case Sass_OP::LT:
      right = rhs->perform(this);
      return left->lessThan(right, *ctx.logger, node->pstate())
        ? bool_true : bool_false;
    case Sass_OP::LTE:
      right = rhs->perform(this);
      return left->lessThanOrEquals(right, *ctx.logger, node->pstate())
        ? bool_true : bool_false;
    case Sass_OP::ADD:
      right = rhs->perform(this);
      return left->plus(right, *ctx.logger, node->pstate());
    case Sass_OP::SUB:
      right = rhs->perform(this);
      return left->minus(right, *ctx.logger, node->pstate());
    case Sass_OP::MUL:
      right = rhs->perform(this);
      return left->times(right, *ctx.logger, node->pstate());
    case Sass_OP::DIV:
      right = rhs->perform(this);
      return doDivision(left, right,
        node->allowsSlash(), *ctx.logger, node->pstate());
    case Sass_OP::MOD:
      right = rhs->perform(this);
      return left->modulo(right, *ctx.logger, node->pstate());
    }
    // Satisfy compiler
    return nullptr;
  }

  Value* Eval::operator()(Unary_Expression* node)
  {
    ValueObj operand = node->operand()->perform(this);
    switch (node->optype()) {
    case Unary_Expression::Type::PLUS:
      return operand->unaryPlus(*ctx.logger, node->pstate());
    case Unary_Expression::Type::MINUS:
      return operand->unaryMinus(*ctx.logger, node->pstate());
    case Unary_Expression::Type::NOT:
      return operand->unaryNot(*ctx.logger, node->pstate());
    case Unary_Expression::Type::SLASH:
      return operand->unaryDivide(*ctx.logger, node->pstate());
    }
    // Satisfy compiler
    return nullptr;
  }

  /// Like `_environment.getFunction`, but also returns built-in
  /// globally-avaialble functions.
  Callable* Eval::_getFunction(
    const IdxRef& fidx,
    const sass::string& name,
    const sass::string& ns)
  {
    if (fidx.isValid()) { return ctx.varRoot.getFunction(fidx); }
    IdxRef idx = ctx.varRoot.getLexicalFunction33(name);
    if (idx.isValid()) { return ctx.varRoot.getFunction(idx); }
    return nullptr;

    // auto external = ctx.functions.find(name);
    // if (external != ctx.functions.end()) {
    //   return external->second;
    // }
  }

  /// Like `_environment.getFunction`, but also returns built-in
  /// globally-avaialble functions.
  UserDefinedCallable* Eval::_getMixin(
    const IdxRef& fidx,
    const EnvString& name,
    const sass::string& ns)
  {
    if (fidx.isValid()) { return ctx.varRoot.getMixin(fidx); }
    IdxRef idx(ctx.varRoot.getLexicalMixin33(name));
    if (idx.isValid()) { return ctx.varRoot.getMixin(idx); }
    return nullptr;

    // auto external = ctx.functions.find(name);
    // if (external != ctx.functions.end()) {
    //   return external->second;
    // }
  }

  Value* Eval::_runWithBlock(UserDefinedCallable* callable, Trace* trace)
  {
    CallableDeclaration* declaration = callable->declaration();
    for (Statement* statement : declaration->block()->elements()) {
      ValueObj result = statement->perform(this);
    }
    return nullptr;
  }

  Value* BuiltInCallable::execute(Eval& eval, ArgumentInvocation* arguments, const SourceSpan& pstate)
  {
    auto& traces = eval.ctx.logger->callStack;
    if (!traces.empty() && traces.back().name == "call") {
      traces.back().name = name().orig();
      ValueObj rv = eval._runBuiltInCallable(
        arguments, this, pstate);
      rv = rv->withoutSlash();
      traces.back().name = "call";
      return rv.detach();
    }
    else {
#ifndef GOFAST
      callStackFrame frame(*eval.ctx.logger,
        Backtrace(pstate, name().orig(), true));
#endif
      ValueObj rv = eval._runBuiltInCallable(
        arguments, this, pstate);
      rv = rv->withoutSlash();
      return rv.detach();
    }
  }

  Value* BuiltInCallables::execute(Eval& eval, ArgumentInvocation* arguments, const SourceSpan& pstate)
  {
    auto& traces = eval.ctx.logger->callStack;
    if (!traces.empty() && traces.back().name == "call") {
      traces.back().name = name().orig();
      ValueObj rv = eval._runBuiltInCallables(
        arguments, this, pstate);
      rv = rv->withoutSlash();
      traces.back().name = "call";
      return rv.detach();
  }
    else {
#ifndef GOFAST
      callStackFrame frame(*eval.ctx.logger,
        Backtrace(pstate, name().orig(), true));
#endif
      ValueObj rv = eval._runBuiltInCallables(
        arguments, this, pstate);
      rv = rv->withoutSlash();
      return rv.detach();
    }
  }

  Value* UserDefinedCallable::execute(Eval& eval, ArgumentInvocation* arguments, const SourceSpan& pstate)
  {
    LocalOption<bool> inMixin(eval.inMixin, false);
#ifndef GOFAST
    callStackFrame frame(*eval.ctx.logger,
      Backtrace(pstate, name().orig(), true));
#endif
    ValueObj rv = eval._runUserDefinedCallable(
      arguments, this, nullptr, false, &Eval::_runAndCheck, nullptr, pstate);
    rv = rv->withoutSlash();
    return rv.detach();
  }

  Value* ExternalCallable::execute(Eval& eval, ArgumentInvocation* arguments, const SourceSpan& pstate)
  {
    ValueObj rv = eval._runExternalCallable(
      arguments, this, pstate);
    rv = rv->withoutSlash();
    return rv.detach();
  }

  Value* PlainCssCallable::execute(Eval& eval, ArgumentInvocation* arguments, const SourceSpan& pstate)
  {
    if (!arguments->named().empty() || arguments->kwdRest() != nullptr) {
      throw Exception::SassRuntimeException2(
        "Plain CSS functions don't support keyword arguments.",
        *eval.ctx.logger, pstate);
    }
    bool addComma = false;
    sass::sstream strm;
    strm << name() << "(";
    for (Expression* argument : arguments->positional()) {
      if (addComma) { strm << ", "; }
      else { addComma = true; }
      strm << eval._evaluateToCss(argument);
    }
    if (ExpressionObj rest = arguments->restArg()) {
      rest = rest->perform(&eval);
      if (addComma) { strm << ", "; }
      else { addComma = true; }
      strm << eval._serialize(rest);
    }
    strm << ")";
    return SASS_MEMORY_NEW(
      String_Constant, pstate,
      strm.str());
  }
    

  Value* Eval::_runFunctionCallable(
    ArgumentInvocation* arguments,
    Callable* callable,
    const SourceSpan& pstate)
  {
    return callable->execute(*this, arguments, pstate);
  }

  Value* Eval::operator()(IfExpression* node)
  {
    // return visitIfExpression(node);
    auto pair = _evaluateMacroArguments(*node);
    const sass::vector<ExpressionObj>& positional = pair.first;
    const KeywordMap<ExpressionObj>& named = pair.second;
    // Dart sass has static declaration for IfExpression    
    // node->declaration()->verify(positional, named);
    // We might fail if named arguments are missing or too few passed
    ExpressionObj condition = positional.size() > 0 ? positional[0] : named.at(Keys::$condition);
    ExpressionObj ifTrue = positional.size() > 1 ? positional[1] : named.at(Keys::$ifTrue);
    ExpressionObj ifFalse = positional.size() > 2 ? positional[2] : named.at(Keys::$ifFalse);
    ValueObj rv = condition ? condition->perform(this) : nullptr;
    ExpressionObj ex = rv && rv->isTruthy() ? ifTrue : ifFalse;
    return ex->perform(this);
  }

  Value* Eval::operator()(FunctionExpression* node)
  {
    const sass::string& plainName(node->name()->getPlainString());

    // Function Expression might be simple and static, or dynamic css call
    CallableObj function = _getFunction(node->fidx(), plainName, node->ns());

    if (function == nullptr) {
      function = SASS_MEMORY_NEW(PlainCssCallable,
        node->pstate(),
        performInterpolation(node->name(), false));
      // debug_ast(function);
    }

    LOCAL_FLAG(_inFunction, true);
    ValueObj value = _runFunctionCallable(
      node->arguments(), function, node->pstate());
    // std::cerr << "FN " << (void*)function << "\n";
    // std::cerr << "VAL " << (void*)value << "\n";
    // function->setDbg(true);
    // value->setDbg(true);
    return value.detach();

  }

  Value* Eval::operator()(Variable* v)
  {
    IdxRef vidx = v->vidx();
    if (vidx.isValid()) {
      // std::cerr << "Get var " << vidx.frame << ":" << vidx.offset << "\n";
      Expression* ex = ctx.varRoot.getVariable(vidx);
      Value* value = ex->perform(this);
      return value->withoutSlash();
    }

    // std::cerr << "Old school variable\n";

    ExpressionObj ex = ctx.varRoot.getLexicalVariable44(v->name());

    if (ex.isNull()) {
      error("Undefined variable.",
        v->pstate(), traces);
    }

    Value* value = ex->perform(this);
    return value->withoutSlash();
  }

  Value* Eval::operator()(Color_RGBA* c)
  {
    return c;
  }

  Value* Eval::operator()(Color_HSLA* c)
  {
    return c;
  }

  Value* Eval::operator()(Number* n)
  {
    return n;
  }

  Value* Eval::operator()(Boolean* b)
  {
    return b;
  }

  Interpolation* Eval::evalInterpolation(InterpolationObj interpolation, bool warnForColor)
  {
    sass::vector<ExpressionObj> results;
    results.reserve(interpolation->length());
    InterpolationObj rv = SASS_MEMORY_NEW(
      Interpolation, interpolation->pstate());
    for (Expression* value : interpolation->elements()) {
      ValueObj result = value->perform(this);
      if (Cast<Null>(result)) continue;
      if (result == nullptr) continue;
      results.emplace_back(result);
    }
    rv->elements(results);
    return rv.detach();
  }

  /// Evaluates [interpolation].
  ///
  /// If [warnForColor] is `true`, this will emit a warning for any named color
  /// values passed into the interpolation.
  sass::string Eval::performInterpolation(InterpolationObj interpolation, bool warnForColor)
  {
    sass::vector<Mapping> mappings;
    SourceSpan pstate(interpolation->pstate());
    mappings.emplace_back(Mapping(pstate.getSrcId(), pstate.position, Offset()));
    interpolation = evalInterpolation(interpolation, warnForColor);
    sass::string css(interpolation->to_css(mappings));
    return css;

    sass::vector<sass::string> results;
    for (auto value : interpolation->elements()) {

      ValueObj result = value->perform(this);
      if (Cast<Null>(result)) continue;
      if (result == nullptr) continue;

      /*
      if (warnForColor &&
        result is SassColor &&
        namesByColor.containsKey(result)) {
        var alternative = BinaryOperationExpression(
          BinaryOperator.plus,
          StringExpression(Interpolation([""], null), quotes: true),
          expression);
        _warn(
          "You probably don't mean to use the color value "
          "${namesByColor[result]} in interpolation here.\n"
          "It may end up represented as $result, which will likely produce "
          "invalid CSS.\n"
          "Always quote color names when using them as strings or map keys "
          '(for example, "${namesByColor[result]}").\n'
          "If you really want to use the color value here, use '$alternative'.",
          expression.span);
      }
      */

      results.emplace_back(result->to_css(false));

    }

    return Util::join_strings(results, "");

  }


  /// Evaluates [interpolation] and wraps the result in a [CssValue].
  ///
  /// If [trim] is `true`, removes whitespace around the result. If
  /// [warnForColor] is `true`, this will emit a warning for any named color
  /// values passed into the interpolation.
  sass::string Eval::interpolationToValue(InterpolationObj interpolation,
    bool trim, bool warnForColor)
  {
    sass::string result = performInterpolation(interpolation, warnForColor);
    if (trim) { Util::ascii_str_trim(result); }
    return result;
    // return CssValue(trim ? trimAscii(result, excludeEscape: true) : result,
    //   interpolation.span);
  }

  ArgumentInvocation* Eval::visitArgumentInvocation(ArgumentInvocation* args)
  {
    // Create a copy of everything
    ArgumentInvocationObj arguments = SASS_MEMORY_NEW(
      ArgumentInvocation, args->pstate(), args->positional(),
      args->named(), args->restArg(), args->kwdRest());
    for (ExpressionObj& item : arguments->positional()) {
      item = item->perform(this);
    }
    return arguments.detach();
  }

  String_Constant* Eval::operator()(Interpolation* s)
  {
    return SASS_MEMORY_NEW(String_Constant,
      SourceSpan::fake("[pstate7]"),
      interpolationToValue(s, false, true));
  }

  Value* Eval::operator()(StringExpression* node)
  {
    // Don't use [performInterpolation] here because we need to get
    // the raw text from strings, rather than the semantic value.
    InterpolationObj itpl = node->text();
    sass::vector<sass::string> strings;
    for (const auto& item : itpl->elements()) {
      ValueObj result = item->perform(this);
      if (StringLiteral * lit = Cast<StringLiteral>(result)) {
        strings.emplace_back(lit->text());
      }
      else if (String_Constant * lit = Cast<String_Constant>(result)) {
        strings.emplace_back(lit->value());
      }
      else if (Cast<Null>(result)) {
        // Silently skip null values
      }
      else {
        strings.emplace_back(serialize(result));
      }
    }

    sass::string joined(Util::join_strings(strings, ""));

    return SASS_MEMORY_NEW(String_Constant,
      node->pstate(), std::move(joined), node->hasQuotes());

  }

  Value* Eval::operator()(String_Constant* s)
  {
    return s;
  }

  Value* Eval::operator()(StringLiteral* s)
  {
    return s;
  }

  /// Evaluates [expression] and calls `toCssString()` and wraps a
/// [SassScriptException] to associate it with [span].
  sass::string Eval::_evaluateToCss(Expression* expression, bool quote)
  {
    ValueObj evaled = expression->perform(this);
    if (!evaled->isNull()) {
      if (quote) return evaled->to_string();
      else return evaled->to_css();
    }
    else {
      return "";
    }
  }

  /// Calls `value.toCssString()` and wraps a [SassScriptException] to associate
/// it with [nodeWithSpan]'s source span.
///
/// This takes an [AstNode] rather than a [FileSpan] so it can avoid calling
/// [AstNode.span] if the span isn't required, since some nodes need to do
/// real work to manufacture a source span.
  sass::string Eval::_serialize(Expression* value, bool quote)
  {
    // _addExceptionSpan(nodeWithSpan, () = > value.toCssString(quote: quote));
    return value->to_css();
  }


  /// Evlauates [condition] and converts it to a plain CSS string, with
/// parentheses if necessary.
///
/// If [operator] is passed, it's the operator for the surrounding
/// [SupportsOperation], and is used to determine whether parentheses are
/// necessary if [condition] is also a [SupportsOperation].
  sass::string Eval::_parenthesize(SupportsCondition* condition) {
    SupportsNegation* negation = Cast<SupportsNegation>(condition);
    SupportsOperation* operation = Cast<SupportsOperation>(condition);
    if (negation != nullptr || operation != nullptr) {
      return "(" + _visitSupportsCondition(condition) + ")";
    }
    else {
      return _visitSupportsCondition(condition);
    }
  }

  sass::string Eval::_parenthesize(SupportsCondition* condition, SupportsOperation::Operand operand) {
    SupportsNegation* negation = Cast<SupportsNegation>(condition);
    SupportsOperation* operation = Cast<SupportsOperation>(condition);
    if (negation || (operation && operand != operation->operand())) {
      return "(" + _visitSupportsCondition(condition) + ")";
    }
    else {
      return _visitSupportsCondition(condition);
    }
  }

  sass::string Eval::_visitSupportsCondition(SupportsCondition* condition)
  {
    if (auto operation = Cast<SupportsOperation>(condition)) {
      sass::sstream strm;
      SupportsOperation::Operand operand = operation->operand();
      strm << _parenthesize(operation->left(), operand);
      strm << " " << (operand == SupportsOperation::AND ? "and " : "or ");
      strm << _parenthesize(operation->right(), operand);
      return strm.str();
    }
    else if (auto negation = Cast<SupportsNegation>(condition)) {
      sass::sstream strm;
      strm << "not ";
      strm << _parenthesize(negation->condition());
      return strm.str();
    }
    else if (auto interpolation = Cast<SupportsInterpolation>(condition)) {
      return _evaluateToCss(interpolation->value(), false);
    }
    else if (auto declaration = Cast<SupportsDeclaration>(condition)) {
      sass::sstream strm;
      strm << "(" << _evaluateToCss(declaration->feature()) << ":";
      strm << " " << _evaluateToCss(declaration->value()) << ")";
      return strm.str();
    }
    else {
      return "";
    }

  }

  String_Constant* Eval::operator()(SupportsCondition* condition)
  {
    return SASS_MEMORY_NEW(String_Constant,
      condition->pstate(), _visitSupportsCondition(condition));
  }

  Value* Eval::operator()(Null* n)
  {
    return n;
  }

  /// Adds the values in [map] to [values].
  ///
  /// Throws a [SassRuntimeException] associated with [nodeForSpan]'s source
  /// span if any [map] keys aren't strings.
  ///
  /// If [convert] is passed, that's used to convert the map values to the value
  /// type for [values]. Otherwise, the [Value]s are used as-is.
  ///
  /// This takes an [AstNode] rather than a [FileSpan] so it can avoid calling
  /// [AstNode.span] if the span isn't required, since some nodes need to do
  /// real work to manufacture a source span.
  void Eval::_addRestMap(KeywordMap<ValueObj>& values, SassMap* map, const SourceSpan& nodeForSpan) {
    // convert ??= (value) = > value as T;

    for(auto kv : map->elements()) {
      if (String_Constant* str = Cast<String_Constant>(kv.first)) {
        values["$" + str->value()] = kv.second; // convert?
      }
      else {
        sass::sstream strm;
        strm << "Variable keyword argument map must have string keys.\n";
        strm << kv.first->to_sass() << " is not a string in " << map->to_sass() << ".";
        callStackFrame frame(*ctx.logger, Backtrace(nodeForSpan));
        throw Exception::SassRuntimeException2(strm.str(),
          *ctx.logger, nodeForSpan);
      }
    }
  }

  /// Adds the values in [map] to [values].
  ///
  /// Throws a [SassRuntimeException] associated with [nodeForSpan]'s source
  /// span if any [map] keys aren't strings.
  ///
  /// If [convert] is passed, that's used to convert the map values to the value
  /// type for [values]. Otherwise, the [Value]s are used as-is.
  ///
  /// This takes an [AstNode] rather than a [FileSpan] so it can avoid calling
  /// [AstNode.span] if the span isn't required, since some nodes need to do
  /// real work to manufacture a source span.
  void Eval::_addRestMap2(KeywordMap<ExpressionObj>& values, SassMap* map, const SourceSpan& pstate) {
    // convert ??= (value) = > value as T;

    for (auto kv : map->elements()) {
      if (String_Constant* str = Cast<String_Constant>(kv.first)) {
        values["$" + str->value()] = SASS_MEMORY_NEW(
          ValueExpression, pstate, kv.second);
      }
      else {
        throw Exception::SassRuntimeException2(
          "Variable keyword argument map must have string keys.\n" +
          str->value() + " is not a string in " + map->to_sass() + ".",
          *ctx.logger, pstate);
      }
    }
  }

  ArgumentResults2& Eval::_evaluateArguments(ArgumentInvocation* arguments)
  {

    sass::vector<ValueObj>& positional = arguments->evaluated.positional();
    sass::vector<ExpressionObj>& positionalIn = arguments->positional();
    KeywordMap<ValueObj>& named = arguments->evaluated.named();
    positional.resize(positionalIn.size());
    for (size_t i = 0, iL = positionalIn.size(); i < iL; i++) {
      positional[i] = positionalIn[i]->perform(this);
    }

    named.clear();
    // Reserving on ordered-map seems slower?
    keywordMapMap2(named, arguments->named());
    // for (auto kv : named) { kv.second = kv.second->perform(this); }

    // var positionalNodes =
    //   trackSpans ? arguments.positional.map(_expressionNode).toList() : null;
    // var namedNodes = trackSpans
    //   ? mapMap<String, Expression, String, AstNode>(arguments.named,
    //     value: (_, expression) = > _expressionNode(expression))
    //   : null;

    if (arguments->restArg() == nullptr) {
      arguments->evaluated.separator(SASS_UNDEF);
      return arguments->evaluated;
    }

    ValueObj rest = arguments->restArg()->perform(this);
    // var restNodeForSpan = trackSpans ? _expressionNode(arguments.rest) : null;

    Sass_Separator separator = SASS_UNDEF;

    if (SassMap * restMap = Cast<SassMap>(rest)) {
      _addRestMap(named, restMap, arguments->restArg()->pstate());
    }
    else if (SassList * list = Cast<SassList>(rest)) {
      const sass::vector<ValueObj>& values = rest->asVector();
      std::copy(values.begin(), values.end(),
        std::back_inserter(positional));
      separator = list->separator();
      if (SassArgumentList * args = Cast<SassArgumentList>(rest)) {
        auto kwds = args->keywords();
        for (auto kv : kwds) {
          named[kv.first] = kv.second;
        }
      }
    }
    else {
      positional.emplace_back(rest);
    }

    if (arguments->kwdRest() == nullptr) {
      arguments->evaluated.separator(separator);
      return arguments->evaluated;
    }

    ValueObj keywordRest = arguments->kwdRest()->perform(this);
    // var keywordRestNodeForSpan = trackSpans ? _expressionNode(arguments.keywordRest) : null;

    if (Map * restMap = Cast<Map>(keywordRest)) {
      _addRestMap(named, restMap, arguments->kwdRest()->pstate());
      arguments->evaluated.separator(separator);
      return arguments->evaluated;
    }
    else {
      error("Variable keyword arguments must be a map (was $keywordRest).",
        keywordRest->pstate(), traces);
    }

    throw std::runtime_error("thrown before");

  }

  Value* Eval::operator()(Argument* a)
  {
    ExpressionObj val = a->value()->perform(this);
    // bool is_rest_argument = a->is_rest_argument();
    // bool is_keyword_argument = a->is_keyword_argument();

    if (a->is_rest_argument()) {
      if (val->concrete_type() == Expression::MAP) {
        // is_rest_argument = false;
        // is_keyword_argument = true;
      }
      else if(val->concrete_type() != Expression::LIST) {
        SassList_Obj wrapper = SASS_MEMORY_NEW(SassList,
          val->pstate(), sass::vector<ValueObj> {}, SASS_COMMA);
        wrapper->append(val);
        val = wrapper;
      }
    }
    // ArgumentObj rv = SASS_MEMORY_NEW(Argument,
    //                        a->pstate(),
    //                        val,
    //                        a->name(),
    //                        is_rest_argument,
    //                        is_keyword_argument);
    // std::cerr << "ADASD\n";
    val = val->perform(this);
    return Cast<Value>(val.detach());

  }

  Value* Eval::operator()(Parent_Reference* p)
  {
    if (SelectorListObj parents = original()) {
      return Listize::listize(parents);
    } else {
      return SASS_MEMORY_NEW(Null, p->pstate());
    }
  }

  Value* Eval::_runAndCheck(UserDefinedCallable* callable, Trace* trace)
  {
    CallableDeclaration* declaration = callable->declaration();
    for (Statement* statement : declaration->block()->elements()) {
      // Normal statements in functions must return nullptr
      Value* value = statement->perform(this);
      if (value != nullptr) return value;
    }
    throw Exception::SassRuntimeException2(
      "Function finished without @return.",
      *ctx.logger, declaration->pstate());
  }

  Value* Eval::visitSupportsRule(SupportsRule* node)
  {
    ValueObj condition = node->condition()->perform(this);
    EnvScope scoped(ctx.varRoot, node->idxs());

    BlockObj bb = SASS_MEMORY_NEW(Block, SourceSpan::fake(""));
    bb->is_root(blockStack.back()->is_root());
    blockStack.emplace_back(bb);
    visitBlock(node->block());
    blockStack.pop_back();

    CssSupportsRuleObj ff = SASS_MEMORY_NEW(CssSupportsRule,
      node->pstate(),
      condition,
      bb->elements());

    // ff->block(bb);
    // ff->tabs(f->tabs());
    blockStack.back()->append(ff);

    return nullptr;
  }

  sass::vector<CssMediaQueryObj> Eval::mergeMediaQueries(
    const sass::vector<CssMediaQueryObj>& lhs,
    const sass::vector<CssMediaQueryObj>& rhs)
  {
    sass::vector<CssMediaQueryObj> queries;
    for (CssMediaQueryObj query1 : lhs) {
      for (CssMediaQueryObj query2 : rhs) {
        CssMediaQueryObj result = query1->merge(query2);
        if (result && !result->empty()) {
          queries.emplace_back(result);
        }
      }
    }
    return queries;
  }

  Value* Eval::visitMediaRule(MediaRule* node)
  {
    Expression_Obj mq;
    sass::string str_mq;
    SourceSpan state(node->pstate());
    if (node->query()) {
      state = node->query()->pstate();
      str_mq = performInterpolation(node->query(), false);
    }
    // char* str = sass_copy_c_string(str_mq.c_str());
    // ctx.strings.emplace_back(str);
    auto qwe = SASS_MEMORY_NEW(SourceFile,
      state.getPath(), str_mq.c_str(), state.getSrcId());
    MediaQueryParser p2(ctx, qwe);
    // Create a new CSS only representation of the media rule
    CssMediaRuleObj css = SASS_MEMORY_NEW(CssMediaRule,
      node->pstate(), sass::vector<CssMediaQueryObj>(), node->block()->elements());
    sass::vector<CssMediaQueryObj> parsed = p2.parse();
    if (mediaStack.size() && mediaStack.back()) {
      auto parent = mediaStack.back()->queries();
      css->concat(mergeMediaQueries(parent, parsed));
    }
    else {
      css->concat(parsed);
    }
    mediaStack.emplace_back(css);

    BlockObj blk = SASS_MEMORY_NEW(Block, SourceSpan::fake(""));
    blk->is_root(blockStack.back()->is_root());
    blockStack.emplace_back(blk);
    node->block()->perform(this);
    blockStack.pop_back();
    // css->block(blk);
    css->elements(blk->elements());
    mediaStack.pop_back();

    // The parent to add declarations too
    Block* parent = blockStack.back();
    parent->append(css);
    // return css.detach();
    return nullptr;
  }

  Value* Eval::visitAtRootRule(AtRootRule* node)
  {

    EnvScope scoped(ctx.varRoot, node->idxs());


    // std::cerr << "visitAtRootRule\n";
    InterpolationObj itpl = node->query();
    AtRootQueryObj query;



    if (node->query()) {
      query = AtRootQuery::parse(
        performInterpolation(
          node->query(), true),
        ctx);
    }
    else {
      query = AtRootQuery::defaultQuery(node->pstate());
    }

    LOCAL_FLAG(_inKeyframes, false);
    LOCAL_FLAG(_inUnknownAtRule, false);
    LOCAL_FLAG(at_root_without_rule,
      query && query->excludesStyleRules());

    BlockObj bb = SASS_MEMORY_NEW(Block, SourceSpan::fake(""));
    bb->is_root(blockStack.back()->is_root());
    blockStack.emplace_back(bb);
    visitBlock(node->block());
    blockStack.pop_back();

    blockStack.back()->append(
      SASS_MEMORY_NEW(CssAtRootRule,
        node->pstate(), query, bb->elements()));

    return nullptr;
  }

  /// Evaluates [interpolation] and wraps the result in a [CssValue].
///
/// If [trim] is `true`, removes whitespace around the result. If
/// [warnForColor] is `true`, this will emit a warning for any named color
/// values passed into the interpolation.
  CssString* Eval::interpolationToCssString(InterpolationObj interpolation,
    bool trim, bool warnForColor)
  {
    if (interpolation.isNull()) return nullptr;
    sass::string result = performInterpolation(interpolation, warnForColor);
    if (trim) { Util::ascii_str_trim(result); } // ToDo: excludeEscape: true
    return SASS_MEMORY_NEW(CssString, interpolation->pstate(), result);
    // return CssValue(trim ? trimAscii(result, excludeEscape: true) : result,
    //   interpolation.span);
  }

  Value* Eval::visitAtRule(AtRule* node)
  {
    // The parent to add stuff too
    Block* parent = blockStack.back();

    CssStringObj name = interpolationToCssString(node->name(), false, false);
    CssStringObj value = interpolationToCssString(node->value(), true, true);

    sass::string normalized(Util::unvendor(name->text()));
    bool isKeyframe = normalized == "keyframes";
    LOCAL_FLAG(_inUnknownAtRule, !isKeyframe);
    LOCAL_FLAG(_inKeyframes, isKeyframe);

    if (node->block()) {

      BlockObj blk = SASS_MEMORY_NEW(Block, SourceSpan::fake(""));
      blk->is_root(blockStack.back()->is_root());
      blockStack.emplace_back(blk);
      node->block()->perform(this);
      blockStack.pop_back();

      CssAtRule* result = SASS_MEMORY_NEW(CssAtRule,
        node->pstate(),
        name,
        value,
        blk->elements());
      //result->block(blk);
      parent->append(result);

    }
    else {

      CssAtRule* result = SASS_MEMORY_NEW(CssAtRule,
        node->pstate(),
        name,
        value,
        sass::vector<StatementObj>());
      result->isChildless(true);
      parent->append(result);

    }

    return nullptr;

  }

  Value* Eval::visitDeclaration(Declaration* node)
  {

    if (!isInStyleRule() && !_inUnknownAtRule && !_inKeyframes) {
      error(
        "Declarations may only be used within style rules.",
        node->pstate(), traces);
    }

    CssStringObj name = interpolationToCssString(node->name(), false, true);

    bool is_custom_property = node->is_custom_property();

    if (!_declarationName.empty()) {
      name->text(_declarationName + "-" + name->text());
    }

    CssValueObj cssValue;
    if (node->value()) {
      ValueObj value = node->value()->perform(this);
      cssValue = SASS_MEMORY_NEW(CssValue,
        value->pstate(), value);
    }

    // The parent to add declarations too
    Block* parent = blockStack.back();

    // If the value is an empty list, preserve it, because converting it to CSS
    // will throw an error that we want the user to see.
    if (cssValue != nullptr && (!cssValue->value()->isBlank()
      || cssValue->value()->asVector().empty())) {
      parent->append(SASS_MEMORY_NEW(CssDeclaration,
        node->pstate(), name, cssValue, is_custom_property));
    }
    else if (is_custom_property) {
      throw Exception::SassRuntimeException2(
        "Custom property values may not be empty.",
        *ctx.logger, node->value()->pstate()
      );
    }

    if (node->block()) {
      LocalOption<sass::string> ll1(_declarationName, name->text());
      for (Statement* child : node->block()->elements()) {
        ValueObj result = child->perform(this);
        // if (result) parent->append(result);
      }
    }
    return nullptr;
  }

  Value* Eval::visitVariableDeclaration(Assignment* a)
  {
    const IdxRef& vidx(a->vidx());
    const EnvString& name(a->variable());
    SASS_ASSERT(vidx.isValid(), "Invalid VIDX");

    if (a->is_global()) {

      // Check if we are at the global scope
      if (ctx.varRoot.isGlobal()) {
        if (!ctx.varRoot.hasGlobalVariable33(name)) {
#ifndef GOFAST
          callStackFrame frame(*ctx.logger,
            Backtrace(a->pstate()));
#endif
          deprecatedDart(
            "As of Dart Sass 2.0.0, !global assignments won't be able to\n"
            "declare new variables. Since this assignment is at the root of the stylesheet,\n"
            "the !global flag is unnecessary and can safely be removed.",
            sass::string::npos, *ctx.logger, a->pstate());
        }
      }
      else {
        if (!ctx.varRoot.hasGlobalVariable33(name)) {
#ifndef GOFAST
          callStackFrame frame(*ctx.logger,
            Backtrace(a->pstate()));
#endif
          deprecatedDart(
            "As of Dart Sass 2.0.0, !global assignments won't be able to\n"
            "declare new variables. Consider adding `" + name.orig() + ": null` at the root of the\n"
            "stylesheet.",
            sass::string::npos, *ctx.logger, a->pstate());
        }
      }

    }

    // has global flag?
    if (a->is_global()) {
      // has global and default?
      if (a->is_default()) {
        ExpressionObj& value = ctx.varRoot.getVariable(vidx);
        if (value.isNull() || value->concrete_type() == Expression::NULL_VAL) {
          ctx.assigningTo = vidx;
          ctx.varRoot.setVariable(vidx,
            a->value()->perform(this));
          ctx.assigningTo = {};
        }
      }
      else {
        ctx.assigningTo = vidx;
        ctx.varRoot.setVariable(vidx,
          a->value()->perform(this));
        ctx.assigningTo = {};
      }

    }
    // has only default flag?
    else if (a->is_default()) {
      ExpressionObj& value = ctx.varRoot.getVariable(vidx);
      if (value.isNull() || value->concrete_type() == Expression::NULL_VAL) {
        ctx.assigningTo = vidx;
        ctx.varRoot.setVariable(vidx,
          a->value()->perform(this));
        ctx.assigningTo = {};
      }
    }
    // no global nor default
    else {
      ctx.assigningTo = vidx;
      ctx.varRoot.setVariable(vidx,
        a->value()->perform(this));
      ctx.assigningTo = {};
    }

    return nullptr;
  }

  Value* Eval::visitLoudComment(LoudComment* c)
  {
    if (_inFunction) return nullptr;
    sass::string text(performInterpolation(c->text(), false));
    bool preserve = text[2] == '!';
    blockStack.back()->append(SASS_MEMORY_NEW(CssComment, c->pstate(), text, preserve));
    return nullptr;
  }

  Value* Eval::visitIfRule(If* i)
  {
    ValueObj rv;

    EnvScope scoped(ctx.varRoot, i->idxs());

    ValueObj result = i->predicate()->perform(this);
    if (result->isTruthy()) {
      rv = i->block()->perform(this);
    }
    else {
      Block* alt = i->alternative();
      if (alt) rv = alt->perform(this);
    }
    return rv.detach();

  }

  // For does not create a new env scope
  // But iteration vars are reset afterwards
  Value* Eval::visitForRule(For* f)
  {
#ifndef GOFAST
    callStackFrame(traces,
      Backtrace(f->pstate(), "@for"));
#endif

    EnvScope scoped(ctx.varRoot, f->idxs());

    // const EnvString& variable(f->variable());
    ValueObj low = f->lower_bound()->perform(this);
    ValueObj high = f->upper_bound()->perform(this);
    NumberObj sass_start = low->assertNumber(*ctx.logger);
    NumberObj sass_end = high->assertNumber(*ctx.logger);
    // check if units are valid for sequence
    if (sass_start->unit() != sass_end->unit()) {
      sass::sstream msg; msg << "Incompatible units "
        << sass_start->unit() << " and "
        << sass_end->unit() << ".";
      error(msg.str(), low->pstate(), traces);
    }
    double start = sass_start->value();
    double end = sass_end->value();
    // only create iterator once in this environment
    Block* body = f->block();
    ValueObj val;
    if (start < end) {
      if (f->is_inclusive()) ++end;
      for (double i = start; i < end; ++i) {
        NumberObj it = SASS_MEMORY_NEW(Number, low->pstate(), i, sass_end->unit());
        ctx.varRoot.setVariable(f->idxs()->vidxs.frame, 0, it);
        // env.set_local(variable, it);
        val = body->perform(this);
        if (val) break;
      }
    }
    else {
      if (f->is_inclusive()) --end;
      for (double i = start; i > end; --i) {
        NumberObj it = SASS_MEMORY_NEW(Number, low->pstate(), i, sass_end->unit());
        ctx.varRoot.setVariable(f->idxs()->vidxs.frame, 0, it);
        // env.set_local(variable, it);
        val = body->perform(this);
        if (val) break;
      }
    }
    return val.detach();

  }

  Value* Eval::visitExtendRule(ExtendRule* e)
  {

    if (!isInStyleRule() && !isInMixin() && !isInContentBlock()) {
      error(
        "@extend may only be used within style rules.",
        e->pstate(), traces);
    }

    InterpolationObj itpl = e->selector();

    sass::string text = interpolationToValue(itpl, true, false);

    bool allowParent = true;
    if (blockStack.size() > 2) {
      Block* b = blockStack.at(blockStack.size() - 2);
      if (b->is_root()) allowParent = false;
    }

    SelectorListObj slist = itplToSelector(itpl,
      plainCss, allowParent);

    /*
    var list = _adjustParseError(
      targetText,
      () = > SelectorList.parse(
        trimAscii(targetText.value, excludeEscape: true),
        logger: _logger,
        allowParent : false));
        */

        // Sass_Import_Entry parent = ctx.import_stack.back();
    /*
    char* cstr = sass_copy_c_string(text.c_str());
    ctx.strings.emplace_back(cstr);
    auto qwe = SASS_MEMORY_NEW(SourceFile,
      "foobar", cstr, -1);
    SelectorParser p2(ctx, qwe);
    // p2.scanner.offset = itpl->pstate();
    p2.scanner.srcid = itpl->pstate().getSrcId();
    p2.scanner.sourceUrl = itpl->pstate().getPath();
    if (blockStack.size() > 2) {
      Block* b = blockStack.at(blockStack.size() - 2);
      if (b->is_root()) p2._allowParent = false;
    }

    SelectorListObj slist = p2.parse();
    */

    if (slist) {

      for (auto complex : slist->elements()) {

        if (complex->length() != 1) {
          error("complex selectors may not be extended.", complex->pstate(), traces);
        }

        if (const CompoundSelector * compound = complex->first()->getCompound()) {

          if (compound->length() != 1) {

            sass::sstream sels; bool addComma = false;
            sels << "Compound selectors may no longer be extended.\n";
            sels << "Consider `@extend ";
            for (auto sel : compound->elements()) {
              if (addComma) sels << ", ";
              sels << sel->to_sass();
              addComma = true;
            }
            sels << "` instead.\n";
            sels << "See http://bit.ly/ExtendCompound for details.";

            warning(sels.str(), *ctx.logger, compound->pstate());

            // Make this an error once deprecation is over
            for (SimpleSelectorObj simple : compound->elements()) {
              // Pass every selector we ever see to extender (to make them findable for extend)
              ctx.extender.addExtension(selector(), simple, mediaStack.back(), e->isOptional());
            }

          }
          else {
            // Pass every selector we ever see to extender (to make them findable for extend)
            ctx.extender.addExtension(selector(), compound->first(), mediaStack.back(), e->isOptional());
          }

        }
        else {
          error("complex selectors may not be extended.", complex->pstate(), traces);
        }
      }
    }

    return nullptr;
  }

  Value* Eval::visitStyleRule(StyleRule* r)
  {

    EnvScope scope(ctx.varRoot, r->idxs());

    Interpolation* itpl = r->interpolation();
    LocalOption<StyleRuleObj> oldStyleRule(_styleRule, r);

    if (_inKeyframes) {

      BlockObj bb = SASS_MEMORY_NEW(Block, SourceSpan::fake(""));
      bb->is_root(blockStack.back()->is_root());
      blockStack.emplace_back(bb);
      r->block()->perform(this);
      blockStack.pop_back();

      auto text = interpolationToValue(itpl, true, false);
      // char* cstr = sass_copy_c_string(text.c_str());
      // ctx.strings.emplace_back(cstr);
      // auto qwe = SASS_MEMORY_NEW(SourceFile,
      //   itpl->pstate().getPath(), cstr, itpl->pstate().getSrcId());

      auto qwe = SASS_MEMORY_NEW(SyntheticFile, text.c_str(), itpl->pstate().source, itpl->pstate());

      KeyframeSelectorParser parser(ctx, qwe);
      sass::vector<sass::string> selector(parser.parse());

      CssStrings* strings =
      SASS_MEMORY_NEW(CssStrings,
        itpl->pstate(), selector);

      auto block = SASS_MEMORY_NEW(CssKeyframeBlock,
        itpl->pstate(), strings, bb->elements());

      Keyframe_Rule_Obj k = SASS_MEMORY_NEW(Keyframe_Rule, r->pstate(), bb->elements());
      if (r->interpolation()) {
        selectorStack.push_back({});
        auto val = interpolationToValue(r->interpolation(), true, false);
        k->name2(SASS_MEMORY_NEW(StringLiteral, SourceSpan::fake("[pstate9]"), val));
        selectorStack.pop_back();
      }

      // blockStack.back()->append(k);
      blockStack.back()->append(block);

      return nullptr;
      // return k.detach();
    }

    SelectorListObj slist;
    if (r->interpolation()) {
      Sass_Import_Entry imp = ctx.import_stack.back();
      bool plainCss = imp->type == SASS_IMPORT_CSS;
      slist = itplToSelector(r->interpolation(), plainCss);
    }

    // reset when leaving scope
    SASS_ASSERT(slist, "must have selectors");

    SelectorListObj evaled = slist->resolveParentSelectors(
      selectorStack.back(), traces, !at_root_without_rule);
    LOCAL_FLAG(at_root_without_rule, false);

    selectorStack.emplace_back(evaled);
    // The copy is needed for parent reference evaluation
    // dart-sass stores it as `originalSelector` member
    originalStack.emplace_back(SASS_MEMORY_COPY(evaled));
      ctx.extender.addSelector(evaled, mediaStack.back());

    BlockObj blk = SASS_MEMORY_NEW(Block, SourceSpan::fake(""));
    blk->is_root(blockStack.back()->is_root());
    blockStack.emplace_back(blk);
    r->block()->perform(this);
    blockStack.pop_back();

    originalStack.pop_back();
    selectorStack.pop_back();

    CssStyleRule* rr = SASS_MEMORY_NEW(CssStyleRule,
      r->pstate(),
      evaled,
      blk->elements());

    rr->tabs(r->tabs());
    blockStack.back()->append(rr);
    return nullptr;

  }

  SelectorListObj Eval::itplToSelector(Interpolation* itpl, bool plainCss, bool allowParent)
  {

    sass::vector<Mapping> mappings;
    SourceSpan pstate(itpl->pstate());
    mappings.emplace_back(Mapping(pstate.getSrcId(), pstate.position, Offset()));
    InterpolationObj evaled = evalInterpolation(itpl, false);
    sass::string css(evaled->to_css(mappings));
    Util::ascii_str_trim(css);
    auto text = css;


    // auto text = interpolationToValue(itpl, true, false);
    // char* cstr = sass_copy_c_string(text.c_str());
    // ctx.strings.emplace_back(cstr);
    // auto around = SASS_MEMORY_NEW(SourceFile, pstate.source, Mappings());
    auto synthetic = SASS_MEMORY_NEW(SyntheticFile, text.c_str(), mappings, pstate.source, pstate);

    // std::cerr << qwe2->getLine(0) << "\n";
    // std::cerr << qwe2->getLine(1) << "\n";
    // std::cerr << qwe2->getLine(2) << "\n";
    // std::cerr << qwe2->getLine(3) << "\n";
    // exit(1);

    try {
      // Everything parsed, will be parsed from perspective of local content
      // Pass the source-map in for the interpolation, so the scanner can
      // update the positions according to previous source-positions
      // Is a parser state solely represented by a source map or do we
      // need an intermediate format for them?
      SelectorParser p2(ctx, synthetic);
      p2._allowPlaceholder = plainCss == false;
      if (blockStack.size() > 2) {
        Block* b = blockStack.at(blockStack.size() - 2);
        if (b->is_root()) p2._allowParent = false;
      }
      p2._allowParent = allowParent && plainCss == false;
      return p2.parse();
    }
    catch (Exception::Base& err) {
      // err.pstate.src = itpl->pstate().src;
      // err.pstate += itpl->pstate() + itpl->pstate().offset;
      throw err;
    }
  }

  Value* Eval::visitEachRule(Each* e)
  {
    const IDXS* vidx(e->idxs());
    const sass::vector<EnvString>& variables(e->variables());
    EnvScope scoped(ctx.varRoot, e->idxs());
    Expression_Obj expr = e->list()->perform(this);
    SassListObj list;
    MapObj map;
    if (expr->concrete_type() == Expression::MAP) {
      map = Cast<Map>(expr);
    }
    else if (expr->concrete_type() != Expression::LIST) {
      list = SASS_MEMORY_NEW(SassList, expr->pstate(),
        sass::vector<ValueObj>(), SASS_COMMA);
      list->append(expr);
    }
    else if (SassList * slist = Cast<SassList>(expr)) {
      list = SASS_MEMORY_NEW(SassList, expr->pstate(),
        sass::vector<ValueObj>(), slist->separator());
      list->hasBrackets(slist->hasBrackets());
      for (auto item : slist->elements()) {
        list->append(item);
      }
    }

    Block_Obj body = e->block();
    ValueObj val;

    if (map) {
      for (auto kv : map->elements()) {
        ValueObj key = kv.first;
        ValueObj value = kv.second;
        if (variables.size() == 1) {
          SassList* variable = SASS_MEMORY_NEW(SassList,
            map->pstate(), sass::vector<ValueObj>(), SASS_SPACE);
          ctx.varRoot.setVariable(vidx->vidxs.frame, 0, variable);
          // env.set_local(variables[0], variable);
          variable->append(key);
          variable->append(value);
        }
        else {
          ctx.varRoot.setVariable(vidx->vidxs.frame, 0, key);
          ctx.varRoot.setVariable(vidx->vidxs.frame, 1, value);
          // env.set_local(variables[0], key);
          // env.set_local(variables[1], value);
        }
        val = body->perform(this);
        if (val) break;
      }
    }
    else {
      for (size_t i = 0, L = list->length(); i < L; ++i) {
        Expression* item = list->get(i);
        // unwrap value if the expression is an argument
        if (Argument * arg = Cast<Argument>(item)) item = arg->value();
        // check if we got passed a list of args (investigate)
        if (SassList * scalars = Cast<SassList>(item)) {
          if (variables.size() == 1) {
            ctx.varRoot.setVariable(vidx->vidxs.frame, 0, scalars);
            // env.set_local(variables[0], scalars);
          }
          else {
            for (size_t j = 0, K = variables.size(); j < K; ++j) {
              Value* res = j >= scalars->length()
                ? SASS_MEMORY_NEW(Null, expr->pstate())
                : scalars->get(j)->perform(this);
              ctx.varRoot.setVariable(vidx->vidxs.frame, j, res);
              // env.set_local(variables[j], res);
            }
          }
        }
        else {
          if (variables.size() > 0) {
            // env.set_local(variables.at(0), item);
            ctx.varRoot.setVariable(vidx->vidxs.frame, 0, item);
            for (size_t j = 1, K = variables.size(); j < K; ++j) {
              Value* res = SASS_MEMORY_NEW(Null, expr->pstate());
              ctx.varRoot.setVariable(vidx->vidxs.frame, j, res);
              // env.set_local(variables[j], res);
            }
          }
        }
        val = body->perform(this);
        if (val) break;
      }
    }
    return val.detach();
  }

  Value* Eval::visitWhileRule(WhileRule* node)
  {
    Block* body = node->block();
    Expression* condition = node->condition();
    ValueObj result = condition->perform(this);

    EnvScope scoped(ctx.varRoot, node->idxs());

    // Evaluate the first run in outer scope
    // All successive runs are from innter scope
    if (result->isTruthy()) {

      while (true) {
        result = body->perform(this);
        if (result) {
          return result.detach();
        }
        result = condition->perform(this);
        if (!result->isTruthy()) break;
      }

    }

    return nullptr;

  }

  Value* Eval::visitContentRule(ContentRule* c)
  {
    if (!ctx.varRoot.contentExists) return nullptr;
    // convert @content directives into mixin calls to the underlying thunk

    // UserDefinedCallable* content = env->getContent();
    IdxRef cidx = ctx.varRoot.getLexicalMixin33(Keys::contentRule);
    if (!cidx.isValid()) return nullptr;
    UserDefinedCallable* content = ctx.varRoot.getMixin(cidx);
    if (content == nullptr) return nullptr;
    // if (content != content2) {
    //   std::cerr << "not yet there\n";
    // }


    LOCAL_FLAG(inMixin, false);


    // EnvScope scoped(ctx.varRoot, before->declaration()->idxs());

    Block_Obj trace_block = SASS_MEMORY_NEW(Block, c->pstate());
    Trace_Obj trace = SASS_MEMORY_NEW(Trace, c->pstate(), Strings::contentRule, trace_block);

    blockStack.emplace_back(trace_block);

#ifndef GOFAST
    callStackFrame frame(*ctx.logger,
      Backtrace(c->pstate(), Strings::contentRule));
#endif

    // Appends to trace
    ValueObj qwe = _runUserDefinedCallable(
      c->arguments(),
      content,
      nullptr,
      false,
      &Eval::_runWithBlock,
      trace,
      c->pstate());

    blockStack.pop_back();

    // _runUserDefinedCallable(node.arguments, content, node, () {
    // return nullptr;
    // Adds it twice?
    blockStack.back()->append(trace);
    return nullptr;

  }

  Value* Eval::visitMixinRule(MixinRule* rule)
  {

    UserDefinedCallableObj cb =
      SASS_MEMORY_NEW(UserDefinedCallable,
        rule->pstate(), rule->name(), rule, nullptr);

    if (rule->fidx().isValid()) {
      ctx.varRoot.setMixin(rule->fidx(), cb);
    }
    else {
      std::cerr << "why not set?";
    }

    // scope->setMixin2(rule->name(), cb);

    return nullptr;
  }

  Value* Eval::visitFunctionRule(FunctionRule* rule)
  {

    CallableObj cb = SASS_MEMORY_NEW(UserDefinedCallable,
      rule->pstate(), rule->name(), rule, nullptr);

    if (rule->fidx().isValid()) {
      ctx.varRoot.setFunction(rule->fidx(), cb);
      // return nullptr;
    }

    return nullptr;
  }

  Value* Eval::visitIncludeRule(IncludeRule* node)
  {
    // Function Expression might be simple and static, or dynamic css call
    UserDefinedCallableObj mixin = _getMixin(node->midx(), node->name(), node->ns());

    // EnvScope scoped(ctx.varRoot,
    //   mixin->declaration()->idxs());

    if (mixin == nullptr) {
      throw Exception::SassRuntimeException2(
        "Undefined mixin.",
        *ctx.logger,
        node->pstate());
    }

    UserDefinedCallableObj contentCallable;

    EnvSnapshot snapshot(ctx.varRoot, node->content());

    LocalOption<bool> hasContent(ctx.varRoot.contentExists, node->content());

    if (node->content() != nullptr) {

      contentCallable = SASS_MEMORY_NEW(
        UserDefinedCallable, 
        node->pstate(), node->name(),
        node->content(), &snapshot);
      // std::cerr << "setting contentCallable\n";

      MixinRule* rule = Cast<MixinRule>(mixin->declaration());
      node->content()->cidx(rule->cidx());
      if (!rule->cidx().isValid()) {
        std::cerr << "Invalid cidx2\n";
      }
      // std::cerr << "setuop cidx bind " << rule << "\n";

      if (!rule || !rule->has_content()) {
        // debug_ast(rule);
        callStackFrame frame(*ctx.logger,
          Backtrace(node->content()->pstate()));
        throw Exception::SassRuntimeException2(
          "Mixin doesn't accept a content block.",
          *ctx.logger, node->pstate());
      }
    }

    Block_Obj trace_block = SASS_MEMORY_NEW(Block, node->pstate());
    Trace_Obj trace = SASS_MEMORY_NEW(Trace, node->pstate(), node->name().orig(), trace_block);

    blockStack.emplace_back(trace_block);
    LOCAL_FLAG(inMixin, true);
    // env_stack.emplace_back(&closure2);

#ifndef GOFAST
    callStackFrame frame(*ctx.logger,
      Backtrace(node->pstate(), mixin->name().orig(), true));
#endif

    ValueObj qwe = _runUserDefinedCallable(
      node->arguments(),
      mixin,
      contentCallable,
      true,
      &Eval::_runWithBlock,
      trace,
      node->pstate());

    // env_stack.pop_back();
    blockStack.pop_back();

    /*
    _inMixin = oldInMixin;
    _content = oldContent;
    */

    // From old implementation
    // ctx.callee_stack.pop_back();
    // env_stack.pop_back();
    // traces.pop_back();

    // debug_ast(trace);
    blockStack.back()->append(trace);
    return nullptr;

  }


  Value* Eval::visitSilentComment(SilentComment* c)
  {
    blockStack.back()->append(c);
    return nullptr;
  }

  Value* Eval::visitImportStub99(Import_Stub* i)
  {

#ifndef GOFAST
    // Add a stack frame for this import rule
    callStackFrame frame(traces,
      Backtrace(i->pstate(), "@import"));
#endif

    // we don't seem to need that actually afterall
    Sass_Import_Entry import = sass_make_import(
      i->imp_path().c_str(),
      i->abs_path().c_str(),
      0, 0
    );

    Block_Obj trace_block = SASS_MEMORY_NEW(Block, i->pstate());
    Trace_Obj trace = SASS_MEMORY_NEW(Trace, i->pstate(), i->imp_path(), trace_block, 'i');
    blockStack.back()->append(trace);
    blockStack.emplace_back(trace_block);

    const sass::string& abs_path(i->resource().abs_path);
    StyleSheet sheet = ctx.sheets.at(abs_path);
    import->type = sheet.syntax;
    ctx.import_stack.emplace_back(import);
    append_block(sheet.root);



    sass_delete_import(ctx.import_stack.back());
    ctx.import_stack.pop_back();
    blockStack.pop_back();
    return nullptr;
  }

  Value* Eval::visitDynamicImport99(DynamicImport* rule)
  {
    std::cerr << "visit dynamicImport" << "\n";
    blockStack.back()->append(rule);
    return nullptr;
  }

  Value* Eval::visitStaticImport99(StaticImport* rule)
  {
    // ToDo: must convert itpl to string
    if (rule->url()) {
      CssStringObj url = interpolationToCssString(rule->url(), false, false);
      rule->url({});
      rule->url2(url);
      // rule->url(rule->url()->perform(this));
    }
    if (rule->media()) {
      sass::string str(interpolationToValue(rule->media(), true, true));
      rule->media(SASS_MEMORY_NEW(Interpolation, SourceSpan::fake("[pstate1]")));
      rule->media()->append(SASS_MEMORY_NEW(StringLiteral, SourceSpan::fake("[pstate2]"), str));
    }
    // block_stack.back()->append(rule);
    return nullptr;
  }

  Value* Eval::visitImportRule99(ImportRule* rule)
  {
    for (ImportBaseObj import : rule->elements()) {
      if (import) import->perform(this);
    }
    blockStack.back()->append(rule);
    return nullptr;
  }

  Value* Eval::visitImport99(Import* imp)
  {
    // std::cerr << "visit an import\n";
    Import_Obj result = SASS_MEMORY_NEW(Import, imp->pstate());
    if (!imp->queries2().empty()) {
      sass::vector<sass::string> results;
      sass::vector<ExpressionObj> queries = imp->queries2();
      for (auto& query : queries) {
        ExpressionObj evaled = query->perform(this);
        results.emplace_back(evaled->to_string());
      }
      sass::string reparse(Util::join_strings(results, ", "));
      SourceSpan state(imp->pstate());
      // char* str = sass_copy_c_string(reparse.c_str());
      // ctx.strings.emplace_back(str);
      auto qwe = SASS_MEMORY_NEW(SourceFile,
        state.getPath(), reparse.c_str(), state.getSrcId());
      MediaQueryParser p2(ctx, qwe);
      auto queries2 = p2.parse();
      result->queries(queries2);
      result->import_queries({});
    }
    for (size_t i = 0, S = imp->urls().size(); i < S; ++i) {
      result->urls().emplace_back(imp->urls()[i]->perform(this));
    }
    // all resources have been dropped for Input_Stubs

    for (size_t i = 0, S = imp->incs().size(); i < S; ++i) {
      Include inc = imp->incs()[i];
      // std::cerr << "convert incs to import stubs " << inc.type << "\n";
      Import_StubObj stub = SASS_MEMORY_NEW(Import_Stub,
        imp->pstate(), inc);
      stub->perform(this);
    }

    blockStack.back()->append(result);
    return nullptr;

  }

  // process and add to last block on stack
  void Eval::append_block(Block* block)
  {
    // Block* parent = blockStack.back();
    // if (b->is_root()) call_stack.emplace_back(b);
    for (Statement* item : block->elements()) {
      item->perform(this);
      // if (parent && child) parent->append(child);
    }
    // if (b->is_root()) call_stack.pop_back();
  }

  bool Eval::isInMixin()
  {
    for (Sass_Callee& callee : ctx.callee_stack) {
      if (callee.type == SASS_CALLEE_MIXIN) return true;
    }
    return false;
  }

  Block* Eval::visitRootBlock99(Block* b)
  {


    // copy the block object (add items later)
    Block_Obj bb = SASS_MEMORY_NEW(Block,
      b->pstate(),
      b->length(),
      b->is_root());
    // setup block and env stack
    blockStack.emplace_back(bb);
    // operate on block
    // this may throw up!

    // Block* parent = blockStack.back();
    // if (b->is_root()) call_stack.emplace_back(b);
    for (Statement* item : b->elements()) {
      ValueObj child = item->perform(this);
      // if (parent && child) parent->append(child);
    }

    // revert block and env stack
    blockStack.pop_back();

    // return copy
    return bb.detach();
  }

  SelectorListObj& Eval::selector()
  {
    if (selectorStack.size() > 0) {
      auto& sel = selectorStack.back();
      if (sel.isNull()) return sel;
      return sel;
    }
    // Avoid the need to return copies
    // We always want an empty first item
    selectorStack.push_back({});
    return selectorStack.back();
  }

  SelectorListObj& Eval::original()
  {
    if (originalStack.size() > 0) {
      auto& sel = originalStack.back();
      if (sel.isNull()) return sel;
      return sel;
    }
    // Avoid the need to return copies
    // We always want an empty first item
    originalStack.push_back({});
    return originalStack.back();
  }

  bool Eval::isInContentBlock() const
  {
    return false;
  }

  // Whether we'   currently building the output of a style rule.

  bool Eval::isInStyleRule() {
    return !at_root_without_rule &&
      selectorStack.size() > 1;
    // return !_styleRule.isNull() &&
    //  !_atRootExcludingStyleRule;
  }


}
