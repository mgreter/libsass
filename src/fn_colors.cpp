// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include <iomanip>
#include "ast.hpp"
#include "fn_utils.hpp"
#include "fn_colors.hpp"
#include "util.hpp"
#include "util_string.hpp"
#include "listize.hpp"

namespace Sass {

  namespace Functions {

    double scaleValue(double current, double scale, double max) {
      return current + (scale > 0.0 ? max - current : current) * scale;
    }

    bool isVar(const Value* obj) {
      const SassString* s = Cast<SassString>(obj);
      if (s == nullptr) return false;
      if (s->quote_mark() != 0) return false;
      const std::string& str = s->value();
      return starts_with(str, "var(");
    }

    bool isSpecialNumber(const Value* obj) {
      const SassString* s = Cast<SassString>(obj);
      if (s == nullptr) return false;
      if (s->quote_mark() != 0) return false;
      const std::string& str = s->value();
      if (str.length() < 6) return false;
      return starts_with(str, "calc(")
        || starts_with(str, "var(")
        || starts_with(str, "env(")
        || starts_with(str, "min(")
        || starts_with(str, "max(");
    }

    double _percentageOrUnitless2(Number* number, double max, std::string name, Backtraces traces) {
      double value = 0.0;
      if (!number->hasUnits()) {
        value = number->value();
      }
      else if (number->hasUnit("%")) {
        value = max * number->value() / 100;
      }
      else {
        error(name + ": Expected " + number->to_css() +
          " to have no units or \"%\".", number->pstate(), traces);
      }
      if (value < 0.0) return 0.0;
      if (value > max) return max;
      return value;
    }

    /// Returns whether [value] is an unquoted string that start with `var(` and
    /// contains `/`.
    bool _isVarSlash(Expression* value) {
      if (String_Constant * string = Cast<String_Constant>(value)) {
        return starts_with(string->value(), "var(") &&
          string_constains(string->value(), '/');
      }
      if (StringLiteral * string = Cast<StringLiteral>(value)) {
        return starts_with(string->text(), "var(") &&
          string_constains(string->text(), '/');
      }
      return false;
    }

    Expression* _parseChannels2(std::string name, std::vector<std::string> argumentNames, Value* channels, ParserState pstate, Backtraces traces)
    {
      if (isVar(channels)) {
        std::stringstream fncall;
        fncall << name << "("
          << channels->to_css() << ")";
        return SASS_MEMORY_NEW(StringLiteral,
          pstate, fncall.str());
      }

      SassList* list = Cast<SassList>(channels);

      if (!list) {
        list = SASS_MEMORY_NEW(SassList, pstate);
        list->append(channels);
      }

      bool isCommaSeparated = list->separator() == SASS_COMMA;
      bool isBracketed = list->hasBrackets();
      if (isCommaSeparated || isBracketed) {
        std::stringstream msg;
        msg << "$channels must be";
        if (isBracketed) msg << " an unbracketed";
        if (isCommaSeparated) {
          msg << (isBracketed ? "," : " a");
          msg << " space-separated";
        }
        msg << " list.";
        error(msg.str(), pstate, traces);
      }

      if (list->length() > 3) {
        std::stringstream msg;
        msg << "Only 3 elements allowed, but "
          << list->length() << " were passed.";
        error(msg.str(), pstate, traces);
      }
      else if (list->length() < 3) {
        bool hasVar = false;
        for (Value* item : list->elements()) {
          if (isVar(item)) {
            hasVar = true;
            break;
          }
        }
        if (hasVar || (!list->empty() && _isVarSlash(list->last()))) {
          std::stringstream fncall;
          fncall << name << "(";
          for (size_t i = 0, iL = list->length(); i < iL; i++) {
            if (i > 0) { fncall << " "; }
            fncall << list->get(i)->to_css();
          }
          fncall << ")";
          return SASS_MEMORY_NEW(StringLiteral,
            pstate, fncall.str());
        }
        else {
          std::string argument = argumentNames[list->length()];
          error(
            "Missing element " + argument + ".",
            pstate, traces);
        }
      }

      Number* secondNumber = Cast<Number>(list->get(2));
      String_Constant* secondString = Cast<String_Constant>(list->get(2));
      if (secondNumber && secondNumber->hasAsSlash()) {
        SassList* rv = SASS_MEMORY_NEW(SassList, pstate);
        rv->append(list->get(0));
        rv->append(list->get(1));
        rv->append(secondNumber->lhsAsSlash());
        rv->append(secondNumber->rhsAsSlash());
        return rv;
      }
      else if (secondString &&
        secondString->quote_mark() == 0 &&
        string_constains(secondString->value(), '/')) {
        std::stringstream fncall;
        fncall << name << "(";
        for (size_t i = 0, iL = list->length(); i < iL; i++) {
          if (i > 0) { fncall << " "; }
          fncall << list->get(i)->to_css();

        }
        fncall << ")";
        return SASS_MEMORY_NEW(StringLiteral,
          pstate, fncall.str());
      }
      else {
        return list;
      }

    }

    SassString* _functionString(std::string name, std::vector<ValueObj> arguments, const ParserState& pstate)
    {
      bool addComma = false;
      std::stringstream fncall;
      fncall << name << "(";
      for (Value* arg : arguments) {
        if (addComma) fncall << ", ";
        fncall << arg->to_css();
        addComma = true;
      }
      fncall << ")";
      return SASS_MEMORY_NEW(SassString,
        pstate, fncall.str());
    }

    SassString* _functionRgbString(std::string name, Color_RGBA* color, Value* alpha, const ParserState& pstate)
    {
      std::stringstream fncall;
      fncall << name << "(";
      fncall << color->r() << ", ";
      fncall << color->g() << ", ";
      fncall << color->b() << ", ";
      fncall << alpha->to_css() << ")";
      return SASS_MEMORY_NEW(SassString,
        pstate, fncall.str());
    }

    Value* _rgbTwoArg2(std::string name, std::vector<ValueObj> arguments, const ParserState& pstate)
    {
      // Check if any `calc()` or `var()` are passed
      if (isVar(arguments[0])) {
        return _functionString(
          name, arguments, pstate);
      }
      else if (isVar(arguments[1])) {
        if (Color* first = Cast<Color>(arguments[0])) {
          SassColorObj rgba = first->toRGBA();
          return _functionRgbString(name,
            rgba, arguments[1], pstate);
        }
        else {
          return _functionString(
            name, arguments, pstate);
        }
      }
      else if (isSpecialNumber(arguments[1])) {
        Color* color = arguments[0]->assertColor("color");
        SassColorObj rgba = color->toRGBA();
        return _functionRgbString(name,
          rgba, arguments[1], pstate);
      }

      SassColor* color = arguments[0]->assertColor("color");
      SassNumber* alpha = arguments[1]->assertNumber("alpha");
      color = SASS_MEMORY_COPY(color);
      color->a(_percentageOrUnitless2(
        alpha, 1.0, "alpha", {}));
      color->disp("");
      return color;
    }

    namespace Colors {

      BUILT_IN_FN(rgb_4)
      {
        return _rgb("rgb", arguments, rgb_4_sig, pstate, {});
      }

      BUILT_IN_FN(rgb_3)
      {
        return _rgb("rgb", arguments, rgb_3_sig, pstate, {});
      }

      BUILT_IN_FN(rgb_2)
      {
        return _rgbTwoArg2("rgb", arguments, pstate);
      }


      BUILT_IN_FN(rgb_1)
      {
        ExpressionObj parsed = _parseChannels2(
          "rgb", { "$red", "$green", "$blue" },
          arguments[0], pstate, {});
        if (StringLiteral * str = Cast<StringLiteral>(parsed)) {
          parsed.detach();
          return str;
        }
        if (List * list = Cast<List>(parsed)) {
          // return _rgb("rgb", list->elements(), rgb_1_sig, pstate, {});
        }
        if (SassList * list = Cast<SassList>(parsed)) {
          return _rgb("rgb", list->elements(), rgb_1_sig, pstate, {});
        }
        return SASS_MEMORY_NEW(SassString, pstate, arguments[0]->to_css());
      }


      BUILT_IN_FN(rgba_4)
      {
        return _rgb("rgba", arguments, rgba_4_sig, pstate, {});
      }


      BUILT_IN_FN(rgba_3)
      {
        return _rgb("rgba", arguments, rgba_3_sig, pstate, {});
      }


      BUILT_IN_FN(rgba_2)
      {
        return _rgbTwoArg2("rgba", arguments, pstate);
      }


      BUILT_IN_FN(rgba_1)
      {
        ExpressionObj parsed = _parseChannels2(
          "rgba", { "$red", "$green", "$blue" },
          arguments[0], pstate, {});
        if (StringLiteral * str = Cast<StringLiteral>(parsed)) {
          parsed.detach();
          return str;
        }
        if (List * list = Cast<List>(parsed)) {
          // return _rgb("rgba", list->elements(), rgba_1_sig, pstate, {});
        }
        if (SassList * list = Cast<SassList>(parsed)) {
          return _rgb("rgba", list->elements(), rgba_1_sig, pstate, {});
        }
        return SASS_MEMORY_NEW(SassString, pstate, arguments[0]->to_css());
      }


      BUILT_IN_FN(hsl_4)
      {
        return _hsl("hsl", arguments, hsl_4_sig, pstate, {});
      }


      BUILT_IN_FN(hsl_3)
      {
        return _hsl("hsl", arguments, hsl_3_sig, pstate, {});
      }


