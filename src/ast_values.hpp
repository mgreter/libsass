#ifndef SASS_AST_VALUES_H
#define SASS_AST_VALUES_H

// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"
#include "ast.hpp"

#include "logger.hpp"
#include "operators.hpp"
#include "environment.hpp"

namespace Sass {

  extern sass::string empty_string;

  const sass::string StrEmpty("");

  const sass::string StrTypeMap = "map";
  const sass::string StrTypeList = "list";
  const sass::string StrTypeNull = "null";
  const sass::string StrTypeBool = "bool";
  const sass::string StrTypeColor = "color";
  const sass::string StrTypeString = "string";
  const sass::string StrTypeNumber = "number";
  const sass::string StrTypeArgList = "arglist";
  const sass::string StrTypeFunction = "function";

  //////////////////////////////////////////////////////////////////////
  // base class for values that support operations
  //////////////////////////////////////////////////////////////////////
  class Value : public Expression {
    sass::vector<ValueObj> list;
  public:
    // We expect our implementors to hand over the parser state
    Value(SourceSpan&& pstate, bool d = false, bool e = false, bool i = false, Type ct = NONE);
    Value(const SourceSpan& pstate, bool d = false, bool e = false, bool i = false, Type ct = NONE);

    // Whether the value will be represented in CSS as the empty string.
    virtual bool isBlank() const { return false; }

    // Return the length of this item as a list
    virtual size_t lengthAsList() const { return 1; }

    virtual SassNumber* isNumber() { return nullptr; }
    virtual SassColor* isColor() { return nullptr; }
    virtual String_Constant* isString() { return nullptr; }
    virtual const SassNumber* isNumber() const { return nullptr; }
    virtual const SassColor* isColor() const { return nullptr; }
    virtual const String_Constant* isString() const { return nullptr; }

    virtual bool isVectorEmpty() const {
      return list.empty();
    }

    // ToDo: this creates a circular reference
    virtual const sass::vector<ValueObj>& asVector() {
      if (list.empty()) {
        list.emplace_back(this);
        list.back().makeWeak();
      }
      return list;
    }

    // Return the list separator
    virtual Sass_Separator separator() const {
      return SASS_UNDEF;
    }

    // Return the list separator
    virtual bool hasBrackets() {
      return false;
    }

    // Return the list separator
    virtual bool isTruthy() const {
      return true;
    }

    // Return the list separator
    virtual bool isNull() const {
      return false;
    }

    virtual Value* withoutSlash() override {
      return this;
    }

    virtual Value* removeSlash() override {
      return this;
    }

    // Return normalized index for vector from overflowable sass index
    size_t sassIndexToListIndex(Value* sassIndex, Logger& logger, const sass::string& name = StrEmpty);

    Value* assertValue(Logger& logger, const sass::string& name = StrEmpty) {
      return this;
    }

    virtual Color_RGBA* assertColor(Logger& logger, const sass::string& name = StrEmpty) {
      callStackFrame frame(logger,
        Backtrace(pstate()));
      throw Exception::SassScriptException2(
        to_string() + " is not a color.",
        logger, pstate(), name);
    }

    virtual Color_HSLA* assertColorHsla(Logger& logger, const sass::string& name = StrEmpty) {
      callStackFrame frame(logger,
        Backtrace(pstate()));
      throw Exception::SassScriptException2(
        to_string() + " is not a color.",
        logger, pstate(), name);
    }

    virtual SassFunction* assertFunction(Logger& logger, const sass::string& name = StrEmpty) {
      callStackFrame frame(logger,
        Backtrace(pstate()));
      throw Exception::SassScriptException2(
        to_string() + " is not a function reference.",
        logger, pstate(), name);
    }

    // SassFunction assertFunction(sass::string name = "") = >
    //   throw _exception("$this is not a function reference.", name);

    virtual Map* assertMap(Logger& logger, const sass::string& name = StrEmpty) {
      callStackFrame frame(logger,
        Backtrace(pstate()));
      throw Exception::SassScriptException2(
        to_string() + " is not a map.",
        logger, pstate(), name);
    }

    virtual Number* assertNumber(Logger& logger, const sass::string& name = StrEmpty) {
      callStackFrame frame(logger,
        Backtrace(pstate()));
      throw Exception::SassScriptException2(
        to_string() + " is not a number.",
        logger, pstate(), name);
    }

    virtual Number* assertNumberOrNull(Logger& logger, const sass::string& name = StrEmpty) {
      if (this->isNull()) return nullptr;
      return this->assertNumber(logger, name);
    }

    virtual String_Constant* assertString(Logger& logger, const SourceSpan& parent, const sass::string& name) {
      callStackFrame frame(logger,
        Backtrace(pstate()));
      throw Exception::SassScriptException2(
        inspect() + " is not a string.",
        logger, pstate(), name);
    }

    virtual String_Constant* assertStringOrNull(Logger& logger, const SourceSpan& parent, const sass::string& name) {
      if (this->isNull()) return nullptr;
      return this->assertString(logger, parent, name);
    }

    virtual SassArgumentList* assertArgumentList(Logger& logger, const sass::string& name = StrEmpty) {
      callStackFrame frame(logger,
        Backtrace(pstate()));
      throw Exception::SassScriptException2(
        to_string() + " is not an argument list.",
        logger, pstate(), name);
    }

    /// Converts a `selector-parse()`-style input into a string that can be
    /// parsed.
    ///
    /// Returns `null` if [this] isn't a type or a structure that can be parsed as
    /// a selector.
    bool _selectorStringOrNull(Logger& logger, sass::string& rv);


