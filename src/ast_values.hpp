#ifndef SASS_AST_VALUES_H
#define SASS_AST_VALUES_H

// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"
#include "ast.hpp"

#include "logger.hpp"
#include "operators.hpp"
#include "environment.hpp"
#include "dart_helpers.hpp"

#define IMPLEMENT_BASE_DOWNCAST(Val, Fn) \
  virtual Val* Fn() { return nullptr; } \
  virtual const Val* Fn() const { return nullptr; } \

#define IMPLEMENT_DOWNCAST(Val, Fn) \
  Val* Fn() override final { return this; } \
  const Val* Fn() const override final { return this; } \

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

  // Helper class to iterator over Values.
  // Depending on the type of the Value (e.g. List vs String),
  // we either want to iterate over a container or a single value.
  class Values {

  public:

    // We know four value types
    enum Type {
      MapIterator,
      ListIterator,
      SingleIterator,
      NullPtrIterator,
    };

    // Internal iterator helper class
    class iterator
    {

    private:

      // The value we are iterating over
      Value* val;
      // The detected value/iterator type
      Type type;
      // The final item to iterator to
      // For null ptr this is zero, for
      // single items this is 1 and for
      // lists/maps the container size.
      size_t last;
      // The current iteration item
      size_t cur;

    public:

      // Some typedefs to satisfy C++ type traits
      typedef std::input_iterator_tag iterator_category;
      typedef iterator self_type;
      typedef Value* value_type;
      typedef Value* reference;
      typedef Value* pointer;
      typedef int difference_type;

      // Create iterator for start (or end)
      iterator(Value* val, bool end);

      // Copy constructor
      iterator(const iterator& it) :
        val(it.val),
        type(it.type),
        last(it.last),
        cur(it.cur) {}

      // Dereference current item
      reference operator*();

      // Move to the next item
      iterator& operator++();

      // Compare operators
      bool operator==(const iterator& other) const;
      bool operator!=(const iterator& other) const;

    };
    // EO class iterator

    // The value we are iterating over
    Value* val;
    // The detected value/iterator type
    Type type;

    // Standard value constructor
    Values(Value* val) : val(val) {}

    // Get iterator at the given position
    iterator begin() { return iterator(val, false); }
    iterator end() { return iterator(val, true); }

  };
  // EO class Values

  //////////////////////////////////////////////////////////////////////
  // base class for values that support operations
  //////////////////////////////////////////////////////////////////////
  class Value : public Expression {
  public:

    // Standard value constructor
    Value(const SourceSpan& pstate);

    // Whether the value will be represented in CSS as the empty string.
    virtual bool isBlank() const { return false; }

    // Convert to C-API pointer without implementation

    inline struct SassValue* toSassValue()
    {
      return reinterpret_cast<struct SassValue*>(this);
    }

    // Return the length of this item as a list
    virtual size_t lengthAsList() const { return 1; }

    virtual size_t hash() const = 0;

    virtual enum Sass_Tag getTag() const = 0;

    // We often need to downcast values to its special type
    // Virtual methods are faster than dynamic casting
    IMPLEMENT_BASE_DOWNCAST(Map, isMap);
    IMPLEMENT_BASE_DOWNCAST(SassList, isList);
    IMPLEMENT_BASE_DOWNCAST(Number, isNumber);
    IMPLEMENT_BASE_DOWNCAST(Color_RGBA, isColor);
    IMPLEMENT_BASE_DOWNCAST(Color_RGBA, isColorRGBA);
    IMPLEMENT_BASE_DOWNCAST(Color_HSLA, isColorHSLA);
    IMPLEMENT_BASE_DOWNCAST(Boolean, isBoolean);
    IMPLEMENT_BASE_DOWNCAST(Function, isFunction);
    IMPLEMENT_BASE_DOWNCAST(SassString, isString);
    IMPLEMENT_BASE_DOWNCAST(Custom_Error, isError);
    IMPLEMENT_BASE_DOWNCAST(Custom_Warning, isWarning);
    IMPLEMENT_BASE_DOWNCAST(ArgumentList, isArgList);

    // Only used for nth sass function
    // Single values act like lists with 1 item
    // Doesn't allow overflow of index (throw error)
    // Allows negative index but no overflow either
    virtual Value* getValueAt(Value* index, Logger& logger);

    virtual Values iterator() {
      return Values{ this };
    }

    virtual size_t indexOf(Value* value) {
      return *this == *value ? 0 : sass::string::npos;
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

    virtual Value* withoutSlash() {
      return this;
    }

    virtual Value* removeSlash() {
      return this;
    }

    // Return normalized index for vector from overflowable sass index
    size_t sassIndexToListIndex(Value* sassIndex, Logger& logger, const sass::string& name = StrEmpty);

    Value* assertValue(Logger& logger, const sass::string& name = StrEmpty) {
      return this;
    }

    virtual Color_RGBA* assertColor(Logger& logger, const sass::string& name = StrEmpty) {
      SourceSpan span(pstate());
      callStackFrame frame(logger, span);
      throw Exception::SassScriptException2(
        to_string() + " is not a color.",
        logger, pstate(), name);
    }

    virtual Color_HSLA* assertColorHsla(Logger& logger, const sass::string& name = StrEmpty) {
      SourceSpan span(pstate());
      callStackFrame frame(logger, span);
      throw Exception::SassScriptException2(
        to_string() + " is not a color.",
        logger, pstate(), name);
    }

    virtual Function* assertFunction(Logger& logger, const sass::string& name = StrEmpty) {
      SourceSpan span(pstate());
      callStackFrame frame(logger, span);
      throw Exception::SassScriptException2(
        to_string() + " is not a function reference.",
        logger, pstate(), name);
    }

    // Function assertFunction(sass::string name = "") = >
    //   throw _exception("$this is not a function reference.", name);

    virtual Map* assertMap(Logger& logger, const sass::string& name = StrEmpty) {
      SourceSpan span(pstate());
      callStackFrame frame(logger, span);
      throw Exception::SassScriptException2(
        to_string() + " is not a map.",
        logger, pstate(), name);
    }

    virtual Number* assertNumber(Logger& logger, const sass::string& name = StrEmpty) {
      SourceSpan span(pstate());
      callStackFrame frame(logger, span);
      throw Exception::SassScriptException2(
        to_string() + " is not a number.",
        logger, pstate(), name);
    }

    virtual Number* assertNumberOrNull(Logger& logger, const sass::string& name = StrEmpty) {
      if (this->isNull()) return nullptr;
      return this->assertNumber(logger, name);
    }

    virtual SassString* assertString(Logger& logger, const SourceSpan& parent, const sass::string& name) {
      SourceSpan span(pstate());
      callStackFrame frame(logger, span);
      throw Exception::SassScriptException2(
        inspect() + " is not a string.",
        logger, pstate(), name);
    }

    virtual SassString* assertStringOrNull(Logger& logger, const SourceSpan& parent, const sass::string& name) {
      if (this->isNull()) return nullptr;
      return this->assertString(logger, parent, name);
    }

    virtual ArgumentList* assertArgumentList(Logger& logger, const sass::string& name = StrEmpty) {
      SourceSpan span(pstate());
      callStackFrame frame(logger, span);
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

      // callStackFrame frame(logger, pstate_);
      throw Exception::SassScriptException2(
        inspect() + " is not a valid selector: it must be a string,\n"
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

    /// The SassScript `>` operation.
    virtual bool greaterThan(Value* other, Logger& logger, const SourceSpan& pstate) const {
      logger.callStack.push_back(pstate);
      throw Exception::SassScriptException2(
        "Undefined operation \"" + to_css()
        + " > " + other->to_css() + "\".",
        logger, pstate);
    }

    /// The SassScript `>=` operation.
    virtual bool greaterThanOrEquals(Value* other, Logger& logger, const SourceSpan& pstate) const {
      logger.callStack.push_back(pstate);
      throw Exception::SassScriptException2(
        "Undefined operation \"" + to_css()
        + " >= " + other->to_css() + "\".",
        logger, pstate);
    }

    /// The SassScript `<` operation.
    virtual bool lessThan(Value* other, Logger& logger, const SourceSpan& pstate) const {
      logger.callStack.push_back(pstate);
      throw Exception::SassScriptException2(
        "Undefined operation \"" + to_css()
        + " < " + other->to_css() + "\".",
        logger, pstate);
    }

    /// The SassScript `<=` operation.
    virtual bool lessThanOrEquals(Value* other, Logger& logger, const SourceSpan& pstate) const {
      logger.callStack.push_back(pstate);
      throw Exception::SassScriptException2(
        "Undefined operation \"" + to_css()
        + " <= " + other->to_css() + "\".",
        logger, pstate);
    }

    /// The SassScript `*` operation.
    virtual Value* times(Value* other, Logger& logger, const SourceSpan& pstate) const {
      logger.callStack.push_back(pstate);
      throw Exception::SassScriptException2(
        "Undefined operation \"" + to_css()
        + " * " + other->to_css() + "\".",
        logger, pstate);
    }

    /// The SassScript `%` operation.
    virtual Value* modulo(Value* other, Logger& logger, const SourceSpan& pstate) const {
      logger.callStack.push_back(pstate);
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


    // virtual sass::string toStringent(bool quote = true) const = 0;

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

    // Some objects are not meant to be compared
    // ToDo: maybe fall back to pointer comparison?
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
    // invert operators on the specialized implementations
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
    public Vectorized<Value> {
    virtual bool is_arglist() const { return false; }
  private:
    enum Sass_Separator separator_;
    ADD_PROPERTY(bool, hasBrackets);
  public:

    enum Sass_Tag getTag() const override final { return SASS_LIST; }

    SassList(const SourceSpan& pstate,
      const sass::vector<ValueObj>& values = {},
      enum Sass_Separator seperator = SASS_SPACE,
      bool hasBrackets = false);

    SassList(const SourceSpan& pstate,
      sass::vector<ValueObj>&& values,
      enum Sass_Separator seperator = SASS_SPACE,
      bool hasBrackets = false);

    size_t indexOf(Value* value) override final {
      return Sass::indexOf(elements(), value);
    }

    Sass_Separator separator() const override {
      return separator_;
    }

    void separator(Sass_Separator separator) {
      separator_ = separator;
    }

    bool hasBrackets() override {
      return hasBrackets_;
    }

    // Only used for nth sass function
    // Doesn't allow overflow of index (throw error)
    // Allows negative index but no overflow either
    Value* getValueAt(Value* index, Logger& logger) override final;

    SassList* isList() override final { return this; }
    const SassList* isList() const override final { return this; }

    Map* assertMap(Logger& logger, const sass::string& name = StrEmpty) override final;

    // Return the length of this item as a list
    size_t lengthAsList() const override {
      return length();
    }

    bool isBlank() const override final;

    virtual const sass::string& type() const override { return StrTypeList; }

    bool is_invisible() const override
    { return isBlank() && !hasBrackets(); }

    virtual size_t hash() const override final;

    OVERRIDE_EQ_OPERATIONS(Value);
    ATTACH_CLONE_OPERATIONS(SassList);
    ATTACH_CRTP_PERFORM_METHODS();
  };

  class ArgumentList : public SassList {
    EnvKeyFlatMap<ValueObj> _keywords;
    bool _wereKeywordsAccessed;
  public:
    const sass::string& type() const override final{
      return StrTypeArgList;
    }
    bool is_arglist() const override final {
      return true;
    }
    ArgumentList* isArgList() override final { return this; }
    const ArgumentList* isArgList() const override final { return this; }
    ArgumentList* assertArgumentList(Logger& logger, const sass::string& name = StrEmpty) override final {
      return this;
    }

    EnvKeyFlatMap<ValueObj>& keywords() {
      _wereKeywordsAccessed = true;
      return _keywords;
    }

    bool wereKeywordsAccessed() const {
      return _wereKeywordsAccessed;
    }

    Map* keywordsAsSassMap() const;

    ArgumentList(const SourceSpan& pstate,
      sass::vector<ValueObj>&& values = {},
      Sass_Separator sep = SASS_SPACE,
      EnvKeyFlatMap<ValueObj>&& keywords = {});

    ArgumentList(const SourceSpan& pstate,
      const sass::vector<ValueObj>& values = {},
      Sass_Separator sep = SASS_SPACE,
      const EnvKeyFlatMap<ValueObj>& keywords = {});

    OVERRIDE_EQ_OPERATIONS(Value);
    ATTACH_CLONE_OPERATIONS(ArgumentList);
    ATTACH_CRTP_PERFORM_METHODS();
  };

  ///////////////////////////////////////////////////////////////////////
  // Key value pairs.
  ///////////////////////////////////////////////////////////////////////
  class Map : public Value, public Hashed<ValueObj, ValueObj> {
    ValueObj pair;
  public:

    enum Sass_Tag getTag() const override final { return SASS_MAP; }

    Map(const SourceSpan& pstate);
    Map(const SourceSpan& pstate, const Hashed<ValueObj, ValueObj>& copy);
    Map(const SourceSpan& pstate, Hashed<ValueObj, ValueObj>&& move);

    Map(const SourceSpan& pstate, Hashed::ordered_map_type&& move);

    const sass::string& type() const override final { return StrTypeMap; }
    bool is_invisible() const override { return empty(); }

    Map* isMap() override final { return this; }
    const Map* isMap() const override final { return this; }

    Map* assertMap(Logger& logger, const sass::string& name = StrEmpty) override { return this; }

    // Only used for nth sass function
    // Doesn't allow overflow of index (throw error)
    // Allows negative index but no overflow either
    Value* getValueAt(Value* index, Logger& logger) override final;

    // Return the list separator
    Sass_Separator separator() const override final {
      return empty() ? SASS_UNDEF : SASS_COMMA;
    }

    // Return the length of this item as a list
    size_t lengthAsList() const override {
      return size();
    }

    size_t indexOf(Value* value) override final {
      // Implement without creating any copies
      if (SassList* list = value->isList()) {
        if (list->length() == 2) {
          Value* key = list->get(0);
          Value* val = list->get(1);
          size_t idx = 0;
          for (auto kv : elements_) {
            if (*kv.first == *key) {
              if (*kv.second == *val) {
                return idx;
              }
            }
            ++idx;
          }
        }
      }
      return sass::string::npos;
    }

    Value* get2(size_t idx);


    size_t hash() const override final;

    bool operator== (const Value& rhs) const override final;

    ATTACH_CLONE_OPERATIONS(Map)
    ATTACH_CRTP_PERFORM_METHODS()
  };


  //////////////////////////////////////////////////////////////////////////
  // Binary expressions. Represents logical, relational, and arithmetic
  // operations. Templatized to avoid large switch statements and repetitive
  // sub-classing.
  //////////////////////////////////////////////////////////////////////////
  class Binary_Expression : public Expression {
  private:
    HASH_PROPERTY(Operand, op)
    HASH_PROPERTY(ExpressionObj, left)
    HASH_PROPERTY(ExpressionObj, right)
    HASH_PROPERTY(bool, allowsSlash)
    mutable size_t hash_;
  public:
    Binary_Expression(const SourceSpan& pstate,
                      Operand op, ExpressionObj lhs, ExpressionObj rhs);

    enum Sass_OP optype() const { return op_.operand; }
    // ATTACH_CLONE_OPERATIONS(Binary_Expression)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ////////////////////////////////////////////////////
  // Function reference.
  ////////////////////////////////////////////////////
  class Function final : public Value {
  public:
    ADD_PROPERTY(CallableObj, callable);
  public:

    enum Sass_Tag getTag() const override final { return SASS_FUNCTION; }

    Function(
      const SourceSpan& pstate,
      CallableObj callable);

    size_t hash() const override final { return 0; }
    const sass::string& type() const override final { return StrTypeFunction; }
    bool is_invisible() const override { return true; }

    Function* isFunction() override final { return this; }
    const Function* isFunction() const override final { return this; }

    bool operator== (const Value& rhs) const override;

    Function* assertFunction(Logger& logger, const sass::string& name = StrEmpty) override final { return this; }

    // ATTACH_CLONE_OPERATIONS(Function)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ///////////////////////
  // Variable references.
  ///////////////////////
  class Variable final : public Expression {
    ADD_CONSTREF(EnvKey, name);
    ADD_PROPERTY(IdxRef, vidx);
  public:
    Variable(const SourceSpan& pstate,
      const EnvKey& name,
      IdxRef vidx);
    // ATTACH_CLONE_OPERATIONS(Variable)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ////////////////////////////////////////////////
  // Numbers, percentages, dimensions, and colors.
  ////////////////////////////////////////////////
  class Number : public Value, public Units {
    HASH_PROPERTY(double, value);
    // ADD_PROPERTY(double, epsilon);
    ADD_PROPERTY(bool, zero);

    // The representation of this number as two
    // slash-separated numbers, if it has one.
    ADD_CONSTREF(NumberObj, lhsAsSlash);
    ADD_CONSTREF(NumberObj, rhsAsSlash);



    mutable size_t hash_;
  public:
    enum Sass_Tag getTag() const override final { return SASS_NUMBER; }
    Number(const SourceSpan& pstate, double val, const sass::string& u = StrEmpty, bool zero = true);
    Number(const SourceSpan& pstate, double val, Units&& units, bool zero = true);

    virtual Number* isNumber() override final { return this; }
    virtual const Number* isNumber() const override final { return this; }

    bool greaterThan(Value* other, Logger& logger, const SourceSpan& pstate) const override final {
      if (Number* rhs = other->isNumber()) {
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
          /* ToDo: do we always get useful back-traces? */
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
      callStackFrame frame(logger, pstate);
      throw Exception::SassScriptException2(
        "Undefined operation \"" + to_css()
        + " > " + other->to_css() + "\".",
        logger, pstate);
    }

    bool greaterThanOrEquals(Value* other, Logger& logger, const SourceSpan& pstate) const override final {
      if (Number * rhs = other->isNumber()) {
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
          /* ToDo: do we always get useful back-traces? */
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
      callStackFrame frame(logger, pstate);
      throw Exception::SassScriptException2(
        "Undefined operation \"" + to_css()
        + " >= " + other->to_css() + "\".",
        logger, pstate);
    }

    bool lessThan(Value* other, Logger& logger, const SourceSpan& pstate) const override final {
      if (Number * rhs = other->isNumber()) {
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
          /* ToDo: do we always get useful back-traces? */
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
      callStackFrame frame(logger, pstate);
      throw Exception::SassScriptException2(
        "Undefined operation \"" + to_css()
        + " < " + other->to_css() + "\".",
        logger, pstate);
    }

    bool lessThanOrEquals(Value* other, Logger& logger, const SourceSpan& pstate) const override final {
      if (Number * rhs = other->isNumber()) {
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
          /* ToDo: do we always get useful back-traces? */
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
      callStackFrame frame(logger, pstate);
      throw Exception::SassScriptException2(
        "Undefined operation \"" + to_css()
        + " <= " + other->to_css() + "\".",
        logger, pstate);
    }

    Value* modulo(Value* other, Logger& logger, const SourceSpan& pstate) const override final {
      if (const Number * nr = other->isNumber()) {
        try { return Operators::op_numbers(Sass_OP::MOD, *this, *nr, pstate); }
        catch (Exception::IncompatibleUnits& e) {
          callStackFrame frame(logger, pstate);
          throw Exception::SassScriptException2(
            e.what(), logger, pstate);
        }
      }
      if (!other->isColor()) return Value::plus(other, logger, pstate);
      callStackFrame frame(logger, pstate);
      throw Exception::SassScriptException2(
        "Undefined operation \"" + to_css()
        + " % " + other->to_css() + "\".",
        logger, pstate);
    }

    Value* plus(Value* other, Logger& logger, const SourceSpan& pstate) const override final {
      if (const Number * nr = other->isNumber()) {
        try { return Operators::op_numbers(Sass_OP::ADD, *this, *nr, pstate); }
        catch (Exception::IncompatibleUnits& e) {
          callStackFrame frame(logger, pstate);
          throw Exception::SassScriptException2(
            e.what(), logger, pstate);
        }
      }
      if (!other->isColor()) return Value::plus(other, logger, pstate);
      callStackFrame frame(logger, pstate);
      throw Exception::SassScriptException2(
        "Undefined operation \"" + to_css()
        + " + " + other->to_css() + "\".",
        logger, pstate);
    }

    Value* minus(Value* other, Logger& logger, const SourceSpan& pstate) const override final {
      if (const Number * nr = other->isNumber()) {
        try { return Operators::op_numbers(Sass_OP::SUB, *this, *nr, pstate); }
        catch (Exception::IncompatibleUnits& e) {
          callStackFrame frame(logger, pstate);
          throw Exception::SassScriptException2(
            e.what(), logger, pstate);
        }
      }
      if (!other->isColor()) return Value::minus(other, logger, pstate);
      callStackFrame frame(logger, pstate);
      throw Exception::SassScriptException2(
        "Undefined operation \"" + to_css()
        + " - " + other->to_css() + "\".",
        logger, pstate);
    }

    Value* times(Value* other, Logger& logger, const SourceSpan& pstate) const override final {
      if (const Number * nr = other->isNumber()) {
        try { return Operators::op_numbers(Sass_OP::MUL, *this, *nr, pstate); }
        catch (Exception::IncompatibleUnits& e) {
          callStackFrame frame(logger, pstate);
          throw Exception::SassScriptException2(
            e.what(), logger, pstate);
        }
      }
      if (!other->isColor()) return Value::times(other, logger, pstate);
      callStackFrame frame(logger, pstate);
      throw Exception::SassScriptException2(
        "Undefined operation \"" + to_css()
        + " * " + other->to_css() + "\".",
        logger, pstate);
    }

    Value* dividedBy(Value* other, Logger& logger, const SourceSpan& pstate) const override final {
      if (const Number * nr = other->isNumber()) {
        try { return Operators::op_numbers(Sass_OP::DIV, *this, *nr, pstate); }
        catch (Exception::IncompatibleUnits& e) {
          callStackFrame frame(logger, pstate);
          throw Exception::SassScriptException2(
            e.what(), logger, pstate);
        }
      }
      if (!other->isColor()) return Value::dividedBy(other, logger, pstate);
      callStackFrame frame(logger, pstate);
      throw Exception::SassScriptException2(
        "Undefined operation \"" + to_css()
        + " / " + other->to_css() + "\".",
        logger, pstate);
    }

    Value* unaryPlus(Logger& logger, const SourceSpan& pstate) const override final {
      return SASS_MEMORY_COPY(this);
    }

    Value* unaryMinus(Logger& logger, const SourceSpan& pstate) const override final {
      Number* cpy = SASS_MEMORY_COPY(this);
      cpy->value(cpy->value() * -1.0);
      return cpy;
    }

    long assertInt(Logger& logger, const SourceSpan& pstate, const sass::string& name = StrEmpty) {
      if (fuzzyIsInt(value_, logger.epsilon)) {
        return lround(value_);
      }
      SourceSpan span(this->pstate());
      callStackFrame frame(logger, span);
      throw Exception::SassScriptException2(
        inspect() + " is not an int.",
        logger, pstate, name);
    }

    Number* assertNoUnits(Logger& logger, const SourceSpan& pstate, const sass::string& name = StrEmpty) {
      if (!hasUnits()) return this;
      SourceSpan span(this->pstate());
      callStackFrame frame(logger, span);
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
      SourceSpan span(this->pstate());
      callStackFrame frame(logger, span);
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
        SourceSpan span(this->pstate());
        callStackFrame frame(logger, span);
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

    size_t hash() const override final;

    bool operator< (const Number& rhs) const;
    bool operator> (const Number& rhs) const;
    bool operator<= (const Number& rhs) const;
    bool operator>= (const Number& rhs) const;
    bool operator== (const Number& rhs) const;


    bool operator== (const Value& rhs) const override final;
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
    enum Sass_Tag getTag() const override final { return SASS_COLOR; }
    Color(const SourceSpan& pstate, double a = 1, const sass::string& disp = StrEmpty);

    const sass::string& type() const override final { return StrTypeColor; }

    virtual size_t hash() const override = 0;

    virtual bool operator== (const Value& rhs) const override = 0;

    virtual Color_RGBA* copyAsRGBA() const = 0;
    virtual Color_RGBA* toRGBA() = 0;

    virtual Color_HSLA* copyAsHSLA() const = 0;
    virtual Color_HSLA* toHSLA() = 0;

    Value* plus(Value* other, Logger& logger, const SourceSpan& pstate) const override final {
      if (other->isNumber() || other->isColor()) {
        callStackFrame frame(logger, pstate);
        throw Exception::SassScriptException2(
          "Undefined operation \"" + to_css()
          + " + " + other->to_css() + "\".",
          logger, pstate);
      }
      return Value::plus(other, logger, pstate);
    }

    Value* minus(Value* other, Logger& logger, const SourceSpan& pstate) const override final {
      if (other->isNumber() || other->isColor()) {
        callStackFrame frame(logger, pstate);
        throw Exception::SassScriptException2(
          "Undefined operation \"" + to_css()
          + " - " + other->to_css() + "\".",
          logger, pstate);
      }
      return Value::minus(other, logger, pstate);
    }

    Value* dividedBy(Value* other, Logger& logger, const SourceSpan& pstate) const override final {
      if (other->isNumber() || other->isColor()) {
        callStackFrame frame(logger, pstate);
        throw Exception::SassScriptException2(
          "Undefined operation \"" + to_css()
          + " / " + other->to_css() + "\".",
          logger, pstate);
      }
      return Value::dividedBy(other, logger, pstate);
    }

    Value* modulo(Value* other, Logger& logger, const SourceSpan& pstate) const override final {
      callStackFrame frame(logger, pstate);
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

    virtual Color_RGBA* isColor() override final { return this; }
    virtual const Color_RGBA* isColor() const override final { return this; }

    Color_RGBA* isColorRGBA() override final { return this; }
    const Color_RGBA* isColorRGBA() const override final { return this; }

    size_t hash() const override final;

    Color_RGBA* copyAsRGBA() const override final;
    Color_RGBA* toRGBA() override final { return this; }

    Color_HSLA* copyAsHSLA() const override final;
    Color_HSLA* toHSLA() override final { return copyAsHSLA(); }

    Color_RGBA* assertColor(Logger& logger, const sass::string& name = StrEmpty) override final {
      return this;
    }

    Color_HSLA* assertColorHsla(Logger& logger, const sass::string& name = StrEmpty) override final {
      return toHSLA();
    }

    bool operator== (const Value& rhs) const override final;

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
    Color_HSLA(const SourceSpan& pstate, double h, double s, double l, double a = 1, const sass::string& disp = StrEmpty);
    
    Color_HSLA* isColorHSLA() override final { return this; }
    const Color_HSLA* isColorHSLA() const override final { return this; }

    Color_RGBA* assertColor(Logger& logger, const sass::string& name = StrEmpty) override final {
      return toRGBA();
    }

    Color_HSLA* assertColorHsla(Logger& logger, const sass::string& name = StrEmpty) override final {
      return this;
    }

    size_t hash() const override final;

    Color_RGBA* copyAsRGBA() const override final;
    Color_RGBA* toRGBA() override final { return copyAsRGBA(); }

    Color_HSLA* copyAsHSLA() const override final;
    Color_HSLA* toHSLA() override final { return this; }

    bool operator== (const Value& rhs) const override final;

    ATTACH_CLONE_OPERATIONS(Color_HSLA)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  //////////////////////////////
  // Errors from Sass_Values.
  //////////////////////////////
  class Custom_Error final : public Value {
    ADD_CONSTREF(sass::string, message)
  public:
    enum Sass_Tag getTag() const override final { return SASS_ERROR; }
    size_t hash() const override final { return 0; }
    Custom_Error(const SourceSpan& pstate, const sass::string& msg);
    Custom_Error* isError() override final { return this; }
    const Custom_Error* isError() const override final { return this; }
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
    enum Sass_Tag getTag() const override final { return SASS_WARNING; }
    size_t hash() const override final { return 0; }
    Custom_Warning(const SourceSpan& pstate, const sass::string& msg);
    Custom_Warning* isWarning() override final { return this; }
    const Custom_Warning* isWarning() const override final { return this; }
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
    enum Sass_Tag getTag() const override final { return SASS_BOOLEAN; }
    Boolean(const SourceSpan& pstate, bool val);

    Boolean* isBoolean() override final { return this; }
    const Boolean* isBoolean() const override final { return this; }

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

    size_t hash() const override final;

    bool operator== (const Value& rhs) const override final;

    ATTACH_CLONE_OPERATIONS(Boolean)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ///////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////
  class Interpolation final : public AST_Node,
    public VectorizedNopsi<Interpolant> {

  public:
    const sass::string& getPlainString() const;
    const sass::string& getInitialPlain() const;

    virtual bool operator==(const Interpolation& rhs) const {
      return VectorizedNopsi::operator==(rhs);
    }
    virtual bool operator!=(const Interpolation& rhs) const {
      return VectorizedNopsi::operator!=(rhs);
    }

    Interpolation(const SourceSpan& pstate, Interpolant* ex = nullptr);

    Interpolation(
      const SourceSpan& pstate,
      sass::vector<InterpolantObj> items);

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
    StringExpression(const SourceSpan& pstate, InterpolationObj text, bool quote = false);
    StringExpression(SourceSpan&& pstate, InterpolationObj text, bool quote = false);
    ATTACH_CLONE_OPERATIONS(StringExpression)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ////////////////////////////////////////////////////////
  // Flat strings -- the lowest level of raw textual data.
  ////////////////////////////////////////////////////////
  class SassString : public Value {
    HASH_CONSTREF(sass::string, value);
    ADD_PROPERTY(bool, hasQuotes);
  protected:
    mutable size_t hash_;
  public:
    enum Sass_Tag getTag() const override final { return SASS_STRING; }
    virtual SassString* isString() override final { return this; }
    virtual const SassString* isString() const override final { return this; }
    const sass::string& getText() const override final { return value_; }

    SassString(const SourceSpan& pstate, const char* beg, bool hasQuotes = false);
    SassString(const SourceSpan& pstate, const sass::string& val, bool hasQuotes = false);
    SassString(const SourceSpan& pstate, sass::string&& val, bool hasQuotes = false);
    const sass::string& type() const override { return StrTypeString; }
    bool is_invisible() const override;
    size_t hash() const override;
    bool operator== (const Value& rhs) const override;
    bool operator== (const SassString& rhs) const;
    // quotes are forced on inspection
    virtual sass::string inspect() const override;
    virtual bool isBlank() const override;
    SassString* assertString(Logger& logger, const SourceSpan& parent, const sass::string& name = StrEmpty) override final {
      return this;
    }

    Value* plus(Value* other, Logger& logger, const SourceSpan& pstate) const override final {
      if (const SassString * str = other->isString()) {
        sass::string text(value() + str->value());
        return SASS_MEMORY_NEW(SassString,
          pstate, std::move(text), hasQuotes());
      }
      sass::string text(value() + other->toCssString());
      return SASS_MEMORY_NEW(SassString,
        pstate, std::move(text), hasQuotes());
    }

    ATTACH_CLONE_OPERATIONS(SassString)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  //////////////////
  // The null value.
  //////////////////
  class Null final : public Value {
  public:
    Null(const SourceSpan& pstate);
    enum Sass_Tag getTag() const override final { return SASS_NULL; }
    const sass::string& type() const override final { return StrTypeNull; }
    bool is_invisible() const override { return true; }

    size_t hash() const override final;

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

    bool operator== (const Value& rhs) const override final;

    // ATTACH_CLONE_OPERATIONS(Null)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  //////////////////////////////////
  // The Parent Reference Expression.
  //////////////////////////////////
  class Parent_Reference final : public Value {
  public:
    size_t hash() const override final { return 0; }
    enum Sass_Tag getTag() const override final { return SASS_PARENT; }
    Parent_Reference(const SourceSpan& pstate);
    // const sass::string& type() const override final { return "parent"; }
    bool operator==(const Value& rhs) const override {
      return true; // they are always equal
    };
    // ATTACH_CLONE_OPERATIONS(Parent_Reference)
    ATTACH_CRTP_PERFORM_METHODS()
  };

}

#endif