      BUILT_IN_FN(hsl_2)
      {
        // hsl(123, var(--foo)) is valid CSS because --foo might be `10%, 20%`
        // and functions are parsed after variable substitution.
        if (isVar(arguments[0]) || isVar(arguments[1])) {
          return _functionString("hsl", arguments, pstate);
        }
        throw Exception::SassScriptException(
          "Missing argument $lightness.");
      }


      BUILT_IN_FN(hsl_1)
      {
        ExpressionObj parsed = _parseChannels2(
          "hsl", { "$hue", "$saturation", "$lightness" },
          arguments[0], pstate, {});
        if (StringLiteral * str = Cast<StringLiteral>(parsed)) {
          parsed.detach();
          return str;
        }
        if (List * list = Cast<List>(parsed)) {
          // return _hsl("hsl", list->elements(), hsl_1_sig, pstate, {});
        }
        if (SassList * list = Cast<SassList>(parsed)) {
          return _hsl("hsl", list->elements(), hsl_1_sig, pstate, {});
        }
        return SASS_MEMORY_NEW(SassString, pstate, arguments[0]->to_css());
      }


      BUILT_IN_FN(hsla_4)
      {
        return _hsl("hsla", arguments, hsla_4_sig, pstate, {});
      }


      BUILT_IN_FN(hsla_3)
      {
        return _hsl("hsla", arguments, hsla_3_sig, pstate, {});
      }


      BUILT_IN_FN(hsla_2)
      {
        // hsl(123, var(--foo)) is valid CSS because --foo might be `10%, 20%`
        // and functions are parsed after variable substitution.
        if (isVar(arguments[0]) || isVar(arguments[1])) {
          return _functionString("hsla", arguments, pstate);
        }
        throw Exception::SassScriptException(
          "Missing argument $lightness.");
      }


      BUILT_IN_FN(hsla_1)
      {
        ExpressionObj parsed = _parseChannels2(
          "hsla", { "$hue", "$saturation", "$lightness" },
          arguments[0], pstate, {});
        if (StringLiteral * str = Cast<StringLiteral>(parsed)) {
          parsed.detach();
          return str;
        }
        if (List * list = Cast<List>(parsed)) {
          // return _hsl("hsla", list->elements(), hsl_1_sig, pstate, {});
        }
        if (SassList * list = Cast<SassList>(parsed)) {
          return _hsl("hsla", list->elements(), hsl_1_sig, pstate, {});
        }
        return SASS_MEMORY_NEW(SassString, pstate, arguments[0]->to_css());
      }

      BUILT_IN_FN(red)
      {
        return SASS_MEMORY_NEW(SassNumber, pstate,
          arguments[0]->assertColor("color")->r());
      }

      BUILT_IN_FN(green)
      {
        return SASS_MEMORY_NEW(SassNumber, pstate,
          arguments[0]->assertColor("color")->g());
      }

      BUILT_IN_FN(blue)
      {
        return SASS_MEMORY_NEW(SassNumber, pstate,
          arguments[0]->assertColor("color")->b());
      }

      BUILT_IN_FN(mix)
      {
        Color* color1 = arguments[0]->assertColor("color1");
        Color* color2 = arguments[1]->assertColor("color2");
        Number* weight = arguments[2]->assertNumber("weight");
        return SASS_MEMORY_NEW(SassString, pstate, "mix");
      }

      BUILT_IN_FN(invert)
      {
        return SASS_MEMORY_NEW(SassString, pstate, "invert");
      }

      BUILT_IN_FN(hue)
      {
        return SASS_MEMORY_NEW(SassNumber, pstate,
          arguments[0]->assertColorHsla("color")->h(), "deg");
      }

      BUILT_IN_FN(saturation)
      {
        return SASS_MEMORY_NEW(SassNumber, pstate,
          arguments[0]->assertColorHsla("color")->s(), "%");
      }


      BUILT_IN_FN(lightness)
      {
        return SASS_MEMORY_NEW(SassNumber, pstate,
          arguments[0]->assertColorHsla("color")->l(), "%");
      }

      BUILT_IN_FN(adjustHue)
      {
        SassColor* color = arguments[0]->assertColor("color");
        SassNumber* degrees = arguments[1]->assertNumber("degrees");
        Color_HSLA_Obj copy = color->copyAsHSLA();
        copy->h(copy->h() + degrees->value());
        return copy.detach();
      }

      BUILT_IN_FN(complement)
      {
        return SASS_MEMORY_NEW(SassString, pstate, "complement");
      }

      BUILT_IN_FN(grayscale)
      {
        // Gracefully handle if number is passed
        if (Cast<SassNumber>(arguments[0])) {
          return _functionString("grayscale",
            arguments, pstate);
        }
        SassColor* color = arguments[0]->assertColor("color");
        Color_HSLA_Obj copy = color->copyAsHSLA();
        copy->s(0.0); // remove saturation
        return copy.detach();
      }

      BUILT_IN_FN(lighten)
      {
        SassColor* color = arguments[0]->assertColor("color");
        SassNumber* amount = arguments[1]->assertNumber("amount");
        double nr = amount->valueInRange(0.0, 100.0, epsilon, "amount");
        Color_HSLA_Obj copy = color->copyAsHSLA();
        copy->l(clamp(copy->l() + nr, 0.0, 100.0));
        return copy.detach();
      }

      BUILT_IN_FN(darken)
      {
        SassColor* color = arguments[0]->assertColor("color");
        SassNumber* amount = arguments[1]->assertNumber("amount");
        double nr = amount->valueInRange(0.0, 100.0, epsilon, "amount");
        Color_HSLA_Obj copy = color->copyAsHSLA();
        copy->l(clamp(copy->l() - nr, 0.0, 100.0));
        return copy.detach();
      }

      BUILT_IN_FN(saturate_2)
      {
        SassColor* color = arguments[0]->assertColor("color");
        SassNumber* amount = arguments[1]->assertNumber("amount");
        double nr = amount->valueInRange(0.0, 100.0, epsilon, "amount");
        Color_HSLA_Obj copy = color->copyAsHSLA();
        copy->s(clamp(copy->s() - nr, 0.0, 100.0));
        return copy.detach();
      }

      BUILT_IN_FN(saturate_1)
      {
        SassNumber* number = arguments[0]->assertNumber("amount");
        return SASS_MEMORY_NEW(SassString, pstate,
          "saturate(" + number->to_css() + ")");
      }

      BUILT_IN_FN(desaturate)
      {
        SassColor* color = arguments[0]->assertColor("color");
        SassNumber* amount = arguments[1]->assertNumber("amount");
        double nr = amount->valueInRange(0.0, 100.0, epsilon, "amount");
        Color_HSLA_Obj copy = color->copyAsHSLA();
        copy->s(clamp(copy->s() - nr, 0.0, 100.0));
        return copy.detach();
      }

      BUILT_IN_FN(opacify)
      {
        SassColor* color = arguments[0]->assertColor("color");
        SassNumber* amount = arguments[1]->assertNumber("amount");
        double nr = amount->valueInRange(0.0, 100.0, epsilon, "amount");
        Color_HSLA_Obj copy = color->copyAsHSLA();
        copy->a(clamp(copy->a() + nr, 0.0, 1.0));
        return copy.detach();
      }

      BUILT_IN_FN(transparentize)
      {
        SassColor* color = arguments[0]->assertColor("color");
        SassNumber* amount = arguments[1]->assertNumber("amount");
        double nr = amount->valueInRange(0.0, 100.0, epsilon, "amount");
        Color_HSLA_Obj copy = color->copyAsHSLA();
        copy->a(clamp(copy->a() - nr, 0.0, 1.0));
        return copy.detach();
      }


      BUILT_IN_FN(alpha_1)
      {
        Value* argument = arguments[0];
        /*
        if (argument is SassString &&
          !argument.hasQuotes &&
          argument.text.contains(_microsoftFilterStart)) {
          // Suport the proprietary Microsoft alpha() function.
          return _functionString("alpha", arguments);
        }
        */
        SassColor* color = argument->assertColor("color");
        return SASS_MEMORY_NEW(SassNumber, pstate, color->a());
      }


      BUILT_IN_FN(alpha_any)
      {
        std::vector<ValueObj> argList
          = arguments[0]->asVector();
        /*
        if (argList.isNotEmpty &&
          argList.every((argument) = >
            argument is SassString &&
            !argument.hasQuotes &&
            argument.text.contains(_microsoftFilterStart))) {
          // Suport the proprietary Microsoft alpha() function.
          return _functionString("alpha", arguments);
        }
        */

        return SASS_MEMORY_NEW(SassString, pstate, "alpha_any");
      }


      BUILT_IN_FN(opacity)
      {
        // Gracefully handle if number is passed
        if (Cast<SassNumber>(arguments[0])) {
          return _functionString("opacity",
            arguments, pstate);
        }
        SassColor* color = arguments[0]->assertColor("color");
        return SASS_MEMORY_NEW(SassNumber, pstate, color->a());
      }