    /// Converts a `selector-parse()`-style input into a string that can be
    /// parsed.
    ///
    /// Throws a [SassScriptException] if [this] isn't a type or a structure that
    /// can be parsed as a selector.
    sass::string _selectorString(Logger& logger, const SourceSpan& pstate, const sass::string& name = StrEmpty) {
      sass::string str;
      if (_selectorStringOrNull(logger, str)) {
        return str;
      }

      throw Exception::SassScriptException2(
        to_sass() + " is not a valid selector: it must be a string,\n"
        "a list of strings, or a list of lists of strings.",
        logger, pstate_, name);
    }

    /// Parses [this] as a selector list, in the same manner as the
    /// `selector-parse()` function.
    ///
    /// Throws a [SassScriptException] if this isn't a type that can be parsed as a
    /// selector, or if parsing fails. If [allowParent] is `true`, this allows
    /// [ParentSelector]s. Otherwise, they're considered parse errors.
    ///
    /// If this came from a function argument, [name] is the argument name
    /// (without the `$`). It's used for error reporting.
    SelectorList* assertSelector(Context& ctx, const sass::string& name = StrEmpty, bool allowParent = false);


    /// Parses [this] as a compound selector, in the same manner as the
    /// `selector-parse()` function.
    ///
    /// Throws a [SassScriptException] if this isn't a type that can be parsed as a
    /// selector, or if parsing fails. If [allowParent] is `true`, this allows
    /// [ParentSelector]s. Otherwise, they're considered parse errors.
    ///
    /// If this came from a function argument, [name] is the argument name
    /// (without the `$`). It's used for error reporting.
    CompoundSelector* assertCompoundSelector(Context& ctx, const sass::string& name = StrEmpty, bool allowParent = false);

    // General 
    SassList* changeListContents(
      sass::vector<ValueObj> values,
      Sass_Separator separator,
      bool hasBrackets);

    // Pass explicit list separator
    SassList* changeListContents(
      sass::vector<ValueObj> values,
      Sass_Separator separator) {
      return changeListContents(values,
        separator, hasBrackets());
    }

    // Pass explicit brackets config
    SassList* changeListContents(
      sass::vector<ValueObj> values,
      bool hasBrackets) {
      return changeListContents(values,
        separator(), hasBrackets);
    }

    // Re-use current list settings
    SassList* changeListContents(
      sass::vector<ValueObj> values) {
      return changeListContents(values,
        separator(), hasBrackets());
    }

    /// The SassScript `>` operation.
    virtual bool greaterThan(Value* other, Logger& logger, const SourceSpan& pstate) const {
      throw Exception::SassScriptException2(
        "Undefined operation \"" + to_css()
        + " > " + other->to_css() + "\".",
        logger, pstate);
    }

    /// The SassScript `>=` operation.
    virtual bool greaterThanOrEquals(Value* other, Logger& logger, const SourceSpan& pstate) const {
      throw Exception::SassScriptException2(
        "Undefined operation \"" + to_css()
        + " >= " + other->to_css() + "\".",
        logger, pstate);
    }

    /// The SassScript `<` operation.
    virtual bool lessThan(Value* other, Logger& logger, const SourceSpan& pstate) const {
      throw Exception::SassScriptException2(
        "Undefined operation \"" + to_css()
        + " < " + other->to_css() + "\".",
        logger, pstate);
    }

    /// The SassScript `<=` operation.
    virtual bool lessThanOrEquals(Value* other, Logger& logger, const SourceSpan& pstate) const {
      throw Exception::SassScriptException2(
        "Undefined operation \"" + to_css()
        + " <= " + other->to_css() + "\".",
        logger, pstate);
    }

    /// The SassScript `*` operation.
    virtual Value* times(Value* other, Logger& logger, const SourceSpan& pstate) const {
      throw Exception::SassScriptException2(
        "Undefined operation \"" + to_css()
        + " * " + other->to_css() + "\".",
        logger, pstate);
    }

    /// The SassScript `%` operation.
    virtual Value* modulo(Value* other, Logger& logger, const SourceSpan& pstate) const {
      throw Exception::SassScriptException2(
        "Undefined operation \"" + to_css()
        + " % " + other->to_css() + "\".",
        logger, pstate);
    }

    /// Returns a valid CSS representation of [this].
    ///
    /// Throws a [SassScriptException] if [this] can't be represented in plain
    /// CSS. Use [toString] instead to get a string representation even if this
    /// isn't valid CSS.
    ///
    /// If [quote] is `false`, quoted strings are emitted without quotes.
    sass::string toCssString(bool quote = true) const;

    /// The SassScript `=` operation.
    virtual Value* singleEquals(Value* other, Logger& logger, const SourceSpan& pstate) const;

    /// The SassScript `+` operation.
    virtual Value* plus(Value* other, Logger& logger, const SourceSpan& pstate) const;

    /// The SassScript `-` operation.
    virtual Value* minus(Value* other, Logger& logger, const SourceSpan& pstate) const;

    /// The SassScript `/` operation.
    virtual Value* dividedBy(Value* other, Logger& logger, const SourceSpan& pstate) const;

    /// The SassScript unary `+` operation.
    virtual Value* unaryPlus(Logger& logger, const SourceSpan& pstate) const;

    /// The SassScript unary `-` operation.
    virtual Value* unaryMinus(Logger& logger, const SourceSpan& pstate) const;

