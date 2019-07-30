#include "ast_values.hpp"
#include "ast_values.hpp"
#include "ast_values.hpp"
// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"
#include "ast.hpp"
#include "character.hpp"
#include "interpolation.hpp"
#include "parser_selector.hpp"

namespace Sass {

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Value::Value(ParserState pstate, bool d, bool e, bool i, Type ct)
  : Expression(pstate, d, e, i, ct)
  { }

  // Return normalized index for vector from overflowable sass index

  long Value::sassIndexToListIndex(Value * sassIndex, double epsilon, std::string name)
  {
    long index = sassIndex->assertNumber(name)->assertInt(epsilon, name);
    if (index == 0) throw Exception::SassScriptException("List index may not be 0.", name);
    if (abs(index) > lengthAsList()) {
      std::stringstream strm;
      strm << "Invalid index " << index << " for a list ";
      strm << "with " << lengthAsList() << " elements.";
      throw Exception::SassScriptException(strm.str(), name);
    }

    return index < 0 ? lengthAsList() + index : index - 1;
  }

  Value* Value::assertValue(std::string name) {
    // Noop, but usefull for breakpoints
    return this;
  }

  SassColor* Value::assertColor(std::string name) {
    throw Exception::SassScriptException(
      to_string() + " is not a color.", name);
  }

  Color_HSLA* Value::assertColorHsla(std::string name) {
    throw Exception::SassScriptException(
      to_string() + " is not a color.", name);
  }

  SassFunction* Value::assertFunction(std::string name)
  {
    throw Exception::SassScriptException(
      to_string() + " is not a function reference.", name);
  }

  SassMap* Value::assertMap(std::string name) {
    throw Exception::SassScriptException(
      to_string() + " is not a map.", name);
  }

  SassNumber* Value::assertNumber(std::string name) {
    throw Exception::SassScriptException(
      to_string() + " is not a number.", name);
  }

  SassNumber* Value::assertNumberOrNull(std::string name)
  {
    if (this->isNull()) return nullptr;
    return this->assertNumber(name);
  }

  SassString* Value::assertString(std::string name) {
    throw Exception::SassScriptException(
      to_string() + " is not a string.", name);
  }

  SassString* Value::assertStringOrNull(std::string name) {
    if (this->isNull()) return nullptr;
    return this->assertString(name);
  }

  SassArgumentList* Value::assertArgumentList(std::string name) {
    throw Exception::SassScriptException(
      to_string() + " is not an argument list.", name);
  }

  /// Converts a `selector-parse()`-style input into a string that can be
  /// parsed.
  ///
  /// Returns `null` if [this] isn't a type or a structure that can be parsed as
  /// a selector.