      BUILT_IN_FN(ieHexStr)
      {
        SassColor* color = arguments[0]->assertColor("color");
        // clip should not be needed here
        double r = clip(color->r(), 0.0, 255.0);
        double g = clip(color->g(), 0.0, 255.0);
        double b = clip(color->b(), 0.0, 255.0);
        double a = clip(color->a(), 0.0, 1.0) * 255.0;
        std::stringstream ss;
        ss << '#' << std::setw(2) << std::setfill('0') << std::uppercase;
        ss << std::hex << std::setw(2) << fuzzyRound(a, epsilon);
        ss << std::hex << std::setw(2) << fuzzyRound(r, epsilon);
        ss << std::hex << std::setw(2) << fuzzyRound(g, epsilon);
        ss << std::hex << std::setw(2) << fuzzyRound(b, epsilon);
        return SASS_MEMORY_NEW(SassString, pstate, ss.str());
      }

      Number* getKwdArg(NormalizedMap<ValueObj>& keywords, std::string name)
      {
        auto kv = keywords.find(name);
        // Return null since args are optional
        if (kv == keywords.end()) return nullptr;
        // Get the number object from found keyword
        SassNumber* num = kv->second->assertNumber(name);
        // Only consume keyword once
        keywords.erase(name);
        // Return the number
        return num;
      }

      Number* getKwdArgInRang(NormalizedMap<ValueObj>& keywords,
        std::string name, double min, double max)
      {
        auto kv = keywords.find(name);
        // Return null since args are optional
        if (kv == keywords.end()) return nullptr;
        // Get the number object from found keyword
        SassNumber* rv = kv->second->assertNumber(name);
        // Only consume keyword once
        keywords.erase(name);
        // Return the number
        return rv;
      }

      BUILT_IN_FN(adjust)
      {
        SassColor* color = arguments[0]->assertColor("color");
        SassArgumentList* argumentList = arguments[1]
          ->assertArgumentList("kwargs");
        std::vector<ValueObj> positional = argumentList->asVector();
        if (!argumentList->asVector().empty()) {
          throw Exception::SassRuntimeException(
            "Only one positional argument is allowed. All "
            "other arguments must be passed by name.", pstate);
        }

        NormalizedMap<ValueObj> keywords = argumentList->keywords();

        SassNumber* nr_r = getKwdArg(keywords, "$red");
        SassNumber* nr_g = getKwdArg(keywords, "$green");
        SassNumber* nr_b = getKwdArg(keywords, "$blue");
        SassNumber* nr_h = getKwdArg(keywords, "$hue");
        SassNumber* nr_s = getKwdArg(keywords, "$saturation");
        SassNumber* nr_l = getKwdArg(keywords, "$lightness");
        SassNumber* nr_a = getKwdArg(keywords, "$alpha");

        double r = nr_r ? nr_r->valueInRange(-255.0, 255.0, epsilon, "red") : 0.0;
        double g = nr_g ? nr_g->valueInRange(-255.0, 255.0, epsilon, "green") : 0.0;
        double b = nr_b ? nr_b->valueInRange(-255.0, 255.0, epsilon, "blue") : 0.0;
        double s = nr_s ? nr_s->valueInRange(-100.0, 100.0, epsilon, "saturation") : 0.0;
        double l = nr_l ? nr_l->valueInRange(-100.0, 100.0, epsilon, "lightness") : 0.0;
        double a = nr_a ? nr_a->valueInRange(-1.0, 1.0, epsilon, "alpha") : 0.0;
        double h = nr_h ? nr_h->value() : 0.0; // Hue is a very special case

        if (!keywords.empty()) {
          std::vector<std::string> keys;
          for (auto kv : keywords) {
            keys.push_back(kv.first);
          }
          std::stringstream msg;
          size_t iL = keys.size();
          msg << "No argument" <<
            (iL > 1 ? "s" : "") <<
            " named ";
          for (size_t i = 0; i < iL; i++) {
            if (i > 0) {
              msg << (i == iL - 1 ?
                " and " : ", ");
            }
            msg << keys[i];
          }
          msg << ".";
          throw Exception::SassRuntimeException(msg.str(), pstate);
        }

        bool hasRgb = r || g || b;
        bool hasHsl = h || s || l;

        if (hasRgb && hasHsl) {
          throw Exception::SassRuntimeException(
            "RGB parameters may not be passed along with HSL parameters.",
            pstate);
        }
        else if (hasRgb) {
          Color_RGBA_Obj c = color->copyAsRGBA();
          if (nr_r) c->r(clamp(c->r() + r, 0.0, 255.0));
          if (nr_g) c->g(clamp(c->g() + g, 0.0, 255.0));
          if (nr_b) c->b(clamp(c->b() + b, 0.0, 255.0));
          if (nr_a) c->a(clamp(c->a() + a, 0.0, 1.0));
          return c.detach();
        }
        else if (hasHsl) {
          Color_HSLA_Obj c = color->copyAsHSLA();
          if (nr_h) c->h(absmod(c->h() + h, 360.0));
          if (nr_s) c->s(clamp(c->s() + s, 0.0, 100.0));
          if (nr_l) c->l(clamp(c->l() + l, 0.0, 100.0));
          if (nr_a) c->a(clamp(c->a() + a, 0.0, 1.0));
          return c.detach();
        }
        else if (a) {
          Color_Obj c = SASS_MEMORY_COPY(color);
          if (nr_a) c->a(clamp(c->a() + a, 0.0, 1.0));
          return c.detach();
        }
        return color;
      }

      BUILT_IN_FN(change)
      {
        SassColor* color = arguments[0]->assertColor("color");
        SassArgumentList* argumentList = arguments[1]
          ->assertArgumentList("kwargs");
        std::vector<ValueObj> positional = argumentList->asVector();
        if (!argumentList->asVector().empty()) {
          throw Exception::SassRuntimeException(
            "Only one positional argument is allowed. All "
            "other arguments must be passed by name.", pstate);
        }

        NormalizedMap<ValueObj> keywords = argumentList->keywords();

        SassNumber* nr_r = getKwdArg(keywords, "$red");
        SassNumber* nr_g = getKwdArg(keywords, "$green");
        SassNumber* nr_b = getKwdArg(keywords, "$blue");
        SassNumber* nr_h = getKwdArg(keywords, "$hue");
        SassNumber* nr_s = getKwdArg(keywords, "$saturation");
        SassNumber* nr_l = getKwdArg(keywords, "$lightness");
        SassNumber* nr_a = getKwdArg(keywords, "$alpha");

        double r = nr_r ? nr_r->valueInRange(0.0, 255.0, epsilon, "red") : 0.0;
        double g = nr_g ? nr_g->valueInRange(0.0, 255.0, epsilon, "green") : 0.0;
        double b = nr_b ? nr_b->valueInRange(0.0, 255.0, epsilon, "blue") : 0.0;
        double s = nr_s ? nr_s->valueInRange(0.0, 100.0, epsilon, "saturation") : 0.0;
        double l = nr_l ? nr_l->valueInRange(0.0, 100.0, epsilon, "lightness") : 0.0;
        double a = nr_a ? nr_a->valueInRange(0.0, 1.0, epsilon, "alpha") : 0.0;
        double h = nr_h ? nr_h->value() : 0.0; // Hue is a very special case

        if (!keywords.empty()) {
          std::vector<std::string> keys;
          for (auto kv : keywords) {
            keys.push_back(kv.first);
          }
          std::stringstream msg;
          size_t iL = keys.size();
          msg << "No argument" <<
            (iL > 1 ? "s" : "") <<
            " named ";
          for (size_t i = 0; i < iL; i++) {
            if (i > 0) {
              msg << (i == iL - 1 ?
                " and " : ", ");
            }
            msg << keys[i];
          }
          msg << ".";
          throw Exception::SassRuntimeException(msg.str(), pstate);
        }

        bool hasRgb = r || g || b;
        bool hasHsl = h || s || l;

        if (hasRgb && hasHsl) {
          throw Exception::SassRuntimeException(
            "RGB parameters may not be passed along with HSL parameters.",
            pstate);
        }
        else if (hasRgb) {
          Color_RGBA_Obj c = color->copyAsRGBA();
          if (nr_r) c->r(clamp(r, 0.0, 255.0));
          if (nr_g) c->g(clamp(g, 0.0, 255.0));
          if (nr_b) c->b(clamp(b, 0.0, 255.0));
          if (nr_a) c->a(clamp(a, 0.0, 1.0));
          return c.detach();
        }
        else if (hasHsl) {
          Color_HSLA_Obj c = color->copyAsHSLA();
          if (nr_h) c->h(absmod(h, 360.0));
          if (nr_s) c->s(clamp(s, 0.0, 100.0));
          if (nr_l) c->l(clamp(l, 0.0, 100.0));
          if (nr_a) c->a(clamp(a, 0.0, 1.0));
          return c.detach();
        }
        else if (a) {
          Color_Obj c = SASS_MEMORY_COPY(color);
          if (nr_a) c->a(clamp(a, 0.0, 1.0));
          return c.detach();
        }
        return color;
      }