    /// The SassScript unary `/` operation.
    virtual Value* unaryDivide(Logger& logger, const SourceSpan& pstate) const;

    /// The SassScript unary `not` operation.
    virtual Value* unaryNot(Logger& logger, const SourceSpan& pstate) const;

    // Some obects are not meant to be compared
    // ToDo: maybe fallback to pointer comparison?
    virtual bool operator== (const Value& rhs) const {
      throw Exception::UndefinedOperation(this, &rhs, Sass_OP::EQ);
    };
    virtual bool operator!= (const Value& rhs) const {
      throw Exception::UndefinedOperation(this, &rhs, Sass_OP::NEQ);
    };
    virtual bool operator< (const Value& rhs) const {
      throw Exception::UndefinedOperation(this, &rhs, Sass_OP::LT);
    }
    virtual bool operator> (const Value& rhs) const {
      throw Exception::UndefinedOperation(this, &rhs, Sass_OP::GT);
    }
    virtual bool operator<= (const Value& rhs) const {
      throw Exception::UndefinedOperation(this, &rhs, Sass_OP::LTE);
    }
    virtual bool operator>= (const Value& rhs) const {
      throw Exception::UndefinedOperation(this, &rhs, Sass_OP::GTE);
    }

    // We can give some reasonable implementations by using
    // inverst operators on the specialized implementations
    virtual bool operator== (const Expression& rhs) const override {
      if (const Value * value = Cast<Value>(&rhs)) {
        return *value  == *this;
      }
      throw Exception::UndefinedOperation(this, &rhs, Sass_OP::EQ);
    };

    ATTACH_VIRTUAL_COPY_OPERATIONS(Value);

  };

  ///////////////////////////////////////////////////////////////////////
  // Lists of values, both comma- and space-separated (distinguished by a
  // type-tag.) Also used to represent variable-length argument lists.
  ///////////////////////////////////////////////////////////////////////
  class SassList : public Value,
    public Vectorized<ValueObj> {
    virtual bool is_arglist() const { return false; }
  private:
    enum Sass_Separator separator_;
    ADD_PROPERTY(bool, hasBrackets);
  public:

    SassList(SourceSpan&& pstate,
      const sass::vector<ValueObj>& values = {},
      enum Sass_Separator seperator = SASS_SPACE,
      bool hasBrackets = false);

    SassList(SourceSpan&& pstate,
      sass::vector<ValueObj>&& values,
      enum Sass_Separator seperator = SASS_SPACE,
      bool hasBrackets = false);

    Sass_Separator separator() const override {
      return separator_;
    }

    void separator(Sass_Separator separator) {
      separator_ = separator;
    }

    bool isVectorEmpty() const override {
      return empty();
    }

    const sass::vector<ValueObj>& asVector() override {
      return elements(); // return reference
    }

    bool hasBrackets() override {
      return hasBrackets_;
    }

    Map* assertMap(Logger& logger, const sass::string& name = StrEmpty) override final;

    // Return the length of this item as a list
    size_t lengthAsList() const override {
      return length();
    }

    bool isBlank() const override final;

    virtual const sass::string& type() const override { return StrTypeList; }

    bool is_invisible() const override
    { return isBlank() && !hasBrackets(); }

    virtual size_t hash() const override;

    OVERRIDE_EQ_OPERATIONS(Value);
    ATTACH_CLONE_OPERATIONS(SassList);
    ATTACH_CRTP_PERFORM_METHODS();
  };

  class SassArgumentList : public SassList {
    KeywordMap<ValueObj> _keywords;
    bool _wereKeywordsAccessed;
  public:
    const sass::string& type() const override final{
      return StrTypeArgList;
    }
    bool is_arglist() const override final {
      return true;
    }
    SassArgumentList* assertArgumentList(Logger& logger, const sass::string& name = StrEmpty) override final {
      return this;
    }

    KeywordMap<ValueObj>& keywords() {
      _wereKeywordsAccessed = true;
      return _keywords;
    }

    bool wereKeywordsAccessed() const {
      return _wereKeywordsAccessed;
    }

    Map* keywordsAsSassMap() const;

    SassArgumentList(SourceSpan&& pstate,
      sass::vector<ValueObj>&& values = {},
      Sass_Separator sep = SASS_SPACE,
      KeywordMap<ValueObj>&& keywords = {});

    SassArgumentList(SourceSpan&& pstate,
      const sass::vector<ValueObj>& values = {},
      Sass_Separator sep = SASS_SPACE,
      const KeywordMap<ValueObj>& keywords = {});

    OVERRIDE_EQ_OPERATIONS(Value);
    ATTACH_CLONE_OPERATIONS(SassArgumentList);
    ATTACH_CRTP_PERFORM_METHODS();
  };

  ///////////////////////////////////////////////////////////////////////
  // Key value paris.
  ///////////////////////////////////////////////////////////////////////
  class Map : public Value, public Hashed<ValueObj, ValueObj> {
    sass::vector<ValueObj> list;
  public:
    Map(SourceSpan&& pstate);
    Map(SourceSpan&& pstate, const Hashed<ValueObj, ValueObj>& copy);
    Map(SourceSpan&& pstate, Hashed<ValueObj, ValueObj>&& move);
    const sass::string& type() const override final { return StrTypeMap; }
    bool is_invisible() const override { return empty(); }

    Map* assertMap(Logger& logger, const sass::string& name = StrEmpty) override { return this; }

