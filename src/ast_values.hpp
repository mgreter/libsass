#ifndef SASS_AST_VALUES_H
#define SASS_AST_VALUES_H

// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"
#include "ast.hpp"

#include "environment.hpp"
#include "ordered_map.hpp"

namespace Sass {

  //////////////////////////////////////////////////////////////////////
  // base class for values that support operations
  //////////////////////////////////////////////////////////////////////
  class Value : public Expression {
  public:
    Value(ParserState pstate, bool d = false, bool e = false, bool i = false, Type ct = NONE);

    // Whether the value will be represented in CSS as the empty string.
    virtual bool isBlank() const { return false; }

    // Return the length of this item as a list
    virtual long lengthAsList() const { return 1; }

    virtual std::vector<ValueObj> asVector() {
      std::vector<ValueObj> list;
      list.push_back(this);
      return list;
    }

    // Return the list separator
    virtual Sass_Separator separator() {
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

    // Return normalized index for vector from overflowable sass index
    long sassIndexToListIndex(Value* sassIndex, double epsilon, std::string name = "");

    Value* assertValue(std::string name = "");

    virtual SassColor* assertColor(std::string name = "");
    virtual Color_HSLA* assertColorHsla(std::string name = "");

    virtual SassFunction* assertFunction(std::string name = "");

    // SassFunction assertFunction(std::string name = "") = >
    //   throw _exception("$this is not a function reference.", name);

    virtual SassMap* assertMap(std::string name = "");

    virtual SassNumber* assertNumber(std::string name = "");
    virtual SassNumber* assertNumberOrNull(std::string name = "");

    virtual SassString* assertString(std::string name = "");
    virtual SassString* assertStringOrNull(std::string name = "");

    virtual SassArgumentList* assertArgumentList(std::string name = "");

    // General 
    SassList* changeListContents(
      std::vector<ValueObj> values,
      Sass_Separator separator,
      bool hasBrackets);

    // Pass explicit list separator
    SassList* changeListContents(
      std::vector<ValueObj> values,
      Sass_Separator separator) {
      return changeListContents(values,
        separator, hasBrackets());
    }

    // Pass explicit brackets config
    SassList* changeListContents(
      std::vector<ValueObj> values,
      bool hasBrackets) {
      return changeListContents(values,
        separator(), hasBrackets);
    }

    // Re-use current list settings
    SassList* changeListContents(
      std::vector<ValueObj> values) {
      return changeListContents(values,
        separator(), hasBrackets());
    }

    /// The SassScript `>` operation.
    virtual SassBoolean* greaterThan(Value* other);

    /// The SassScript `>=` operation.
    virtual SassBoolean* greaterThanOrEquals(Value* other);

    /// The SassScript `<` operation.
    virtual SassBoolean* lessThan(Value* other);

    /// The SassScript `<=` operation.
    virtual SassBoolean* lessThanOrEquals(Value* other);


    /// The SassScript `*` operation.
    virtual Value* times(Value* other);

    /// The SassScript `%` operation.
    virtual Value* modulo(Value* other);



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
  class List : public Value, public Vectorized<Expression_Obj> {
  private:
    ADD_PROPERTY(enum Sass_Separator, separator)
    ADD_PROPERTY(bool, is_arglist)
    ADD_PROPERTY(bool, is_bracketed)
  public:
    List(ParserState pstate, size_t size = 0, enum Sass_Separator sep = SASS_SPACE, bool argl = false, bool bracket = false);
    std::string type() const override { return is_arglist_ ? "arglist" : "list"; }
    static std::string type_name() { return "list"; }
    const char* sep_string(bool compressed = false) const {
      return separator() == SASS_SPACE ?
        " " : (compressed ? "," : ", ");
    }
    bool is_invisible() const override { return empty() && !is_bracketed(); }
    Expression_Obj value_at_index(size_t i);
    std::vector<Expression_Obj> values();

    virtual size_t hash() const override;
    virtual size_t size() const;

    NormalizedMap<ExpressionObj> getNormalizedArgMap();

    virtual bool operator== (const Value& rhs) const override;

    ATTACH_COPY_OPERATIONS(List)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  class SassList : public Value,
    public Vectorized<ValueObj> {
    virtual bool is_arglist() const { return false; }
  private:
    ADD_PROPERTY(enum Sass_Separator, separator);
    ADD_PROPERTY(bool, hasBrackets);
  public:

    SassList(ParserState pstate,
      std::vector<ValueObj> values = {},
      enum Sass_Separator seperator = SASS_SPACE,
      bool hasBrackets = false);

    Sass_Separator separator() override {
      return separator_;
    }

    std::vector<ValueObj> asVector() override {
      return elements();
    }

    bool hasBrackets() override {
      return hasBrackets_;
    }

    // Return the length of this item as a list
    long lengthAsList() const override {
      return length();
    }

    bool isBlank() const override final;

    std::string type() const override { return "list"; }
    static std::string type_name() { return "list"; }

    bool is_invisible() const override
    { return isBlank() && !hasBrackets(); }

    virtual size_t hash() const override;

    ATTACH_EQ_OPERATIONS(Value);
    ATTACH_COPY_OPERATIONS(SassList);
    ATTACH_CRTP_PERFORM_METHODS();
  };

  typedef NormalizedMap<ValueObj> keywordMap;
  class SassArgumentList : public SassList {
    ADD_PROPERTY(NormalizedMap<ValueObj>, keywords);
  public:
    bool is_arglist() const override final {
      return true;
    }
    SassArgumentList* assertArgumentList(std::string name = "") {
      return this;
    }

    SassMap* keywordsAsSassMap() const;

    SassArgumentList(ParserState pstate,
      std::vector<ValueObj> values = {},
      Sass_Separator sep = SASS_SPACE,
      NormalizedMap<ValueObj> keywords = {});
    ATTACH_EQ_OPERATIONS(Value);
    ATTACH_COPY_OPERATIONS(SassArgumentList);
    ATTACH_CRTP_PERFORM_METHODS();
  };

  ///////////////////////////////////////////////////////////////////////
  // Key value paris.
  ///////////////////////////////////////////////////////////////////////
  class Map : public Value, public Hashed<ValueObj, ValueObj, Map_Obj> {
  public:
    Map(ParserState pstate, size_t size = 0);
    std::string type() const override { return "map"; }
    static std::string type_name() { return "map"; }
    bool is_invisible() const override { return empty(); }
    SassListObj to_list(ParserState& pstate);
    SassMap* assertMap(std::string name) override { return this; }

    // Return the list separator
    Sass_Separator separator() override final {
      return empty() ? SASS_UNDEF : SASS_COMMA;
    }

    std::vector<ValueObj> asVector() override final {
      std::vector<ValueObj> list;
      for (size_t i = 0; i < length(); i++) {
        list.push_back(SASS_MEMORY_NEW(
          SassList, getKey(i)->pstate(),
          { getKey(i), getValue(i) },
          SASS_SPACE));
      }
      return list;
    }

    virtual size_t hash() const override;

    virtual bool operator== (const Value& rhs) const override;

    ATTACH_COPY_OPERATIONS(Map)
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
    Binary_Expression(ParserState pstate,
                      Operand op, Expression_Obj lhs, Expression_Obj rhs);

    const std::string separator();

    virtual void set_delayed(bool delayed) override;

    virtual size_t hash() const override;
    enum Sass_OP optype() const { return op_.operand; }
    // ATTACH_COPY_OPERATIONS(Binary_Expression)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ////////////////////////////////////////////////////
  // Function reference.
  ////////////////////////////////////////////////////
  class Function final : public Value {
  public:
    ADD_PROPERTY(Definition_Obj, definition)
    ADD_PROPERTY(bool, is_css)
  public:
    Function(ParserState pstate, Definition_Obj def, bool css);

    std::string type() const override { return "function"; }
    static std::string type_name() { return "function"; }
    bool is_invisible() const override { return true; }

    std::string name();

    bool operator== (const Value& rhs) const override;

    // ATTACH_COPY_OPERATIONS(Function)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  class SassFunction final : public Value {
  public:
    ADD_PROPERTY(CallableObj, callable);
  public:
    SassFunction(
      ParserState pstate,
      CallableObj callable);

    std::string type() const override { return "function"; }
    static std::string type_name() { return "function"; }
    bool is_invisible() const override { return true; }

    bool operator== (const Value& rhs) const override;

    SassFunction* assertFunction(std::string name = "") override final { return this; }

    // ATTACH_COPY_OPERATIONS(Function)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  //////////////////
  // Function calls.
  //////////////////
  class FunctionExpression final : public Expression {
    HASH_CONSTREF(InterpolationObj, sname)
    HASH_PROPERTY(Arguments_Obj, arguments)
    HASH_PROPERTY(Function_Obj, func)
    ADD_PROPERTY(std::string, ns)
    ADD_PROPERTY(bool, via_call)
    ADD_PROPERTY(void*, cookie)
    mutable size_t hash_;
  public:
    FunctionExpression(ParserState pstate, std::string n, Arguments_Obj args, std::string ns);

    FunctionExpression(ParserState pstate, InterpolationObj n, ArgumentsObj args, std::string ns);

    std::string name() const;
    bool is_css();

    size_t hash() const override;

    // ATTACH_COPY_OPERATIONS(FunctionExpression)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ///////////////////////
  // Variable references.
  ///////////////////////
  class Variable final : public Expression {
    ADD_CONSTREF(std::string, name)
  public:
    Variable(ParserState pstate, std::string n);
    virtual size_t hash() const override;
    // ATTACH_COPY_OPERATIONS(Variable)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ////////////////////////////////////////////////
  // Numbers, percentages, dimensions, and colors.
  ////////////////////////////////////////////////
  class Number final : public Value, public Units {
    HASH_PROPERTY(double, value);
    ADD_PROPERTY(double, epsilon);
    ADD_PROPERTY(bool, zero);

    // The representation of this number as two
    // slash-separated numbers, if it has one.
    ADD_PROPERTY(NumberObj, lhsAsSlash)
    ADD_PROPERTY(NumberObj, rhsAsSlash)



    mutable size_t hash_;
  public:
    Number(ParserState pstate, double val, std::string u = "", bool zero = true);
    Number(ParserState pstate, double val, Units units, bool zero = true);

    long assertInt(double epsilon, std::string name = "") {
      if (fuzzyIsInt(value_, epsilon)) {
        return fuzzyAsInt(value_, epsilon);
      }
      throw Exception::SassScriptException(
        inspect() + " is not an int.", name);
    }

    Number* assertNoUnits(std::string name = "") {
      if (!hasUnits()) return this;
      throw Exception::SassScriptException(
        "Expected " + inspect() + " to have no units.",
        name);
    }

    bool hasUnit(std::string unit) {
      return numerators.size() == 1 &&
        denominators.empty() &&
        numerators.front() == unit;
    }

    Number* assertUnit(std::string unit, std::string name = "") {
      if (hasUnit(unit)) return this;
      throw Exception::SassScriptException(
        "Expected " + inspect() + " to have unit \"" + unit + "\".",
        name);
    }


    Number* assertNumber(std::string name = "") override {
      return this;
    }

    double valueInRange(double min, double max, double epsilon, std::string name = "") const {
      double result = value_;
      if (!fuzzyCheckRange(value_, min, max, epsilon, result)) {
        std::stringstream msg;
        msg << "Expected " << inspect() << " to be within ";
        msg << min << unit() << " and " << max << unit() << ".";
        throw Exception::SassScriptException(msg.str(), name);
      }
      return result;
    }



    bool zero() { return zero_; }

    std::string type() const override { return "number"; }
    static std::string type_name() { return "number"; }

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
    bool operator== (const Number& rhs) const;


    bool operator== (const Value& rhs) const override;
    // COMPLEMENT_OPERATORS(Number)
    ATTACH_COPY_OPERATIONS(Number)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  //////////
  // Colors.
  //////////
  class Color : public Value {
    ADD_CONSTREF(std::string, disp)
    HASH_PROPERTY(double, a)
  protected:
    mutable size_t hash_;
  public:
    Color(ParserState pstate, double a = 1, const std::string disp = "");

    std::string type() const override { return "color"; }
    static std::string type_name() { return "color"; }

    virtual size_t hash() const override = 0;

    virtual bool operator== (const Value& rhs) const override = 0;

    virtual Color_RGBA* copyAsRGBA() const = 0;
    virtual Color_RGBA* toRGBA() = 0;

    virtual Color_HSLA* copyAsHSLA() const = 0;
    virtual Color_HSLA* toHSLA() = 0;

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
    Color_RGBA(ParserState pstate, double r, double g, double b, double a = 1.0, const std::string disp = "");

    std::string type() const override { return "color"; }
    static std::string type_name() { return "color"; }

    size_t hash() const override;

    Color_RGBA* copyAsRGBA() const override;
    Color_RGBA* toRGBA() override { return this; }

    Color_HSLA* copyAsHSLA() const override;
    Color_HSLA* toHSLA() override { return copyAsHSLA(); }

    SassColor* assertColor(std::string name = "") override {
      return this;
    }

    Color_HSLA* assertColorHsla(std::string name = "") override {
      return toHSLA();
    }

    bool operator== (const Value& rhs) const override;

    ATTACH_COPY_OPERATIONS(Color_RGBA)
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
    Color_HSLA(ParserState pstate, double h, double s, double l, double a = 1, const std::string disp = "");

    std::string type() const override { return "color"; }
    static std::string type_name() { return "color"; }

    SassColor* assertColor(std::string name = "") override {
      return toRGBA();
    }

    Color_HSLA* assertColorHsla(std::string name = "") override {
      return this;
    }

    size_t hash() const override;

    Color_RGBA* copyAsRGBA() const override;
    Color_RGBA* toRGBA() override { return copyAsRGBA(); }

    Color_HSLA* copyAsHSLA() const override;
    Color_HSLA* toHSLA() override { return this; }

    bool operator== (const Value& rhs) const override;

    ATTACH_COPY_OPERATIONS(Color_HSLA)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  //////////////////////////////
  // Errors from Sass_Values.
  //////////////////////////////
  class Custom_Error final : public Value {
    ADD_CONSTREF(std::string, message)
  public:
    Custom_Error(ParserState pstate, std::string msg);
    bool operator== (const Value& rhs) const override;
    // ATTACH_COPY_OPERATIONS(Custom_Error)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  //////////////////////////////
  // Warnings from Sass_Values.
  //////////////////////////////
  class Custom_Warning final : public Value {
    ADD_CONSTREF(std::string, message)
  public:
    Custom_Warning(ParserState pstate, std::string msg);
    bool operator== (const Value& rhs) const override;
    // ATTACH_COPY_OPERATIONS(Custom_Warning)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ////////////
  // Booleans.
  ////////////
  class Boolean final : public Value {
    HASH_PROPERTY(bool, value)
    mutable size_t hash_;
  public:
    Boolean(ParserState pstate, bool val);
    operator bool() override { return value_; }

    // Return the list separator
    bool isTruthy() const override final {
      return value_;
    }

    std::string type() const override { return "bool"; }
    static std::string type_name() { return "bool"; }

    size_t hash() const override;

    bool is_false() override { return !value_; }

    bool operator== (const Value& rhs) const override;

    ATTACH_COPY_OPERATIONS(Boolean)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ////////////////////////////////////////////////////////////////////////
  // Abstract base class for Sass string values. Includes interpolated and
  // "flat" strings.
  ////////////////////////////////////////////////////////////////////////
  class String : public Value {
  public:
    String(ParserState pstate, bool delayed = false);
    static std::string type_name() { return "string"; }
    virtual ~String() = 0;
    virtual bool operator==(const Value& rhs) const override {
      return this->to_string() == rhs.to_string();
    };
    ATTACH_VIRTUAL_COPY_OPERATIONS(String);
    ATTACH_CRTP_PERFORM_METHODS()
  };
  inline String::~String() { };


  ///////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////
  class Interpolation final : public String,
    public Vectorized<ExpressionObj> {

  public:
    std::string getPlainString();
    StringLiteral* getInitialPlain();

    virtual bool operator==(const Interpolation& rhs) const {
      return String::operator==(rhs)
        && Vectorized::operator==(rhs);
    }
    virtual bool operator!=(const Interpolation& rhs) const {
      return String::operator!=(rhs)
        || Vectorized::operator!=(rhs);
    }

    Interpolation(ParserState pstate, Expression* ex = nullptr);
    // ATTACH_COPY_OPERATIONS(Interpolation)
    ATTACH_CRTP_PERFORM_METHODS();
  };

  ///////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////
  class StringExpression final : public String {
    ADD_PROPERTY(InterpolationObj, text)
    ADD_PROPERTY(bool, hasQuotes)
  public:
    uint8_t findBestQuote();
    static StringExpression* plain(ParserState pstate, std::string text, bool quotes = false);
    InterpolationObj getAsInterpolation(bool escape = false, uint8_t quote = 0);
    StringExpression(ParserState pstate, InterpolationObj text, bool quote = false);
    ATTACH_COPY_OPERATIONS(StringExpression)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ///////////////////////////////////////////////////////////////////////
  // A native string wrapped as an expression
  ///////////////////////////////////////////////////////////////////////
  class StringLiteral final : public String {
    ADD_PROPERTY(std::string, text)
  public:
    StringLiteral(ParserState pstate, std::string text);
    bool isBlank() const override final;
    ATTACH_COPY_OPERATIONS(StringLiteral);
    ATTACH_CRTP_PERFORM_METHODS();
    ATTACH_EQ_OPERATIONS(Value);
  };

  ////////////////////////////////////////////////////////
  // Flat strings -- the lowest level of raw textual data.
  ////////////////////////////////////////////////////////
  class String_Constant : public String {
    ADD_PROPERTY(char, quote_mark)
    HASH_CONSTREF(std::string, value)
  protected:
    mutable size_t hash_;
  public:
    String_Constant(ParserState pstate, std::string val, bool css = true);
    String_Constant(ParserState pstate, const char* beg, bool css = true);
    String_Constant(ParserState pstate, const char* beg, const char* end, bool css = true);
    String_Constant(ParserState pstate, const Token& tok, bool css = true);
    std::string type() const override { return "string"; }
    static std::string type_name() { return "string"; }
    bool is_invisible() const override;
    size_t hash() const override;
    bool operator== (const Value& rhs) const override;
    // quotes are forced on inspection
    virtual std::string inspect() const override;
    bool hasQuotes() const {
      return quote_mark_ == '\0';
    }
    virtual SassString* assertString(std::string name = "") {
      return this;
    }
    ATTACH_COPY_OPERATIONS(String_Constant)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ////////////////////////////////////////////////////////
  // Possibly quoted string (unquote on instantiation)
  ////////////////////////////////////////////////////////
  class String_Quoted final : public String_Constant {
  public:
    String_Quoted(ParserState pstate, std::string val, char q = 0,
      bool keep_utf8_escapes = false, bool skip_unquoting = false,
      bool strict_unquoting = true, bool css = true);
    bool operator== (const Value& rhs) const override;
    // quotes are forced on inspection
    std::string inspect() const override;
    ATTACH_COPY_OPERATIONS(String_Quoted)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  //////////////////
  // The null value.
  //////////////////
  class Null final : public Value {
  public:
    Null(ParserState pstate);
    std::string type() const override { return "null"; }
    static std::string type_name() { return "null"; }
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

    bool operator== (const Value& rhs) const override;

    // ATTACH_COPY_OPERATIONS(Null)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  //////////////////////////////////
  // The Parent Reference Expression.
  //////////////////////////////////
  class Parent_Reference final : public Value {
  public:
    Parent_Reference(ParserState pstate);
    std::string type() const override { return "parent"; }
    static std::string type_name() { return "parent"; }
    bool operator==(const Value& rhs) const override {
      return true; // they are always equal
    };
    // ATTACH_COPY_OPERATIONS(Parent_Reference)
    ATTACH_CRTP_PERFORM_METHODS()
  };

}

#endif