      BUILT_IN_FN(scale)
      {
        SassColor* color = arguments[0]->assertColor("color");
        SassArgumentList* argumentList = arguments[1]
          ->assertArgumentList("kwargs");
        std::vector<ValueObj> positional = argumentList->asVector();
        if (!argumentList->asVector().empty()) {
          throw Exception::SassRuntimeException(
            "Only one positional argument is allowed. All "
            "other arguments must be passed by name.", pstate);
        }

        NormalizedMap<ValueObj> keywords = argumentList->keywords();

        SassNumber* nr_r = getKwdArg(keywords, "$red");
        SassNumber* nr_g = getKwdArg(keywords, "$green");
        SassNumber* nr_b = getKwdArg(keywords, "$blue");
        SassNumber* nr_h = getKwdArg(keywords, "$hue");
        SassNumber* nr_s = getKwdArg(keywords, "$saturation");
        SassNumber* nr_l = getKwdArg(keywords, "$lightness");
        SassNumber* nr_a = getKwdArg(keywords, "$alpha");

        double r = nr_r ? nr_r->assertUnit("%", "red")->valueInRange(-100.0, 100.0, epsilon, "red") / 100.0 : 0.0;
        double g = nr_g ? nr_g->assertUnit("%", "green")->valueInRange(-100.0, 100.0, epsilon, "green") / 100.0 : 0.0;
        double b = nr_b ? nr_b->assertUnit("%", "blue")->valueInRange(-100.0, 100.0, epsilon, "blue") / 100.0 : 0.0;
        double h = nr_h ? nr_h->assertUnit("%", "hue")->valueInRange(-100.0, 100.0, epsilon, "hue") / 100.0 : 0.0;
        double s = nr_s ? nr_s->assertUnit("%", "saturation")->valueInRange(-100.0, 100.0, epsilon, "saturation") / 100.0 : 0.0;
        double l = nr_l ? nr_l->assertUnit("%", "lightness")->valueInRange(-100.0, 100.0, epsilon, "lightness") / 100.0 : 0.0;
        double a = nr_a ? nr_a->assertUnit("%", "alpha")->valueInRange(-100.0, 100.0, epsilon, "alpha") / 100.0 : 0.0;

        if (!keywords.empty()) {
          std::vector<std::string> keys;
          for (auto kv : keywords) {
            keys.push_back(kv.first);
          }
          std::stringstream msg;
          size_t iL = keys.size();
          msg << "No argument" <<
            (iL > 1 ? "s" : "") <<
            " named ";
          for (size_t i = 0; i < iL; i++) {
            if (i > 0) {
              msg << (i == iL - 1 ?
                " and " : ", ");
            }
            msg << keys[i];
          }
          msg << ".";
          throw Exception::SassRuntimeException(msg.str(), pstate);
        }

        bool hasRgb = r || g || b;
        bool hasHsl = h || s || l;

        if (hasRgb && hasHsl) {
          throw Exception::SassRuntimeException(
            "RGB parameters may not be passed along with HSL parameters.",
            pstate);
        }
        else if (hasRgb) {
          Color_RGBA_Obj c = color->copyAsRGBA();
          if (nr_r) c->r(scaleValue(c->r(), r, 255.0));
          if (nr_g) c->g(scaleValue(c->g(), g, 255.0));
          if (nr_b) c->b(scaleValue(c->b(), b, 255.0));
          if (nr_a) c->a(scaleValue(c->a(), a, 1.0));
          return c.detach();
        }
        else if (hasHsl) {
          Color_HSLA_Obj c = color->copyAsHSLA();
          if (nr_s) c->s(scaleValue(c->s(), s, 100.0));
          if (nr_l) c->l(scaleValue(c->l(), l, 100.0));
          if (nr_a) c->a(scaleValue(c->a(), a, 1.0));
          return c.detach();
        }
        else if (a) {
          Color_Obj c = SASS_MEMORY_COPY(color);
          if (nr_a) c->a(scaleValue(c->a(), a, 1.0));
          return c.detach();
        }
        return color;
      }

    }


    std::vector<ExpressionObj> getArguments(Env& env, std::vector<std::string> names)
    {
      std::vector<ExpressionObj> arguments;
      for (const std::string& name : names) {
        arguments.push_back(env.get_local(name));
      }
      return arguments;
    }

    bool isVar(const AST_Node* obj) {
      const String_Constant* s = Cast<String_Constant>(obj);
      if (s == nullptr) return false;
      if (s->quote_mark() != 0) return false;
      const std::string& str = s->value();
      return starts_with(str, "var(");
    }

    bool isSpecialNumber(const AST_Node* obj) {
      const String_Constant* s = Cast<String_Constant>(obj);
      if (s == nullptr) return false;
      if (s->quote_mark() != 0) return false;
      const std::string& str = s->value();
      if (str.length() < 6) return false;
      return starts_with(str, "calc(")
        || starts_with(str, "var(")
        || starts_with(str, "env(")
        || starts_with(str, "min(")
        || starts_with(str, "max(");
    }

    void checkPositionalArgs(List* args, Backtraces traces)
    {
      for (Expression* ex : args->elements()) {
        if (Argument * arg = Cast<Argument>(ex)) {
          if (arg->name().empty()) {
            error("Only one positional argument is allowed. "
              "All other arguments must be passed by name.",
              arg->pstate(), traces);
          }
        }
      }
    }

    /// Asserts that [number] is a percentage or has no units, and normalizes the
    /// value.
    ///
    /// If [number] has no units, its value is clamped to be greater than `0` or
    /// less than [max] and returned. If [number] is a percentage, it's scaled to be
    /// within `0` and [max]. Otherwise, this throws a [SassScriptException].
    ///
    /// [name] is used to identify the argument in the error message.
    double _percentageOrUnitless(Number* number, double max, std::string name, Backtraces traces) {
      double value = 0.0;
      if (!number->hasUnits()) {
        value = number->value();
      }
      else if (number->hasUnit("%")) {
        value = max * number->value() / 100;
      }
      else {
        error(name + ": Expected " + number->to_css() +
          " to have no units or \"%\".", number->pstate(), traces);
      }
      if (value < 0.0) return 0.0;
      if (value > max) return max;
      return value;
    }

    void valueInRange(double val, double lo, double hi, const std::string& name, ParserState pstate, Backtraces traces)
    {
      // Error: Expected -100.1% to be within 0% and 255%.
      if (val < lo || val > hi) {
        std::stringstream msg;
        if (!name.empty()) msg << name << ": ";
        msg << "Expected " << val;
        msg << " to be within " << lo;
        msg << " and " << hi << ".";
        error(msg.str(), pstate, traces);
      }
    }

    Number* getColorScale(NormalizedMap<ExpressionObj>& keywords, const std::string& name, ParserState pstate, Backtraces traces)
    {

      auto pos = keywords.find(name);
      if (pos == keywords.end()) return nullptr;
      Expression* ex = pos->second;
      if (Number * nr = Cast<Number>(ex)) {
        if (!nr->hasUnit("%")) {
          error(name + ": Expected " + ex->to_string() +
            " to have unit \"%\".", ex->pstate(), traces);
        }
        if (nr->value() < -100.0 || nr->value() > 100.0) {
          std::stringstream msg;
          if (!name.empty()) msg << name << ": ";
          msg << "Expected " << nr->to_string();
          msg << " to be within -100% and 100%.";
          error(msg.str(), nr->pstate(), traces);
        }
        keywords.erase(name);
        return nr;
      }
      else {
        error(name + ": " + ex->to_string() +
          " is not a number.", ex->pstate(), traces);
      }
      return nullptr;
    }


    Number* getColorArg(NormalizedMap<ExpressionObj>& keywords, const std::string& name, ParserState pstate, Backtraces traces)
    {

      auto pos = keywords.find(name);
      if (pos == keywords.end()) return nullptr;
      Expression* ex = pos->second;
      if (Number * nr = Cast<Number>(ex)) {
        keywords.erase(name);
        return nr;
      }
      else {
        error(name + ": " + ex->to_string() +
          " is not a number.", ex->pstate(), traces);
      }
      return nullptr;
    }

    Number* getColorArgInRange(NormalizedMap<ExpressionObj>& keywords, const std::string& name, double lo, double hi, ParserState pstate, Backtraces traces)
    {

      auto pos = keywords.find(name);
      if (pos == keywords.end()) return nullptr;
      Expression* ex = pos->second;
      if (Number * nr = Cast<Number>(ex)) {
        if (!nr->hasUnit("%")) {
          // error(name + ": Expected " + ex->to_string() +
          //   " to have unit \"%\".", ex->pstate(), traces);
        }
        valueInRange(nr->value(), lo, hi, name, nr->pstate(), traces);
        keywords.erase(name);
        return nr;
      }
      else {
        error(name + ": " + ex->to_string() +
          " is not a number.", ex->pstate(), traces);
      }
      return nullptr;
    }

    double fuzzyRound2(double value) {
      return value;
    }


    /// Returns whether [value] is an unquoted string that start with `var(` and
    /// contains `/`.
    bool _isVarSlash2(Expression* value) {
      if (String_Constant * string = Cast<String_Constant>(value)) {
        return starts_with(string->value(), "var(") &&
          string_constains(string->value(), '/');
      }
      if (StringLiteral * string = Cast<StringLiteral>(value)) {
        return starts_with(string->text(), "var(") &&
          string_constains(string->text(), '/');
      }
      return false;
    }