    // Return the list separator
    Sass_Separator separator() const override final {
      return empty() ? SASS_UNDEF : SASS_COMMA;
    }

    // Return the length of this item as a list
    size_t lengthAsList() const override {
      return size();
    }

    bool isVectorEmpty() const override final {
      return elements_.empty();
    }

    const sass::vector<ValueObj>& asVector() override final {
      if (list.empty()) {
        for (auto kv : elements_) {
          list.emplace_back(SASS_MEMORY_NEW(
            SassList, kv.first->pstate(),
              sass::vector<ValueObj>{ kv.first, kv.second },
            SASS_SPACE));
        }
      }
      return list;
    }

    virtual size_t hash() const override;

    virtual bool operator== (const Value& rhs) const override;

    ATTACH_CLONE_OPERATIONS(Map)
    ATTACH_CRTP_PERFORM_METHODS()
  };


  //////////////////////////////////////////////////////////////////////////
  // Binary expressions. Represents logical, relational, and arithmetic
  // operations. Templatized to avoid large switch statements and repetitive
  // subclassing.
  //////////////////////////////////////////////////////////////////////////
  class Binary_Expression : public Expression {
  private:
    HASH_PROPERTY(Operand, op)
    HASH_PROPERTY(Expression_Obj, left)
    HASH_PROPERTY(Expression_Obj, right)
    HASH_PROPERTY(bool, allowsSlash)
    mutable size_t hash_;
  public:
    Binary_Expression(const SourceSpan& pstate,
                      Operand op, Expression_Obj lhs, Expression_Obj rhs);

    virtual size_t hash() const override;
    enum Sass_OP optype() const { return op_.operand; }
    // ATTACH_CLONE_OPERATIONS(Binary_Expression)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ////////////////////////////////////////////////////
  // Function reference.
  ////////////////////////////////////////////////////
  class SassFunction final : public Value {
  public:
    ADD_PROPERTY(CallableObj, callable);
  public:
    SassFunction(
      SourceSpan&& pstate,
      CallableObj callable);

    const sass::string& type() const override final { return StrTypeFunction; }
    bool is_invisible() const override { return true; }

    bool operator== (const Value& rhs) const override;

    SassFunction* assertFunction(Logger& logger, const sass::string& name = StrEmpty) override final { return this; }

    // ATTACH_CLONE_OPERATIONS(Function)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ///////////////////////
  // Variable references.
  ///////////////////////
  class Variable final : public Expression {
    ADD_CONSTREF(EnvString, name);
    ADD_PROPERTY(IdxRef, vidx);
    ADD_PROPERTY(IdxRef, pidx);
  public:
    Variable(const SourceSpan& pstate,
      const EnvString& name,
      IdxRef vidx, IdxRef pidx);
    virtual size_t hash() const override;
    // ATTACH_CLONE_OPERATIONS(Variable)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ////////////////////////////////////////////////
  // Numbers, percentages, dimensions, and colors.
  ////////////////////////////////////////////////
  class Number final : public Value, public Units {
    HASH_PROPERTY(double, value);
    // ADD_PROPERTY(double, epsilon);
    ADD_PROPERTY(bool, zero);

    // The representation of this number as two
    // slash-separated numbers, if it has one.
    ADD_CONSTREF(NumberObj, lhsAsSlash);
    ADD_CONSTREF(NumberObj, rhsAsSlash);



    mutable size_t hash_;
  public:
    Number(SourceSpan&& pstate, double val, const sass::string& u = StrEmpty, bool zero = true);
    Number(SourceSpan&& pstate, double val, Units&& units, bool zero = true);

    virtual SassNumber* isNumber() override final { return this; }
    virtual const SassNumber* isNumber() const override final { return this; }

    bool greaterThan(Value* other, Logger& logger, const SourceSpan& pstate) const override final {
      if (SassNumber* rhs = other->isNumber()) {
        // unitless or only having one unit are equivalent (3.4)
        // therefore we need to reduce the units beforehand
        size_t lhs_units = numerators.size() + denominators.size();
        size_t rhs_units = rhs->numerators.size() + rhs->denominators.size();
        if (!lhs_units || !rhs_units) {
          return value() > rhs->value();
        }
        Number l(*this), r(rhs); l.reduce(); r.reduce();
        // ensure both have same units
        l.normalize(); r.normalize();
        Units& lhs_unit = l, & rhs_unit = r;
        if (!(lhs_unit == rhs_unit)) {
          /* ToDo: do we always get usefull backtraces? */
          throw Exception::SassScriptException2(
            "Incompatible units " + lhs_unit.unit()
            + " and " + rhs_unit.unit() + ".",
            logger, pstate);
        }
        if (lhs_unit == rhs_unit) {
          return l.value() > r.value();
        }
        else {
          // ToDo: implement directly?
          return lhs_unit > rhs_unit;
        }
      }
      callStackFrame frame(logger, Backtrace(pstate));
      throw Exception::SassScriptException2(
        "Undefined operation \"" + to_css()
        + " > " + other->to_css() + "\".",
        logger, pstate);
    }

