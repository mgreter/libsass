/*****************************************************************************/
/* Part of LibSass, released under the MIT license (See LICENSE.txt).        */
/*****************************************************************************/
#include "ast_nodes.hpp"

#include "sources.hpp"
#include "ast_css.hpp"
#include "ast_imports.hpp"
#include "compiler.hpp"
#include "exceptions.hpp"
#include "stylesheet.hpp"
#include "ast_values.hpp"
#include "ast_selectors.hpp"
#include "parser_selector.hpp"
#include "parser_at_root_query.hpp"

namespace Sass {

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Interpolant::Interpolant(
    const SourceSpan& pstate) :
    AstNode(pstate)
  {}

  Interpolant::Interpolant(
    SourceSpan&& pstate) :
    AstNode(std::move(pstate))
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  ItplString::ItplString(
    const SourceSpan& pstate,
    sass::string&& text) :
    Interpolant(pstate),
    text_(std::move(text))
  {}

  ItplString::ItplString(
    const SourceSpan& pstate,
    const sass::string& text) :
    Interpolant(pstate),
    text_(text)
  {}

  ///////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////

  Interpolation::Interpolation(
    const SourceSpan& pstate,
    Interpolant* interpolation) :
    AstNode(pstate)
  {
    if (interpolation != nullptr) {
      append(interpolation);
    }
  }

  ///////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////

  Expression::Expression(SourceSpan&& pstate)
    : Interpolant(std::move(pstate))
  {}

  ///////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////

  const sass::string& Interpolation::getPlainString() const
  {
    if (size() != 1) {
      return Strings::empty;
    }
    return first()->getText();
  }