    Expression* _parseChannels(std::string name, std::vector<std::string> argumentNames, Value* channels, ParserState pstate, Backtraces traces)
    {
      if (isVar(channels)) {
        std::stringstream fncall;
        fncall << name << "("
          << channels->to_css() << ")";
        return SASS_MEMORY_NEW(StringLiteral,
          pstate, fncall.str());
      }

      SassList* list = Cast<SassList>(channels);

      if (!list) {
        list = SASS_MEMORY_NEW(SassList, pstate);
        list->append(channels);
      }

      bool isCommaSeparated = list->separator() == SASS_COMMA;
      bool isBracketed = list->hasBrackets();
      if (isCommaSeparated || isBracketed) {
        std::stringstream msg;
        msg << "$channels must be";
        if (isBracketed) msg << " an unbracketed";
        if (isCommaSeparated) {
          msg << (isBracketed ? "," : " a");
          msg << " space-separated";
        }
        msg << " list.";
        error(msg.str(), pstate, traces);
      }

      if (list->length() > 3) {
        std::stringstream msg;
        msg << "Only 3 elements allowed, but "
          << list->length() << " were passed.";
        error(msg.str(), pstate, traces);
      }
      else if (list->length() < 3) {
        bool hasVar = false;
        for (Expression* item : list->elements()) {
          if (isVar(item)) {
            hasVar = true;
            break;
          }
        }
        if (hasVar || (!list->empty() && _isVarSlash2(list->last()))) {
          std::stringstream fncall;
          fncall << name << "(";
          for (size_t i = 0, iL = list->length(); i < iL; i++) {
            if (i > 0) { fncall << " "; }
            fncall << list->get(i)->to_css();
          }
          fncall << ")";
          return SASS_MEMORY_NEW(StringLiteral,
            pstate, fncall.str());
        }
        else {
          std::string argument = argumentNames[list->length()];
          error(
            "Missing element " + argument + ".",
            pstate, traces);
        }
      }

      Number* secondNumber = Cast<Number>(list->get(2));
      String_Constant* secondString = Cast<String_Constant>(list->get(2));
      if (secondNumber && secondNumber->hasAsSlash()) {
        SassList* rv = SASS_MEMORY_NEW(SassList, pstate);
        rv->append(list->get(0));
        rv->append(list->get(1));
        rv->append(secondNumber->lhsAsSlash());
        rv->append(secondNumber->rhsAsSlash());
        return rv;
      }
      else if (secondString &&
        secondString->quote_mark() == 0 &&
        string_constains(secondString->value(), '/')) {
        std::stringstream fncall;
        fncall << name << "(";
        for (size_t i = 0, iL = list->length(); i < iL; i++) {
          if (i > 0) { fncall << " "; }
          fncall << list->get(i)->to_css();

        }
        fncall << ")";
        return SASS_MEMORY_NEW(StringLiteral,
          pstate, fncall.str());
      }
      else {
        return list;
      }

    }

    AST_Node* getArg(std::string name, Env& env)
    {
      if (env.has_local(name)) {
        return env.get_local(name);
      }
      return nullptr;
    }

    Color* assertColor(AST_Node* node, std::string name, ParserState pstate, Backtraces traces)
    {
      if (node == nullptr) return nullptr;
      Color* col = Cast<Color>(node);
      if (col == nullptr) {
        error(name + ": " + node->to_string()
          + " is not a color.", pstate, traces);
      }
      return col;
    }

    Number* assertNumber(AST_Node* node, std::string name, ParserState pstate, Backtraces traces)
    {
      if (node == nullptr) return nullptr;
      Number* nr = Cast<Number>(node);
      if (nr == nullptr) {
        error(name + ": " + node->to_string()
          + " is not a number.", pstate, traces);
      }
      return nr;
    }

    Value* _rgb(std::string name, std::vector<ExpressionObj> arguments, Signature sig, ParserState pstate, Backtraces traces)
    {
      AST_Node* _r = arguments[0];
      AST_Node* _g = arguments[1];
      AST_Node* _b = arguments[2];
      AST_Node* _a = nullptr;
      if (arguments.size() > 3) {
        _a = arguments[3];
      }
      // Check if any `calc()` or `var()` are passed
      if (isSpecialNumber(_r) || isSpecialNumber(_g) || isSpecialNumber(_b) || isSpecialNumber(_a)) {
        std::stringstream fncall;
        fncall << name << "(";
        fncall << _r->to_css() << ", ";
        fncall << _g->to_css() << ", ";
        fncall << _b->to_css();
        if (_a) { fncall << ", " << _a->to_css(); }
        fncall << ")";
        return SASS_MEMORY_NEW(StringLiteral, pstate, fncall.str());
      }

      Number* r = assertNumber(_r, "$red", pstate, traces);
      Number* g = assertNumber(_g, "$green", pstate, traces);
      Number* b = assertNumber(_b, "$blue", pstate, traces);
      Number* a = _a ? assertNumber(_a, "$alpha", pstate, traces) : nullptr;

      return SASS_MEMORY_NEW(Color_RGBA, pstate,
        fuzzyRound2(_percentageOrUnitless(r, 255, "$red", traces)),
        fuzzyRound2(_percentageOrUnitless(g, 255, "$green", traces)),
        fuzzyRound2(_percentageOrUnitless(b, 255, "$blue", traces)),
        a ? _percentageOrUnitless(a, 1.0, "$alpha", traces) : 1.0);

    }

    Value* _rgb(std::string name, std::vector<ValueObj> arguments, Signature sig, ParserState pstate, Backtraces traces)
    {
      AST_Node* _r = arguments[0];
      AST_Node* _g = arguments[1];
      AST_Node* _b = arguments[2];
      AST_Node* _a = nullptr;
      if (arguments.size() > 3) {
        _a = arguments[3];
      }
      // Check if any `calc()` or `var()` are passed
      if (isSpecialNumber(_r) || isSpecialNumber(_g) || isSpecialNumber(_b) || isSpecialNumber(_a)) {
        std::stringstream fncall;
        fncall << name << "(";
        fncall << _r->to_css() << ", ";
        fncall << _g->to_css() << ", ";
        fncall << _b->to_css();
        if (_a) { fncall << ", " << _a->to_css(); }
        fncall << ")";
        return SASS_MEMORY_NEW(StringLiteral, pstate, fncall.str());
      }

      Number* r = assertNumber(_r, "$red", pstate, traces);
      Number* g = assertNumber(_g, "$green", pstate, traces);
      Number* b = assertNumber(_b, "$blue", pstate, traces);
      Number* a = _a ? assertNumber(_a, "$alpha", pstate, traces) : nullptr;

      return SASS_MEMORY_NEW(Color_RGBA, pstate,
        fuzzyRound2(_percentageOrUnitless(r, 255, "$red", traces)),
        fuzzyRound2(_percentageOrUnitless(g, 255, "$green", traces)),
        fuzzyRound2(_percentageOrUnitless(b, 255, "$blue", traces)),
        a ? _percentageOrUnitless(a, 1.0, "$alpha", traces) : 1.0);

    }

    Value* _hsl(std::string name, std::vector<ExpressionObj> arguments, Signature sig, ParserState pstate, Backtraces traces)
    {
      AST_Node* _h = arguments[0];
      AST_Node* _s = arguments[1];
      AST_Node* _l = arguments[2];
      AST_Node* _a = nullptr;
      if (arguments.size() > 3) {
        _a = arguments[3];
      }
      // Check if any `calc()` or `var()` are passed
      if (isSpecialNumber(_h) || isSpecialNumber(_s) || isSpecialNumber(_l) || isSpecialNumber(_a)) {
        std::stringstream fncall;
        fncall << name << "(";
        fncall << _h->to_css() << ", ";
        fncall << _s->to_css() << ", ";
        fncall << _l->to_css();
        if (_a) { fncall << ", " << _a->to_css(); }
        fncall << ")";
        return SASS_MEMORY_NEW(StringLiteral, pstate, fncall.str());
      }

      Number* h = assertNumber(_h, "$hue", pstate, traces);
      Number* s = assertNumber(_s, "$saturation", pstate, traces);
      Number* l = assertNumber(_l, "$lightness", pstate, traces);
      Number* a = _a ? assertNumber(_a, "$alpha", pstate, traces) : nullptr;

      return SASS_MEMORY_NEW(Color_HSLA, pstate,
        h->value(),
        clamp(s->value(), 0.0, 100.0),
        clamp(l->value(), 0.0, 100.0),
        a ? _percentageOrUnitless(a, 1.0, "$alpha", traces) : 1.0);

    }

    Value* _hsl(std::string name, std::vector<ValueObj> arguments, Signature sig, ParserState pstate, Backtraces traces)
    {
      Value* _h = arguments[0];
      Value* _s = arguments[1];
      Value* _l = arguments[2];
      Value* _a = nullptr;
      if (arguments.size() > 3) {
        _a = arguments[3];
      }
      // Check if any `calc()` or `var()` are passed
      if (isSpecialNumber(_h) || isSpecialNumber(_s) || isSpecialNumber(_l) || isSpecialNumber(_a)) {
        std::stringstream fncall;
        fncall << name << "(";
        fncall << _h->to_css() << ", ";
        fncall << _s->to_css() << ", ";
        fncall << _l->to_css();
        if (_a) { fncall << ", " << _a->to_css(); }
        fncall << ")";
        return SASS_MEMORY_NEW(StringLiteral, pstate, fncall.str());
      }

      Number* h = assertNumber(_h, "$hue", pstate, traces);
      Number* s = assertNumber(_s, "$saturation", pstate, traces);
      Number* l = assertNumber(_l, "$lightness", pstate, traces);
      Number* a = _a ? assertNumber(_a, "$alpha", pstate, traces) : nullptr;

      return SASS_MEMORY_NEW(Color_HSLA, pstate,
        h->value(),
        clamp(s->value(), 0.0, 100.0),
        clamp(l->value(), 0.0, 100.0),
        a ? _percentageOrUnitless(a, 1.0, "$alpha", traces) : 1.0);

    }