    bool greaterThanOrEquals(Value* other, Logger& logger, const SourceSpan& pstate) const override final {
      if (SassNumber * rhs = other->isNumber()) {
        // unitless or only having one unit are equivalent (3.4)
        // therefore we need to reduce the units beforehand
        size_t lhs_units = numerators.size() + denominators.size();
        size_t rhs_units = rhs->numerators.size() + rhs->denominators.size();
        if (!lhs_units || !rhs_units) {
          return value() >= rhs->value();
        }
        Number l(*this), r(rhs); l.reduce(); r.reduce();
        // ensure both have same units
        l.normalize(); r.normalize();
        Units& lhs_unit = l, & rhs_unit = r;
        if (!(lhs_unit == rhs_unit)) {
          /* ToDo: do we always get usefull backtraces? */
          throw Exception::SassScriptException2(
            "Incompatible units " + lhs_unit.unit()
            + " and " + rhs_unit.unit() + ".",
            logger, pstate);
        }
        if (lhs_unit == rhs_unit) {
          return l.value() >= r.value();
        }
        else {
          // ToDo: implement directly?
          return lhs_unit >= rhs_unit;
        }
      }
      callStackFrame frame(logger, Backtrace(pstate));
      throw Exception::SassScriptException2(
        "Undefined operation \"" + to_css()
        + " >= " + other->to_css() + "\".",
        logger, pstate);
    }

    bool lessThan(Value* other, Logger& logger, const SourceSpan& pstate) const override final {
      if (SassNumber * rhs = other->isNumber()) {
        // unitless or only having one unit are equivalent (3.4)
        // therefore we need to reduce the units beforehand
        size_t lhs_units = numerators.size() + denominators.size();
        size_t rhs_units = rhs->numerators.size() + rhs->denominators.size();
        if (!lhs_units || !rhs_units) {
          return value() < rhs->value();
        }
        Number l(*this), r(rhs); l.reduce(); r.reduce();
        // ensure both have same units
        l.normalize(); r.normalize();
        Units& lhs_unit = l, & rhs_unit = r;
        if (!(lhs_unit == rhs_unit)) {
          /* ToDo: do we always get usefull backtraces? */
          throw Exception::SassScriptException2(
            "Incompatible units " + lhs_unit.unit()
            + " and " + rhs_unit.unit() + ".",
            logger, pstate);
        }
        if (lhs_unit == rhs_unit) {
          return l.value() < r.value();
        }
        else {
          return lhs_unit < rhs_unit;
        }
      }
      callStackFrame frame(logger, Backtrace(pstate));
      throw Exception::SassScriptException2(
        "Undefined operation \"" + to_css()
        + " < " + other->to_css() + "\".",
        logger, pstate);
    }

    bool lessThanOrEquals(Value* other, Logger& logger, const SourceSpan& pstate) const override final {
      if (SassNumber * rhs = other->isNumber()) {
        // unitless or only having one unit are equivalent (3.4)
        // therefore we need to reduce the units beforehand
        size_t lhs_units = numerators.size() + denominators.size();
        size_t rhs_units = rhs->numerators.size() + rhs->denominators.size();
        if (!lhs_units || !rhs_units) {
          return value() <= rhs->value();
        }
        Number l(*this), r(rhs); l.reduce(); r.reduce();
        // ensure both have same units
        l.normalize(); r.normalize();
        Units& lhs_unit = l, & rhs_unit = r;
        if (!(lhs_unit == rhs_unit)) {
          /* ToDo: do we always get usefull backtraces? */
          throw Exception::SassScriptException2(
            "Incompatible units " + lhs_unit.unit()
            + " and " + rhs_unit.unit() + ".",
            logger, pstate);
        }
        if (lhs_unit == rhs_unit) {
          return l.value() <= r.value();
        }
        else {
          return lhs_unit <= rhs_unit;
        }
      }
      callStackFrame frame(logger, Backtrace(pstate));
      throw Exception::SassScriptException2(
        "Undefined operation \"" + to_css()
        + " <= " + other->to_css() + "\".",
        logger, pstate);
    }

    Value* modulo(Value* other, Logger& logger, const SourceSpan& pstate) const override final {
      if (const SassNumber * nr = other->isNumber()) {
        try { return Operators::op_numbers(Sass_OP::MOD, *this, *nr, pstate); }
        catch (Exception::IncompatibleUnits& e) {
          callStackFrame frame(logger, Backtrace(pstate));
          throw Exception::SassScriptException2(
            e.what(), logger, pstate);
        }
      }
      if (!other->isColor()) return Value::plus(other, logger, pstate);
      callStackFrame frame(logger, Backtrace(pstate));
      throw Exception::SassScriptException2(
        "Undefined operation \"" + to_css()
        + " % " + other->to_css() + "\".",
        logger, pstate);
    }

    Value* plus(Value* other, Logger& logger, const SourceSpan& pstate) const override final {
      if (const SassNumber * nr = other->isNumber()) {
        try { return Operators::op_numbers(Sass_OP::ADD, *this, *nr, pstate); }
        catch (Exception::IncompatibleUnits& e) {
          callStackFrame frame(logger, Backtrace(pstate));
          throw Exception::SassScriptException2(
            e.what(), logger, pstate);
        }
      }
      if (!other->isColor()) return Value::plus(other, logger, pstate);
      callStackFrame frame(logger, Backtrace(pstate));
      throw Exception::SassScriptException2(
        "Undefined operation \"" + to_css()
        + " + " + other->to_css() + "\".",
        logger, pstate);
    }

    Value* minus(Value* other, Logger& logger, const SourceSpan& pstate) const override final {
      if (const SassNumber * nr = other->isNumber()) {
        try { return Operators::op_numbers(Sass_OP::SUB, *this, *nr, pstate); }
        catch (Exception::IncompatibleUnits& e) {
          callStackFrame frame(logger, Backtrace(pstate));
          throw Exception::SassScriptException2(
            e.what(), logger, pstate);
        }
      }
      if (!other->isColor()) return Value::minus(other, logger, pstate);
      callStackFrame frame(logger, Backtrace(pstate));
      throw Exception::SassScriptException2(
        "Undefined operation \"" + to_css()
        + " - " + other->to_css() + "\".",
        logger, pstate);
    }

