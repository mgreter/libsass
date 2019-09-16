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

  Value::Value(SourceSpan&& pstate, bool d, bool e, bool i, Type ct)
    : Expression(std::move(pstate), d, e, i, ct)
  { }

  Value::Value(const SourceSpan& pstate, bool d, bool e, bool i, Type ct)
    : Expression(pstate, d, e, i, ct)
  { }

  // Return normalized index for vector from overflowable sass index
  size_t Value::sassIndexToListIndex(Value* sassIndex, Logger& logger, const sass::string& name)
  {
    long index = sassIndex->assertNumber(logger, name)->assertInt(logger, sassIndex->pstate(), name);
    if (index == 0) throw Exception::SassScriptException2(
      "List index may not be 0.", logger, sassIndex->pstate(), name);
    if ((size_t)abs(index) > lengthAsList()) {
      sass::sstream strm;
      strm << "Invalid index " << index << " for a list ";
      strm << "with " << lengthAsList() << " elements.";
      throw Exception::SassScriptException2(
        strm.str(), logger, sassIndex->pstate(), name);
    }

    return index < 0 ? lengthAsList() + index : index - 1;
  }

  // Converts a `selector-parse()`-style input into a string that can be
  // parsed.
  ///
  // Returns `null` if [this] isn't a type or a structure that can be parsed as
  // a selector.

  bool Value::_selectorStringOrNull(Logger& logger, sass::string& rv) {

	  if (String_Constant* str = Cast<String_Constant>(this)) {
      rv = str->value();
      return true;
	  }

    if (SassList * list = Cast<SassList>(this)) {

      sass::vector<ValueObj> values = list->asVector();

      if (values.empty()) return false;

      sass::vector<sass::string> result;
      if (list->separator() == SASS_COMMA) {
        for (auto complex : values) {
          SassList* cplxLst = Cast<SassList>(complex);
          String_Constant* cplxStr = Cast<String_Constant>(complex);
          if (cplxStr) {
            result.emplace_back(cplxStr->value());
          }
          else if (cplxLst &&
            cplxLst->separator() == SASS_SPACE) {
            sass::string string = complex->_selectorString(
            logger, pstate());
            if (string.empty()) return false;
            result.emplace_back(string);
          }
          else {
            return false;
          }
        }
      }
      else {
        for (auto compound : values) {
          String_Constant* cmpdStr = Cast<String_Constant>(compound);
          if (cmpdStr) {
            result.emplace_back(cmpdStr->value());
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


  // Parses [this] as a selector list, in the same manner as the
  // `selector-parse()` function.
  ///
  // Throws a [SassScriptException] if this isn't a type that can be parsed as a
  // selector, or if parsing fails. If [allowParent] is `true`, this allows
  // [ParentSelector]s. Otherwise, they're considered parse errors.
  ///
  // If this came from a function argument, [name] is the argument name
  // (without the `$`). It's used for error reporting.

  SelectorList* Value::assertSelector(Context& ctx, const sass::string& name, bool allowParent) {
#ifndef GOFAST
    callStackFrame frame(*ctx.logger, Backtrace(pstate()));
#endif
    sass::string string = _selectorString(*ctx.logger, pstate(), name);
    // char* str = sass_copy_c_string(string.c_str());
    // ctx.strings.emplace_back(str);
    // auto qwe = SASS_MEMORY_NEW(SourceFile, "sass://parse-selector", str, -1);
    auto qwe = SASS_MEMORY_NEW(SyntheticFile, string.c_str(), pstate_.source, pstate_);
    SelectorParser p2(ctx, qwe);
    p2._allowParent = allowParent;
    auto sel = p2.parse();
    return sel.detach();
  }

  CompoundSelector* Value::assertCompoundSelector(Context& ctx, const sass::string& name, bool allowParent) {
#ifndef GOFAST
    callStackFrame frame(*ctx.logger, Backtrace(pstate()));
#endif
    sass::string string = _selectorString(*ctx.logger, pstate(), name);
    // char* str = sass_copy_c_string(string.c_str());
    // ctx.strings.emplace_back(str);
    // auto qwe = SASS_MEMORY_NEW(SourceFile, "sass://parse-selector", str, -1);
    auto qwe = SASS_MEMORY_NEW(SyntheticFile, string.c_str(), pstate_.source, pstate_);
    SelectorParser p2(ctx, qwe);
    p2._allowParent = allowParent;
    auto sel = p2.parseCompoundSelector();
    return sel.detach();
  }

  SassList* Value::changeListContents(
    sass::vector<ValueObj> values,
    Sass_Separator separator,
    bool hasBrackets)
  {
    return SASS_MEMORY_NEW(SassList, pstate(),
      std::move(values), separator, hasBrackets);
  }



  // Returns a valid CSS representation of [this].
  ///
  // Throws a [SassScriptException] if [this] can't be represented in plain
  // CSS. Use [toString] instead to get a string representation even if this
  // isn't valid CSS.
  ///
  // If [quote] is `false`, quoted strings are emitted without quotes.

  inline sass::string Value::toCssString(bool quote) const {
    return to_css(true);
  }

  // The SassScript `=` operation.
  inline Value* Value::singleEquals(
    Value* other, Logger& logger, const SourceSpan& pstate) const
  {
    sass::string text(toCssString());
    text += "=" + other->toCssString();
    return SASS_MEMORY_NEW(String_Constant, pstate, text);
  }

  // The SassScript `+` operation.
  inline Value* Value::plus(
    Value* other, Logger& logger, const SourceSpan& pstate) const {
    if (String_Constant * str = Cast<String_Constant>(other)) {
      sass::string text(toCssString() + str->value());
      return SASS_MEMORY_NEW(String_Constant,
        pstate, text, str->hasQuotes());
    }
    else {
      sass::string text(toCssString());
      text += other->toCssString();
      return SASS_MEMORY_NEW(String_Constant, pstate, text);
    }
  }

  // The SassScript `-` operation.
  inline Value* Value::minus(
    Value* other, Logger& logger, const SourceSpan& pstate) const {
    sass::string text(toCssString());
    text += "-" + other->toCssString();
    return SASS_MEMORY_NEW(String_Constant, pstate, text);
  }

  // The SassScript `/` operation.
  inline Value* Value::dividedBy(Value* other, Logger& logger, const SourceSpan& pstate) const {
    sass::string text(toCssString());
    text += "/" + other->toCssString();
    return SASS_MEMORY_NEW(String_Constant, pstate, text);
  }

  // The SassScript unary `+` operation.
  inline Value* Value::unaryPlus(Logger& logger, const SourceSpan& pstate) const
  {
    sass::string text("+" + toCssString());
    return SASS_MEMORY_NEW(String_Constant, pstate, text);
  }

  // The SassScript unary `-` operation.
  inline Value* Value::unaryMinus(Logger& logger, const SourceSpan& pstate) const
  {
    sass::string text("-" + toCssString());
    return SASS_MEMORY_NEW(String_Constant, pstate, text);
  }

  // The SassScript unary `/` operation.
  inline Value* Value::unaryDivide(Logger& logger, const SourceSpan& pstate) const
  {
    sass::string text("/" + toCssString());
    return SASS_MEMORY_NEW(String_Constant, pstate, text);
  }

  // The SassScript unary `not` operation.
  inline Value* Value::unaryNot(Logger& logger, const SourceSpan& pstate) const
  {
    return SASS_MEMORY_NEW(Boolean,
      pstate, false);
  }

  Value::Value(const Value* ptr)
  : Expression(ptr) { }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Map::Map(SourceSpan&& pstate)
  : Value(std::move(pstate)),
    Hashed()
  { concrete_type(MAP); }

  Map::Map(SourceSpan&& pstate, const Hashed<ValueObj, ValueObj>& copy) :
    Value(pstate),
    Hashed(copy)
  {
    concrete_type(MAP);
  }

  Map::Map(SourceSpan&& pstate, Hashed<ValueObj, ValueObj>&& move) :
    Value(std::move(pstate)),
    Hashed(std::move(move))
  {
    concrete_type(MAP);
  }

  Map::Map(const Map* ptr)
  : Value(ptr),
    Hashed(*ptr)
  { concrete_type(MAP); }

  // Maps are equal if they have the same items
  // at the same key, order is not important.
  bool Map::operator== (const Value& rhs) const
  {
    if (const Map* r = Cast<Map>(&rhs)) {
      if (size() != r->size()) return false;
      for (auto kv : elements_) {
        auto lv = kv.second;
        auto rv = r->at(kv.first);
        if (!lv && rv) return false;
        else if (!rv && lv) return false;
        else if (!(*lv == *rv)) return false;
      }
      return true;
    }
    if (const SassList * r = Cast<SassList>(&rhs)) {
      return r->empty() && empty();
    }
    return false;
  }

  size_t Map::hash() const
  {
    if (hash_ == 0) {
      for (auto kv : elements_) {
        hash_combine(hash_, kv.first->hash());
        hash_combine(hash_, kv.second->hash());
      }
    }

    return hash_;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Binary_Expression::Binary_Expression(const SourceSpan& pstate,
                    Operand op, Expression_Obj lhs, Expression_Obj rhs)
  : Expression(pstate), op_(op), left_(lhs), right_(rhs), allowsSlash_(false), hash_(0)
  { }

  // const sass::string Binary_Expression::separator()
  // {
  //   return sass_op_separator(optype());
  // }

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

  Variable::Variable(const SourceSpan& pstate, const EnvString& name, IdxRef vidx, IdxRef pidx)
  : Expression(pstate), name_(name), vidx_(vidx), pidx_(pidx)
  { concrete_type(VARIABLE); }

  size_t Variable::hash() const
  {
    return name_.hash();
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Number::Number(SourceSpan&& pstate, double val, const sass::string& u, bool zero)
  : Value(std::move(pstate)),
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
        sass::string unit(u.substr(l, r == sass::string::npos ? r : r - l));
        if (!unit.empty()) {
          if (nominator) numerators.emplace_back(unit);
          else denominators.emplace_back(unit);
        }
        if (r == sass::string::npos) break;
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

  Number::Number(SourceSpan&& pstate, double val, Units&& units, bool zero)
    : Value(std::move(pstate)),
    Units(std::move(units)),
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
        hash_combine(hash_, std::hash<sass::string>()(numerator));
      for (const auto denominator : denominators)
        hash_combine(hash_, std::hash<sass::string>()(denominator));
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

  bool isSimpleNumberComparison(const Number& lhs, const Number& rhs)
  {
    // Gather statistics from the units
    size_t l_n_units = lhs.numerators.size();
    size_t r_n_units = rhs.numerators.size();
    size_t l_d_units = lhs.denominators.size();
    size_t r_d_units = rhs.denominators.size();
    size_t l_units = l_n_units + l_d_units;
    size_t r_units = r_n_units + r_d_units;

    // Old ruby sass behavior (deprecated)
    if (l_units == 0) return true;
    if (r_units == 0) return true;

    // check if both sides have exactly the same units
    if (l_n_units == r_n_units && l_d_units == r_d_units) {
      return (lhs.numerators == rhs.numerators)
        && (lhs.denominators == rhs.denominators);
    }

    return false;
  }

  bool Number::operator== (const Number& rhs) const
  {
    // Ignore units in certain cases
    if (isSimpleNumberComparison(*this, rhs)) {
      return NEAR_EQUAL(value(), rhs.value());
    }
    // Otherwise we need copies
    Number l(*this), r(rhs);
    // Reduce and normalize
    l.reduce(); r.reduce();
    l.normalize(); r.normalize();
    // Ensure both have same units
    Units &lhs_unit = l, &rhs_unit = r;
    return lhs_unit == rhs_unit &&
      NEAR_EQUAL(l.value(), r.value());
  }

  bool Number::operator< (const Number& rhs) const
  {
    // Ignore units in certain cases
    if (isSimpleNumberComparison(*this, rhs)) {
      return value() < rhs.value();
    }
    // Otherwise we need copies
    Number l(*this), r(rhs);
    // Reduce and normalize
    l.reduce(); r.reduce();
    l.normalize(); r.normalize();
    // Ensure both have same units
    Units& lhs_unit = l, & rhs_unit = r;
    if (lhs_unit == rhs_unit) {
      return l.value() < r.value();
    }
    /* ToDo: do we always get usefull backtraces? */
    throw Exception::IncompatibleUnits(*this, rhs);
  }

  bool Number::operator> (const Number& rhs) const
  {
    // Ignore units in certain cases
    if (isSimpleNumberComparison(*this, rhs)) {
      return value() > rhs.value();
    }
    // Otherwise we need copies
    Number l(*this), r(rhs);
    // Reduce and normalize
    l.reduce(); r.reduce();
    l.normalize(); r.normalize();
    // Ensure both have same units
    Units& lhs_unit = l, & rhs_unit = r;
    if (lhs_unit == rhs_unit) {
      return l.value() > r.value();
    }
    /* ToDo: do we always get usefull backtraces? */
    throw Exception::IncompatibleUnits(*this, rhs);
  }

  bool Number::operator<= (const Number& rhs) const
  {
    // Ignore units in certain cases
    if (isSimpleNumberComparison(*this, rhs)) {
      return value() <= rhs.value();
    }
    // Otherwise we need copies
    Number l(*this), r(rhs);
    // Reduce and normalize
    l.reduce(); r.reduce();
    l.normalize(); r.normalize();
    // Ensure both have same units
    Units& lhs_unit = l, & rhs_unit = r;
    if (lhs_unit == rhs_unit) {
      return l.value() <= r.value();
    }
    /* ToDo: do we always get usefull backtraces? */
    throw Exception::IncompatibleUnits(*this, rhs);
  }

  bool Number::operator>= (const Number& rhs) const
  {
    // Ignore units in certain cases
    if (isSimpleNumberComparison(*this, rhs)) {
      return value() >= rhs.value();
    }
    // Otherwise we need copies
    Number l(*this), r(rhs);
    // Reduce and normalize
    l.reduce(); r.reduce();
    l.normalize(); r.normalize();
    // Ensure both have same units
    Units& lhs_unit = l, & rhs_unit = r;
    if (lhs_unit == rhs_unit) {
      return l.value() >= r.value();
    }
    /* ToDo: do we always get usefull backtraces? */
    throw Exception::IncompatibleUnits(*this, rhs);
  }
  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Color::Color(const SourceSpan& pstate, double a, const sass::string& disp)
  : Value(pstate),
    disp_(disp), a_(a),
    hash_(0)
  { concrete_type(COLOR); }

  Color::Color(SourceSpan&& pstate, double a, const sass::string& disp)
    : Value(std::move(pstate)),
    disp_(disp), a_(a),
    hash_(0)
  {
    concrete_type(COLOR);
  }

  Color::Color(const Color* ptr)
  : Value(ptr),
    // reset on copy
    disp_(""),
    a_(ptr->a_),
    hash_(ptr->hash_)
  { concrete_type(COLOR); }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Color_RGBA::Color_RGBA(const SourceSpan& pstate, double r, double g, double b, double a, const sass::string& disp)
  : Color(pstate, a, disp),
    r_(r), g_(g), b_(b)
  { concrete_type(COLOR); }

  Color_RGBA::Color_RGBA(SourceSpan&& pstate, double r, double g, double b, double a, const sass::string& disp)
    : Color(std::move(pstate), a, disp),
    r_(r), g_(g), b_(b)
  {
    concrete_type(COLOR);
  }

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
      hash_ = std::hash<sass::string>()("RGBA");
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

  Color_HSLA::Color_HSLA(SourceSpan&& pstate, double h, double s, double l, double a, const sass::string& disp)
  : Color(std::move(pstate), a, disp),
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
      hash_ = std::hash<sass::string>()("HSLA");
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

  Custom_Error::Custom_Error(SourceSpan&& pstate, const sass::string& msg)
  : Value(std::move(pstate)), message_(msg)
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

  Custom_Warning::Custom_Warning(SourceSpan&& pstate, const sass::string& msg)
  : Value(std::move(pstate)), message_(msg)
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

  Boolean::Boolean(SourceSpan&& pstate, bool val)
  : Value(std::move(pstate)), value_(val),
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
  sass::string empty_string = "";

  // Should be? asPlain in dart-sass
  const sass::string& Interpolation::getPlainString() const
  {
    if (length() != 1) {
      return empty_string;
    }
    else if (StringLiteral * str = Cast<StringLiteral>(first())) {
      return str->text();
    }
    else if (String_Constant * str = Cast<String_Constant>(first())) {
      return str->value();
    }
    else {
      return empty_string;
    }
  }

  const sass::string& Interpolation::getInitialPlain() const
  {
    if (StringLiteral * str = Cast<StringLiteral>(first())) {
      return str->text();
    }
    else if (String_Constant * str = Cast<String_Constant>(first())) {
      return str->value();
    }
    return empty_string;
  }

  Interpolation::Interpolation(const SourceSpan& pstate, Expression* ex) :
    Expression(pstate), Vectorized<ExpressionObj>()
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

  StringExpression* StringExpression::plain(const SourceSpan& pstate, const sass::string& text, bool quotes)
  {
    Interpolation* itpl = SASS_MEMORY_NEW(Interpolation, pstate);
    itpl->append(SASS_MEMORY_NEW(StringLiteral, pstate, text));
    return SASS_MEMORY_NEW(StringExpression, pstate, itpl, quotes);
  }

  InterpolationObj StringExpression::getAsInterpolation(bool escape, uint8_t quote)
  {
    InterpolationObj itpl = SASS_MEMORY_NEW(Interpolation,
      SourceSpan::fake("[pstate55]"));

    if (!hasQuotes()) return text_;
    // quote ? ? = hasQuotes ? _bestQuote() : null;
    if (!quote && hasQuotes()) quote = findBestQuote();
    InterpolationBuffer buffer(pstate_);

    using namespace Character;

    if (quote != 0) {
      buffer.write(quote);
    }

    for (auto value : text_->elements()) {
      // assert(value is Expression || value is String);
      if (StringLiteral * str = Cast<StringLiteral>(value)) {
        sass::string value(str->text());
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
    return buffer.getInterpolation(pstate_);


    // itpl->has
    // return text_;
  }

  StringExpression::StringExpression(SourceSpan&& pstate, InterpolationObj text, bool quote) :
    Expression(std::move(pstate)), text_(text), hasQuotes_(quote)
  {

  }

  StringExpression::StringExpression(const StringExpression* ptr) :
    Expression(ptr),
    text_(ptr->text_),
    hasQuotes_(ptr->hasQuotes_)
  {
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  StringLiteral::StringLiteral(SourceSpan&& pstate, const sass::string& text) :
    Value(std::move(pstate)), text_(text)
  {
  }

  StringLiteral::StringLiteral(SourceSpan&& pstate, sass::string&& text) :
    Value(std::move(pstate)), text_(std::move(text))
  {
  }

  StringLiteral::StringLiteral(const StringLiteral* ptr) :
    Value(ptr),
    text_(ptr->text_)
  {
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  String_Constant::String_Constant(SourceSpan&& pstate, const sass::string& val, bool hasQuotes)
  : Value(std::move(pstate)), value_(val), hasQuotes_(hasQuotes), hash_(0)
  {
    concrete_type(STRING);
  }

  String_Constant::String_Constant(SourceSpan&& pstate, sass::string&& val, bool hasQuotes)
    : Value(std::move(pstate)), value_(std::move(val)), hasQuotes_(hasQuotes), hash_(0)
  {
    concrete_type(STRING);
  }

  String_Constant::String_Constant(SourceSpan&& pstate, const char* beg, bool hasQuotes)
  : Value(std::move(pstate)), value_(beg), hasQuotes_(hasQuotes), hash_(0)
  {
    concrete_type(STRING);
  }

  String_Constant::String_Constant(const String_Constant* ptr)
  : Value(ptr),
    value_(ptr->value_),
    hasQuotes_(ptr->hasQuotes_),
    hash_(ptr->hash_)
  {
    concrete_type(STRING);
  }

  bool String_Constant::is_invisible() const {
    return !hasQuotes_ && value_.empty();
  }

  bool String_Constant::operator== (const Value& rhs) const
  {
    if (auto cstr = Cast<String_Constant>(&rhs)) {
      return value() == cstr->value();
    }
    return false;
  }

  sass::string String_Constant::inspect() const
  {
    return quote(value_, '*');
  }

  inline bool String_Constant::isBlank() const {
    if (hasQuotes_) return false;
    return value_.empty();
  }

  size_t String_Constant::hash() const
  {
    if (hash_ == 0) {
      hash_ = std::hash<sass::string>()(value_);
    }
    return hash_;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Null::Null(SourceSpan&& pstate)
  : Value(std::move(pstate))
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

  Parent_Reference::Parent_Reference(SourceSpan&& pstate)
  : Value(std::move(pstate))
  { concrete_type(PARENT); }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  SassList::SassList(
    SourceSpan&& pstate,
    const sass::vector<ValueObj>& values,
    Sass_Separator separator,
    bool hasBrackets) :
    Value(std::move(pstate)),
    Vectorized(values),
    separator_(separator),
    hasBrackets_(hasBrackets)
  {
    concrete_type(LIST);
  }

  SassList::SassList(
    SourceSpan&& pstate,
    sass::vector<ValueObj>&& values,
    Sass_Separator separator,
    bool hasBrackets) :
    Value(std::move(pstate)),
    Vectorized(std::move(values)),
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

  Map* SassList::assertMap(Logger& logger, const sass::string& name) {
    if (!empty()) { return Value::assertMap(logger, name); }
    else { return SASS_MEMORY_NEW(Map, pstate()); }
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

  IMPLEMENT_AST_OPERATORS(Map);
  IMPLEMENT_AST_OPERATORS(SassList);
  IMPLEMENT_AST_OPERATORS(SassArgumentList);
  // IMPLEMENT_AST_OPERATORS(Binary_Expression);
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
  // IMPLEMENT_AST_OPERATORS(Null);
  // IMPLEMENT_AST_OPERATORS(Parent_Reference);

  IMPLEMENT_AST_OPERATORS(StringExpression);

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  SassArgumentList::SassArgumentList(
    SourceSpan&& pstate,
    const sass::vector<ValueObj>& values,
    Sass_Separator separator,
    const KeywordMap<ValueObj>& keywords) :
    SassList(std::move(pstate), values, separator, false),
    _keywords(keywords),
    _wereKeywordsAccessed(false)
  {
  }

  SassArgumentList::SassArgumentList(
    SourceSpan&& pstate,
    sass::vector<ValueObj>&& values,
    Sass_Separator separator,
    KeywordMap<ValueObj>&& keywords) :
    SassList(std::move(pstate), std::move(values), separator, false),
    _keywords(std::move(keywords)),
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
  Map* SassArgumentList::keywordsAsSassMap() const
  {
    Map* map = SASS_MEMORY_NEW(Map, pstate());
    for (auto kv : _keywords) {
      String_Constant* keystr = SASS_MEMORY_NEW( // XXXXXXXXYYYY
        String_Constant, pstate(), kv.first.orig().substr(1));
      map->insert(keystr, kv.second);
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
    SourceSpan&& pstate,
    CallableObj callable) :
    Value(std::move(pstate)),
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

}