    Value* _rgbTwoArg(std::string name, Env& env, Signature sig, ParserState pstate, Backtraces traces)
    {
      AST_Node* _c = getArg("$color", env);
      AST_Node* _a = getArg("$alpha", env);
      // Check if any `calc()` or `var()` are passed
      if (isVar(_c)) {
        std::stringstream fncall;
        fncall << name << "("
          << _c->to_css() << ", "
          << _a->to_css() << ")";
        return SASS_MEMORY_NEW(StringLiteral,
          pstate, fncall.str());
      }
      else if (isVar(_a)) {
        if (Color* c = Cast<Color>(_c)) {
          Color_RGBAObj col = c->toRGBA();
          std::stringstream fncall;
          fncall << name << "("
            << col->r() << ", "
            << col->g() << ", "
            << col->b() << ", "
            << _a->to_css() << ")";
          return SASS_MEMORY_NEW(StringLiteral,
            pstate, fncall.str());
        }
        else {
          std::stringstream fncall;
          fncall << name << "("
            << _c->to_css() << ", "
            << _a->to_css() << ")";
          return SASS_MEMORY_NEW(StringLiteral,
            pstate, fncall.str());
        }
      }
      else if (isSpecialNumber(_a)) {
        Color* c = ARGCOL("$color");
        Color_RGBAObj col = c->toRGBA();
        std::stringstream fncall;
        fncall << name << "("
          << col->r() << ", "
          << col->g() << ", "
          << col->b() << ", "
          << _a->to_css() << ")";
        return SASS_MEMORY_NEW(StringLiteral,
          pstate, fncall.str());
      }

      Color* c = ARGCOL("$color");
      Number* a = ARGNUM("$alpha");

      double alpha = _percentageOrUnitless(a, 1.0, "$alpha", traces);

      c = SASS_MEMORY_COPY(c);
      c->a(alpha);
      c->disp("");
      return c;
    }

    Signature rgba_4_sig = "rgba($red, $green, $blue, $alpha)";
    BUILT_IN(rgba_4)
    {
      std::vector<ExpressionObj> arguments =
        getArguments(env, { "$red", "$green", "$blue", "$alpha" });
      return _rgb("rgba", arguments, rgba_4_sig, pstate, traces);
    }

    Signature rgba_3_sig = "rgba($red, $green, $blue)";
    BUILT_IN(rgba_3)
    {
      std::vector<ExpressionObj> arguments =
        getArguments(env, { "$red", "$green", "$blue" });
      return _rgb("rgba", arguments, rgba_3_sig, pstate, traces);
    }

    Signature rgba_2_sig = "rgba($color, $alpha)";
    BUILT_IN(rgba_2)
    {
      return _rgbTwoArg("rgba", env, rgba_2_sig, pstate, traces);
    }

    Signature rgba_1_sig = "rgba($channels)";
    BUILT_IN(rgba_1)
    {
      Value* channels = ARG("$channels", Value, "a value");
      ExpressionObj parsed = _parseChannels("rgba",
        { "$red", "$green", "$blue" }, channels, pstate, traces);
      if (StringLiteral * str = Cast<StringLiteral>(parsed)) {
        parsed.detach();
        return str;
      }
      if (List * list = Cast<List>(parsed)) {
        return _rgb("rgba", list->elements(), rgba_1_sig, pstate, traces);
      }
      if (SassList * list = Cast<SassList>(parsed)) {
        return _rgb("rgba", list->elements(), rgba_1_sig, pstate, traces);
      }
      return nullptr;
    }

    Signature rgb_4_sig = "rgb($red, $green, $blue, $alpha)";
    BUILT_IN(rgb_4)
    {
      std::vector<ExpressionObj> arguments =
        getArguments(env, { "$red", "$green", "$blue", "$alpha" });
      return _rgb("rgb", arguments, rgb_4_sig, pstate, traces);
    }

    Signature rgb_3_sig = "rgb($red, $green, $blue)";
    BUILT_IN(rgb_3)
    {
      std::vector<ExpressionObj> arguments =
        getArguments(env, { "$red", "$green", "$blue" });
      return _rgb("rgb", arguments, rgb_3_sig, pstate, traces);
    }

    Signature rgb_2_sig = "rgb($color, $alpha)";
    BUILT_IN(rgb_2)
    {
      return _rgbTwoArg("rgb", env, rgb_2_sig, pstate, traces);
    }

    Signature rgb_1_sig = "rgb($channels)";
    BUILT_IN(rgb_1)
    {
      Value* channels = ARG("$channels", Value, "a value");
      ExpressionObj parsed = _parseChannels("rgb",
        { "$red", "$green", "$blue" }, channels, pstate, traces);
      if (StringLiteral * str = Cast<StringLiteral>(parsed)) {
        parsed.detach();
        return str;
      }
      if (List * list = Cast<List>(parsed)) {
        return _rgb("rgb", list->elements(), rgb_1_sig, pstate, traces);
      }
      if (SassList * list = Cast<SassList>(parsed)) {
        return _rgb("rgb", list->elements(), rgb_1_sig, pstate, traces);
      }
      return nullptr;
    }

    Signature hsl_4_sig = "hsl($hue, $saturation, $lightness, $alpha)";
    BUILT_IN(hsl_4)
    {
      std::vector<ExpressionObj> arguments =
        getArguments(env, { "$hue", "$saturation", "$lightness", "$alpha" });
      return _hsl("hsl", arguments, hsl_4_sig, pstate, traces);
    }

    Signature hsl_3_sig = "hsl($hue, $saturation, $lightness)";
    BUILT_IN(hsl_3)
    {
      std::vector<ExpressionObj> arguments =
        getArguments(env, { "$hue", "$saturation", "$lightness"});
      return _hsl("hsl", arguments, hsl_3_sig, pstate, traces);
    }

    Signature hsl_2_sig = "hsl($color, $alpha)";
    BUILT_IN(hsl_2)
    {
      // hsl(123, var(--foo)) is valid CSS because --foo might be `10%, 20%` and
      // functions are parsed after variable substitution.
      if (isVar(env["$color"]) || isVar(env["$alpha"])) {
        std::stringstream fncall;
        fncall << "hsl("
        << env["$color"]->to_css() << ", "
        << env["$alpha"]->to_css() << ")";
        return SASS_MEMORY_NEW(StringLiteral,
          pstate, fncall.str());
      }
      else {
        error("Missing argument $lightness.",
          pstate, traces);
      }
      return nullptr;
    }

    Signature hsl_1_sig = "hsl($channels)";
    BUILT_IN(hsl_1)
    {
      Value* channels = ARG("$channels", Value, "a value");
      ExpressionObj parsed = _parseChannels("hsl",
        { "$hue", "$saturation", "$lightness" }, channels, pstate, traces);
      if (StringLiteral * str = Cast<StringLiteral>(parsed)) {
        parsed.detach();
        return str;
      }
      if (List * list = Cast<List>(parsed)) {
        return _hsl("hsl", list->elements(), hsl_1_sig, pstate, traces);
      }
      if (SassList * list = Cast<SassList>(parsed)) {
        return _hsl("hsl", list->elements(), hsl_1_sig, pstate, traces);
      }
      return nullptr;
    }

    Signature hsla_4_sig = "hsla($hue, $saturation, $lightness, $alpha)";
    BUILT_IN(hsla_4)
    {
      std::vector<ExpressionObj> arguments =
        getArguments(env, { "$hue", "$saturation", "$lightness", "$alpha" });
      return _hsl("hsla", arguments, hsla_4_sig, pstate, traces);
    }

    Signature hsla_3_sig = "hsla($hue, $saturation, $lightness)";
    BUILT_IN(hsla_3)
    {
      std::vector<ExpressionObj> arguments =
        getArguments(env, { "$hue", "$saturation", "$lightness" });
      return _hsl("hsla", arguments, hsla_3_sig, pstate, traces);
    }

    Signature hsla_2_sig = "hsla($color, $alpha)";
    BUILT_IN(hsla_2)
    {
      // hsl(123, var(--foo)) is valid CSS because --foo might be `10%, 20%` and
      // functions are parsed after variable substitution.
      if (isVar(env["$color"]) || isVar(env["$alpha"])) {
        std::stringstream fncall;
        fncall << "hsla("
          << env["$color"]->to_css() << ", "
          << env["$alpha"]->to_css() << ")";
        return SASS_MEMORY_NEW(StringLiteral,
          pstate, fncall.str());
      }
      else {
        error("Missing argument $lightness.",
          pstate, traces);
      }
      return nullptr;
    }

    Signature hsla_1_sig = "hsla($channels)";
    BUILT_IN(hsla_1)
    {
      Value* channels = ARG("$channels", Value, "a value");
      ExpressionObj parsed = _parseChannels("hsla",
        { "$hue", "$saturation", "$lightness" }, channels, pstate, traces);
      if (StringLiteral * str = Cast<StringLiteral>(parsed)) {
        parsed.detach();
        return str;
      }
      if (List * list = Cast<List>(parsed)) {
        return _hsl("hsla", list->elements(), hsla_1_sig, pstate, traces);
      }
      if (SassList * list = Cast<SassList>(parsed)) {
        return _hsl("hsla", list->elements(), hsla_1_sig, pstate, traces);
      }
      return nullptr;
    }

    ////////////////
    // RGB FUNCTIONS
    ////////////////

    Signature red_sig = "red($color)";
    BUILT_IN(red)
    {
      Color_RGBA_Obj color = ARGCOL("$color")->toRGBA();
      return SASS_MEMORY_NEW(Number, pstate, color->r());
    }