    Value* times(Value* other, Logger& logger, const SourceSpan& pstate) const override final {
      if (const SassNumber * nr = other->isNumber()) {
        try { return Operators::op_numbers(Sass_OP::MUL, *this, *nr, pstate); }
        catch (Exception::IncompatibleUnits& e) {
          callStackFrame frame(logger, Backtrace(pstate));
          throw Exception::SassScriptException2(
            e.what(), logger, pstate);
        }
      }
      if (!other->isColor()) return Value::times(other, logger, pstate);
      callStackFrame frame(logger, Backtrace(pstate));
      throw Exception::SassScriptException2(
        "Undefined operation \"" + to_css()
        + " * " + other->to_css() + "\".",
        logger, pstate);
    }

    Value* dividedBy(Value* other, Logger& logger, const SourceSpan& pstate) const override final {
      if (const SassNumber * nr = other->isNumber()) {
        try { return Operators::op_numbers(Sass_OP::DIV, *this, *nr, pstate); }
        catch (Exception::IncompatibleUnits& e) {
          callStackFrame frame(logger, Backtrace(pstate));
          throw Exception::SassScriptException2(
            e.what(), logger, pstate);
        }
      }
      if (!other->isColor()) return Value::dividedBy(other, logger, pstate);
      callStackFrame frame(logger, Backtrace(pstate));
      throw Exception::SassScriptException2(
        "Undefined operation \"" + to_css()
        + " / " + other->to_css() + "\".",
        logger, pstate);
    }

    Value* unaryPlus(Logger& logger, const SourceSpan& pstate) const override final {
      return SASS_MEMORY_COPY(this);
    }

    Value* unaryMinus(Logger& logger, const SourceSpan& pstate) const override final {
      SassNumber* cpy = SASS_MEMORY_COPY(this);
      cpy->value(cpy->value() * -1.0);
      return cpy;
    }

    long assertInt(Logger& logger, const SourceSpan& pstate, const sass::string& name = StrEmpty) {
      if (fuzzyIsInt(value_, logger.epsilon)) {
        return fuzzyAsInt(value_, logger.epsilon);
      }
      callStackFrame frame(logger, Backtrace(this->pstate()));
      throw Exception::SassScriptException2(
        inspect() + " is not an int.",
        logger, pstate, name);
    }

    Number* assertNoUnits(Logger& logger, const SourceSpan& pstate, const sass::string& name = StrEmpty) {
      if (!hasUnits()) return this;
      callStackFrame frame(logger, Backtrace(this->pstate()));
      throw Exception::SassScriptException2(
        "Expected " + inspect() + " to have no units.",
        logger, pstate, name);
    }

    bool hasUnit(const sass::string& unit) {
      return numerators.size() == 1 &&
        denominators.empty() &&
        numerators.front() == unit;
    }

    Value* withoutSlash() override final
    {
      if (!hasAsSlash()) return this;
      // we are the only holder of this item
      // therefore should be safe to alter it
      if (this->refcount == 1) {
        lhsAsSlash_.clear();
        rhsAsSlash_.clear();
        return this;
      }
      // Otherwise we need to make a copy first
      Number* copy = SASS_MEMORY_COPY(this);
      copy->lhsAsSlash_.clear();
      copy->rhsAsSlash_.clear();
      return copy;
    }


    Number* assertUnit(Logger& logger, const SourceSpan& pstate, const sass::string& unit, const sass::string& name = StrEmpty) {
      if (hasUnit(unit)) return this;
      callStackFrame frame(logger, Backtrace(this->pstate()));
      throw Exception::SassScriptException2(
        "Expected " + inspect() + " to have unit \"" + unit + "\".",
        logger, pstate, name);
    }


    Number* assertNumber(Logger& logger, const sass::string& name = StrEmpty) override {
      return this;
    }

    double valueInRange(double min, double max,
      Logger& logger, const SourceSpan& pstate, const sass::string& name = StrEmpty) const {
      double result = value_;
      if (!fuzzyCheckRange(value_, min, max, logger.epsilon, result)) {
        sass::sstream msg;
        msg << "Expected " << inspect() << " to be within ";
        msg << min << unit() << " and " << max << unit() << ".";
        callStackFrame frame(logger, Backtrace(this->pstate()));
        throw Exception::SassScriptException2(
          msg.str(), logger, pstate, name);
      }
      return result;
    }



    bool zero() { return zero_; }

    const sass::string& type() const override final { return StrTypeNumber; }

    bool hasAsSlash() {
      return !lhsAsSlash_.isNull()
        && !rhsAsSlash_.isNull();
    }

    // cancel out unnecessary units
    // result will be in input units
    void reduce();

    // normalize units to defaults
    // needed to compare two numbers
    void normalize();

    size_t hash() const override;

    bool operator< (const Number& rhs) const;
    bool operator> (const Number& rhs) const;
    bool operator<= (const Number& rhs) const;
    bool operator>= (const Number& rhs) const;
    bool operator== (const Number& rhs) const;


