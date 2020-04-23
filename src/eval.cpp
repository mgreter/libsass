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
#include "context.hpp"
#include "backtrace.hpp"
#include "debugger.hpp"
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


  Eval::Eval(Compiler& compiler) :
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
    compiler(compiler),
    extender(Extender::NORMAL, compiler.logger123->callStack),
    traces(*compiler.logger123)
  {

    mediaStack.push_back({});
    blockStack.emplace_back(nullptr);
    selectorStack.push_back({});
    originalStack.push_back({});

    bool_true = SASS_MEMORY_NEW(Boolean, SourceSpan::tmp("[TRUE]"), true);
    bool_false = SASS_MEMORY_NEW(Boolean, SourceSpan::tmp("[FALSE]"), false);
  }
  Eval::~Eval() { }

  bool Eval::isRoot() const
  {
    return blockStack.size() < 2;
  }

  void debug_call(
    UserDefinedCallable* callable,
    ArgumentResults& arguments) {

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
    ArgumentResults& evaluated,
    UserDefinedCallable* callable,
    UserDefinedCallable* content,
    bool isMixinCall,
    Value* (Eval::* run)(UserDefinedCallable*, Trace*),
    Trace* trace,
    const SourceSpan& pstate)
  {

    // On user defined fn we set the variables on the stack
    // ArgumentResults& evaluated = _evaluateArguments(arguments); // , false

    auto idxs = callable->declaration()->idxs();
    EnvScope scoped(compiler.varRoot, idxs);

    if (content) {
      auto cidx = content->declaration()->cidx();
      if (cidx.isValid()) {
        compiler.varRoot.setMixin(cidx, content);
      }
      else {
        std::cerr << "Invalid cidx1 on " << content << "\n";
      }
    }

    EnvKeyFlatMap<ValueObj>& named = evaluated.named();
    sass::vector<ValueObj>& positional = evaluated.positional();
    CallableDeclaration* declaration = callable->declaration();
    ArgumentDeclaration* declaredArguments = declaration->arguments();
    if (!declaredArguments) throw std::runtime_error("Mixin declaration has no arguments");
    const sass::vector<ArgumentObj>& declared = declaredArguments->arguments();

    // Create a new scope from the callable, outside variables are not visible?
    if (declaredArguments) declaredArguments->verify(positional.size(), named, pstate, traces);
    size_t minLength = std::min(positional.size(), declared.size());

    for (size_t i = 0; i < minLength; i++) {
      compiler.varRoot.setVariable(idxs->varFrame, (uint32_t)i,
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
      compiler.varRoot.setVariable(idxs->varFrame, (uint32_t)i, result);

      // callenv.set_local(
      //   argument->name(),
      //   result);
    }

    // bool isNamedEmpty = named.empty();
    ArgumentListObj argumentList;
    if (!declaredArguments->restArg().empty()) {
      sass::vector<ValueObj> values;
      if (positional.size() > declared.size()) {
        values = sublist(positional, declared.size());
      }
      Sass_Separator separator = evaluated.separator();
      if (separator == SASS_UNDEF) separator = SASS_COMMA;
      argumentList = SASS_MEMORY_NEW(ArgumentList,
        pstate, std::move(values), separator, std::move(named));
      auto size = declared.size();
      compiler.varRoot.setVariable(idxs->varFrame, (uint32_t)size, argumentList);
      // callenv.set_local(declaredArguments->restArg(), argumentList);
    }

    // sass::sstream invocation;
    // ArgumentInvocationObj args =
    //   visitArgumentInvocation(arguments);
    // invocation << callable->name();
    // invocation << args->toString();
    // callStackFrame frame(*compiler.logger123,
    //   BackTrace(pstate, callable->name()));

    ValueObj result = (this->*run)(callable, trace);

    // compiler.content = asd;

    // if (isNamedEmpty) return result.detach();
    // if (argumentList == nullptr) return result.detach();
    // if (argumentList->wereKeywordsAccessed()) return result.detach();

    return result.detach();

    // throw Exception::SassScriptException("Nonono");

  }

  Value* Eval::_runBuiltInCallable(
    ArgumentInvocation* arguments,
    BuiltInCallable* callable,
    const SourceSpan& pstate,
    bool selfAssign)
  {
    // On builtin we pass it to the function (has only positional args)
    ArgumentResults& evaluated(arguments->evaluated);
    _evaluateArguments(arguments, evaluated); // 12%
    EnvKeyFlatMap<ValueObj>& named(evaluated.named());
    // named.clear();
    sass::vector<ValueObj>& positional(evaluated.positional());
    const SassFnPair& tuple(callable->callbackFor(positional.size(), named)); // 0.13%

    ArgumentDeclaration* overload = tuple.first;
    const SassFnSig& callback = tuple.second;
    const sass::vector<ArgumentObj>& declaredArguments(overload->arguments());

    overload->verify(positional.size(), named, pstate, *compiler.logger123); // 0.66%

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
    ArgumentListObj argumentList;
    if (!overload->restArg().empty()) {
      sass::vector<ValueObj> rest;
      if (positional.size() > declaredArguments.size()) {
        rest = sublist(positional, declaredArguments.size());
        removeRange(positional, declaredArguments.size(), positional.size());
      }

      Sass_Separator separator = evaluated.separator();
      if (separator == SASS_UNDEF) separator = SASS_COMMA;
      argumentList = SASS_MEMORY_NEW(ArgumentList,
        pstate, std::move(rest), separator, std::move(named));
      positional.emplace_back(argumentList);
    }

    ValueObj result;
    // try {
//    double epsilon = compiler.logger->epsilon;
    // This one
    // sass::sstream invocation;
    // ArgumentInvocationObj args =
    //   visitArgumentInvocation(arguments);
    // invocation << callable->name();
    // invocation << args->toString();
    result = callback(pstate, positional, compiler, *compiler.logger123, *this, selfAssign); // 7%
      // }

    if (argumentList == nullptr) return result.detach();
    if (isNamedEmpty) return result.detach();
    /* if (argumentList.wereKeywordsAccessed) */ return result.detach();
    sass::sstream strm;
    strm << "No " << pluralize("argument", named.size());
    strm << " named " << toSentence(named, "or") << ".";
    throw Exception::SassRuntimeException2(
      strm.str(), *compiler.logger123);
  }



  Value* Eval::_runBuiltInCallables(
    ArgumentInvocation* arguments,
    BuiltInCallables* callable,
    const SourceSpan& pstate,
    bool selfAssign)
  {
    ArgumentResults& evaluated(arguments->evaluated);
    _evaluateArguments(arguments, evaluated); // 33%
    EnvKeyFlatMap<ValueObj>& named(evaluated.named());
    sass::vector<ValueObj>& positional(evaluated.positional());
    const SassFnPair& tuple(callable->callbackFor(positional.size(), named)); // 4.7%

    ArgumentDeclaration* overload = tuple.first;
    const SassFnSig& callback = tuple.second;
    const sass::vector<ArgumentObj>& declaredArguments(overload->arguments());

    overload->verify(positional.size(), named, pstate, *compiler.logger123); // 7.5%

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
    ArgumentListObj argumentList;
    if (!overload->restArg().empty()) {
      sass::vector<ValueObj> rest;
      if (positional.size() > declaredArguments.size()) {
        rest = sublist(positional, declaredArguments.size());
        removeRange(positional, declaredArguments.size(), positional.size());
      }

      Sass_Separator separator = evaluated.separator();
      if (separator == SASS_UNDEF) separator = SASS_COMMA;
      argumentList = SASS_MEMORY_NEW(ArgumentList,
        pstate, std::move(rest), separator, std::move(named));
      positional.emplace_back(argumentList);
    }

    ValueObj result;
    // try {
//    double epsilon = compiler.logger->epsilon;
    // This one
    // sass::sstream invocation;
    // ArgumentInvocationObj args =
    //   visitArgumentInvocation(arguments);
    // invocation << callable->name();
    // invocation << args->toString();
    result = callback(pstate, positional, compiler, *compiler.logger123, *this, selfAssign); // 13%
      // }

    // Collect the items
    evaluated.clear();


    if (argumentList == nullptr) return result.detach();
    if (isNamedEmpty) return result.detach();
    /* if (argumentList.wereKeywordsAccessed) */ return result.detach();
    sass::sstream strm;
    strm << "No " << pluralize("argument", named.size());
    strm << " named " << toSentence(named, "or") << ".";
    throw Exception::SassRuntimeException2(
      strm.str(), *compiler.logger123);
  }









  Value* Eval::_runExternalCallable(
    ArgumentInvocation* arguments,
    ExternalCallable* callable,
    const SourceSpan& pstate)
  {
    ArgumentResults& evaluated(arguments->evaluated);
    _evaluateArguments(arguments, evaluated);
    EnvKeyFlatMap<ValueObj>& named = evaluated.named();
    sass::vector<ValueObj>& positional = evaluated.positional();
    ArgumentDeclaration* overload = callable->declaration();

    sass::string name(callable->name());

//    return SASS_MEMORY_NEW(SassString, "[asd]", "Hossa");

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
    ArgumentListObj argumentList;
    if (!overload->restArg().empty()) {
      sass::vector<ValueObj> rest;
      if (positional.size() > declaredArguments.size()) {
        rest = sublist(positional, declaredArguments.size());
        removeRange(positional, declaredArguments.size(), positional.size());
      }

      Sass_Separator separator = evaluated.separator();
      if (separator == SASS_UNDEF) separator = SASS_COMMA;
      argumentList = SASS_MEMORY_NEW(ArgumentList,
        overload->pstate(),
        std::move(rest), separator, std::move(named));
      positional.emplace_back(argumentList);
    }
    // try {
    // double epsilon = std::pow(0.1, compiler.c_options.precision + 1);

    struct SassFunction* entry = callable->function();

    struct SassValue* c_args = sass_make_list(SASS_COMMA, false);
    for (size_t i = 0; i < positional.size(); i++) {
      sass_list_push(c_args, positional[i]->toSassValue());
    }

    struct SassValue* c_val =
      entry->function(c_args, entry, compiler.wrap());

    if (sass_value_get_tag(c_val) == SASS_ERROR) {
      sass::string message("error in C function " + name + ": ");// +sass_error_get_message(c_val));
      sass_delete_value(c_val);
      sass_delete_value(c_args);
      error(message, pstate, traces);
    }
    else if (sass_value_get_tag(c_val) == SASS_WARNING) {
      sass::string message("warning in C function " + name + ": ");// +sass_warning_get_message(c_val));
      sass_delete_value(c_val);
      sass_delete_value(c_args);
      error(message, pstate, traces);
    }
    ValueObj result(reinterpret_cast<Value*>(c_val));
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
    Sass_Inspect_Options serializeOpt(compiler);
    serializeOpt.output_style = SASS_STYLE_TO_CSS;
    SassOutputOptionsCpp out(serializeOpt);
    Inspect serialize(out, false);
    serialize.in_declaration = true;
    serialize.quotes = false;
    node->perform(&serialize);
    return serialize.get_buffer();
  }

  std::pair<
    sass::vector<ExpressionObj>,
    EnvKeyFlatMap<ExpressionObj>
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
    EnvKeyFlatMap<ExpressionObj> named = arguments->named();
    // var rest = invocation.arguments.rest.accept(this);
    ValueObj rest = arguments->restArg()->perform(this);

    if (Map* restMap = rest->isMap()) {
      _addRestMap2(named, restMap, arguments->restArg()->pstate());
    }
    else if (SassList * restList = rest->isList()) {
      for (Value* value : restList->elements()) {
        positional.emplace_back(SASS_MEMORY_NEW(
          ValueExpression, value->pstate(), value));
      }
      // separator = list->separator();
      if (ArgumentList * args = rest->isArgList()) {
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

    if (Map* restMap = keywordRest->isMap()) {
      _addRestMap2(named, restMap, arguments->kwdRest()->pstate());
      return std::make_pair(positional, named);
    }

    throw Exception::SassRuntimeException2(
      "Variable keyword arguments must be a map (was $keywordRest).",
      *compiler.logger123);

  }

  Value* Eval::visitChildren(const sass::vector<StatementObj>& children)
  {
    for (const auto& child : children) {
      ValueObj val = child->perform(this);
      if (val) return val.detach();
    }
    return nullptr;
  }

  Value* Eval::operator()(Return* r)
  {
    return r->value()->perform(this);
  }

  void Eval::visitWarnRule(WarnRule* node)
  {
    // SassOutputStyle outstyle = options().output_style;
    // options().output_style = NESTED;
    ValueObj message = node->expression()->perform(this);


    if (Callable* fn = compiler.varRoot.getLexicalFunction(Keys::warnRule)) {

      // We know that warn override function can only be external
      SASS_ASSERT(Cast<ExternalCallable*>, "External warn override");
      ExternalCallable* def = static_cast<ExternalCallable*>(fn);
      struct SassFunction* c_function = def->function();
      SassFunctionLambda c_func = sass_function_get_function(c_function);
      // EnvScope scoped(compiler.varRoot, def->idxs());

      struct SassValue* c_args = sass_make_list(SASS_COMMA, false);
      sass_list_push(c_args, message->toSassValue());
      struct SassValue* c_val = c_func(c_args, c_function, nullptr);
      sass_delete_value(c_args);
      sass_delete_value(c_val);

    }
    else {

      sass::string result(unquote(message->to_string()));


      BackTrace trace(node->pstate());
      callStackFrame frame(*compiler.logger123, trace);
      compiler.logger123->addWarn43(result, false);

    }
    // options().output_style = outstyle;

  }

  Value* Eval::operator()(WarnRule* node)
  {
    visitWarnRule(node);
    return nullptr;
  }

  void Eval::visitErrorRule(ErrorRule* node)
  {

    ValueObj message = node->expression()->perform(this);

    if (Callable* fn = compiler.varRoot.getLexicalFunction(Keys::errorRule)) {

      ExternalCallable* def = Cast<ExternalCallable>(fn);
      struct SassFunction* c_function = def->function();
      SassFunctionLambda c_func = sass_function_get_function(c_function);
      // EnvScope scoped(compiler.varRoot, def->idxs());

      struct SassValue* c_args = sass_make_list(SASS_COMMA, false);
      sass_list_push(c_args, message->toSassValue());
      struct SassValue* c_val = c_func(c_args, c_function, nullptr); // &compiler
      sass_delete_value(c_args);
      sass_delete_value(c_val);

    }
    else { 

      sass::string result(message->to_string());
      // options().output_style = outstyle;
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

    ValueObj message = node->expression()->perform(this);

    if (Callable* fn = compiler.varRoot.getLexicalFunction(Keys::debugRule)) {

      ExternalCallable* def = Cast<ExternalCallable>(fn);
      struct SassFunction* c_function = def->function();
      SassFunctionLambda c_func = sass_function_get_function(c_function);
      // EnvScope scoped(compiler.varRoot, def->idxs());

      struct SassValue* c_args = sass_make_list(SASS_COMMA, false);
      sass_list_push(c_args, message->toSassValue());
      struct SassValue* c_val = c_func(c_args, c_function, nullptr); // &compiler
      sass_delete_value(c_args);
      sass_delete_value(c_val);

    }
    else {

      sass::string result(unquote(message->inspect()));
      sass::string output_path(node->pstate().getDebugPath());
      // options().output_style = outstyle;

      // How to access otherwise?
      compiler.logger123->warnings <<
        output_path << ":" << node->pstate().getLine() << " DEBUG: " << result;
      compiler.logger123->warnings << STRMLF;

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
    // We have a bug lurking somewhere
    // without detach it gets deleted?
    ValueObj value = node->value();
    return value.detach();
  }

  Map* Eval::operator()(MapExpression* m)
  {
    ExpressionObj key;
    MapObj map(SASS_MEMORY_NEW(Map, m->pstate()));
    const sass::vector<ExpressionObj>& kvlist(m->kvlist());
    for (size_t i = 0, L = kvlist.size(); i < L; i += 2)
    {
      // First evaluate the key
      key = kvlist[i]->perform(this);
      // Check for key duplication
      if (map->has(key)) {
        traces.emplace_back(kvlist[i]->pstate());
        throw Exception::DuplicateKeyError(traces, *map, *key);
      }
      // Second insert the evaluated value for key
      map->insert(key, kvlist[i + 1]->perform(this));
    }
    return map.detach();
  }

  Map* Eval::operator()(Map* m)
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
    Number* rv = result->isNumber();
    Number* lnr = left->isNumber();
    Number* rnr = right->isNumber();
    if (rv) {
      if (allowSlash && left && right) {
        rv->lhsAsSlash(lnr);
        rv->rhsAsSlash(rnr);
      }
      else {
        rv->lhsAsSlash({});
        rv->lhsAsSlash({});
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
        right, *compiler.logger123, node->pstate());
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
      return left->greaterThan(right, *compiler.logger123, node->pstate())
        ? bool_true : bool_false;
    case Sass_OP::GTE:
      right = rhs->perform(this);
      return left->greaterThanOrEquals(right, *compiler.logger123, node->pstate())
        ? bool_true : bool_false;
    case Sass_OP::LT:
      right = rhs->perform(this);
      return left->lessThan(right, *compiler.logger123, node->pstate())
        ? bool_true : bool_false;
    case Sass_OP::LTE:
      right = rhs->perform(this);
      return left->lessThanOrEquals(right, *compiler.logger123, node->pstate())
        ? bool_true : bool_false;
    case Sass_OP::ADD:
      right = rhs->perform(this);
      return left->plus(right, *compiler.logger123, node->pstate());
    case Sass_OP::SUB:
      right = rhs->perform(this);
      return left->minus(right, *compiler.logger123, node->pstate());
    case Sass_OP::MUL:
      right = rhs->perform(this);
      return left->times(right, *compiler.logger123, node->pstate());
    case Sass_OP::DIV:
      right = rhs->perform(this);
      return doDivision(left, right,
        node->allowsSlash(), *compiler.logger123, node->pstate());
    case Sass_OP::MOD:
      right = rhs->perform(this);
      return left->modulo(right, *compiler.logger123, node->pstate());
    }
    // Satisfy compiler
    return nullptr;
  }

  Value* Eval::operator()(Unary_Expression* node)
  {
    ValueObj operand = node->operand()->perform(this);
    switch (node->optype()) {
    case Unary_Expression::Type::PLUS:
      return operand->unaryPlus(*compiler.logger123, node->pstate());
    case Unary_Expression::Type::MINUS:
      return operand->unaryMinus(*compiler.logger123, node->pstate());
    case Unary_Expression::Type::NOT:
      return operand->unaryNot(*compiler.logger123, node->pstate());
    case Unary_Expression::Type::SLASH:
      return operand->unaryDivide(*compiler.logger123, node->pstate());
    }
    // Satisfy compiler
    return nullptr;
  }

  /// Like `_environment.getFunction`, but also returns built-in
  /// globally-available functions.
  Callable* Eval::_getFunction(
    const IdxRef& fidx,
    const sass::string& name,
    const sass::string& ns)
  {
    return fidx.isValid() ?
      compiler.varRoot.getFunction(fidx) :
      compiler.varRoot.getLexicalFunction(name);
  }

  /// Like `_environment.getMixin`, but also returns built-in
  /// globally-available mixins.
  Callable* Eval::_getMixin(
    const IdxRef& midx,
    const EnvKey& name,
    const sass::string& ns)
  {
    return midx.isValid() ?
      compiler.varRoot.getMixin(midx) :
      compiler.varRoot.getLexicalMixin(name);
  }

  Value* Eval::_runWithBlock(UserDefinedCallable* callable, Trace* trace)
  {
    ValueObj result; // declare outside for loop to re-use
    CallableDeclaration* declaration = callable->declaration();
    for (Statement* statement : declaration->elements()) {
      // Makes sure results get cleaned up
      result = statement->perform(this);
    }
    return nullptr;
  }

  Value* BuiltInCallable::execute(Eval& eval, ArgumentInvocation* arguments, const SourceSpan& pstate, bool selfAssign)
  {
    BackTrace trace(pstate, name().orig(), true);
    callStackFrame frame(*eval.compiler.logger123, trace);
    ValueObj rv = eval._runBuiltInCallable(
      arguments, this, pstate, selfAssign);
    rv = rv->withoutSlash();
    return rv.detach();
  }

  Value* BuiltInCallables::execute(Eval& eval, ArgumentInvocation* arguments, const SourceSpan& pstate, bool selfAssign)
  {
    BackTrace trace(pstate, name().orig(), true);
    callStackFrame frame(*eval.compiler.logger123, trace);
    ValueObj rv = eval._runBuiltInCallables(
      arguments, this, pstate, selfAssign);
    rv = rv->withoutSlash();
    return rv.detach();
  }

  Value* UserDefinedCallable::execute(Eval& eval, ArgumentInvocation* arguments, const SourceSpan& pstate, bool selfAssign)
  {
    LocalOption<bool> inMixin(eval.inMixin, false);
    BackTrace trace(pstate, name().orig(), true);
    callStackFrame frame(*eval.compiler.logger123, trace);
    ArgumentResults& evaluated(arguments->evaluated);
    eval._evaluateArguments(arguments, evaluated);
    ValueObj rv = eval._runUserDefinedCallable(evaluated,
      this, nullptr, false, &Eval::_runAndCheck, nullptr, pstate);
    rv = rv->withoutSlash();
    return rv.detach();
  }

  Value* ExternalCallable::execute(Eval& eval, ArgumentInvocation* arguments, const SourceSpan& pstate, bool selfAssign)
  {
    BackTrace trace(pstate, name(), true);
    callStackFrame frame(*eval.compiler.logger123, trace);
    ValueObj rv = eval._runExternalCallable(
      arguments, this, pstate);
    rv = rv->withoutSlash();
    return rv.detach();
  }

  Value* PlainCssCallable::execute(Eval& eval, ArgumentInvocation* arguments, const SourceSpan& pstate, bool selfAssign)
  {
    if (!arguments->named().empty()) {
      callStackFrame frame(eval.traces,
        arguments->named().pstate());
      throw Exception::SassRuntimeException2(
        "Plain CSS functions don't support keyword arguments.",
        *eval.compiler.logger123);
    }
    if (arguments->kwdRest() != nullptr) {
      callStackFrame frame(eval.traces,
        arguments->kwdRest()->pstate());
      throw Exception::SassRuntimeException2(
        "Plain CSS functions don't support keyword arguments.",
        *eval.compiler.logger123);
    }
    bool addComma = false;
    sass::string strm;
    strm += name();
    strm += "(";
    for (Expression* argument : arguments->positional()) {
      if (addComma) { strm += ", "; }
      else { addComma = true; }
      strm += eval._evaluateToCss(argument);
    }
    if (ExpressionObj rest = arguments->restArg()) {
      rest = rest->perform(&eval);
      if (addComma) { strm += ", "; }
      else { addComma = true; }
      strm += eval._serialize(rest);
    }
    strm += ")";
    return SASS_MEMORY_NEW(
      SassString, pstate, strm);
  }
    

  Value* Eval::_runFunctionCallable(
    ArgumentInvocation* arguments,
    Callable* callable,
    const SourceSpan& pstate, bool selfAssign)
  {
    return callable->execute(*this, arguments, pstate, selfAssign);
  }

  Value* Eval::operator()(IfExpression* node)
  {
    auto pair = _evaluateMacroArguments(*node);
    const sass::vector<ExpressionObj>& positional(pair.first);
    const EnvKeyFlatMap<ExpressionObj>& named(pair.second);
    // We might fail if named arguments are missing or too few passed
    ExpressionObj condition = positional.size() > 0 ? positional[0] : named.at(Keys::$condition);
    ExpressionObj ifTrue = positional.size() > 1 ? positional[1] : named.at(Keys::$ifTrue);
    ExpressionObj ifFalse = positional.size() > 2 ? positional[2] : named.at(Keys::$ifFalse);
    ValueObj rv = condition ? condition->perform(this) : nullptr;
    ExpressionObj ex = rv && rv->isTruthy() ? ifTrue : ifFalse;
    return ex ? ex->perform(this) : nullptr;
  }

  Value* Eval::operator()(FunctionExpression* node)
  {
    // Function Expression might be simple and static, or dynamic CSS call
    const sass::string& plainName(node->name()->getPlainString());
    CallableObj function = node->fidx().isValid()
      ? compiler.varRoot.getFunction(node->fidx())
      : compiler.varRoot.getLexicalFunction(plainName);

    if (function == nullptr) {
      function = SASS_MEMORY_NEW(PlainCssCallable,
        node->pstate(), performInterpolation(node->name(), false));
    }

    LOCAL_FLAG(_inFunction, true);
    return _runFunctionCallable(
      node->arguments(), function, node->pstate(), node->selfAssign());
  }

  Value* Eval::operator()(Variable* v)
  {
    IdxRef vidx = v->vidx();
    if (vidx.isValid()) {
      Expression* ex = compiler.varRoot.getVariable(vidx);
      return ex->perform(this)->withoutSlash();
    }

    ValueObj ex = compiler.varRoot
      .getLexicalVariable(v->name());

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
    sass::vector<InterpolantObj> results;
    results.reserve(interpolation->length());
    InterpolationObj rv = SASS_MEMORY_NEW(
      Interpolation, interpolation->pstate());
    for (Interpolant* value : interpolation->elements()) {
      if (ItplString* lit = value->getItplString()) {
        results.emplace_back(lit);
      }
      else {
        ValueObj result = value->perform(this);
        if (result == nullptr) continue;
        if (result->isNull()) continue;
        results.emplace_back(result);
      }
    }
    rv->elementsM(std::move(results));
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
    mappings.emplace_back(Mapping(pstate.getSrcIdx(), pstate.position, Offset()));
    interpolation = evalInterpolation(interpolation, warnForColor);
    sass::string css(interpolation->to_css(mappings));
    return css;

    sass::vector<sass::string> results;
    for (auto value : interpolation->elements()) {

      ValueObj result = value->perform(this);
      if (result == nullptr) continue;
      if (result->isNull()) continue;

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

    return StringUtils::join(results, "");

  }


  /// Evaluates [interpolation].
  ///
  /// If [warnForColor] is `true`, this will emit a warning for any named color
  /// values passed into the interpolation.
  SourceData* Eval::performInterpolationToSource(InterpolationObj interpolation, bool warnForColor)
  {
    sass::vector<Mapping> mappings;
    SourceSpan pstate(interpolation->pstate());
    mappings.emplace_back(Mapping(pstate.getSrcIdx(), pstate.position, Offset()));
    interpolation = evalInterpolation(interpolation, warnForColor);
    return SASS_MEMORY_NEW(SourceItpl,
      interpolation->to_css(mappings),
      interpolation->pstate());

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
    if (trim) { StringUtils::makeTrimmed(result); }
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

  SassString* Eval::operator()(Interpolation* s)
  {
    return SASS_MEMORY_NEW(SassString, s->pstate(),
      interpolationToValue(s, false, true));
  }

  Value* Eval::operator()(StringExpression* node)
  {
    // Don't use [performInterpolation] here because we need to get
    // the raw text from strings, rather than the semantic value.
    InterpolationObj itpl = node->text();
    sass::vector<sass::string> strings;
    for (const auto& item : itpl->elements()) {
      if (ItplString * lit = item->getItplString()) {
        strings.emplace_back(lit->text());
      }
      else {
        ValueObj result = item->perform(this);
        if (SassString* lit = result->isString()) {
          strings.emplace_back(lit->value());
        }
        else if (!result->isNull()) {
          strings.emplace_back(serialize(result));
        }
      }
    }

    sass::string joined(StringUtils::join(strings, ""));

    return SASS_MEMORY_NEW(SassString, node->pstate(),
      std::move(joined), node->hasQuotes());

  }

  Value* Eval::operator()(SassString* s)
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

  /// Evaluates [condition] and converts it to a plain CSS string, with
  sass::string Eval::_visitSupportsCondition(SupportsCondition* condition)
  {
    if (auto operation = Cast<SupportsOperation>(condition)) {
      sass::string strm;
      SupportsOperation::Operand operand = operation->operand();
      strm += _parenthesize(operation->left(), operand);
      strm += (operand == SupportsOperation::AND ? " and " : " or ");
      strm += _parenthesize(operation->right(), operand);
      return strm;
    }
    else if (auto negation = Cast<SupportsNegation>(condition)) {
      return "not " + _parenthesize(negation->condition());
    }
    else if (auto interpolation = Cast<SupportsInterpolation>(condition)) {
      return _evaluateToCss(interpolation->value(), false);
    }
    else if (auto declaration = Cast<SupportsDeclaration>(condition)) {
      sass::string strm("(");
      strm += _evaluateToCss(declaration->feature()); strm += ": ";
      strm += _evaluateToCss(declaration->value()); strm += ")";
      return strm;
    }
    else {
      return Strings::empty;
    }

  }

  SassString* Eval::operator()(SupportsCondition* condition)
  {
    return SASS_MEMORY_NEW(SassString, condition->pstate(),
      _visitSupportsCondition(condition));
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
  void Eval::_addRestMap(EnvKeyFlatMap<ValueObj>& values, Map* map, const SourceSpan& pstate) {
    // convert ??= (value) = > value as T;

    for(auto kv : map->elements()) {
      if (SassString* str = kv.first->isString()) {
        values["$" + str->value()] = kv.second; // convert?
      }
      else {
        callStackFrame frame(*compiler.logger123, pstate);
        throw Exception::SassRuntimeException2(
          "Variable keyword argument map must have string keys.\n" +
          kv.first->inspect() + " is not a string in " + map->inspect() + ".",
          *compiler.logger123);
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
  void Eval::_addRestMap2(EnvKeyFlatMap<ExpressionObj>& values, Map* map, const SourceSpan& pstate) {
    // convert ??= (value) = > value as T;

    for (auto kv : map->elements()) {
      if (SassString* str = kv.first->isString()) {
        values["$" + str->value()] = SASS_MEMORY_NEW(
          ValueExpression, pstate, kv.second);
      }
      else {
        callStackFrame frame(*compiler.logger123, pstate);
        throw Exception::SassRuntimeException2(
          "Variable keyword argument map must have string keys.\n" +
          kv.first->inspect() + " is not a string in " + map->inspect() + ".",
          *compiler.logger123);
      }
    }
  }

  void Eval::_evaluateArguments(ArgumentInvocation* arguments, ArgumentResults& evaluated)
  {
    sass::vector<ValueObj>& positional(evaluated.positional());
    sass::vector<ExpressionObj>& positionalIn(arguments->positional());
    EnvKeyFlatMap<ValueObj>& named(evaluated.named());
    positional.resize(positionalIn.size());
    for (size_t i = 0, iL = positionalIn.size(); i < iL; i++) {
      positional[i] = positionalIn[i]->perform(this);
    }

    named.clear();
    // Reserving on ordered-map seems slower?
    keywordMapMap2(named, arguments->named());

    if (arguments->restArg() == nullptr) {
      evaluated.separator(SASS_UNDEF);
      return;
    }

    ValueObj rest = arguments->restArg()->perform(this);
    // var restNodeForSpan = trackSpans ? _expressionNode(arguments.rest) : null;

    Sass_Separator separator = SASS_UNDEF;

    if (Map * restMap = rest->isMap()) {
      _addRestMap(named, restMap, arguments->restArg()->pstate());
    }
    else if (SassList * list = rest->isList()) {
      std::copy(list->begin(), list->end(),
        std::back_inserter(positional));
      separator = list->separator();
      if (ArgumentList * args = rest->isArgList()) {
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
      evaluated.separator(separator);
      return;
    }

    ValueObj keywordRest = arguments->kwdRest()->perform(this);
    // var keywordRestNodeForSpan = trackSpans ? _expressionNode(arguments.keywordRest) : null;

    if (Map * restMap = keywordRest->isMap()) {
      _addRestMap(named, restMap, arguments->kwdRest()->pstate());
      evaluated.separator(separator);
      return;
    }
    else {
      error("Variable keyword arguments must be a map (was $keywordRest).",
        keywordRest->pstate(), traces);
    }

    throw std::runtime_error("thrown before");

  }

  Value* Eval::operator()(Argument* a)
  {
    ValueObj val = a->value()->perform(this);
    if (a->is_rest_argument()) {
      if(!Cast<SassList>(val)) {
        if (!Cast<Map>(val)) {
          SassList_Obj wrapper = SASS_MEMORY_NEW(SassList,
            val->pstate(), sass::vector<ValueObj> {}, SASS_COMMA);
          wrapper->append(val);
          return wrapper->perform(this);
        }
      }
    }
    return val.detach();
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
    for (Statement* statement : declaration->elements()) {
      // Normal statements in functions must return nullptr
      Value* value = statement->perform(this);
      if (value != nullptr) return value;
    }
    throw Exception::SassRuntimeException2(
      "Function finished without @return.",
      *compiler.logger123);
  }

  Value* Eval::visitSupportsRule(SupportsRule* node)
  {

    ValueObj condition(node->condition()->perform(this));
    EnvScope scoped(compiler.varRoot, node->idxs());

    sass::vector<StatementObj> children;
    blockStack.emplace_back(&children);
    visitChildren(node->elements());
    blockStack.pop_back();

    CssSupportsRuleObj ff = SASS_MEMORY_NEW(CssSupportsRule,
      node->pstate(), condition, std::move(children));
    blockStack.back()->push_back(ff);

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
    ExpressionObj mq;
    sass::string str_mq;
    SourceSpan state(node->pstate());
    if (node->query()) {
      state = node->query()->pstate();
      str_mq = performInterpolation(node->query(), false);
    }
    // char* str = sass_copy_c_string(str_mq.c_str());
    // compiler.strings.emplace_back(str);
    SourceDataObj source = SASS_MEMORY_NEW(SourceItpl,
      std::move(str_mq), state);
    MediaQueryParser parser(compiler, source);
    // Create a new CSS only representation of the media rule
    CssMediaRuleObj css = SASS_MEMORY_NEW(CssMediaRule,
      node->pstate(), sass::vector<CssMediaQueryObj>());
    css->CssParentNode::concat(node->elements());
    sass::vector<CssMediaQueryObj> parsed = parser.parse();
    if (mediaStack.size() && mediaStack.back()) {
      auto parent = mediaStack.back()->queries();
      css->concat(mergeMediaQueries(parent, parsed));
    }
    else {
      css->concat(parsed);
    }
    mediaStack.emplace_back(css);

    sass::vector<StatementObj> children;
    blockStack.emplace_back(&children);
    visitChildren(node->elements());
    blockStack.pop_back();
    css->elementsM(std::move(children));
    mediaStack.pop_back();

    // The parent to add declarations too
    blockStack.back()->push_back(css);
    // return css.detach();
    return nullptr;
  }

  Value* Eval::visitAtRootRule(AtRootRule* node)
  {

    EnvScope scoped(compiler.varRoot, node->idxs());


    // std::cerr << "visitAtRootRule\n";
    InterpolationObj itpl = node->query();
    AtRootQueryObj query;



    if (node->query()) {
      query = AtRootQuery::parse(
        performInterpolationToSource(
          node->query(), true),
        compiler);
    }
    else {
      query = AtRootQuery::defaultQuery(node->pstate());
    }

    LOCAL_FLAG(_inKeyframes, false);
    LOCAL_FLAG(_inUnknownAtRule, false);
    LOCAL_FLAG(at_root_without_rule,
      query && query->excludesStyleRules());

    sass::vector<StatementObj> children;
    blockStack.emplace_back(&children);
    visitChildren(node->elements());
    blockStack.pop_back();

    blockStack.back()->push_back(SASS_MEMORY_NEW(CssAtRootRule,
        node->pstate(), query, std::move(children)));

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
    if (trim) { StringUtils::makeTrimmed(result); } // ToDo: excludeEscape: true
    return SASS_MEMORY_NEW(CssString, interpolation->pstate(), result);
    // return CssValue(trim ? trimAscii(result, excludeEscape: true) : result,
    //   interpolation.span);
  }

  Value* Eval::visitAtRule(AtRule* node)
  {
    // The parent to add stuff too
    auto parent = blockStack.back();

    CssStringObj name = interpolationToCssString(node->name(), false, false);
    CssStringObj value = interpolationToCssString(node->value(), true, true);

    sass::string normalized(Util::unvendor(name->text()));
    bool isKeyframe = normalized == "keyframes";
    LOCAL_FLAG(_inUnknownAtRule, !isKeyframe);
    LOCAL_FLAG(_inKeyframes, isKeyframe);

    sass::vector<StatementObj> children;
    blockStack.emplace_back(&children);
    visitChildren(node->elements());
    blockStack.pop_back();

    CssAtRule* result = SASS_MEMORY_NEW(CssAtRule,
      node->pstate(), name, value, std::move(children));
    result->isChildless(node->is_childless());
    parent->push_back(result);

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

    // If the value is an empty list, preserve it, because converting it to CSS
    // will throw an error that we want the user to see.
    if (cssValue != nullptr && (!cssValue->value()->isBlank()
      || cssValue->value()->lengthAsList() == 0)) {
      blockStack.back()->push_back(SASS_MEMORY_NEW(CssDeclaration,
        node->pstate(), name, cssValue, is_custom_property));
    }
    else if (is_custom_property) {
      callStackFrame frame(*compiler.logger123, node->value()->pstate());
      throw Exception::SassRuntimeException2(
        "Custom property values may not be empty.",
        *compiler.logger123
      );
    }

    if (!node->empty()) {
      LocalOption<sass::string> ll1(_declarationName, name->text());
      for (Statement* child : node->elements()) {
        ValueObj result = child->perform(this);
      }
    }
    return nullptr;
  }

  Value* Eval::visitMapMerge(MapMerge* a)
  {
    return nullptr;
  }

  Value* Eval::visitVariableDeclaration(Assignment* a)
  {
    const IdxRef& vidx(a->vidx());
    const EnvKey& name(a->variable());
    SASS_ASSERT(vidx.isValid(), "Invalid VIDX");

    if (a->is_global()) {

      // Check if we are at the global scope
      if (compiler.varRoot.isGlobal()) {
        if (!compiler.varRoot.getGlobalVariable(name)) {
          compiler.logger123->addWarn33(
            "As of LibSass 4.1, !global assignments won't be able to declare new"
            " variables. Since this assignment is at the root of the stylesheet,"
            " the !global flag is unnecessary and can safely be removed.",
            a->pstate(), true);
        }
      }
      else {
        if (!compiler.varRoot.getGlobalVariable(name)) {
          compiler.logger123->addWarn33(
            "As of LibSass 4.1, !global assignments won't be able to declare new variables."
            " Consider adding `" + name.orig() + ": null` at the root of the stylesheet.",
            a->pstate(), true);
        }
      }

      // has global and default?
      if (a->is_default()) {
        ValueObj& value = compiler.varRoot.getVariable(vidx);
        if (value.isNull() || Cast<Null>(value)) {
          compiler.varRoot.setVariable(vidx,
            a->value()->perform(this));
        }
      }
      else {
        compiler.varRoot.setVariable(vidx,
          a->value()->perform(this));
      }

    }
    // has only default flag?
    else if (a->is_default()) {
      ValueObj& value = compiler.varRoot.getVariable(vidx);
      if (value.isNull() || Cast<Null>(value)) {
        compiler.varRoot.setVariable(vidx,
          a->value()->perform(this));
      }
    }
    // no global nor default
    else {
      compiler.varRoot.setVariable(vidx,
        a->value()->perform(this));
    }

    return nullptr;
  }

  Value* Eval::visitLoudComment(LoudComment* c)
  {
    if (_inFunction) return nullptr;
    sass::string text(performInterpolation(c->text(), false));
    bool preserve = text[2] == '!';
    blockStack.back()->push_back(SASS_MEMORY_NEW(CssComment, c->pstate(), text, preserve));
    return nullptr;
  }

  Value* Eval::visitIfRule(If* i)
  {
    ValueObj rv;

    EnvScope scoped(compiler.varRoot, i->idxs());

    ValueObj condition = i->predicate()->perform(this);
    if (condition->isTruthy()) {
      rv = visitChildren(i->elements());
    }
    else {
      for (auto alternative : i->alternatives()) {
        // Returns nullptr anyway?
        rv = alternative->perform(this);
      }
    }
    return rv.detach();

  }

  // For does not create a new env scope
  // But iteration vars are reset afterwards
  Value* Eval::visitForRule(For* f)
  {
    BackTrace trace(f->pstate(), Strings::forRule);
    callStackFrame(traces, trace);
    EnvScope scoped(compiler.varRoot, f->idxs());

    // const EnvKey& variable(f->variable());
    ValueObj low = f->lower_bound()->perform(this);
    ValueObj high = f->upper_bound()->perform(this);
    NumberObj sass_start = low->assertNumber(*compiler.logger123);
    NumberObj sass_end = high->assertNumber(*compiler.logger123);
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
    ValueObj val;
    if (start < end) {
      if (f->is_inclusive()) ++end;
      for (double i = start; i < end; ++i) {
        NumberObj it = SASS_MEMORY_NEW(Number, low->pstate(), i, sass_end->unit());
        compiler.varRoot.setVariable(f->idxs()->varFrame, 0, it);
        // env.set_local(variable, it);
        val = visitChildren(f->elements());
        if (val) break;
      }
    }
    else {
      if (f->is_inclusive()) --end;
      for (double i = start; i > end; --i) {
        NumberObj it = SASS_MEMORY_NEW(Number, low->pstate(), i, sass_end->unit());
        compiler.varRoot.setVariable(f->idxs()->varFrame, 0, it);
        // env.set_local(variable, it);
        val = visitChildren(f->elements());
        if (val) break;
      }
    }
    return val.detach();

  }

  Value* Eval::visitExtendRule(ExtendRule* e)
  {

    // Once we have blocks as parent classes we can use blockStack!
    if (!isInStyleRule() && !isInMixin() && !isInContentBlock()) {
      error(
        "@extend may only be used within style rules.",
        e->pstate(), traces);
    }

    InterpolationObj itpl = e->selector();

    sass::string text = interpolationToValue(itpl, true, false);

    SelectorListObj slist = itplToSelector(itpl,
      plainCss, blockStack.size() <= 2);

    if (slist) {

      for (auto complex : slist->elements()) {

        if (complex->length() != 1) {
          error("complex selectors may not be extended.", complex->pstate(), traces);
        }

        if (const CompoundSelector * compound = complex->first()->getCompound()) {

          if (compound->length() != 1) {

            sass::sstream sels; bool addComma = false;
            sels << "Compound selectors may no longer be extended. Consider `@extend ";
            for (auto sel : compound->elements()) {
              if (addComma) sels << ", ";
              sels << sel->to_string();
              addComma = true;
            }
            sels << "` instead. See http://bit.ly/ExtendCompound for details.";

            compiler.logger123->addWarn33(sels.str(), compound->pstate());

            // Make this an error once deprecation is over
            for (SimpleSelectorObj simple : compound->elements()) {
              // Pass every selector we ever see to extender (to make them findable for extend)
              extender.addExtension(selector(), simple, mediaStack.back(), e->isOptional());
            }

          }
          else {
            // Pass every selector we ever see to extender (to make them findable for extend)
            extender.addExtension(selector(), compound->first(), mediaStack.back(), e->isOptional());
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

    EnvScope scope(compiler.varRoot, r->idxs());

    Interpolation* itpl = r->interpolation();
    LocalOption<StyleRuleObj> oldStyleRule(_styleRule, r);

    if (_inKeyframes) {

      sass::vector<StatementObj> children;
      blockStack.emplace_back(&children);
      visitChildren(r->elements());
      blockStack.pop_back();

      auto text = interpolationToValue(itpl, true, false);
      
      auto qwe = SASS_MEMORY_NEW(SourceItpl,
        std::move(text), itpl->pstate());

      KeyframeSelectorParser parser(compiler, qwe);
      sass::vector<sass::string> selector(parser.parse());

      Keyframe_Rule_Obj k = SASS_MEMORY_NEW(Keyframe_Rule, r->pstate(), std::move(children));
      if (r->interpolation()) {
        selectorStack.push_back({});
        auto val = interpolationToValue(r->interpolation(), true, false);
        k->name2(SASS_MEMORY_NEW(SassString, r->interpolation()->pstate(), val));
        selectorStack.pop_back();
      }

      // blockStack.back()->push_back(k);
      blockStack.back()->push_back(k);

      return nullptr;
      // return k.detach();
    }

    SelectorListObj slist;
    if (r->interpolation()) {
      struct SassImport* imp = compiler.import_stack.back();
      bool plainCss = imp->format == SASS_IMPORT_CSS;
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
      extender.addSelector(evaled, mediaStack.back());

    sass::vector<StatementObj> children;
    blockStack.emplace_back(&children);
    visitChildren(r->elements());
    blockStack.pop_back();

    originalStack.pop_back();
    selectorStack.pop_back();

    CssStyleRule* rr = SASS_MEMORY_NEW(CssStyleRule,
      r->pstate(), evaled, std::move(children));

    rr->tabs(r->tabs());
    blockStack.back()->push_back(rr);
    return nullptr;

  }

  SelectorListObj Eval::itplToSelector(Interpolation* itpl, bool plainCss, bool allowParent)
  {

    sass::vector<Mapping> mappings;
    SourceSpan pstate(itpl->pstate());
    mappings.emplace_back(Mapping(pstate.getSrcIdx(), pstate.position, Offset()));
    InterpolationObj evaled = evalInterpolation(itpl, false);
    sass::string css(evaled->to_css(mappings));
    StringUtils::makeTrimmed(css);
    auto text = css;

    auto synthetic = SASS_MEMORY_NEW(SourceItpl,
      std::move(text), pstate);

    try {
      // Everything parsed, will be parsed from perspective of local content
      // Pass the source-map in for the interpolation, so the scanner can
      // update the positions according to previous source-positions
      // Is a parser state solely represented by a source map or do we
      // need an intermediate format for them?
      SelectorParser p2(compiler, synthetic);
      p2._allowPlaceholder = plainCss == false;
      p2._allowParent = allowParent && plainCss == false;
      return p2.parse();
    }
    catch (Exception::Base& err) {
      throw err;
    }
  }

  Value* Eval::visitEachRule(Each* e)
  {
    const IDXS* vidx(e->idxs());
    const sass::vector<EnvKey>& variables(e->variables());
    EnvScope scoped(compiler.varRoot, e->idxs());
    ValueObj expr = e->list()->perform(this);
    if (MapObj map = expr->isMap()) {
      Map::ordered_map_type els = map->elements(); // Copy ...
      for (auto kv : els) {
        ValueObj key = kv.first;
        ValueObj value = kv.second;
        if (variables.size() == 1) {
          SassList* variable = SASS_MEMORY_NEW(SassList,
            map->pstate(), { key, value }, SASS_SPACE);
          compiler.varRoot.setVariable(vidx->varFrame, 0, variable);
          // env.set_local(variables[0], variable);
        }
        else {
          compiler.varRoot.setVariable(vidx->varFrame, 0, key);
          compiler.varRoot.setVariable(vidx->varFrame, 1, value);
          // env.set_local(variables[0], key);
          // env.set_local(variables[1], value);
        }
        ValueObj val = visitChildren(e->elements());
        if (val) return val.detach();
      }
      return nullptr;
    }

    SassListObj list;
    if (SassList* slist = expr->isList()) {
      list = SASS_MEMORY_NEW(SassList, expr->pstate(),
        slist->elements(), slist->separator());
      list->hasBrackets(slist->hasBrackets());
    }
    else {
      list = SASS_MEMORY_NEW(SassList, expr->pstate(),
        { expr }, SASS_COMMA);
    }
    for (size_t i = 0, L = list->length(); i < L; ++i) {
      Value* item = list->get(i);
      // unwrap value if the expression is an argument
      // if (Argument * arg = Cast<Argument>(item)) item = arg->value();
      // check if we got passed a list of args (investigate)
      if (SassList* scalars = item->isList()) { // Ex
        if (variables.size() == 1) {
          compiler.varRoot.setVariable(vidx->varFrame, 0, scalars);
          // env.set_local(variables[0], scalars);
        }
        else {
          for (size_t j = 0, K = variables.size(); j < K; ++j) {
            Value* res = j >= scalars->length()
              ? SASS_MEMORY_NEW(Null, expr->pstate())
              : scalars->get(j)->perform(this);
            compiler.varRoot.setVariable(vidx->varFrame, (uint32_t)j, res);
            // env.set_local(variables[j], res);
          }
        }
      }
      else {
        if (variables.size() > 0) {
          // env.set_local(variables.at(0), item);
          compiler.varRoot.setVariable(vidx->varFrame, 0, item);
          for (size_t j = 1, K = variables.size(); j < K; ++j) {
            Value* res = SASS_MEMORY_NEW(Null, expr->pstate());
            compiler.varRoot.setVariable(vidx->varFrame, (uint32_t)j, res);
            // env.set_local(variables[j], res);
          }
        }
      }
      ValueObj val = visitChildren(e->elements());
      if (val) return val.detach();
    }

    return nullptr;
  }

  Value* Eval::visitWhileRule(WhileRule* node)
  {

    Expression* condition = node->condition();
    ValueObj result = condition->perform(this);

    EnvScope scoped(compiler.varRoot, node->idxs());

    // Evaluate the first run in outer scope
    // All successive runs are from innter scope
    if (result->isTruthy()) {

      while (true) {
        result = visitChildren(node->elements());
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
    if (!content88) return nullptr;
    UserDefinedCallable* content = content88;
    if (content == nullptr) return nullptr;
    LOCAL_FLAG(inMixin, false);


    // EnvScope scoped(compiler.varRoot, before->declaration()->idxs());
    Trace_Obj trace = SASS_MEMORY_NEW(Trace, c->pstate(), Strings::contentRule);
    blockStack.emplace_back(&trace->elements());

    callStackFrame frame(*compiler.logger123,
      BackTrace(c->pstate(), Strings::contentRule));

    // EnvSnapshotView view(compiler.varRoot, content->snapshot());
    EnvScope scoped(compiler.varRoot, content->declaration()->idxs()); // Not needed, but useful?

    LocalOption<UserDefinedCallable*> asdqwe(content88, content->content());

    // Appends to trace
    ArgumentResults& evaluated(c->arguments()->evaluated);
    _evaluateArguments(c->arguments(), evaluated);
    ValueObj qwe = _runUserDefinedCallable(
      evaluated,
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
    blockStack.back()->push_back(trace);
    return nullptr;

  }

  Value* Eval::visitMixinRule(MixinRule* rule)
  {

    UserDefinedCallableObj cb =
      SASS_MEMORY_NEW(UserDefinedCallable,
        rule->pstate(), rule->name(), rule, nullptr);

    if (rule->fidx().isValid()) {
      compiler.varRoot.setMixin(rule->fidx(), cb);
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
      compiler.varRoot.setFunction(rule->fidx(), cb);
      // return nullptr;
    }

    return nullptr;
  }

  Value* Eval::visitIncludeRule(IncludeRule* node)
  {
    UserDefinedCallableObj mixin = node->midx().isValid() ?
      compiler.varRoot.getMixin(node->midx()) :
      compiler.varRoot.getLexicalMixin(node->name());

    if (mixin == nullptr) {
      throw Exception::SassRuntimeException2(
        "Undefined mixin.",
        *compiler.logger123);
    }

    UserDefinedCallableObj contentCallable;

    if (node->content() != nullptr) {

      contentCallable = SASS_MEMORY_NEW(
        UserDefinedCallable, 
        node->pstate(), node->name(),
        node->content(), content88);

      MixinRule* rule = Cast<MixinRule>(mixin->declaration());
      node->content()->cidx(rule->cidx());

      if (!rule || !rule->has_content()) {
        SourceSpan span(node->content()->pstate());
        callStackFrame frame(*compiler.logger123, span);
        throw Exception::SassRuntimeException2(
          "Mixin doesn't accept a content block.",
          *compiler.logger123);
      }
    }

    Trace_Obj trace = SASS_MEMORY_NEW(Trace, node->pstate(), node->name().orig());
    blockStack.emplace_back(&trace->elements());
    LOCAL_FLAG(inMixin, true);

    callStackFrame frame(*compiler.logger123,
      BackTrace(node->pstate(), mixin->name().orig(), true));

    LocalOption<UserDefinedCallable*> asdqwe2(content88, contentCallable);

    ArgumentResults& evaluated(node->arguments()->evaluated);
    _evaluateArguments(node->arguments(), evaluated);
    ValueObj qwe = _runUserDefinedCallable(
      evaluated,
      mixin,
      nullptr,
      true,
      &Eval::_runWithBlock,
      trace,
      node->pstate());

    // env_stack.pop_back();
    blockStack.pop_back();

    // debug_ast(trace);
    blockStack.back()->push_back(trace);
    return nullptr;

  }


  Value* Eval::visitSilentComment(SilentComment* c)
  {
    blockStack.back()->push_back(c);
    return nullptr;
  }

  Value* Eval::visitIncludeImport99(IncludeImport* rule)
  {

    // Get the include loaded by parser
    const Include& include(rule->include());

    // This will error if the path was not loaded before
    // ToDo: Why don't we attach the sheet to include itself?
    const StyleSheet& sheet = compiler.sheets.at(include.abs_path);

    // Create C-API exposed object to query
    struct SassImport* import = sass_make_import(
      include.imp_path.c_str(),
      include.abs_path.c_str(),
      0, 0, sheet.syntax
    );

    // Add C-API to stack to expose it
    compiler.import_stack.emplace_back(import);
    // compiler.import_stack2.emplace_back(source);

    callStackFrame frame(traces,
      BackTrace(rule->pstate(), Strings::importRule));

    // Evaluate the included sheet
    append_block(sheet.root2);

    // These data object have just been borrowed
    // sass_import_take_source(compiler.import_stack.back());
    // sass_import_take_srcmap(compiler.import_stack.back());
    // Cleanup the import entry we added
    sass_delete_import(compiler.import_stack.back());
    // Finally remove if from the stack
    compiler.import_stack.pop_back();

    return nullptr;
  }

  Value* Eval::visitDynamicImport99(DynamicImport* rule)
  {
    // This should not be called
    // Should be IncludeImport now
    return nullptr;
  }

  sass::vector<CssMediaQueryObj> Eval::evalMediaQueries(Interpolation* itpl)
  {
    SourceDataObj synthetic = performInterpolationToSource(itpl, true);
    MediaQueryParser parser(compiler, synthetic);
    return parser.parse();
  }

  Value* Eval::visitStaticImport99(StaticImport* rule)
  {
    // Create new CssImport object 
    CssImportObj import = SASS_MEMORY_NEW(CssImport, rule->pstate(),
      interpolationToCssString(rule->url(), false, false));
    import->outOfOrder(rule->outOfOrder());

    if (rule->supports()) {
      if (auto supports = Cast<SupportsDeclaration>(rule->supports())) {
        sass::string feature(_evaluateToCss(supports->feature()));
        sass::string value(_evaluateToCss(supports->value()));
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
    blockStack.back()->push_back(import);
    // import has been consumed
    return nullptr;
  }

  // Consume all imports in this rule
  Value* Eval::visitImportRule99(ImportRule* rule)
  {
    ValueObj rv; // ensure to collect memory
    for (ImportBase* import : rule->elements()) {
      if (import) { rv = import->perform(this); }
    }
    return rv.detach();
  }

  // process and add to last block on stack
  void Eval::append_block(Root* block)
  {
    for (Statement* item : block->elements()) {
      item->perform(this);
    }
  }

  bool Eval::isInMixin()
  {
    return inMixin;
    // for (SassCallee& callee : compiler.callee_stack) {
    //   if (callee.type == SASS_CALLEE_MIXIN) return true;
    // }
    // return false;
  }

  Root* Eval::visitRoot32(Root* b)
  {
    // copy the block object (add items later)
    sass::vector<StatementObj> children;

    // setup block and env stack
    blockStack.emplace_back(&children);

    for (Statement* item : b->elements()) {
      ValueObj child = item->perform(this);
    }

    // revert block and env stack
    blockStack.pop_back();

    return SASS_MEMORY_NEW(Root,
      b->pstate(), std::move(children));

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

  bool Eval::isInStyleRule() const {
    return !at_root_without_rule &&
      selectorStack.size() > 1;
    // return !_styleRule.isNull() &&
    //  !_atRootExcludingStyleRule;
  }


}