  bool Value::_selectorStringOrNull(std::string& rv) {

	  if (SassString * str = Cast<SassString>(this)) {
      rv = str->value();
      return true;
	  }

    if (SassList * list = Cast<SassList>(this)) {

      std::vector<ValueObj> values = list->asVector();

      if (values.empty()) return false;

      std::vector<std::string> result;
      if (list->separator() == SASS_COMMA) {
        for (auto complex : values) {
          SassList* cplxLst = Cast<SassList>(complex);
          SassString* cplxStr = Cast<SassString>(complex);
          if (cplxStr) {
            result.push_back(cplxStr->value());
          }
          else if (cplxLst &&
            cplxLst->separator() == SASS_SPACE) {
            std::string string = complex->_selectorString();
            if (string.empty()) return false;
            result.push_back(string);
          }
          else {
            return false;
          }
        }
      }
      else {
        for (auto compound : values) {
          SassString* cmpdStr = Cast<SassString>(compound);
          StringLiteral* cmpdLit = Cast<StringLiteral>(compound);
          if (cmpdStr) {
            result.push_back(cmpdStr->value());
          }
          else if (cmpdLit) {
            result.push_back(cmpdLit->text());
          }
          else {
            return false;
          }
        }
      }
      rv = Util::join_strings(result, list->separator() == SASS_COMMA ? ", " : " ");
      return true;

    }

    return false;

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

  SelectorList* Value::assertSelector(Context& ctx, std::string name, bool allowParent) {
    std::string string = _selectorString(name);
    char* str = sass_copy_c_string(string.c_str());
    ctx.strings.push_back(str);
    SelectorParser p2(ctx, str, "sass://parse-selector", -1);
    p2._allowParent = allowParent;
    auto sel = p2.parse();
    return sel.detach();
  }

  CompoundSelector* Value::assertCompoundSelector(Context& ctx, std::string name, bool allowParent) {
    std::string string = _selectorString(name);
    char* str = sass_copy_c_string(string.c_str());
    ctx.strings.push_back(str);
    SelectorParser p2(ctx, str, "sass://parse-selector", -1);
    p2._allowParent = allowParent;
    auto sel = p2.parseCompoundSelector();
    return sel.detach();
  }

  SassList* Value::changeListContents(
    std::vector<ValueObj> values,
    Sass_Separator separator,
    bool hasBrackets)
  {
    return SASS_MEMORY_NEW(SassList, pstate(),
      values, separator, hasBrackets);
  }

  Boolean* Value::greaterThan(Value* other) {
    throw Exception::SassScriptException(
      "Undefined operation \"" + inspect()
      + " > " + other->inspect() + "\".");
  }

  Boolean* Value::greaterThanOrEquals(Value* other) {
    throw Exception::SassScriptException(
      "Undefined operation \"" + inspect()
      + " >= " + other->inspect() + "\".");
  }

  Boolean* Value::lessThan(Value* other) {
    throw Exception::SassScriptException(
      "Undefined operation \"" + inspect()
      + " < " + other->inspect() + "\".");
  }

  Boolean* Value::lessThanOrEquals(Value* other) {
    throw Exception::SassScriptException(
      "Undefined operation \"" + inspect()
      + " <= " + other->inspect() + "\".");
  }

  Value* Value::times(Value* other) {
    throw Exception::SassScriptException(
      "Undefined operation \"" + inspect()
      + " * " + other->inspect() + "\".");
  }

  // The SassScript `%` operation.
  Value* Value::modulo(Value* other) {
    throw Exception::SassScriptException(
      "Undefined operation \"" + inspect()
      + " % " + other->inspect() + "\".");
  }

  Value::Value(const Value* ptr)
  : Expression(ptr) { }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  List::List(ParserState pstate, size_t size, enum Sass_Separator sep, bool argl, bool bracket)
  : Value(pstate),
    Vectorized<Expression_Obj>(size),
    separator_(sep),
    is_arglist_(argl),
    is_bracketed_(bracket)
  { concrete_type(LIST); }

  List::List(const List* ptr)
  : Value(ptr),
    Vectorized<Expression_Obj>(*ptr),
    separator_(ptr->separator_),
    is_arglist_(ptr->is_arglist_),
    is_bracketed_(ptr->is_bracketed_)
  { concrete_type(LIST); }

  size_t List::hash() const
  {
    if (hash_ == 0) {
      hash_ = std::hash<std::string>()(sep_string());
      hash_combine(hash_, std::hash<bool>()(is_bracketed()));
      for (size_t i = 0, L = length(); i < L; ++i)
        hash_combine(hash_, (elements()[i])->hash());
    }
    return hash_;
  }

  bool List::operator== (const Value& rhs) const
  {
    if (auto r = Cast<List>(&rhs)) {
      if (length() != r->length()) return false;
      if (separator() != r->separator()) return false;
      if (is_bracketed() != r->is_bracketed()) return false;
      for (size_t i = 0, L = length(); i < L; ++i) {
        auto rv = r->at(i);
        auto lv = this->at(i);
        if (!lv && rv) return false;
        else if (!rv && lv) return false;
        else if (*lv != *rv) return false;
      }
      return true;
    }
    if (auto m = Cast<Map>(&rhs)) {
      return m->empty() && empty();
    }
    return false;
  }

  size_t List::size() const {
    if (!is_arglist_) return length();
    // arglist expects a list of arguments
    // so we need to break before keywords
    for (size_t i = 0, L = length(); i < L; ++i) {
      Expression_Obj obj = this->at(i);
      if (Argument* arg = Cast<Argument>(obj)) {
        if (!arg->name().empty()) return i;
      }
    }
    return length();
  }

  SassMap* List::assertMap(std::string name) {
    if (!empty()) { return Value::assertMap(name); }
    else { return SASS_MEMORY_NEW(Map, pstate(), 0); }
  }

  KeywordMap<ExpressionObj> List::getKeywordArgMap()
  {
    KeywordMap<ExpressionObj> map;
    if (is_arglist_) {
      for (Expression* item : elements()) {
        if (Argument * arg = Cast<Argument>(item)) {
          if (arg->name().empty()) continue;
          map[arg->name()] = arg->value();
        }
      }
    }
    return map;
  }


  Expression_Obj List::value_at_index(size_t i) {
    Expression_Obj obj = this->at(i);
    if (is_arglist_) {
      if (Argument* arg = Cast<Argument>(obj)) {
        return arg->value();
      } else {
        return obj;
      }
    } else {
      return obj;
    }
  }

  std::vector<Expression_Obj> List::values()
  {
    if (is_arglist_) {
      std::vector<Expression_Obj> values;
      for (Expression* item : elements()) {
        if (Argument* arg = Cast<Argument>(item)) {
          values.push_back(arg->value());
        }
        else {
          values.push_back(item);
        }
      }
      return values;
    }
    return elements();
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Map::Map(ParserState pstate, size_t size)
  : Value(pstate),
    Hashed(size)
  { concrete_type(MAP); }

  Map::Map(const Map* ptr)
  : Value(ptr),
    Hashed(*ptr)
  { concrete_type(MAP); }

  bool Map::operator== (const Value& rhs) const
  {
    if (auto r = Cast<Map>(&rhs)) {
      if (length() != r->length()) return false;
      for (auto key : keys()) {
        auto rv = r->at(key);
        auto lv = this->at(key);
        if (!lv && rv) return false;
        else if (!rv && lv) return false;
        else if (*lv != *rv) return false;
      }
      return true;
    }
    return false;
  }

  SassListObj Map::to_list(ParserState& pstate) {
    SassListObj ret = SASS_MEMORY_NEW(SassList, pstate, {}, SASS_UNDEF);

    for (auto key : keys()) {
      SassListObj l = SASS_MEMORY_NEW(SassList, pstate, {});
      l->append(key);
      l->append(at(key));
      ret->append(l);
    }

    if (ret->length() > 1) {
      ret->separator(SASS_COMMA);
    }

    return ret;
  }

  size_t Map::hash() const
  {
    if (hash_ == 0) {
      for (auto key : keys()) {
        hash_combine(hash_, key->hash());
        hash_combine(hash_, at(key)->hash());
      }
    }

    return hash_;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Binary_Expression::Binary_Expression(ParserState pstate,
                    Operand op, Expression_Obj lhs, Expression_Obj rhs)
  : Expression(pstate), op_(op), left_(lhs), right_(rhs), allowsSlash_(false), hash_(0)
  { }

  const std::string Binary_Expression::separator()
  {
    return sass_op_separator(optype());
  }

  void Binary_Expression::set_delayed(bool delayed)
  {
    right()->set_delayed(delayed);
    left()->set_delayed(delayed);
    allowsSlash(delayed);
  }

  size_t Binary_Expression::hash() const
  {
    if (hash_ == 0) {
      hash_ = std::hash<size_t>()(optype());
      hash_combine(hash_, left()->hash());
      hash_combine(hash_, right()->hash());
    }
    return hash_;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Function::Function(ParserState pstate, Definition_Obj def, bool css)
  : Value(pstate), definition_(def), is_css_(css)
  { concrete_type(FUNCTION_VAL); }

  bool Function::operator== (const Value& rhs) const
  {
    if (auto r = Cast<Function>(&rhs)) {
      auto d1 = Cast<Definition>(definition());
      auto d2 = Cast<Definition>(r->definition());
      return d1 && d2 && d1 == d2 && is_css() == r->is_css();
    }
    return false;
  }

  std::string Function::name() {
    if (definition_) {
      return definition_->name();
    }
    return "";
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  FunctionExpression::FunctionExpression(ParserState pstate, InterpolationObj n, ArgumentsObj args, std::string ns)
  : Expression(pstate), sname_(n), arguments_(args), ns_(ns), via_call_(false), cookie_(0), hash_(0)
  { concrete_type(FUNCTION); }

  FunctionExpression::FunctionExpression(ParserState pstate, std::string n, Arguments_Obj args, std::string ns)
    : Expression(pstate), sname_(SASS_MEMORY_NEW(Interpolation, pstate, SASS_MEMORY_NEW(StringLiteral, pstate, n))), arguments_(args), ns_(ns), via_call_(false), cookie_(0), hash_(0)
  { concrete_type(FUNCTION); }

  size_t FunctionExpression::hash() const
  {
    if (hash_ == 0) {
      hash_ = std::hash<std::string>()(name());
      for (auto argument : arguments()->elements())
        hash_combine(hash_, argument->hash());
    }
    return hash_;
  }

  std::string FunctionExpression::name() const
  {
    return sname();
  }

  bool FunctionExpression::is_css() {
    if (func_) return func_->is_css();
    return false;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Variable::Variable(ParserState pstate, std::string n)
  : Expression(pstate), name_(n)
  { concrete_type(VARIABLE); }

  size_t Variable::hash() const
  {
    return std::hash<std::string>()(name());
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Number::Number(ParserState pstate, double val, std::string u, bool zero)
  : Value(pstate),
    Units(),
    value_(val),
    // epsilon_(0.00001),
    zero_(zero),
    lhsAsSlash_(),
    rhsAsSlash_(),
    hash_(0)
  {
    size_t l = 0;
    size_t r;
    if (!u.empty()) {
      bool nominator = true;
      while (true) {
        r = u.find_first_of("*/", l);
        std::string unit(u.substr(l, r == std::string::npos ? r : r - l));
        if (!unit.empty()) {
          if (nominator) numerators.push_back(unit);
          else denominators.push_back(unit);
        }
        if (r == std::string::npos) break;
        // ToDo: should error for multiple slashes
        // if (!nominator && u[r] == '/') error(...)
        if (u[r] == '/')
          nominator = false;
        // strange math parsing?
        // else if (u[r] == '*')
        //  nominator = true;
        l = r + 1;
      }
    }
    concrete_type(NUMBER);
  }

  Number::Number(ParserState pstate, double val, Units units, bool zero)
    : Value(pstate),
    Units(units),
    value_(val),
    // epsilon_(0.00001),
    zero_(zero),
    lhsAsSlash_(),
    rhsAsSlash_(),
    hash_(0)
  {
    concrete_type(NUMBER);
  }

  Number::Number(const Number* ptr)
  : Value(ptr),
    Units(ptr),
    value_(ptr->value_),
    // epsilon_(ptr->epsilon_),
    lhsAsSlash_(ptr->lhsAsSlash_),
    rhsAsSlash_(ptr->rhsAsSlash_),
    hash_(ptr->hash_)
  { concrete_type(NUMBER); }

  // cancel out unnecessary units
  void Number::reduce()
  {
    // apply conversion factor
    value_ *= this->Units::reduce();
  }

  void Number::normalize()
  {
    // apply conversion factor
    value_ *= this->Units::normalize();
  }

  size_t Number::hash() const
  {
    if (hash_ == 0) {
      hash_ = std::hash<double>()(value_);
      for (const auto numerator : numerators)
        hash_combine(hash_, std::hash<std::string>()(numerator));
      for (const auto denominator : denominators)
        hash_combine(hash_, std::hash<std::string>()(denominator));
    }
    return hash_;
  }

  bool Number::operator== (const Value& rhs) const
  {
    if (const Number* n = Cast<Number>(&rhs)) {
      return *this == *n;
    }
    return false;
  }

  bool Number::operator== (const Number& rhs) const
  {
    // unitless or only having one unit are equivalent (3.4)
    // therefore we need to reduce the units beforehand
    Number l(*this), r(rhs); l.reduce(); r.reduce();
    size_t lhs_units = l.numerators.size() + l.denominators.size();
    size_t rhs_units = r.numerators.size() + r.denominators.size();
    if (!lhs_units || !rhs_units) {
      return NEAR_EQUAL(l.value(), r.value());
    }
    // ensure both have same units
    l.normalize(); r.normalize();
    Units &lhs_unit = l, &rhs_unit = r;
    return lhs_unit == rhs_unit &&
      NEAR_EQUAL(l.value(), r.value());
  }

  bool Number::operator< (const Number& rhs) const
  {
    // unitless or only having one unit are equivalent (3.4)
    // therefore we need to reduce the units beforehand
    Number l(*this), r(rhs); l.reduce(); r.reduce();
    size_t lhs_units = l.numerators.size() + l.denominators.size();
    size_t rhs_units = r.numerators.size() + r.denominators.size();
    if (!lhs_units || !rhs_units) {
      return l.value() < r.value();
    }
    // ensure both have same units
    l.normalize(); r.normalize();
    Units& lhs_unit = l, & rhs_unit = r;
    if (!(lhs_unit == rhs_unit)) {
      /* ToDo: do we always get usefull backtraces? */
      throw Exception::IncompatibleUnits(*this, rhs);
    }
    if (lhs_unit == rhs_unit) {
      return l.value() < r.value();
    }
    else {
      return lhs_unit < rhs_unit;
    }
  }

  bool Number::operator> (const Number& rhs) const
  {
    // unitless or only having one unit are equivalent (3.4)
    // therefore we need to reduce the units beforehand
    Number l(*this), r(rhs); l.reduce(); r.reduce();
    size_t lhs_units = l.numerators.size() + l.denominators.size();
    size_t rhs_units = r.numerators.size() + r.denominators.size();
    if (!lhs_units || !rhs_units) {
      return l.value() > r.value();
    }
    // ensure both have same units
    l.normalize(); r.normalize();
    Units& lhs_unit = l, & rhs_unit = r;
    if (!(lhs_unit == rhs_unit)) {
      /* ToDo: do we always get usefull backtraces? */
      throw Exception::IncompatibleUnits(*this, rhs);
    }
    if (lhs_unit == rhs_unit) {
      return l.value() > r.value();
    }
    else {
      // ToDo: implement directly?
      return lhs_unit > rhs_unit;
    }
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Color::Color(ParserState pstate, double a, const std::string disp)
  : Value(pstate),
    disp_(disp), a_(a),
    hash_(0)
  { concrete_type(COLOR); }

  Color::Color(const Color* ptr)
  : Value(ptr->pstate()),
    // reset on copy
    disp_(""),
    a_(ptr->a_),
    hash_(ptr->hash_)
  { concrete_type(COLOR); }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Color_RGBA::Color_RGBA(ParserState pstate, double r, double g, double b, double a, const std::string disp)
  : Color(pstate, a, disp),
    r_(r), g_(g), b_(b)
  { concrete_type(COLOR); }

  Color_RGBA::Color_RGBA(const Color_RGBA* ptr)
  : Color(ptr),
    r_(ptr->r_),
    g_(ptr->g_),
    b_(ptr->b_)
  { concrete_type(COLOR); }

  bool Color_RGBA::operator== (const Value& rhs) const
  {
    if (auto r = Cast<Color_RGBA>(&rhs)) {
      return r_ == r->r() &&
        g_ == r->g() &&
        b_ == r->b() &&
        a_ == r->a();
    }
    return false;
  }

  size_t Color_RGBA::hash() const
  {
    if (hash_ == 0) {
      hash_ = std::hash<std::string>()("RGBA");
      hash_combine(hash_, std::hash<double>()(a_));
      hash_combine(hash_, std::hash<double>()(r_));
      hash_combine(hash_, std::hash<double>()(g_));
      hash_combine(hash_, std::hash<double>()(b_));
    }
    return hash_;
  }

  Color_HSLA* Color_RGBA::copyAsHSLA() const
  {

    // Algorithm from http://en.wikipedia.org/wiki/wHSL_and_HSV#Conversion_from_RGB_to_HSL_or_HSV
    double r = r_ / 255.0;
    double g = g_ / 255.0;
    double b = b_ / 255.0;

    double max = std::max(r, std::max(g, b));
    double min = std::min(r, std::min(g, b));
    double delta = max - min;

    double h = 0;
    double s;
    double l = (max + min) / 2.0;

    if (NEAR_EQUAL(max, min)) {
      h = s = 0; // achromatic
    }
    else {
      if (l < 0.5) s = delta / (max + min);
      else         s = delta / (2.0 - max - min);

      if      (r == max) h = (g - b) / delta + (g < b ? 6 : 0);
      else if (g == max) h = (b - r) / delta + 2;
      else if (b == max) h = (r - g) / delta + 4;
    }

    // HSL hsl_struct;
    h = h * 60;
    s = s * 100;
    l = l * 100;

    return SASS_MEMORY_NEW(Color_HSLA,
      pstate(), h, s, l, a(), ""
    );
  }

  Color_RGBA* Color_RGBA::copyAsRGBA() const
  {
    return SASS_MEMORY_COPY(this);
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Color_HSLA::Color_HSLA(ParserState pstate, double h, double s, double l, double a, const std::string disp)
  : Color(pstate, a, disp),
    h_(absmod(h, 360.0)),
    s_(clip(s, 0.0, 100.0)),
    l_(clip(l, 0.0, 100.0))
    // hash_(0)
  { concrete_type(COLOR); }

  Color_HSLA::Color_HSLA(const Color_HSLA* ptr)
  : Color(ptr),
    h_(ptr->h_),
    s_(ptr->s_),
    l_(ptr->l_)
    // hash_(ptr->hash_)
  { concrete_type(COLOR); }

  bool Color_HSLA::operator== (const Value& rhs) const
  {
    if (auto r = Cast<Color_HSLA>(&rhs)) {
      return h_ == r->h() &&
        s_ == r->s() &&
        l_ == r->l() &&
        a_ == r->a();
    }
    return false;
  }

  size_t Color_HSLA::hash() const
  {
    if (hash_ == 0) {
      hash_ = std::hash<std::string>()("HSLA");
      hash_combine(hash_, std::hash<double>()(a_));
      hash_combine(hash_, std::hash<double>()(h_));
      hash_combine(hash_, std::hash<double>()(s_));
      hash_combine(hash_, std::hash<double>()(l_));
    }
    return hash_;
  }

  // hue to RGB helper function
  double h_to_rgb(double m1, double m2, double h)
  {
    h = absmod(h, 1.0);
    if (h*6.0 < 1) return m1 + (m2 - m1)*h*6;
    if (h*2.0 < 1) return m2;
    if (h*3.0 < 2) return m1 + (m2 - m1) * (2.0/3.0 - h)*6;
    return m1;
  }

  Color_RGBA* Color_HSLA::copyAsRGBA() const
  {

    double h = absmod(h_ / 360.0, 1.0);
    double s = clip(s_ / 100.0, 0.0, 1.0);
    double l = clip(l_ / 100.0, 0.0, 1.0);

    // Algorithm from the CSS3 spec: http://www.w3.org/TR/css3-color/#hsl-color.
    double m2;
    if (l <= 0.5) m2 = l*(s+1.0);
    else m2 = (l+s)-(l*s);
    double m1 = (l*2.0)-m2;
    // round the results -- consider moving this into the Color constructor
    double r = (h_to_rgb(m1, m2, h + 1.0/3.0) * 255.0);
    double g = (h_to_rgb(m1, m2, h) * 255.0);
    double b = (h_to_rgb(m1, m2, h - 1.0/3.0) * 255.0);

    return SASS_MEMORY_NEW(Color_RGBA,
      pstate(), r, g, b, a(), ""
    );
  }

  Color_HSLA* Color_HSLA::copyAsHSLA() const
  {
    return SASS_MEMORY_COPY(this);
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Custom_Error::Custom_Error(ParserState pstate, std::string msg)
  : Value(pstate), message_(msg)
  { concrete_type(C_ERROR); }

  bool Custom_Error::operator== (const Value& rhs) const
  {
    if (auto r = Cast<Custom_Error>(&rhs)) {
      return message() == r->message();
    }
    return false;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Custom_Warning::Custom_Warning(ParserState pstate, std::string msg)
  : Value(pstate), message_(msg)
  { concrete_type(C_WARNING); }

  bool Custom_Warning::operator== (const Value& rhs) const
  {
    if (auto r = Cast<Custom_Warning>(&rhs)) {
      return message() == r->message();
    }
    return false;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Boolean::Boolean(ParserState pstate, bool val)
  : Value(pstate), value_(val),
    hash_(0)
  { concrete_type(BOOLEAN); }

  Boolean::Boolean(const Boolean* ptr)
  : Value(ptr),
    value_(ptr->value_),
    hash_(ptr->hash_)
  { concrete_type(BOOLEAN); }

 bool Boolean::operator== (const Value& rhs) const
 {
   if (auto r = Cast<Boolean>(&rhs)) {
     return (value() == r->value());
   }
   return false;
 }

 size_t Boolean::hash() const
  {
    if (hash_ == 0) {
      hash_ = std::hash<bool>()(value_);
    }
    return hash_;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  String::String(ParserState pstate, bool delayed)
  : Value(pstate, delayed)
  { concrete_type(STRING); }
  String::String(const String* ptr)
  : Value(ptr)
  { concrete_type(STRING); }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Should be? asPlain in dart-sass
  std::string Interpolation::getPlainString()
  {
    if (empty()) {
      // std::cerr << "should return empty\n";
      return "";
    }
    if (length() > 1) {
      return "";
    }
    if (StringLiteral * str = Cast<StringLiteral>(first())) {
      return str->text();
    }
    else {
      // deb// ugger << "plain is not a string?\n";
      return "";
    }
  }

  StringLiteral* Interpolation::getInitialPlain()
  {
    if (StringLiteral * str = Cast<StringLiteral>(first())) {
      return str;
    }

    return SASS_MEMORY_NEW(StringLiteral, "[pstate]", "");
  }

  Interpolation::Interpolation(ParserState pstate, Expression* ex) :
    String(pstate), Vectorized<ExpressionObj>()
  {
    if (ex != nullptr) append(ex);
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  uint8_t StringExpression::findBestQuote()
  {
    using namespace Character;
    bool containsDoubleQuote = false;
    for (auto item : text_->elements()) {
      if (auto str = Cast<String_Constant>(item)) {
        auto& value = str->value();
        for (size_t i = 0; i < value.size(); i++) {
          uint8_t codeUnit = value[i];
          if (codeUnit == $single_quote) return $double_quote;
          if (codeUnit == $double_quote) containsDoubleQuote = true;
        }
      }
    }
    return containsDoubleQuote ? $single_quote : $double_quote;
  }

  StringExpression* StringExpression::plain(ParserState pstate, std::string text, bool quotes)
  {
    Interpolation* itpl = SASS_MEMORY_NEW(Interpolation, pstate);
    itpl->append(SASS_MEMORY_NEW(StringLiteral, pstate, text));
    return SASS_MEMORY_NEW(StringExpression, pstate, itpl, quotes);
  }

  InterpolationObj StringExpression::getAsInterpolation(bool escape, uint8_t quote)
  {
    InterpolationObj itpl = SASS_MEMORY_NEW(Interpolation, "[pstate]");

    if (!hasQuotes()) return text_;
    // quote ? ? = hasQuotes ? _bestQuote() : null;
    if (!quote && hasQuotes()) quote = findBestQuote();
    InterpolationBuffer buffer;

    using namespace Character;

    if (quote != 0) {
      buffer.write(quote);
    }

    for (auto value : text_->elements()) {
      // assert(value is Expression || value is String);
      if (StringLiteral * str = Cast<StringLiteral>(value)) {
        std::string value(str->text());
        for (size_t i = 0; i < value.size(); i++) {

          uint8_t codeUnit = value[i];

          if (isNewline(codeUnit)) {
            buffer.write($backslash);
            buffer.write($a);
            if (i != value.size() - 1) {
              uint8_t next = value[i + 1];
              if (isWhitespace(next) || isHex(next)) {
                buffer.write($space);
              }
            }
          }
          else {
            if (codeUnit == quote ||
              codeUnit == $backslash ||
              (escape &&
                codeUnit == $hash &&
                i < value.size() - 1 &&
                value[i + 1] == $lbrace)) {
              // std::cerr << "ADD BACKSLASH\n";
              buffer.write($backslash);
            }
            buffer.write(codeUnit);
          }

        }
      }
      else if (Expression * ex = Cast<Expression>(value)) {
        buffer.add(ex);
      }
      else {
        std::cerr << "nono item in schema\n";
      }
    }

    if (quote != 0) {
      buffer.write(quote);
    }
    return buffer.getInterpolation();


    // itpl->has
    // return text_;
  }

  StringExpression::StringExpression(ParserState pstate, InterpolationObj text, bool quote) :
    String(pstate), text_(text), hasQuotes_(quote)
  {

  }

  StringExpression::StringExpression(const StringExpression* ptr) :
    String(ptr),
    text_(ptr->text_),
    hasQuotes_(ptr->hasQuotes_)
  {
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  StringLiteral::StringLiteral(ParserState pstate, std::string text) :
    String(pstate), text_(text)
  {
  }

  StringLiteral::StringLiteral(const StringLiteral* ptr) :
    String(ptr),
    text_(ptr->text_)
  {
  }

  bool StringLiteral::operator== (const Value& rhs) const
  {
    if (const StringLiteral * str = Cast<StringLiteral>(&rhs)) {
      return text() == str->text();
    }
    return false;
  }

  bool StringLiteral::isBlank() const
  {
    return /* !hasQuotes() && */ text_.empty();
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  String_Constant::String_Constant(ParserState pstate, std::string val, bool css)
  : String(pstate), quote_mark_(0), value_(read_css_string(val, css)), hash_(0)
  { }
  String_Constant::String_Constant(ParserState pstate, const char* beg, bool css)
  : String(pstate), quote_mark_(0), value_(read_css_string(std::string(beg), css)), hash_(0)
  { }
  String_Constant::String_Constant(ParserState pstate, const char* beg, const char* end, bool css)
  : String(pstate), quote_mark_(0), value_(read_css_string(std::string(beg, end-beg), css)), hash_(0)
  { }
  String_Constant::String_Constant(ParserState pstate, const Token& tok, bool css)
  : String(pstate), quote_mark_(0), value_(read_css_string(std::string(tok.begin, tok.end), css)), hash_(0)
  { }

  String_Constant::String_Constant(const String_Constant* ptr)
  : String(ptr),
    quote_mark_(ptr->quote_mark_),
    value_(ptr->value_),
    hash_(ptr->hash_)
  { }

  bool String_Constant::is_invisible() const {
    return value_.empty() && quote_mark_ == 0;
  }

  bool String_Constant::operator== (const Value& rhs) const
  {
    if (auto qstr = Cast<String_Quoted>(&rhs)) {
      return value() == qstr->value();
    }
    else if (auto cstr = Cast<String_Constant>(&rhs)) {
      return value() == cstr->value();
    }
    return false;
  }

  std::string String_Constant::inspect() const
  {
    if (quote_mark_ == 0) {
      return quote(value_, '*');
    }
    else {
      return quote(value_, '*');
    }
  }

  size_t String_Constant::hash() const
  {
    if (hash_ == 0) {
      hash_ = std::hash<std::string>()(value_);
    }
    return hash_;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  String_Quoted::String_Quoted(ParserState pstate, std::string val, char q,
    bool keep_utf8_escapes, bool skip_unquoting,
    bool strict_unquoting, bool css)
  : String_Constant(pstate, val, css)
  {
    if (skip_unquoting == false) {
      value_ = unquote(value_, &quote_mark_, keep_utf8_escapes, strict_unquoting);
      if (q && quote_mark_) quote_mark_ = q;
    }
    else {
      if (q) quote_mark_ = q;
    }
  }

  String_Quoted::String_Quoted(const String_Quoted* ptr)
  : String_Constant(ptr)
  { }

  bool String_Quoted::operator== (const Value& rhs) const
  {
    if (auto qstr = Cast<String_Quoted>(&rhs)) {
      return value() == qstr->value();
    }
    else if (auto cstr = Cast<String_Constant>(&rhs)) {
      return value() == cstr->value();
    }
    return false;
  }

  std::string String_Quoted::inspect() const
  {
    return quote(value_, '*');
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Null::Null(ParserState pstate)
  : Value(pstate)
  { concrete_type(NULL_VAL); }

  bool Null::operator== (const Value& rhs) const
  {
    return Cast<Null>(&rhs) != nullptr;
  }

  size_t Null::hash() const
  {
    return -1;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Parent_Reference::Parent_Reference(ParserState pstate)
  : Value(pstate)
  { concrete_type(PARENT); }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  SassList::SassList(
    ParserState pstate,
    std::vector<ValueObj> values,
    Sass_Separator separator,
    bool hasBrackets) :
    Value(pstate),
    Vectorized(values),
    separator_(separator),
    hasBrackets_(hasBrackets)
  {
    concrete_type(LIST);
  }

  SassList::SassList(
    const SassList* ptr) :
    Value(ptr),
    Vectorized(ptr),
    separator_(ptr->separator_),
    hasBrackets_(ptr->hasBrackets_)
  {
    concrete_type(LIST);
  }

  SassMap* SassList::assertMap(std::string name) {
    if (!empty()) { return Value::assertMap(name); }
    else { return SASS_MEMORY_NEW(Map, pstate(), 0); }
  }

  bool SassList::isBlank() const
  {
    for (const Value* value : elements()) {
      if (!value->isBlank()) return false;
    }
    return true;
  }

  size_t SassList::hash() const
  {
    if (hash_ == 0) {
      hash_ = std::hash<std::size_t>()(separator());
      hash_combine(hash_, std::hash<bool>()(hasBrackets()));
      for (size_t i = 0, L = length(); i < L; ++i)
        hash_combine(hash_, (elements()[i])->hash());
    }
    return hash_;
  }

  bool SassList::operator==(const Value& rhs) const
  {
    if (const SassList* r = Cast<SassList>(&rhs)) {
      if (length() != r->length()) return false;
      if (separator() != r->separator()) return false;
      if (hasBrackets() != r->hasBrackets()) return false;
      for (size_t i = 0, L = length(); i < L; ++i) {
        auto rv = r->get(i);
        auto lv = this->get(i);
        if (!lv && rv) return false;
        else if (!rv && lv) return false;
        else if (!(*lv == *rv)) return false;
      }
      return true;
    }
    if (const Map * r = Cast<Map>(&rhs)) {
      return empty() && r->empty();
    }
    return false;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  IMPLEMENT_AST_OPERATORS(List);
  IMPLEMENT_AST_OPERATORS(Map);
  IMPLEMENT_AST_OPERATORS(SassList);
  IMPLEMENT_AST_OPERATORS(SassArgumentList);
  // IMPLEMENT_AST_OPERATORS(Binary_Expression);
  // IMPLEMENT_AST_OPERATORS(Function);
  // IMPLEMENT_AST_OPERATORS(FunctionExpression);
  // IMPLEMENT_AST_OPERATORS(Variable);
  IMPLEMENT_AST_OPERATORS(Number);
  IMPLEMENT_AST_OPERATORS(Color_RGBA);
  IMPLEMENT_AST_OPERATORS(Color_HSLA);
  // IMPLEMENT_AST_OPERATORS(Custom_Error);
  // IMPLEMENT_AST_OPERATORS(Custom_Warning);
  IMPLEMENT_AST_OPERATORS(Boolean);
  // IMPLEMENT_AST_OPERATORS(Interpolation);
  IMPLEMENT_AST_OPERATORS(StringLiteral);
  IMPLEMENT_AST_OPERATORS(String_Constant);
  IMPLEMENT_AST_OPERATORS(String_Quoted);
  // IMPLEMENT_AST_OPERATORS(Null);
  // IMPLEMENT_AST_OPERATORS(Parent_Reference);

  IMPLEMENT_AST_OPERATORS(StringExpression);

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  SassArgumentList::SassArgumentList(
    ParserState pstate,
    std::vector<ValueObj> values,
    Sass_Separator separator,
    KeywordMap<ValueObj> keywords) :
    SassList(pstate, values, separator, false),
    _keywords(keywords),
    _wereKeywordsAccessed(false)
  {
  }

  SassArgumentList::SassArgumentList(
    const SassArgumentList* ptr) :
    SassList(ptr),
    _keywords(ptr->_keywords),
    _wereKeywordsAccessed(ptr->_wereKeywordsAccessed)
  {
  }

  // Convert native string keys to sass strings
  SassMap* SassArgumentList::keywordsAsSassMap() const
  {
    SassMap* map = SASS_MEMORY_NEW(SassMap, pstate());
    for (auto kv : _keywords) {
      SassString* keystr = SASS_MEMORY_NEW(
        SassString, pstate(), kv);
      map->insert(keystr, _keywords.get(kv));
    }
    return map;
  }

  bool SassArgumentList::operator==(const Value& rhs) const
  {
    if (const SassList * r = Cast<SassList>(&rhs)) {
      return SassList::operator==(*r);
    }
    if (const Map * r = Cast<Map>(&rhs)) {
      return SassList::operator==(*r);
    }
    return false;
  }

  SassFunction::SassFunction(
    ParserState pstate,
    CallableObj callable) :
    Value(pstate),
    callable_(callable)
  {
  }

  bool SassFunction::operator== (const Value& rhs) const
  {
    if (const SassFunction* fn = Cast<SassFunction>(&rhs)) {
      return ObjEqualityFn(callable_, fn->callable());
    }
    return false;
  }

  // std::string Function::name() {
  //   if (definition_) {
  //     return definition_->name();
  //   }
  //   return "";
  // }

}