    bool operator== (const Value& rhs) const override;
    // COMPLEMENT_OPERATORS(Number)
    ATTACH_CLONE_OPERATIONS(Number)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  //////////
  // Colors.
  //////////
  class Color : public Value {
    ADD_CONSTREF(sass::string, disp)
    HASH_PROPERTY(double, a)
  protected:
    mutable size_t hash_;
  public:
    Color(const SourceSpan& pstate, double a = 1, const sass::string& disp = StrEmpty);
    Color(SourceSpan&& pstate, double a = 1, const sass::string& disp = StrEmpty);

    const sass::string& type() const override final { return StrTypeColor; }

    virtual size_t hash() const override = 0;

    virtual bool operator== (const Value& rhs) const override = 0;

    virtual Color_RGBA* copyAsRGBA() const = 0;
    virtual Color_RGBA* toRGBA() = 0;

    virtual Color_HSLA* copyAsHSLA() const = 0;
    virtual Color_HSLA* toHSLA() = 0;

    Value* plus(Value* other, Logger& logger, const SourceSpan& pstate) const override final {
      if (other->isNumber() || other->isColor()) {
        callStackFrame frame(logger, Backtrace(pstate));
        throw Exception::SassScriptException2(
          "Undefined operation \"" + to_css()
          + " + " + other->to_css() + "\".",
          logger, pstate);
      }
      return Value::plus(other, logger, pstate);
    }

    Value* minus(Value* other, Logger& logger, const SourceSpan& pstate) const override final {
      if (other->isNumber() || other->isColor()) {
        callStackFrame frame(logger, Backtrace(pstate));
        throw Exception::SassScriptException2(
          "Undefined operation \"" + to_css()
          + " - " + other->to_css() + "\".",
          logger, pstate);
      }
      return Value::minus(other, logger, pstate);
    }

    Value* dividedBy(Value* other, Logger& logger, const SourceSpan& pstate) const override final {
      if (other->isNumber() || other->isColor()) {
        callStackFrame frame(logger, Backtrace(pstate));
        throw Exception::SassScriptException2(
          "Undefined operation \"" + to_css()
          + " / " + other->to_css() + "\".",
          logger, pstate);
      }
      return Value::dividedBy(other, logger, pstate);
    }

    Value* modulo(Value* other, Logger& logger, const SourceSpan& pstate) const override final {
      callStackFrame frame(logger, Backtrace(pstate));
      throw Exception::SassScriptException2(
        "Undefined operation \"" + to_css()
        + " % " + other->to_css() + "\".",
        logger, pstate);
    }

    ATTACH_VIRTUAL_COPY_OPERATIONS(Color)
  };

  //////////
  // Colors.
  //////////
  class Color_RGBA final : public Color {
    HASH_PROPERTY(double, r)
    HASH_PROPERTY(double, g)
    HASH_PROPERTY(double, b)
  public:
    Color_RGBA(const SourceSpan& pstate, double r, double g, double b, double a = 1.0, const sass::string& disp = StrEmpty);
    Color_RGBA(SourceSpan&& pstate, double r, double g, double b, double a = 1.0, const sass::string& disp = StrEmpty);

    virtual Color_RGBA* isColor() override final { return this; }
    virtual const Color_RGBA* isColor() const override final { return this; }

    size_t hash() const override;

    Color_RGBA* copyAsRGBA() const override;
    Color_RGBA* toRGBA() override { return this; }

    Color_HSLA* copyAsHSLA() const override;
    Color_HSLA* toHSLA() override { return copyAsHSLA(); }

    Color_RGBA* assertColor(Logger& logger, const sass::string& name = StrEmpty) override {
      return this;
    }

    Color_HSLA* assertColorHsla(Logger& logger, const sass::string& name = StrEmpty) override {
      return toHSLA();
    }

    bool operator== (const Value& rhs) const override;

    ATTACH_CLONE_OPERATIONS(Color_RGBA)
    ATTACH_CRTP_PERFORM_METHODS()
  };


  //////////
  // Colors.
  //////////
  class Color_HSLA final : public Color {
    HASH_PROPERTY(double, h)
    HASH_PROPERTY(double, s)
    HASH_PROPERTY(double, l)
  public:
    Color_HSLA(SourceSpan&& pstate, double h, double s, double l, double a = 1, const sass::string& disp = StrEmpty);

    Color_RGBA* assertColor(Logger& logger, const sass::string& name = StrEmpty) override {
      return toRGBA();
    }

    Color_HSLA* assertColorHsla(Logger& logger, const sass::string& name = StrEmpty) override {
      return this;
    }

    size_t hash() const override;

    Color_RGBA* copyAsRGBA() const override;
    Color_RGBA* toRGBA() override { return copyAsRGBA(); }

    Color_HSLA* copyAsHSLA() const override;
    Color_HSLA* toHSLA() override { return this; }

    bool operator== (const Value& rhs) const override;

    ATTACH_CLONE_OPERATIONS(Color_HSLA)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  //////////////////////////////
  // Errors from Sass_Values.
  //////////////////////////////
  class Custom_Error final : public Value {
    ADD_CONSTREF(sass::string, message)
  public:
    Custom_Error(SourceSpan&& pstate, const sass::string& msg);
    bool operator== (const Value& rhs) const override;
    // ATTACH_CLONE_OPERATIONS(Custom_Error)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  //////////////////////////////
  // Warnings from Sass_Values.
  //////////////////////////////
  class Custom_Warning final : public Value {
    ADD_CONSTREF(sass::string, message)
  public:
    Custom_Warning(SourceSpan&& pstate, const sass::string& msg);
    bool operator== (const Value& rhs) const override;
    // ATTACH_CLONE_OPERATIONS(Custom_Warning)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ////////////
  // Booleans.
  ////////////
  class Boolean final : public Value {
    HASH_PROPERTY(bool, value)
    mutable size_t hash_;
  public:
    Boolean(SourceSpan&& pstate, bool val);
    operator bool() override { return value_; }