    Signature green_sig = "green($color)";
    BUILT_IN(green)
    {
      Color_RGBA_Obj color = ARGCOL("$color")->toRGBA();
      return SASS_MEMORY_NEW(Number, pstate, color->g());
    }

    Signature blue_sig = "blue($color)";
    BUILT_IN(blue)
    {
      Color_RGBA_Obj color = ARGCOL("$color")->toRGBA();
      return SASS_MEMORY_NEW(Number, pstate, color->b());
    }

    Color_RGBA* colormix(Context& ctx, ParserState& pstate, Color* color1, Color* color2, double weight) {
      Color_RGBA_Obj c1 = color1->toRGBA();
      Color_RGBA_Obj c2 = color2->toRGBA();
      double p = weight/100;
      double w = 2*p - 1;
      double a = c1->a() - c2->a();

      double w1 = (((w * a == -1) ? w : (w + a)/(1 + w*a)) + 1)/2.0;
      double w2 = 1 - w1;

      return SASS_MEMORY_NEW(Color_RGBA,
                             pstate,
                             Sass::round(w1*c1->r() + w2*c2->r(), ctx.c_options.precision),
                             Sass::round(w1*c1->g() + w2*c2->g(), ctx.c_options.precision),
                             Sass::round(w1*c1->b() + w2*c2->b(), ctx.c_options.precision),
                             c1->a()*p + c2->a()*(1-p));
    }

    Signature mix_sig = "mix($color1, $color2, $weight: 50%)";
    BUILT_IN(mix)
    {
      Color_Obj  color1 = ARGCOL("$color1");
      Color_Obj  color2 = ARGCOL("$color2");
      double weight = DARG_U_PRCT("$weight");
      return colormix(ctx, pstate, color1, color2, weight);

    }

    /////////////////////////////////////////////////////////////////////////
    // Query functions
    /////////////////////////////////////////////////////////////////////////

    Signature hue_sig = "hue($color)";
    BUILT_IN(hue)
    {
      Color_HSLA_Obj col = ARGCOL("$color")->toHSLA();
      return SASS_MEMORY_NEW(Number, pstate, col->h(), "deg");
    }

    Signature saturation_sig = "saturation($color)";
    BUILT_IN(saturation)
    {
      Color_HSLA_Obj col = ARGCOL("$color")->toHSLA();
      return SASS_MEMORY_NEW(Number, pstate, col->s(), "%");
    }

    Signature lightness_sig = "lightness($color)";
    BUILT_IN(lightness)
    {
      Color_HSLA_Obj col = ARGCOL("$color")->toHSLA();
      return SASS_MEMORY_NEW(Number, pstate, col->l(), "%");
    }

    /////////////////////////////////////////////////////////////////////////
    // HSL manipulation functions
    /////////////////////////////////////////////////////////////////////////

    Signature adjust_hue_sig = "adjust-hue($color, $degrees)";
    BUILT_IN(adjust_hue)
    {
      Color* col = ARGCOL("$color");
      double degrees = ARGVAL("$degrees");
      Color_HSLA_Obj copy = col->copyAsHSLA();
      copy->h(absmod(copy->h() + degrees, 360.0));
      return copy.detach();
    }

    Signature lighten_sig = "lighten($color, $amount)";
    BUILT_IN(lighten)
    {
      Color* col = ARGCOL("$color");
      double amount = DARG_U_PRCT("$amount");
      Color_HSLA_Obj copy = col->copyAsHSLA();
      copy->l(clamp(copy->l() + amount, 0.0, 100.0));
      return copy.detach();

    }

    Signature darken_sig = "darken($color, $amount)";
    BUILT_IN(darken)
    {
      Color* col = ARGCOL("$color");
      double amount = DARG_U_PRCT("$amount");
      Color_HSLA_Obj copy = col->copyAsHSLA();
      copy->l(clamp(copy->l() - amount, 0.0, 100.0));
      return copy.detach();
    }

    Signature saturate_1_sig = "saturate($amount)";
    BUILT_IN(saturate_1)
    {
      AST_Node* nr = env.get_local("$amount");
      Number* number = assertNumber(nr,
        "$amount", pstate, traces);
      std::stringstream fncall;
      fncall << "saturate(";
      fncall << number->to_css();
      fncall << ")";
      return SASS_MEMORY_NEW(StringLiteral,
        pstate, fncall.str());
    }

    Signature saturate_2_sig = "saturate($color, $amount)";
    BUILT_IN(saturate_2)
    {
      // CSS3 filter function overload: pass literal through directly
      Color* color = ARGCOL("$color");
      Number* amount = ARGNUM("$amount");
      Color_HSLA_Obj copy = color->copyAsHSLA();
      valueInRange(amount->value(), 0.0, 100.0,
        "$amount", amount->pstate(), traces);
      copy->s(clamp(copy->s() + amount->value(), 0.0, 100.0));
      return copy.detach();
    }

    Signature desaturate_sig = "desaturate($color, $amount)";
    BUILT_IN(desaturate)
    {
      Color* col = ARGCOL("$color");
      double amount = DARG_U_PRCT("$amount");
      Color_HSLA_Obj copy = col->copyAsHSLA();
      copy->s(clamp(copy->s() - amount, 0.0, 100.0));
      return copy.detach();
    }

    Signature grayscale_sig = "grayscale($color)";
    BUILT_IN(grayscale)
    {
      // CSS3 filter function overload: pass literal through directly
      Number* amount = Cast<Number>(env["$color"]);
      if (amount) {
        return SASS_MEMORY_NEW(String_Quoted, pstate, "grayscale(" + amount->to_string(ctx.c_options) + ")");
      }

      Color* col = ARGCOL("$color");
      Color_HSLA_Obj copy = col->copyAsHSLA();
      copy->s(0.0); // just reset saturation
      return copy.detach();
    }

    /////////////////////////////////////////////////////////////////////////
    // Misc manipulation functions
    /////////////////////////////////////////////////////////////////////////

    Signature complement_sig = "complement($color)";
    BUILT_IN(complement)
    {
      Color* col = ARGCOL("$color");
      Color_HSLA_Obj copy = col->copyAsHSLA();
      copy->h(absmod(copy->h() - 180.0, 360.0));
      return copy.detach();
    }

    Signature invert_sig = "invert($color, $weight: 100%)";
    BUILT_IN(invert)
    {
      // CSS3 filter function overload: pass literal through directly
      Number* amount = Cast<Number>(env["$color"]);
      double weight = DARG_U_PRCT("$weight");
      if (amount) {
        // TODO: does not throw on 100% manually passed as value
        if (weight < 100.0) {
          error("Only one argument may be passed to the plain-CSS invert() function.", pstate, traces);
        }
        return SASS_MEMORY_NEW(String_Quoted, pstate, "invert(" + amount->to_string(ctx.c_options) + ")");
      }

      Color* col = ARGCOL("$color");
      Color_RGBA_Obj inv = col->copyAsRGBA();
      inv->r(clip(255.0 - inv->r(), 0.0, 255.0));
      inv->g(clip(255.0 - inv->g(), 0.0, 255.0));
      inv->b(clip(255.0 - inv->b(), 0.0, 255.0));
      return colormix(ctx, pstate, inv, col, weight);
    }

    /////////////////////////////////////////////////////////////////////////
    // Opacity functions
    /////////////////////////////////////////////////////////////////////////

    Signature alpha_sig = "alpha($color)";
    Signature opacity_sig = "opacity($color)";
    BUILT_IN(alpha)
    {
      String_Constant* ie_kwd = Cast<String_Constant>(env["$color"]);
      if (ie_kwd) {
        return SASS_MEMORY_NEW(String_Quoted, pstate, "alpha(" + ie_kwd->value() + ")");
      }

      // CSS3 filter function overload: pass literal through directly
      Number* amount = Cast<Number>(env["$color"]);
      if (amount) {
        return SASS_MEMORY_NEW(String_Quoted, pstate, "opacity(" + amount->to_string(ctx.c_options) + ")");
      }

      return SASS_MEMORY_NEW(Number, pstate, ARGCOL("$color")->a());
    }

    Signature opacify_sig = "opacify($color, $amount)";
    Signature fade_in_sig = "fade-in($color, $amount)";
    BUILT_IN(opacify)
    {
      Color* col = ARGCOL("$color");
      double amount = DARG_U_FACT("$amount");
      Color_Obj copy = SASS_MEMORY_COPY(col);
      copy->a(clip(col->a() + amount, 0.0, 1.0));
      return copy.detach();
    }

    Signature transparentize_sig = "transparentize($color, $amount)";
    Signature fade_out_sig = "fade-out($color, $amount)";
    BUILT_IN(transparentize)
    {
      Color* col = ARGCOL("$color");
      double amount = DARG_U_FACT("$amount");
      Color_Obj copy = SASS_MEMORY_COPY(col);
      copy->a(std::max(col->a() - amount, 0.0));
      return copy.detach();
    }

    ////////////////////////
    // OTHER COLOR FUNCTIONS
    ////////////////////////

