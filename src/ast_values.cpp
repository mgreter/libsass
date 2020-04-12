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

  static std::hash<bool> boolHasher;
  static std::hash<double> doubleHasher;
  static std::hash<std::size_t> sizetHasher;
  static std::hash<sass::string> stringHasher;

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Standard value constructor
  Value::Value(const SourceSpan& pstate)
    : Expression(pstate)
  { }

  // Return normalized index for vector from overflow-able sass index
  size_t Value::sassIndexToListIndex(Value* sassIndex, Logger& logger, const sass::string& name)
  {
    long index = sassIndex->assertNumber(logger, name)
      ->assertInt(logger, sassIndex->pstate(), name);
    size_t size = lengthAsList();
    if (index == 0) throw Exception::SassScriptException2(
      "List index may not be 0.", logger, sassIndex->pstate(), name);
    if (size < size_t(std::abs(index))) {
      sass::sstream strm;
      strm << "Invalid index " << index << " for a list ";
      strm << "with " << size << " elements.";
      throw Exception::SassScriptException2(
        strm.str(), logger, sassIndex->pstate(), name);
    }
    return index < 0 ? size + index : size_t(index) - 1;
  }
  // EO sassIndexToListIndex

  // Converts a `selector-parse()`-style input into a string that can be parsed.
  // Returns `false` if [this] isn't a type or a structure that can be parsed as a selector.
  bool Value::_selectorStringOrNull(Logger& logger, sass::string& rv) {

	  if (SassString* str = isString()) {
      rv = str->value();
      return true;
	  }

    if (SassList * list = isList()) {

      if (list->empty()) return false;

      sass::vector<sass::string> result;
      if (list->separator() == SASS_COMMA) {
        for (auto complex : list->elements()) {
          SassList* cplxLst = complex->isList();
          SassString* cplxStr = complex->isString();
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
        for (auto compound : list->elements()) {
          SassString* cmpdStr = compound->isString();
          if (cmpdStr) {
            result.emplace_back(cmpdStr->value());
          }
          else {
            return false;
          }
        }
      }
      rv = StringUtils::join(result, list->separator() == SASS_COMMA ? ", " : " ");
      return true;

    }

    return false;

  }
  // EO _selectorStringOrNull


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
    SourceSpan span(pstate());
    callStackFrame frame(*ctx.logger, span);
    sass::string string = _selectorString(*ctx.logger, span, name);
    auto qwe = SASS_MEMORY_NEW(ItplFile, string.c_str(), pstate_.source, pstate_);
    SelectorParser p2(ctx, qwe);
    p2._allowParent = allowParent;
    auto sel = p2.parse();
    return sel.detach();
  }

  CompoundSelector* Value::assertCompoundSelector(Context& ctx, const sass::string& name, bool allowParent) {
    SourceSpan span(pstate());
    callStackFrame frame(*ctx.logger, span);
    sass::string string = _selectorString(*ctx.logger, span, name);
    auto qwe = SASS_MEMORY_NEW(ItplFile, string.c_str(), pstate_.source, pstate_);
    SelectorParser p2(ctx, qwe);
    p2._allowParent = allowParent;
    auto sel = p2.parseCompoundSelector();
    return sel.detach();
  }

  // Returns a valid CSS representation of [this].
  ///
  // Throws a [SassScriptException] if [this] can't be represented in plain
  // CSS. Use [toString] instead to get a string representation even if this
  // isn't valid CSS.
  ///
  // If [quote] is `false`, quoted strings are emitted without quotes.

  sass::string Value::toCssString(bool quote) const {
    return to_css(true);
  }

  // The SassScript `=` operation.
  inline Value* Value::singleEquals(
    Value* other, Logger& logger, const SourceSpan& pstate) const
  {
    sass::string text(toCssString());
    text += "=" + other->toCssString();
    return SASS_MEMORY_NEW(SassString, pstate, text);
  }

  // The SassScript `+` operation.
  inline Value* Value::plus(
    Value* other, Logger& logger, const SourceSpan& pstate) const {
    if (SassString * str = other->isString()) {
      sass::string text(toCssString() + str->value());
      return SASS_MEMORY_NEW(SassString,
        pstate, text, str->hasQuotes());
    }
    else {
      sass::string text(toCssString());
      text += other->toCssString();
      return SASS_MEMORY_NEW(SassString, pstate, text);
    }
  }

  // The SassScript `-` operation.
  inline Value* Value::minus(
    Value* other, Logger& logger, const SourceSpan& pstate) const {
    sass::string text(toCssString());
    text += "-" + other->toCssString();
    return SASS_MEMORY_NEW(SassString, pstate, text);
  }

  // The SassScript `/` operation.
  inline Value* Value::dividedBy(Value* other, Logger& logger, const SourceSpan& pstate) const {
    sass::string text(toCssString());
    text += "/" + other->toCssString();
    return SASS_MEMORY_NEW(SassString, pstate, text);
  }

  // The SassScript unary `+` operation.
  inline Value* Value::unaryPlus(Logger& logger, const SourceSpan& pstate) const
  {
    sass::string text("+" + toCssString());
    return SASS_MEMORY_NEW(SassString, pstate, text);
  }

  // The SassScript unary `-` operation.
  inline Value* Value::unaryMinus(Logger& logger, const SourceSpan& pstate) const
  {
    sass::string text("-" + toCssString());
    return SASS_MEMORY_NEW(SassString, pstate, text);
  }

  // The SassScript unary `/` operation.
  inline Value* Value::unaryDivide(Logger& logger, const SourceSpan& pstate) const
  {
    sass::string text("/" + toCssString());
    return SASS_MEMORY_NEW(SassString, pstate, text);
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

  Map::Map(const SourceSpan& pstate)
  : Value(pstate),
    Hashed()
  {}

  Map::Map(const SourceSpan& pstate, const Hashed<ValueObj, ValueObj>& copy) :
    Value(pstate),
    Hashed(copy)
  {}

  Map::Map(const SourceSpan& pstate, Hashed<ValueObj, ValueObj>&& move) :
    Value(pstate),
    Hashed(std::move(move))
  {}

  Map::Map(const SourceSpan& pstate, Hashed::ordered_map_type&& move) :
    Value(pstate),
    Hashed(std::move(move))
  {}

  Map::Map(const Map* ptr)
  : Value(ptr),
    Hashed(*ptr)
  {}

  // Maps are equal if they have the same items
  // at the same key, order is not important.
  bool Map::operator== (const Value& rhs) const
  {
    if (const Map* r = rhs.isMap()) {
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
    if (const SassList * r = rhs.isList()) {
      return r->empty() && empty();
    }
    return false;
  }

  Value* Map::get2(size_t idx) {
    // asVector();

    auto kv = elements_.begin() + idx;
    pair = SASS_MEMORY_NEW(
      SassList, kv->first->pstate(),
      { kv->first, kv->second },
      SASS_SPACE);
    return pair.detach();
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
                    Operand op, ExpressionObj lhs, ExpressionObj rhs)
  : Expression(pstate), op_(op), left_(lhs), right_(rhs), allowsSlash_(false), hash_(0)
  { }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Variable::Variable(const SourceSpan& pstate, const EnvKey& name, IdxRef vidx)
  : Expression(pstate), name_(name), vidx_(vidx)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Number::Number(const SourceSpan& pstate, double val, const sass::string& u, bool zero)
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
  }

  Number::Number(const SourceSpan& pstate, double val, Units&& units, bool zero)
    : Value(pstate),
    Units(std::move(units)),
    value_(val),
    // epsilon_(0.00001),
    zero_(zero),
    lhsAsSlash_(),
    rhsAsSlash_(),
    hash_(0)
  {
  }

  Number::Number(const Number* ptr)
  : Value(ptr),
    Units(ptr),
    value_(ptr->value_),
    // epsilon_(ptr->epsilon_),
    lhsAsSlash_(ptr->lhsAsSlash_),
    rhsAsSlash_(ptr->rhsAsSlash_),
    hash_(ptr->hash_)
  {}

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
      hash_start(hash_, doubleHasher(value_));
      for (const auto numerator : numerators)
        hash_combine(hash_, stringHasher(numerator));
      for (const auto denominator : denominators)
        hash_combine(hash_, stringHasher(denominator));
    }
    return hash_;
  }

  bool Number::operator== (const Value& rhs) const
  {
    if (const Number* n = rhs.isNumber()) {
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
    /* ToDo: do we always get useful back-traces? */
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
    /* ToDo: do we always get useful back-traces? */
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
    /* ToDo: do we always get useful back-traces? */
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
    /* ToDo: do we always get useful back-traces? */
    throw Exception::IncompatibleUnits(*this, rhs);
  }
  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Color::Color(const SourceSpan& pstate, double a, const sass::string& disp)
  : Value(pstate),
    disp_(disp), a_(a),
    hash_(0)
  {}

  Color::Color(const Color* ptr)
  : Value(ptr),
    // reset on copy
    disp_(""),
    a_(ptr->a_),
    hash_(ptr->hash_)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Color_RGBA::Color_RGBA(const SourceSpan& pstate, double r, double g, double b, double a, const sass::string& disp)
  : Color(pstate, a, disp),
    r_(r), g_(g), b_(b)
  {}

  Color_RGBA::Color_RGBA(const Color_RGBA* ptr)
  : Color(ptr),
    r_(ptr->r_),
    g_(ptr->g_),
    b_(ptr->b_)
  {}

  bool Color_RGBA::operator== (const Value& rhs) const
  {
    if (auto r = rhs.isColorRGBA()) {
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
      hash_start(hash_, typeid(Color_RGBA).hash_code());
      hash_combine(hash_, doubleHasher(a_));
      hash_combine(hash_, doubleHasher(r_));
      hash_combine(hash_, doubleHasher(g_));
      hash_combine(hash_, doubleHasher(b_));
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

  Color_HSLA::Color_HSLA(const SourceSpan& pstate, double h, double s, double l, double a, const sass::string& disp)
  : Color(pstate, a, disp),
    h_(absmod(h, 360.0)),
    s_(clip(s, 0.0, 100.0)),
    l_(clip(l, 0.0, 100.0))
    // hash_(0)
  {}

  Color_HSLA::Color_HSLA(const Color_HSLA* ptr)
  : Color(ptr),
    h_(ptr->h_),
    s_(ptr->s_),
    l_(ptr->l_)
    // hash_(ptr->hash_)
  {}

  bool Color_HSLA::operator== (const Value& rhs) const
  {
    if (auto r = rhs.isColorHSLA()) {
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
      hash_start(hash_, typeid(Color_HSLA).hash_code());
      hash_combine(hash_, doubleHasher(a_));
      hash_combine(hash_, doubleHasher(h_));
      hash_combine(hash_, doubleHasher(s_));
      hash_combine(hash_, doubleHasher(l_));
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

  Custom_Error::Custom_Error(const SourceSpan& pstate, const sass::string& msg)
  : Value(pstate), message_(msg)
  {}

  bool Custom_Error::operator== (const Value& rhs) const
  {
    if (auto r = rhs.isError()) {
      return message() == r->message();
    }
    return false;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Custom_Warning::Custom_Warning(const SourceSpan& pstate, const sass::string& msg)
  : Value(pstate), message_(msg)
  {}

  bool Custom_Warning::operator== (const Value& rhs) const
  {
    if (auto r = rhs.isWarning()) {
      return message() == r->message();
    }
    return false;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Boolean::Boolean(const SourceSpan& pstate, bool val)
    : Value(pstate), value_(val),
    hash_(0)
  {}

  Boolean::Boolean(const Boolean* ptr)
  : Value(ptr),
    value_(ptr->value_),
    hash_(ptr->hash_)
  {}

 bool Boolean::operator== (const Value& rhs) const
 {
   if (auto r = rhs.isBoolean()) {
     return (value() == r->value());
   }
   return false;
 }

 size_t Boolean::hash() const
  {
    if (hash_ == 0) {
      hash_ = boolHasher(value_);
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
      return Strings::empty;
    }
    return first()->getText();
  }

  const sass::string& Interpolation::getInitialPlain() const
  {
    if (empty()) return empty_string;
    if (Interpolant* str = first()->getItplString()) { // Ex
      return str->getText();
    }
    return empty_string;
  }

  Interpolation::Interpolation(const SourceSpan& pstate, Interpolant* ex) :
    Expression(pstate), VectorizedBase()
  {
    if (ex != nullptr) append(ex);
  }

  Interpolation::Interpolation(
    const SourceSpan& pstate,
    sass::vector<InterpolantObj> items) :
    Expression(pstate),
    VectorizedBase(std::move(items))
  {
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  uint8_t StringExpression::findBestQuote()
  {
    using namespace Character;
    bool containsDoubleQuote = false;
    for (auto item : text_->elements()) {
      if (auto str = Cast<SassString>(item)) { // Ex
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
    Interpolation* itpl = SASS_MEMORY_NEW(Interpolation, pstate,
      SASS_MEMORY_NEW(SassString, pstate, text)); // Literal
    return SASS_MEMORY_NEW(StringExpression, pstate, itpl, quotes);
  }

  InterpolationObj StringExpression::getAsInterpolation(bool escape, uint8_t quote)
  {

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
      if (ItplString * str = value->getItplString()) { // Ex
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
              buffer.write($backslash);
            }
            buffer.write(codeUnit);
          }

        }
      }
      else {
        buffer.add(value);
      }
    }

    if (quote != 0) {
      buffer.write(quote);
    }
    return buffer.getInterpolation(pstate_);

  }

  StringExpression::StringExpression(SourceSpan&& pstate, InterpolationObj text, bool quote) :
    Expression(std::move(pstate)), text_(text), hasQuotes_(quote)
  {

  }

  StringExpression::StringExpression(const SourceSpan& pstate, InterpolationObj text, bool quote) :
    Expression(pstate), text_(text), hasQuotes_(quote)
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

  ItplString::ItplString(const SourceSpan& pstate, const sass::string& text) :
    Interpolant(pstate), text_(text)
  {
  }

  ItplString::ItplString(const SourceSpan& pstate, sass::string&& text) :
    Interpolant(pstate), text_(std::move(text))
  {
  }

  ItplString::ItplString(const ItplString* ptr) :
    Interpolant(ptr),
    text_(ptr->text_)
  {
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  SassString::SassString(const SourceSpan& pstate, const sass::string& val, bool hasQuotes)
  : Value(pstate), value_(val), hasQuotes_(hasQuotes), hash_(0)
  {}

  SassString::SassString(const SourceSpan& pstate, sass::string&& val, bool hasQuotes)
    : Value(pstate), value_(std::move(val)), hasQuotes_(hasQuotes), hash_(0)
  {}

  SassString::SassString(const SourceSpan& pstate, const char* beg, bool hasQuotes)
  : Value(pstate), value_(beg), hasQuotes_(hasQuotes), hash_(0)
  {}

  SassString::SassString(const SassString* ptr)
  : Value(ptr),
    value_(ptr->value_),
    hasQuotes_(ptr->hasQuotes_),
    hash_(ptr->hash_)
  {}

  bool SassString::is_invisible() const {
    return !hasQuotes_ && value_.empty();
  }

  bool SassString::operator== (const Value& rhs) const
  {
    if (auto cstr = rhs.isString()) {
      return value() == cstr->value();
    }
    return false;
  }

  bool SassString::operator== (const SassString& rhs) const
  {
    return value() == rhs.value();
  }

  sass::string SassString::inspect() const
  {
    return hasQuotes() ? quote(value_, '*') : value_;
  }

  bool SassString::isBlank() const {
    if (hasQuotes_) return false;
    return value_.empty();
  }

  size_t SassString::hash() const
  {
    if (hash_ == 0) {
      hash_ = stringHasher(value_);
    }
    return hash_;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Null::Null(const SourceSpan& pstate)
  : Value(pstate)
  {}

  bool Null::operator== (const Value& rhs) const
  {
    return rhs.isNull();
  }

  size_t Null::hash() const
  {
    return -1;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Parent_Reference::Parent_Reference(const SourceSpan& pstate)
  : Value(pstate)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  SassList::SassList(
    const SourceSpan& pstate,
    const sass::vector<ValueObj>& values,
    Sass_Separator separator,
    bool hasBrackets) :
    Value(pstate),
    Vectorized(values),
    separator_(separator),
    hasBrackets_(hasBrackets)
  {}

  SassList::SassList(
    const SourceSpan& pstate,
    sass::vector<ValueObj>&& values,
    Sass_Separator separator,
    bool hasBrackets) :
    Value(pstate),
    Vectorized(std::move(values)),
    separator_(separator),
    hasBrackets_(hasBrackets)
  {}

  SassList::SassList(
    const SassList* ptr) :
    Value(ptr),
    Vectorized(ptr),
    separator_(ptr->separator_),
    hasBrackets_(ptr->hasBrackets_)
  {}

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
      hash_start(hash_, sizetHasher(separator()));
      hash_combine(hash_, boolHasher(hasBrackets()));
      for (size_t i = 0, L = length(); i < L; ++i)
        hash_combine(hash_, elements()[i]->hash());
    }
    return hash_;
  }

  bool SassList::operator==(const Value& rhs) const
  {
    if (const SassList* r = rhs.isList()) {
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
    if (const Map * r = rhs.isMap()) {
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
  IMPLEMENT_AST_OPERATORS(ItplString);
  IMPLEMENT_AST_OPERATORS(SassString);
  // IMPLEMENT_AST_OPERATORS(Null);
  // IMPLEMENT_AST_OPERATORS(Parent_Reference);

  IMPLEMENT_AST_OPERATORS(StringExpression);

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  SassArgumentList::SassArgumentList(
    const SourceSpan& pstate,
    const sass::vector<ValueObj>& values,
    Sass_Separator separator,
    const EnvKeyFlatMap<ValueObj>& keywords) :
    SassList(pstate, values, separator, false),
    _keywords(keywords),
    _wereKeywordsAccessed(false)
  {
  }

  SassArgumentList::SassArgumentList(
    const SourceSpan& pstate,
    sass::vector<ValueObj>&& values,
    Sass_Separator separator,
    EnvKeyFlatMap<ValueObj>&& keywords) :
    SassList(pstate, std::move(values), separator, false),
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
      SassString* keystr = SASS_MEMORY_NEW( // XXXXXXXXYYYY
        SassString, pstate(), kv.first.orig().substr(1));
      map->insert(keystr, kv.second);
    }
    return map;
  }

  bool SassArgumentList::operator==(const Value& rhs) const
  {
    if (const SassList * r = rhs.isList()) {
      return SassList::operator==(*r);
    }
    if (const Map * r = rhs.isMap()) {
      return SassList::operator==(*r);
    }
    return false;
  }

  SassFunction::SassFunction(
    const SourceSpan& pstate,
    CallableObj callable) :
    Value(pstate),
    callable_(callable)
  {
  }

  bool SassFunction::operator== (const Value& rhs) const
  {
    if (const SassFunction* fn = rhs.isFunction()) {
      return ObjEqualityFn(callable_, fn->callable());
    }
    return false;
  }

  Values::iterator::iterator(Value* val, bool end) :
    val(val), last(0), cur(0)
  {
    if (val == nullptr) {
      type = NullPtrIterator;
    }
    else if (SassMap* map = val->isMap()) {
      type = MapIterator;
      last = map->size();
    }
    else if (SassList* list = val->isList()) {
      type = ListIterator;
      last = list->length();
    }
    else {
      type = SingleIterator;
      last = 1;
    }
    // Move to end position
    if (end) cur = last;
  }

  Values::iterator& Values::iterator::operator++() {
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

  Value* Values::iterator::operator*() {
    switch (type) {
    case MapIterator:
      return static_cast<SassMap*>(val)->get2(cur);
    case ListIterator:
      return static_cast<SassList*>(val)->get(cur);
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

  // Only used for nth sass function
  // Single values act like lists with 1 item
  // Doesn't allow overflow of index (throw error)
  // Allows negative index but no overflow either
  Value* Value::getValueAt(Value* index, Logger& logger)
  {
    // Check out of boundary access
    sassIndexToListIndex(index, logger, "n");
    // Return single value
    return this;
  }

  // Only used for nth sass function
  // Doesn't allow overflow of index (throw error)
  // Allows negative index but no overflow either
  Value* Map::getValueAt(Value* index, Logger& logger)
  {
    // const sass::vector<ValueObj>& values = asVector();
    size_t idx = sassIndexToListIndex(index, logger, "n");
    return get2(idx);
  }

  // Only used for nth sass function
  // Doesn't allow overflow of index (throw error)
  // Allows negative index but no overflow either
  Value* SassList::getValueAt(Value* index, Logger& logger)
  {
    size_t idx = sassIndexToListIndex(index, logger, "n");
    return get(idx);
  }
}