    // Return the list separator
    bool isTruthy() const override final {
      return value_;
    }

    Value* unaryNot(Logger& logger, const SourceSpan& pstate) const override final
    {
      return SASS_MEMORY_NEW(Boolean,
        pstate, !value_);
    }

    const sass::string& type() const override final { return StrTypeBool; }

    size_t hash() const override;

    bool is_false() override { return !value_; }

    bool operator== (const Value& rhs) const override;

    ATTACH_CLONE_OPERATIONS(Boolean)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ///////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////
  class Interpolation final : public Expression,
    public Vectorized<ExpressionObj> {

  public:
    const sass::string& getPlainString() const;
    const sass::string& getInitialPlain() const;

    virtual bool operator==(const Interpolation& rhs) const {
      return Vectorized::operator==(rhs);
    }
    virtual bool operator!=(const Interpolation& rhs) const {
      return Vectorized::operator!=(rhs);
    }

    Interpolation(const SourceSpan& pstate, Expression* ex = nullptr);
    // ATTACH_CLONE_OPERATIONS(Interpolation)
    ATTACH_CRTP_PERFORM_METHODS();
  };

  ///////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////
  class StringExpression final : public Expression {
    ADD_PROPERTY(InterpolationObj, text)
    ADD_PROPERTY(bool, hasQuotes)
  public:
    uint8_t findBestQuote();
    static StringExpression* plain(const SourceSpan& pstate, const sass::string& text, bool quotes = false);
    InterpolationObj getAsInterpolation(bool escape = false, uint8_t quote = 0);
    StringExpression(SourceSpan&& pstate, InterpolationObj text, bool quote = false);
    ATTACH_CLONE_OPERATIONS(StringExpression)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ///////////////////////////////////////////////////////////////////////
  // A native string wrapped as an expression
  ///////////////////////////////////////////////////////////////////////
  class StringLiteral final : public Value {
    ADD_CONSTREF(sass::string, text)
  public:
    StringLiteral(SourceSpan&& pstate, const sass::string& text);
    StringLiteral(SourceSpan&& pstate, sass::string&& text);
    ATTACH_CLONE_OPERATIONS(StringLiteral);
    ATTACH_CRTP_PERFORM_METHODS();
  };

  ////////////////////////////////////////////////////////
  // Flat strings -- the lowest level of raw textual data.
  ////////////////////////////////////////////////////////
  class String_Constant : public Value {
    HASH_CONSTREF(sass::string, value);
    ADD_PROPERTY(bool, hasQuotes);
  protected:
    mutable size_t hash_;
  public:
    virtual String_Constant* isString() override final { return this; }
    virtual const String_Constant* isString() const override final { return this; }

    String_Constant(SourceSpan&& pstate, const char* beg, bool hasQuotes = false);
    String_Constant(SourceSpan&& pstate, const sass::string& val, bool hasQuotes = false);
    String_Constant(SourceSpan&& pstate, sass::string&& val, bool hasQuotes = false);
    const sass::string& type() const override { return StrTypeString; }
    bool is_invisible() const override;
    size_t hash() const override;
    bool operator== (const Value& rhs) const override;
    // quotes are forced on inspection
    virtual sass::string inspect() const override;
    virtual bool isBlank() const override;
    String_Constant* assertString(Logger& logger, const SourceSpan& parent, const sass::string& name = StrEmpty) override final {
      return this;
    }

    Value* plus(Value* other, Logger& logger, const SourceSpan& pstate) const override final {
      if (const String_Constant * str = other->isString()) {
        sass::string text(value() + str->value());
        return SASS_MEMORY_NEW(String_Constant,
          pstate, std::move(text), hasQuotes());
      }
      sass::string text(value() + other->toCssString());
      return SASS_MEMORY_NEW(String_Constant,
        pstate, std::move(text), hasQuotes());
    }

    ATTACH_CLONE_OPERATIONS(String_Constant)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  //////////////////
  // The null value.
  //////////////////
  class Null final : public Value {
  public:
    Null(SourceSpan&& pstate);
    const sass::string& type() const override final { return StrTypeNull; }
    bool is_invisible() const override { return true; }
    operator bool() override { return false; }
    bool is_false() override { return true; }

    size_t hash() const override;

    bool isTruthy() const override final {
      return false;
    }

    bool isBlank() const override final {
      return true;
    }

    bool isNull() const override final {
      return true;
    }

    Value* unaryNot(Logger& logger, const SourceSpan& pstate) const override final
    {
      return SASS_MEMORY_NEW(Boolean,
        pstate, true);
    }

    bool operator== (const Value& rhs) const override;

    // ATTACH_CLONE_OPERATIONS(Null)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  //////////////////////////////////
  // The Parent Reference Expression.
  //////////////////////////////////
  class Parent_Reference final : public Value {
  public:
    Parent_Reference(SourceSpan&& pstate);
    // const sass::string& type() const override final { return "parent"; }
    bool operator==(const Value& rhs) const override {
      return true; // they are always equal
    };
    // ATTACH_CLONE_OPERATIONS(Parent_Reference)
    ATTACH_CRTP_PERFORM_METHODS()
  };

}

#endif