    Signature adjust_color_sig = "adjust-color($color, $kwargs...)";
    BUILT_IN(adjust_color)
    {
      std::vector<ExpressionObj> arguments =
        getArguments(env, { "$color", "$kwargs" });
      Color* color = assertColor(arguments[0], "$color", pstate, traces);
      List* argumentList = Cast<List>(arguments[1]); // is rest...
      checkPositionalArgs(argumentList, traces);
      NormalizedMap<ExpressionObj> keywords =
        argumentList->getNormalizedArgMap();

      // ToDo _fuzzyRoundOrNull
      Number* r = getColorArgInRange(keywords, "$red", -255.0, 255.0, pstate, traces);
      Number* g = getColorArgInRange(keywords, "$green", -255.0, 255.0, pstate, traces);
      Number* b = getColorArgInRange(keywords, "$blue", -255.0, 255.0, pstate, traces);
      Number* h = getColorArg(keywords, "$hue", pstate, traces);
      Number* s = getColorArgInRange(keywords, "$saturation", -100.0, 100.0, pstate, traces);
      Number* l = getColorArgInRange(keywords, "$lightness", -100.0, 100.0, pstate, traces);
      Number* a = getColorArgInRange(keywords, "$alpha", -1.0, 1.0, pstate, traces);

      if (!keywords.empty()) {
        std::vector<std::string> keys;
        for (auto kv : keywords) {

          keys.push_back(kv.first);
        }
        std::stringstream msg;
        size_t iL = keys.size();
        msg << "No argument" <<
          (iL > 1 ? "s" : "") <<
          " named ";
        for (size_t i = 0; i < iL; i++) {
          if (i > 0) {
            msg << (i == iL - 1 ?
              " and " : ", ");
          }
          msg << keys[i];
        }
        msg << ".";
        error(msg.str(), pstate, traces);
      }

      bool hasRgb = r || g || b;
      bool hasHsl = h || s || l;

      if (hasRgb && hasHsl) {
        error("RGB parameters may not be passed along with HSL parameters.", pstate, traces);
      }
      else if (hasRgb) {
        Color_RGBA_Obj c = color->copyAsRGBA();
        if (r) c->r(c->r() + (r ? r->value() : 0.0));
        if (g) c->g(c->g() + (g ? g->value() : 0.0));
        if (b) c->b(c->b() + (b ? b->value() : 0.0));
        if (a) c->a(c->a() + (a ? a->value() : 0.0));
        return c.detach();
      }
      else if (hasHsl) {
        Color_HSLA_Obj c = color->copyAsHSLA();
        if (h) c->h(c->h() + absmod(h->value(), 360.0));
        if (s) c->s(c->s() + (s ? s->value() : 1.0));
        if (l) c->l(c->l() + (l ? l->value() : 1.0));
        if (a) c->a(c->a() + (a ? a->value() : 0.0));
        return c.detach();
      }
      else if (a) {
        Color_Obj c = SASS_MEMORY_COPY(color);
        c->a(c->a() + (a ? a->value() : 0.0));
        c->a(clip(c->a(), 0.0, 1.0));
        return c.detach();
      }
      return color;
    }

    Signature scale_color_sig = "scale-color($color, $kwargs...)";
    BUILT_IN(scale_color)
    {
      std::vector<ExpressionObj> arguments =
      getArguments(env, { "$color", "$kwargs" });
      Color* color = assertColor(arguments[0], "$color", pstate, traces);
      List* argumentList = Cast<List>(arguments[1]); // is rest...
      checkPositionalArgs(argumentList, traces);
      NormalizedMap<ExpressionObj> keywords =
        argumentList->getNormalizedArgMap();

      Number* r = getColorScale(keywords, "$red", pstate, traces);
      Number* g = getColorScale(keywords, "$green", pstate, traces);
      Number* b = getColorScale(keywords, "$blue", pstate, traces);
      Number* s = getColorScale(keywords, "$saturation", pstate, traces);
      Number* l = getColorScale(keywords, "$lightness", pstate, traces);
      Number* a = getColorScale(keywords, "$alpha", pstate, traces);

      if (!keywords.empty()) {
        std::vector<std::string> keys;
        for (auto kv : keywords) {
          keys.push_back(kv.first);
        }
        std::stringstream msg;
        size_t iL = keys.size();
        msg << "No argument" <<
          (iL > 1 ? "s" : "") <<
          " named ";
        for (size_t i = 0; i < iL; i++) {
          if (i > 0) {
            msg << (i == iL - 1 ?
              " and " : ", ");
          }
          msg << keys[i];
        }
        msg << ".";
        error(msg.str(), pstate, traces);
      }

      bool hasRgb = r || g || b;
      bool hasHsl = s || l;

      if (hasRgb) {
        if (hasHsl) {
          error("RGB parameters may not be passed along with HSL parameters.", pstate, traces);
        }

        Color_RGBA_Obj c = color->copyAsRGBA();
        // if (r) c->r(fuzzyRound2(scaleValue(c->r(), r->value() / 100.0, 255.0)));
        // if (g) c->g(fuzzyRound2(scaleValue(c->g(), g->value() / 100.0, 255.0)));
        // if (b) c->b(fuzzyRound2(scaleValue(c->b(), b->value() / 100.0, 255.0)));
        // if (a) c->a(scaleValue(c->a(), a->value() / 100.0, 1.0));
        return c.detach();
      }
      else if (hasHsl) {
        Color_HSLA_Obj c = color->copyAsHSLA();
        // if (s) c->s(scaleValue(c->s(), s->value() / 100.0, 100.0));
        // if (l) c->l(scaleValue(c->l(), l->value() / 100.0, 100.0));
        // if (a) c->a(scaleValue(c->a(), a->value() / 100.0, 1.0));
        return c.detach();
      }
      else if (a) {
        Color_Obj c = SASS_MEMORY_COPY(color);
        // c->a(clamp(scaleValue(c->a(), a->value() / 100.0, 1.0), 0.0, 1.0));
        c->a(clip(c->a(), 0.0, 1.0));
        return c.detach();
      }
      return color;
    }

    Signature change_color_sig = "change-color($color, $kwargs...)";
    BUILT_IN(change_color)
    {
      std::vector<ExpressionObj> arguments =
        getArguments(env, { "$color", "$kwargs" });
      Color* color = assertColor(arguments[0], "$color", pstate, traces);
      List* argumentList = Cast<List>(arguments[1]); // is rest...
      checkPositionalArgs(argumentList, traces);
      NormalizedMap<ExpressionObj> keywords =
        argumentList->getNormalizedArgMap();

      // ToDo _fuzzyRoundOrNull
      Number* r = getColorArgInRange(keywords, "$red", 0.0, 255.0, pstate, traces);
      Number* g = getColorArgInRange(keywords, "$green", 0.0, 255.0, pstate, traces);
      Number* b = getColorArgInRange(keywords, "$blue", 0.0, 255.0, pstate, traces);
      Number* h = getColorArg(keywords, "$hue", pstate, traces);
      Number* s = getColorArgInRange(keywords, "$saturation", 0.0, 100.0, pstate, traces);
      Number* l = getColorArgInRange(keywords, "$lightness", 0.0, 100.0, pstate, traces);
      Number* a = getColorArgInRange(keywords, "$alpha", 0.0, 1.0, pstate, traces);

      if (!keywords.empty()) {
        std::vector<std::string> keys;
        for (auto kv : keywords) {

          keys.push_back(kv.first);
        }
        std::stringstream msg;
        size_t iL = keys.size();
        msg << "No argument" <<
          (iL > 1 ? "s" : "") <<
          " named ";
        for (size_t i = 0; i < iL; i++) {
          if (i > 0) {
            msg << (i == iL - 1 ?
              " and " : ", ");
          }
          msg << keys[i];
        }
        msg << ".";
        error(msg.str(), pstate, traces);
      }

      bool hasRgb = r || g || b;
      bool hasHsl = h || s || l;

      if (hasRgb && hasHsl) {
        error("RGB parameters may not be passed along with HSL parameters.", pstate, traces);
      }
      else if (hasRgb) {
        Color_RGBA_Obj c = color->copyAsRGBA();
        if (r) c->r(r->value());
        if (g) c->g(g->value());
        if (b) c->b(b->value());
        if (a) c->a(a->value());
        return c.detach();
      }
      else if (hasHsl) {
        Color_HSLA_Obj c = color->copyAsHSLA();
        if (h) c->h(absmod(h->value(), 360.0));
        if (s) c->s(s->value());
        if (l) c->l(l->value());
        if (a) c->a(a->value());
        return c.detach();
      }
      else if (a) {
        Color_Obj c = SASS_MEMORY_COPY(color);
        c->a(clip(a->value(), 0.0, 1.0));
        return c.detach();
      }
      return color;
    }

    Signature ie_hex_str_sig = "ie-hex-str($color)";
    BUILT_IN(ie_hex_str)
    {
      Color* col = ARGCOL("$color");
      Color_RGBA_Obj c = col->toRGBA();
      double r = clip(c->r(), 0.0, 255.0);
      double g = clip(c->g(), 0.0, 255.0);
      double b = clip(c->b(), 0.0, 255.0);
      double a = clip(c->a(), 0.0, 1.0) * 255.0;

      std::stringstream ss;
      ss << '#' << std::setw(2) << std::setfill('0');
      ss << std::hex << std::setw(2) << static_cast<unsigned long>(Sass::round(a, ctx.c_options.precision));
      ss << std::hex << std::setw(2) << static_cast<unsigned long>(Sass::round(r, ctx.c_options.precision));
      ss << std::hex << std::setw(2) << static_cast<unsigned long>(Sass::round(g, ctx.c_options.precision));
      ss << std::hex << std::setw(2) << static_cast<unsigned long>(Sass::round(b, ctx.c_options.precision));

      std::string result = ss.str();
      Util::ascii_str_toupper(&result);
      return SASS_MEMORY_NEW(String_Quoted, pstate, result);
    }

  }

}