  const sass::string& Interpolation::getInitialPlain() const
  {
    if (empty()) return Strings::empty;
    if (Interpolant* str = first()->isaItplString()) {
      return str->getText();
    }
    return Strings::empty;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  ImportBase::ImportBase(
    const SourceSpan& pstate) :
    AstNode(pstate)
  {}

  ImportBase::ImportBase(
    const ImportBase* ptr) :
    AstNode(ptr)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  StaticImport::StaticImport(
    const SourceSpan& pstate,
    InterpolationObj url,
    SupportsConditionObj supports,
    InterpolationObj media) :
    ImportBase(pstate),
    url_(url),
    supports_(supports),
    media_(media),
    outOfOrder_(true)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  IncludeImport::IncludeImport(
    const SourceSpan& pstate,
    StyleSheet* sheet) :
    ImportBase(pstate),
    sheet_(sheet)
  {}

  //////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////

  Values::iterator::iterator(Value* val, bool end) :
    val(val), last(0), cur(0)
  {
    if (val == nullptr) {
      type = NullPtrIterator;
    }
    else if (Map* map = val->isaMap()) {
      type = MapIterator;
      last = map->size();
    }
    else if (List* list = val->isaList()) {
      type = ListIterator;
      last = list->size();
    }
    else {
      type = SingleIterator;
      last = 1;
    }
    // Move to end position
    if (end) cur = last;
  }

  Values::iterator& Values::iterator::operator++()
  {
    switch (type) {
    case MapIterator:
      cur = std::min(cur + 1, last);
      break;
    case ListIterator:
      cur = std::min(cur + 1, last);
      break;
    case SingleIterator:
      cur = 1;
      break;
    case NullPtrIterator:
      break;
    }
    return *this;
  }

  Value* Values::iterator::operator*()
  {
    switch (type) {
    case MapIterator:
      return static_cast<Map*>(val)->getPairAsList(cur);
    case ListIterator:
      return static_cast<List*>(val)->get(cur);
    case SingleIterator:
      return val;
    case NullPtrIterator:
      return nullptr;
    }
    return nullptr;
  }

  bool Values::iterator::operator==(const iterator& other) const
  {
    return val == other.val && cur == other.cur;
  }

  bool Values::iterator::operator!=(const iterator& other) const
  {
    return val != other.val || cur != other.cur;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Standard value constructor
  Value::Value(const SourceSpan& pstate) :
    Interpolant(pstate),
    hash_(0)
  {}

  // Copy constructor
  Value::Value(const Value* ptr) :
    Interpolant(ptr->pstate()),
    hash_(0)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // The SassScript `>` operation.
  bool Value::greaterThan(Value* other, Logger& logger, const SourceSpan& pstate) const
  {
    logger.addFinalStackTrace(pstate);
    throw Exception::SassScriptException(
      "Undefined operation \"" + inspect()
      + " > " + other->inspect() + "\".",
      logger, pstate);
  }
  // EO greaterThan

  // The SassScript `>=` operation.
  bool Value::greaterThanOrEquals(Value* other, Logger& logger, const SourceSpan& pstate) const
  {
    logger.addFinalStackTrace(pstate);
    throw Exception::SassScriptException(
      "Undefined operation \"" + inspect()
      + " >= " + other->inspect() + "\".",
      logger, pstate);
  }
  // EO greaterThanOrEquals

  // The SassScript `<` operation.
  bool Value::lessThan(Value* other, Logger& logger, const SourceSpan& pstate) const
  {
    logger.addFinalStackTrace(pstate);
    throw Exception::SassScriptException(
      "Undefined operation \"" + inspect()
      + " < " + other->inspect() + "\".",
      logger, pstate);
  }
  // EO lessThan

  // The SassScript `<=` operation.
  bool Value::lessThanOrEquals(Value* other, Logger& logger, const SourceSpan& pstate) const
  {
    logger.addFinalStackTrace(pstate);
    throw Exception::SassScriptException(
      "Undefined operation \"" + inspect()
      + " <= " + other->inspect() + "\".",
      logger, pstate);
  }
  // EO lessThanOrEquals

  // The SassScript `*` operation.
  Value* Value::times(Value* other, Logger& logger, const SourceSpan& pstate) const
  {
    logger.addFinalStackTrace(pstate);
    throw Exception::SassScriptException(
      "Undefined operation \"" + inspect()
      + " * " + other->inspect() + "\".",
      logger, pstate);
  }
  // EO times

  // The SassScript `%` operation.
  Value* Value::modulo(Value* other, Logger& logger, const SourceSpan& pstate) const
  {
    logger.addFinalStackTrace(pstate);
    throw Exception::SassScriptException(
      "Undefined operation \"" + inspect()
      + " % " + other->inspect() + "\".",
      logger, pstate);
  }
  // EO sassIndexToListIndex

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // The SassScript `=` operation.
  Value* Value::singleEquals(Value* other, Logger& logger, const SourceSpan& pstate) const
  {
    return SASS_MEMORY_NEW(String, pstate,
      toCss(logger) + "=" + other->toCss(logger));
  }

  // The SassScript `+` operation.
  Value* Value::plus(Value* other, Logger& logger, const SourceSpan& pstate) const
  {
    if (String* str = other->isaString()) {
      sass::string text(toCss(logger));
      return SASS_MEMORY_NEW(String, pstate,
        text + str->value(),
        str->hasQuotes());
    }
    else {
      sass::string text(toCss(logger));
      return SASS_MEMORY_NEW(String, pstate,
        text + other->toCss(logger));
    }
  }

  // The SassScript `-` operation.
  Value* Value::minus(Value* other, Logger& logger, const SourceSpan& pstate) const
  {
    sass::string text(toCss(logger));
    return SASS_MEMORY_NEW(String, pstate,
      text + "-" + other->toCss(logger));
  }

  // The SassScript `/` operation.
  Value* Value::dividedBy(Value* other, Logger& logger, const SourceSpan& pstate) const
  {
    sass::string text(toCss(logger));
    return SASS_MEMORY_NEW(String, pstate,
      text + "/" + other->toCss(logger));
  }

  // The SassScript unary `+` operation.
  Value* Value::unaryPlus(Logger& logger, const SourceSpan& pstate) const
  {
    return SASS_MEMORY_NEW(String,
      pstate, "+" + toCss(logger));
  }

  // The SassScript unary `-` operation.
  Value* Value::unaryMinus(Logger& logger, const SourceSpan& pstate) const
  {
    return SASS_MEMORY_NEW(String,
      pstate, "-" + toCss(logger));
  }

  // The SassScript unary `/` operation.
  Value* Value::unaryDivide(Logger& logger, const SourceSpan& pstate) const
  {
    return SASS_MEMORY_NEW(String,
      pstate, "/" + toCss(logger));
  }

  // The SassScript unary `not` operation.
  Value* Value::unaryNot(Logger& logger, const SourceSpan& pstate) const
  {
    return SASS_MEMORY_NEW(Boolean,
      pstate, !isTruthy());
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Assert and return a value or throws if incompatible
  Value* Value::assertValue(Logger& logger, const sass::string& name)
  {
    return this; // Nothing to check here
  }

  // Assert and return a color or throws if incompatible
  const Color* Value::assertColor(Logger& logger, const sass::string& name) const
  {
    logger.addFinalStackTrace(pstate());
    throw Exception::SassScriptException(
      inspect() + " is not a color.",
      logger, pstate(), name);
  }

  // Assert and return a function or throws if incompatible
  Function* Value::assertFunction(Logger& logger, const sass::string& name)
  {
    logger.addFinalStackTrace(pstate());
    throw Exception::SassScriptException(
      inspect() + " is not a function reference.",
      logger, pstate(), name);
  }

  // Assert and return a map or throws if incompatible
  Map* Value::assertMap(Logger& logger, const sass::string& name)
  {
    logger.addFinalStackTrace(pstate());
    throw Exception::SassScriptException(
      inspect() + " is not a map.",
      logger, pstate(), name);
  }

  // Assert and return a number or throws if incompatible
  Number* Value::assertNumber(Logger& logger, const sass::string& name)
  {
    logger.addFinalStackTrace(pstate());
    throw Exception::SassScriptException(
      inspect() + " is not a number.",
      logger, pstate(), name);
  }

  // Assert and return a number/nullptr or throws if incompatible
  Number* Value::assertNumberOrNull(Logger& logger, const sass::string& name)
  {
    if (this->isNull()) return nullptr;
    return this->assertNumber(logger, name);
  }

  // Assert and return a string or throws if incompatible
  String* Value::assertString(Logger& logger, const sass::string& name)
  {
    logger.addFinalStackTrace(pstate());
    throw Exception::SassScriptException(
      inspect() + " is not a string.",
      logger, pstate(), name);
  }

  // Assert and return a string/nullptr or throws if incompatible
  String* Value::assertStringOrNull(Logger& logger, const sass::string& name)
  {
    if (this->isNull()) return nullptr;
    return this->assertString(logger, name);
  }

  // Assert and return a map/nullptr or throws if incompatible
  Map* Value::assertMapOrNull(Logger& logger, const sass::string& name)
  {
    if (this->isNull()) return nullptr;
    return this->assertMap(logger, name);
  }


  // Assert and return an argument list or throws if incompatible
  ArgumentList* Value::assertArgumentList(Logger& logger, const sass::string& name)
  {
    logger.addFinalStackTrace(pstate());
    throw Exception::SassScriptException(
      inspect() + " is not an argument list.",
      logger, pstate(), name);
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Return normalized index for vector from overflow-able sass index
  size_t Value::sassIndexToListIndex(Value* sassIndex, Logger& logger, const sass::string& name)
  {
    long index = sassIndex->assertNumber(logger, name)
      ->assertInt(logger, name);
    if (index == 0) throw Exception::SassScriptException(
      "List index may not be 0.", logger, sassIndex->pstate(), name);
    size_t size = lengthAsList();
    if (size < size_t(std::abs(index))) {
      sass::sstream strm;
      strm << "Invalid index " << index << " for a list ";
      strm << "with " << size << " elements.";
      throw Exception::SassScriptException(
        strm.str(), logger, sassIndex->pstate(), name);
    }
    return index < 0 ? size + index : size_t(index) - 1;
  }

  // Parses [this] as a selector list, in the same manner as the
  // `selector-parse()` function.
  ///
  // Throws a [SassScriptException] if this isn't a type that can be parsed as a
  // selector, or if parsing fails. If [allowParent] is `true`, this allows
  // [ParentSelector]s. Otherwise, they're considered parse errors.
  ///
  // If this came from a function argument, [name] is the argument name
  // (without the `$`). It's used for error reporting.
  SelectorList* Value::assertSelector(Compiler& compiler, const sass::string& name, bool allowParent) const
  {
    callStackFrame frame(compiler, pstate());
    sass::string text(getSelectorString(compiler, name));
    SourceDataObj source = SASS_MEMORY_NEW(SourceItpl, pstate(), std::move(text));
    SelectorParser parser(compiler, source, allowParent);
    return parser.parseSelectorList();
  }

  /// Parses [this] as a compound selector, in the same manner as the
  /// `selector-parse()` function.
  ///
  /// Throws a [SassScriptException] if this isn't a type that can be parsed as a
  /// selector, or if parsing fails. If [allowParent] is `true`, this allows
  /// [ParentSelector]s. Otherwise, they're considered parse errors.
  ///
  /// If this came from a function argument, [name] is the argument name
  /// (without the `$`). It's used for error reporting.
  CompoundSelector* Value::assertCompoundSelector(Compiler& compiler, const sass::string& name, bool allowParent) const
  {
    callStackFrame frame(compiler, pstate());
    sass::string text(getSelectorString(compiler, name));
    SourceDataObj source = SASS_MEMORY_NEW(SourceItpl, pstate(), std::move(text));
    SelectorParser parser(compiler, source, allowParent);
    return parser.parseCompoundSelector();
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Converts a `selector-parse()`-style input into a string that
  // can be parsed. Returns `false` if [this] isn't a type or a
  // structure that can be parsed as a selector.
  bool Value::selectorStringOrNull(Logger& logger, sass::string& rv) const
  {
    if (const String* str = isaString()) {
      rv = str->value();
      return true;
    }
    else if (const List* list = isaList()) {
      if (list->empty()) return false;
      sass::vector<sass::string> result;
      if (list->separator() == SASS_COMMA) {
        for (auto complex : list->elements()) {
          List* cplxLst = complex->isaList();
          String* cplxStr = complex->isaString();
          if (cplxStr) { result.emplace_back(cplxStr->value()); }
          else if (cplxLst && cplxLst->separator() == SASS_SPACE) {
            sass::string string = complex->getSelectorString(logger);
            if (string.empty()) return false;
            result.emplace_back(string);
          }
          else return false;
        }
      }
      else {
        for (auto compound : list->elements()) {
          String* cmpdStr = compound->isaString();
          if (cmpdStr) result.emplace_back(cmpdStr->value());
          else return false;
        }
      }
      rv = StringUtils::join(result, list->separator() == SASS_COMMA ? ", " : " ");
      return true;
    }
    return false;
  }
  // EO selectorStringOrNull

  // Converts a `selector-parse()`-style input into a string that
  // can be parsed. Throws a [SassScriptException] if [this] isn't
  // a type or a structure that can be parsed as a selector.
  sass::string Value::getSelectorString(Logger& logger, const sass::string& name) const
  {
    sass::string str;
    if (selectorStringOrNull(logger, str)) {
      return str;
    }
    throw Exception::SassScriptException(
      inspect() + " is not a valid selector: it must be a string,\n"
      "a list of strings, or a list of lists of strings.",
      logger, pstate(), name);
  }
  // EO selectorStringOrNull

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  AtRootQuery::AtRootQuery(
    SourceSpan&& pstate,
    StringSet&& names,
    bool include) :
    AstNode(std::move(pstate)),
    names_(std::move(names)),
    include_(include)
  {}

  // Whether this includes or excludes style rules.
  inline bool AtRootQuery::rule() const {
    return names_.count("rule") == 1;
  }

  // Whether this includes or excludes media rules.
  inline bool AtRootQuery::media() const {
    return names_.count("media") == 1;
  }

  // Whether this includes or excludes *all* rules.
  inline bool AtRootQuery::all() const {
    return names_.count("all") == 1;
  }

  // Returns whether [this] excludes a node with the given [name].
  bool AtRootQuery::excludesName(const sass::string& name) const {
    return (names_.count(name) == 1) != include();
  }

  // Returns whether [this] excludes [node].
  bool AtRootQuery::excludes(CssParentNode* node) const
  {
    if (all()) return !include();
    if (rule() && node->isaCssStyleRule()) return !include();
    return excludesName(node->getAtRuleName());
  }

  // Whether this excludes `@media` rules.
  // Note that this takes [include] into account.
  bool AtRootQuery::excludesMedia() const {
    return (all() || media()) != include();
  }

  // Whether this excludes style rules.
  // Note that this takes [include] into account.
  bool AtRootQuery::excludesStyleRules() const {
    return (all() || rule()) != include();
  }

  // Parses an at-root query from [contents]. If passed, [url]
  // is the name of the file from which [contents] comes.
  // Throws a [SassFormatException] if parsing fails.
  AtRootQuery* AtRootQuery::parse(SourceData* source, Compiler& ctx)
  {
    AtRootQueryParser parser(ctx, source);
    return parser.parse();
  }


  // The default at-root query, which excludes only style rules.
  AtRootQuery* AtRootQuery::defaultQuery(SourceSpan&& pstate)
  {
    StringSet wihtoutStyleRule;
    wihtoutStyleRule.insert("rule");
    return SASS_MEMORY_NEW(AtRootQuery, std::move(pstate),
      std::move(wihtoutStyleRule), false);
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

}