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
#include "logger.hpp"

namespace Sass {

  namespace Functions {

    /// Returns [color1] and [color2], mixed together and weighted by [weight].
    SassColor* _mixColors(SassColor* color1, SassColor* color2,
      SassNumber* weight, const SourceSpan& pstate, Logger& logger)
    {

      // This algorithm factors in both the user-provided weight (w) and the
      // difference between the alpha values of the two colors (a) to decide how
      // to perform the weighted average of the two RGB values.
      // It works by first normalizing both parameters to be within [-1, 1], where
      // 1 indicates "only use color1", -1 indicates "only use color2", and all
      // values in between indicated a proportionately weighted average.
      // Once we have the normalized variables w and a, we apply the formula
      // (w + a)/(1 + w*a) to get the combined weight (in [-1, 1]) of color1. This
      // formula has two especially nice properties:
      //   * When either w or a are -1 or 1, the combined weight is also that
      //     number (cases where w * a == -1 are undefined, and handled as a
      //     special case).
      //   * When a is 0, the combined weight is w, and vice versa.
      // Finally, the weight of color1 is renormalized to be within [0, 1] and the
      // weight of color2 is given by 1 minus the weight of color1.
      double weightScale = weight->valueInRange(0.0, 100.0,
        logger, pstate, "weight") / 100.0;
      double normalizedWeight = weightScale * 2 - 1;
      double alphaDistance = color1->a() - color2->a();

      double combinedWeight1 = normalizedWeight * alphaDistance == -1
        ? normalizedWeight : (normalizedWeight + alphaDistance) /
        (1.0 + normalizedWeight * alphaDistance);
      double weight1 = (combinedWeight1 + 1) / 2;
      double weight2 = 1 - weight1;

      return SASS_MEMORY_NEW(Color_RGBA, pstate,
        fuzzyRound(color1->r() * weight1 + color2->r() * weight2, logger.epsilon),
        fuzzyRound(color1->g() * weight1 + color2->g() * weight2, logger.epsilon),
        fuzzyRound(color1->b() * weight1 + color2->b() * weight2, logger.epsilon),
        color1->a() * weightScale + color2->a() * (1 - weightScale));
    }

    double scaleValue(double current, double scale, double max) {
      return current + (scale > 0.0 ? max - current : current) * scale;
    }

    bool isVar(const Value* obj) {
      const String_Constant* s = Cast<String_Constant>(obj);
      if (s == nullptr) return false;
      if (s->hasQuotes()) return false;
      const sass::string& str = s->value();
      return starts_with(str, "var(");
    }

    bool isSpecialNumber(const Value* obj) {
      const String_Constant* s = Cast<String_Constant>(obj);
      if (s == nullptr) return false;
      if (s->hasQuotes()) return false;
      const sass::string& str = s->value();
      if (str.length() < 6) return false;
      return starts_with(str, "calc(")
        || starts_with(str, "var(")
        || starts_with(str, "env(")
        || starts_with(str, "min(")
        || starts_with(str, "max(");
    }

    double _percentageOrUnitless2(Logger& logger, Number* number, double max, sass::string name, Backtraces traces) {
      double value = 0.0;
      if (!number->hasUnits()) {
        value = number->value();
      }
      else if (number->hasUnit(Strings::percent)) {
        value = max * number->value() / 100;
      }
      else {
        // callStackFrame frame(logger, Backtrace(number->pstate()));
        error(name + ": Expected " + number->to_css() +
          " to have no units or \"%\".", number->pstate(), logger);
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

    Expression* _parseChannels2(sass::string name, sass::vector<sass::string> argumentNames, Value* channels, const SourceSpan& pstate, Backtraces traces)
    {
      if (isVar(channels)) {
        sass::sstream fncall;
        fncall << name << "("
          << channels->to_css() << ")";
        return SASS_MEMORY_NEW(String_Constant,
          pstate, fncall.str());
      }

      SassListObj list = Cast<SassList>(channels);

      if (!list) {
        list = SASS_MEMORY_NEW(SassList, pstate);
        list->append(channels);
      }

      bool isCommaSeparated = list->separator() == SASS_COMMA;
      bool isBracketed = list->hasBrackets();
      if (isCommaSeparated || isBracketed) {
        sass::sstream msg;
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
        sass::sstream msg;
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
          sass::sstream fncall;
          fncall << name << "(";
          for (size_t i = 0, iL = list->length(); i < iL; i++) {
            if (i > 0) { fncall << " "; }
            fncall << list->get(i)->to_css();
          }
          fncall << ")";
          return SASS_MEMORY_NEW(String_Constant,
            pstate, fncall.str());
        }
        else {
          sass::string argument = argumentNames[list->length()];
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
        !secondString->hasQuotes() &&
        string_constains(secondString->value(), '/')) {
        sass::sstream fncall;
        fncall << name << "(";
        for (size_t i = 0, iL = list->length(); i < iL; i++) {
          if (i > 0) { fncall << " "; }
          fncall << list->get(i)->to_css();

        }
        fncall << ")";
        return SASS_MEMORY_NEW(String_Constant,
          pstate, fncall.str());
      }
      else {
        return list;
      }

    }

    String_Constant* _functionString(sass::string name, sass::vector<ValueObj> arguments, const SourceSpan& pstate)
    {
      bool addComma = false;
      sass::sstream fncall;
      fncall << name << "(";
      for (Value* arg : arguments) {
        if (addComma) fncall << ", ";
        fncall << arg->to_css();
        addComma = true;
      }
      fncall << ")";
      return SASS_MEMORY_NEW(String_Constant,
        pstate, fncall.str());
    }

    String_Constant* _functionRgbString(sass::string name, Color_RGBA* color, Value* alpha, const SourceSpan& pstate)
    {
      sass::sstream fncall;
      fncall << name << "(";
      fncall << color->r() << ", ";
      fncall << color->g() << ", ";
      fncall << color->b() << ", ";
      fncall << alpha->to_css() << ")";
      return SASS_MEMORY_NEW(String_Constant,
        pstate, fncall.str());
    }

    Value* _rgbTwoArg2(sass::string name, sass::vector<ValueObj> arguments, const SourceSpan& pstate, Logger& logger)
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
        SassColorObj color = arguments[0]->assertColor(logger, Strings::color);
        SassColorObj rgba = color->toRGBA();
        return _functionRgbString(name,
          rgba, arguments[1], pstate);
      }

      SassColorObj color = arguments[0]->assertColor(logger, Strings::color);
      SassNumber* alpha = arguments[1]->assertNumber(logger, Strings::alpha);
      // callStackFrame frame(logger,
      //   Backtrace(pstate, name));
      color = SASS_MEMORY_COPY(color);
      color->a(_percentageOrUnitless2(
        logger, alpha, 1.0, "$alpha", {}));
      color->disp("");
      return color.detach();
    }

    namespace Colors {

      BUILT_IN_FN(rgb_4)
      {
        return _rgb(Strings::rgb, arguments, "", pstate, *ctx.logger);
      }

      BUILT_IN_FN(rgb_3)
      {
        return _rgb(Strings::rgb, arguments, "", pstate, *ctx.logger);
      }

      BUILT_IN_FN(rgb_2)
      {
        return _rgbTwoArg2(Strings::rgb, arguments, pstate, *ctx.logger);
      }


      BUILT_IN_FN(rgb_1)
      {
        ExpressionObj parsed = _parseChannels2(
          Strings::rgb, { "$red", "$green", "$blue" },
          arguments[0], pstate, {});
        if (String_Constant * str = Cast<String_Constant>(parsed)) {
          parsed.detach();
          return str;
        }
        if (SassList * list = Cast<SassList>(parsed)) {
          return _rgb(Strings::rgb, list->elements(), "", pstate, *ctx.logger);
        }
        return SASS_MEMORY_NEW(String_Constant, pstate, arguments[0]->to_css());
      }


      BUILT_IN_FN(rgba_4)
      {
        return _rgb("rgba", arguments, "", pstate, *ctx.logger);
      }


      BUILT_IN_FN(rgba_3)
      {
        return _rgb(Strings::rgba, arguments, "", pstate, *ctx.logger);
      }


      BUILT_IN_FN(rgba_2)
      {
        return _rgbTwoArg2(Strings::rgba, arguments, pstate, *ctx.logger);
      }


      BUILT_IN_FN(rgba_1)
      {
        ExpressionObj parsed = _parseChannels2(
          Strings::rgba, { "$red", "$green", "$blue" },
          arguments[0], pstate, {});
        if (String_Constant * str = Cast<String_Constant>(parsed)) {
          parsed.detach();
          return str;
        }
        if (SassList * list = Cast<SassList>(parsed)) {
          return _rgb(Strings::rgba, list->elements(), "", pstate, *ctx.logger);
        }
        return SASS_MEMORY_NEW(String_Constant, pstate, arguments[0]->to_css());
      }


      BUILT_IN_FN(hsl_4)
      {
        return _hsl(Strings::hsl, arguments, "", pstate, *ctx.logger);
      }


      BUILT_IN_FN(hsl_3)
      {
        return _hsl(Strings::hsl, arguments, "", pstate, *ctx.logger);
      }


      BUILT_IN_FN(hsl_2)
      {
        // hsl(123, var(--foo)) is valid CSS because --foo might be `10%, 20%`
        // and functions are parsed after variable substitution.
        if (isVar(arguments[0]) || isVar(arguments[1])) {
          return _functionString(Strings::hsl, arguments, pstate);
        }
        throw Exception::SassScriptException2(
          "Missing argument $lightness.",
          *ctx.logger, pstate);
      }


      BUILT_IN_FN(hsl_1)
      {
        ExpressionObj parsed = _parseChannels2(
          Strings::hsl, { "$hue", "$saturation", "$lightness" },
          arguments[0], pstate, {});
        if (String_Constant * str = Cast<String_Constant>(parsed)) {
          parsed.detach();
          return str;
        }
        if (SassList * list = Cast<SassList>(parsed)) {
          return _hsl(Strings::hsl, list->elements(), "", pstate, *ctx.logger);
        }
        return SASS_MEMORY_NEW(String_Constant, pstate, arguments[0]->to_css());
      }


      BUILT_IN_FN(hsla_4)
      {
        return _hsl("hsla", arguments, "", pstate, *ctx.logger);
      }


      BUILT_IN_FN(hsla_3)
      {
        return _hsl(Strings::hsla, arguments, "", pstate, *ctx.logger);
      }


      BUILT_IN_FN(hsla_2)
      {
        // hsl(123, var(--foo)) is valid CSS because --foo might be `10%, 20%`
        // and functions are parsed after variable substitution.
        if (isVar(arguments[0]) || isVar(arguments[1])) {
          return _functionString(Strings::hsla, arguments, pstate);
        }
        throw Exception::SassScriptException2(
          "Missing argument $lightness.",
          *ctx.logger, pstate);
      }


      BUILT_IN_FN(hsla_1)
      {
        ExpressionObj parsed = _parseChannels2(
          Strings::hsla, { "$hue", "$saturation", "$lightness" },
          arguments[0], pstate, {});
        if (String_Constant * str = Cast<String_Constant>(parsed)) {
          parsed.detach();
          return str;
        }
        if (SassList * list = Cast<SassList>(parsed)) {
          return _hsl(Strings::hsla, list->elements(), "", pstate, *ctx.logger);
        }
        return SASS_MEMORY_NEW(String_Constant, pstate, arguments[0]->to_css());
      }

      BUILT_IN_FN(red)
      {
        return SASS_MEMORY_NEW(SassNumber, pstate,
          round(arguments[0]->assertColor(*ctx.logger, Strings::color)->r()));
      }

      BUILT_IN_FN(green)
      {
        return SASS_MEMORY_NEW(SassNumber, pstate,
          round(arguments[0]->assertColor(*ctx.logger, Strings::color)->g()));
      }

      BUILT_IN_FN(blue)
      {
        return SASS_MEMORY_NEW(SassNumber, pstate,
          round(arguments[0]->assertColor(*ctx.logger, Strings::color)->b()));
      }

      BUILT_IN_FN(invert)
      {
        SassNumber* weight = arguments[1]->assertNumber(*ctx.logger, "weight");
        if (Cast<SassNumber>(arguments[0])) {
          if (weight->value() != 100 || !weight->hasUnit(Strings::percent)) {
            throw Exception::SassRuntimeException2(
              "Only one argument may be passed "
              "to the plain-CSS invert() function.",
              *ctx.logger, pstate);
          }
          return _functionString("invert", { arguments[0] }, pstate);
        }

        SassColorObj color = arguments[0]->assertColor(*ctx.logger, Strings::color);
        SassColorObj inverse = color->copyAsRGBA();
        inverse->r(clamp(255.0 - color->r(), 0.0, 255.0));
        inverse->g(clamp(255.0 - color->g(), 0.0, 255.0));
        inverse->b(clamp(255.0 - color->b(), 0.0, 255.0));
        return _mixColors(inverse, color,
          weight, pstate, *ctx.logger);
      }

      BUILT_IN_FN(hue)
      {
        Color_HSLA_Obj color = arguments[0]->assertColorHsla(*ctx.logger, Strings::color);
        return SASS_MEMORY_NEW(SassNumber, pstate, color->h(), "deg");
      }

      BUILT_IN_FN(saturation)
      {
        Color_HSLA_Obj color = arguments[0]->assertColorHsla(*ctx.logger, Strings::color);
        return SASS_MEMORY_NEW(SassNumber, pstate, color->s(), Strings::percent);
      }


      BUILT_IN_FN(lightness)
      {
        Color_HSLA_Obj color = arguments[0]->assertColorHsla(*ctx.logger, Strings::color);
        return SASS_MEMORY_NEW(SassNumber, pstate, color->l(), Strings::percent);
      }

      BUILT_IN_FN(adjustHue)
      {
        SassColorObj color = arguments[0]->assertColor(*ctx.logger, Strings::color);
        SassNumber* degrees = arguments[1]->assertNumber(*ctx.logger, "degrees");
        Color_HSLA_Obj copy = color->copyAsHSLA();
        copy->h(absmod(copy->h() + degrees->value(), 360.0));
        return copy.detach();
      }

      BUILT_IN_FN(complement)
      {
        SassColorObj color = arguments[0]->assertColor(*ctx.logger, Strings::color);
        Color_HSLA_Obj copy = color->copyAsHSLA();
        copy->h(absmod(copy->h() + 180.0, 360.0));
        return copy.detach();
      }

      BUILT_IN_FN(grayscale)
      {
        // Gracefully handle if number is passed
        if (Cast<SassNumber>(arguments[0])) {
          return _functionString("grayscale",
            arguments, pstate);
        }
        SassColorObj color = arguments[0]->assertColor(*ctx.logger, Strings::color);
        Color_HSLA_Obj copy = color->copyAsHSLA();
        copy->s(0.0); // remove saturation
        return copy.detach();
      }

      BUILT_IN_FN(lighten)
      {
        SassColorObj color = arguments[0]->assertColor(*ctx.logger, Strings::color);
        SassNumber* amount = arguments[1]->assertNumber(*ctx.logger, Strings::amount);
        double nr = amount->valueInRange(0.0, 100.0, *ctx.logger, pstate, Strings::amount);
        Color_HSLA_Obj copy = color->copyAsHSLA();
        copy->l(clamp(copy->l() + nr, 0.0, 100.0));
        return copy.detach();
      }

      BUILT_IN_FN(darken)
      {
        SassColorObj color = arguments[0]->assertColor(*ctx.logger, Strings::color);
        SassNumber* amount = arguments[1]->assertNumber(*ctx.logger, Strings::amount);
        double nr = amount->valueInRange(0.0, 100.0, *ctx.logger, pstate, Strings::amount);
        Color_HSLA_Obj copy = color->copyAsHSLA();
        copy->l(clamp(copy->l() - nr, 0.0, 100.0));
        return copy.detach();
      }

      BUILT_IN_FN(saturate_2)
      {
        SassColorObj color = arguments[0]->assertColor(*ctx.logger, Strings::color);
        SassNumber* amount = arguments[1]->assertNumber(*ctx.logger, Strings::amount);
        double nr = amount->valueInRange(0.0, 100.0, *ctx.logger, pstate, Strings::amount);
        Color_HSLA_Obj copy = color->copyAsHSLA();
        // ToDo: this was working without before?
        if (copy->h() == 0 && nr > 0.0) copy->h(100.0);
        copy->s(clamp(copy->s() + nr, 0.0, 100.0));
        return copy.detach();
      }

      BUILT_IN_FN(saturate_1)
      {
        SassNumber* number = arguments[0]->assertNumber(*ctx.logger, Strings::amount);
        return SASS_MEMORY_NEW(String_Constant, pstate,
          "saturate(" + number->to_css() + ")");
      }

      BUILT_IN_FN(desaturate)
      {
        SassColorObj color = arguments[0]->assertColor(*ctx.logger, Strings::color);
        SassNumber* amount = arguments[1]->assertNumber(*ctx.logger, Strings::amount);
        double nr = amount->valueInRange(0.0, 100.0, *ctx.logger, pstate, Strings::amount);
        Color_HSLA_Obj copy = color->copyAsHSLA();
        copy->s(clamp(copy->s() - nr, 0.0, 100.0));
        return copy.detach();
      }

      BUILT_IN_FN(opacify)
      {
        SassColorObj color = arguments[0]->assertColor(*ctx.logger, Strings::color);
        SassNumber* amount = arguments[1]->assertNumber(*ctx.logger, Strings::amount);
        double nr = amount->valueInRange(0.0, 1.0, *ctx.logger, pstate, Strings::amount);
        Color_HSLA_Obj copy = color->copyAsHSLA();
        copy->a(clamp(copy->a() + nr, 0.0, 1.0));
        return copy.detach();
      }

      BUILT_IN_FN(transparentize)
      {
        SassColorObj color = arguments[0]->assertColor(*ctx.logger, Strings::color);
        SassNumber* amount = arguments[1]->assertNumber(*ctx.logger, Strings::amount);
        double nr = amount->valueInRange(0.0, 1.0, *ctx.logger, pstate, Strings::amount);
        Color_HSLA_Obj copy = color->copyAsHSLA();
        copy->a(clamp(copy->a() - nr, 0.0, 1.0));
        return copy.detach();
      }


      // Implements regex check against /^[a-zA-Z]+\s*=/
      bool isMsFilterStart(const sass::string& text)
      {
        auto it = text.begin();
        // The filter must start with alpha
        if (!Util::ascii_isalpha(*it)) return false;
        while (it != text.end() && Util::ascii_isalpha(*it)) ++it;
        while (it != text.end() && Util::ascii_isspace(*it)) ++it;
        return it != text.end() && *it == '=';
      }

      BUILT_IN_FN(alpha_one)
      {
        Value* argument = arguments[0];

        if (String_Constant * string = Cast<String_Constant>(argument)) {
          if (isMsFilterStart(string->value())) {
            return _functionString(Strings::alpha, arguments, pstate);
          }
        }

        /*
        if (argument is String_Constant &&
          !argument.hasQuotes &&
          argument.text.contains(_microsoftFilterStart)) {
          // Suport the proprietary Microsoft alpha() function.
          return _functionString(Strings::alpha, arguments);
        }
        */
        SassColorObj color = argument->assertColor(*ctx.logger, Strings::color);
        return SASS_MEMORY_NEW(SassNumber, pstate, color->a());
      }


      BUILT_IN_FN(alpha_any)
      {
        sass::vector<ValueObj> argList
          = arguments[0]->asVector();
        if (!argList.empty()) {
          bool isOnlyIeFilters = true;
          for (Value* value : argList) {
            if (String_Constant * string = Cast<String_Constant>(value)) {
              if (!isMsFilterStart(string->value())) {
                isOnlyIeFilters = false;
                break;
              }
            }
            else {
              isOnlyIeFilters = false;
              break;
            }
          }
          if (isOnlyIeFilters) {
            // Suport the proprietary Microsoft alpha() function.
            return _functionString(Strings::alpha, arguments, pstate);
          }
        }

        SASS_ASSERT(argList.size() != 1,
          "ArgList must only have one item");
        if (argList.empty()) {
          throw Exception::SassRuntimeException2(
            "Missing argument $color.",
            *ctx.logger, pstate);
        }
        sass::sstream strm;
        strm << "Only 1 argument allowed, but ";
        strm << argList.size() << " were passed.";
        throw Exception::SassRuntimeException2(
          strm.str(), *ctx.logger, pstate);
      }


      BUILT_IN_FN(opacity)
      {
        // Gracefully handle if number is passed
        if (Cast<SassNumber>(arguments[0])) {
          return _functionString("opacity",
            arguments, pstate);
        }
        SassColorObj color = arguments[0]->assertColor(*ctx.logger, Strings::color);
        return SASS_MEMORY_NEW(SassNumber, pstate, color->a());
      }


      BUILT_IN_FN(ieHexStr)
      {
        SassColorObj color = arguments[0]->assertColor(*ctx.logger, Strings::color);
        // clip should not be needed here
        double r = clip(color->r(), 0.0, 255.0);
        double g = clip(color->g(), 0.0, 255.0);
        double b = clip(color->b(), 0.0, 255.0);
        double a = clip(color->a(), 0.0, 1.0) * 255.0;
        sass::sstream ss;
        ss << '#' << std::setw(2) << std::setfill('0') << std::uppercase;
        ss << std::hex << std::setw(2) << fuzzyRound(a, ctx.logger->epsilon);
        ss << std::hex << std::setw(2) << fuzzyRound(r, ctx.logger->epsilon);
        ss << std::hex << std::setw(2) << fuzzyRound(g, ctx.logger->epsilon);
        ss << std::hex << std::setw(2) << fuzzyRound(b, ctx.logger->epsilon);
        return SASS_MEMORY_NEW(String_Constant, pstate, ss.str());
      }

      Number* getKwdArg(KeywordMap<ValueObj>& keywords, const EnvString& name, Logger& logger)
      {
        EnvString variable("$" + name.norm());
        auto kv = keywords.find(variable);
        // Return null since args are optional
        if (kv == keywords.end()) return nullptr;
        // Get the number object from found keyword
        SassNumber* num = kv->second->assertNumber(logger, name.orig());
        // Only consume keyword once
        keywords.erase(kv);
        // Return the number
        return num;
      }

      BUILT_IN_FN(adjust)
      {
        SassColorObj color = arguments[0]->assertColor(*ctx.logger, Strings::color);
        SassArgumentList* argumentList = arguments[1]
          ->assertArgumentList(*ctx.logger, "kwargs");
        sass::vector<ValueObj> positional = argumentList->asVector();
        if (!argumentList->asVector().empty()) {
          callStackFrame frame(*ctx.logger, Backtrace(
            color->pstate(), "color-adjust"));
          throw Exception::SassRuntimeException2(
            "Only one positional argument is allowed. All "
            "other arguments must be passed by name.",
            *ctx.logger, pstate);
        }

        // ToDo: solve without erase ...
        KeywordMap<ValueObj> keywords = argumentList->keywords();

        SassNumber* nr_r = getKwdArg(keywords, Keys::red, *ctx.logger);
        SassNumber* nr_g = getKwdArg(keywords, Keys::green, *ctx.logger);
        SassNumber* nr_b = getKwdArg(keywords, Keys::blue, *ctx.logger);
        SassNumber* nr_h = getKwdArg(keywords, Keys::hue, *ctx.logger);
        SassNumber* nr_s = getKwdArg(keywords, Keys::saturation, *ctx.logger);
        SassNumber* nr_l = getKwdArg(keywords, Keys::lightness, *ctx.logger);
        SassNumber* nr_a = getKwdArg(keywords, Keys::alpha, *ctx.logger);

        double r = nr_r ? nr_r->valueInRange(-255.0, 255.0, *ctx.logger, pstate, Strings::red) : 0.0;
        double g = nr_g ? nr_g->valueInRange(-255.0, 255.0, *ctx.logger, pstate, Strings::green) : 0.0;
        double b = nr_b ? nr_b->valueInRange(-255.0, 255.0, *ctx.logger, pstate, Strings::blue) : 0.0;
        double s = nr_s ? nr_s->valueInRange(-100.0, 100.0, *ctx.logger, pstate, Strings::saturation) : 0.0;
        double l = nr_l ? nr_l->valueInRange(-100.0, 100.0, *ctx.logger, pstate, Strings::lightness) : 0.0;
        double a = nr_a ? nr_a->valueInRange(-1.0, 1.0, *ctx.logger, pstate, Strings::alpha) : 0.0;
        double h = nr_h ? nr_h->value() : 0.0; // Hue is a very special case

        if (!keywords.empty()) {
          sass::vector<sass::string> keys;
          for (auto& kv : keywords) {
            keys.emplace_back(kv.first.orig());
          }
          sass::sstream msg;
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
          throw Exception::SassRuntimeException2(
            msg.str(), *ctx.logger, pstate);
        }

        bool hasRgb = nr_r || nr_g || nr_b;
        bool hasHsl = nr_h || nr_s || nr_l;

        if (hasRgb && hasHsl) {
          throw Exception::InvalidSyntax(pstate, *ctx.logger,
            "RGB parameters may not be passed along with HSL parameters.");
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
        else if (nr_a) {
          Color_Obj c = SASS_MEMORY_COPY(color);
          if (nr_a) c->a(clamp(c->a() + a, 0.0, 1.0));
          return c.detach();
        }
        return color.detach();
      }

      BUILT_IN_FN(change)
      {
        SassColorObj color = arguments[0]->assertColor(*ctx.logger, Strings::color);
        SassArgumentList* argumentList = arguments[1]
          ->assertArgumentList(*ctx.logger, "kwargs");
        sass::vector<ValueObj> positional = argumentList->asVector();
        if (!argumentList->asVector().empty()) {
          callStackFrame frame(*ctx.logger, Backtrace(
            color->pstate(), "color-change"));
          throw Exception::SassRuntimeException2(
            "Only one positional argument is allowed. All "
            "other arguments must be passed by name.",
            *ctx.logger, pstate);
        }

        // ToDo: solve without erase ...
        KeywordMap<ValueObj> keywords = argumentList->keywords();

        SassNumber* nr_r = getKwdArg(keywords, Keys::red, *ctx.logger);
        SassNumber* nr_g = getKwdArg(keywords, Keys::green, *ctx.logger);
        SassNumber* nr_b = getKwdArg(keywords, Keys::blue, *ctx.logger);
        SassNumber* nr_h = getKwdArg(keywords, Keys::hue, *ctx.logger);
        SassNumber* nr_s = getKwdArg(keywords, Keys::saturation, *ctx.logger);
        SassNumber* nr_l = getKwdArg(keywords, Keys::lightness, *ctx.logger);
        SassNumber* nr_a = getKwdArg(keywords, Keys::alpha, *ctx.logger);

        double r = nr_r ? nr_r->valueInRange(0.0, 255.0, *ctx.logger, pstate, Strings::red) : 0.0;
        double g = nr_g ? nr_g->valueInRange(0.0, 255.0, *ctx.logger, pstate, Strings::green) : 0.0;
        double b = nr_b ? nr_b->valueInRange(0.0, 255.0, *ctx.logger, pstate, Strings::blue) : 0.0;
        double s = nr_s ? nr_s->valueInRange(0.0, 100.0, *ctx.logger, pstate, Strings::saturation) : 0.0;
        double l = nr_l ? nr_l->valueInRange(0.0, 100.0, *ctx.logger, pstate, Strings::lightness) : 0.0;
        double a = nr_a ? nr_a->valueInRange(0.0, 1.0, *ctx.logger, pstate, Strings::alpha) : 0.0;
        double h = nr_h ? nr_h->value() : 0.0; // Hue is a very special case

        if (!keywords.empty()) {
          sass::vector<sass::string> keys;
          for (auto& kv : keywords) {
            keys.emplace_back(kv.first.orig());
          }
          sass::sstream msg;
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
          throw Exception::SassRuntimeException2(
            msg.str(), *ctx.logger, pstate);
        }

        bool hasRgb = nr_r || nr_g || nr_b;
        bool hasHsl = nr_h || nr_s || nr_l;

        if (hasRgb && hasHsl) {
          throw Exception::InvalidSyntax(pstate, *ctx.logger,
            "RGB parameters may not be passed along with HSL parameters.");
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
        else if (nr_a) {
          Color_Obj c = SASS_MEMORY_COPY(color);
          if (nr_a) c->a(clamp(a, 0.0, 1.0));
          return c.detach();
        }
        return color.detach();
      }

      BUILT_IN_FN(scale)
      {
        SassColorObj color = arguments[0]->assertColor(*ctx.logger, Strings::color);
        SassArgumentList* argumentList = arguments[1]
          ->assertArgumentList(*ctx.logger, "kwargs");
        sass::vector<ValueObj> positional = argumentList->asVector();
        if (!argumentList->asVector().empty()) {
          callStackFrame frame(*ctx.logger, Backtrace(
            color->pstate(), "scale-color"));
          throw Exception::SassRuntimeException2(
            "Only one positional argument is allowed. All "
            "other arguments must be passed by name.",
            *ctx.logger, pstate);
        }

        // ToDo: solve without erase ...
        KeywordMap<ValueObj> keywords = argumentList->keywords();

        SassNumber* nr_r = getKwdArg(keywords, Keys::red, *ctx.logger);
        SassNumber* nr_g = getKwdArg(keywords, Keys::green, *ctx.logger);
        SassNumber* nr_b = getKwdArg(keywords, Keys::blue, *ctx.logger);
        SassNumber* nr_s = getKwdArg(keywords, Keys::saturation, *ctx.logger);
        SassNumber* nr_l = getKwdArg(keywords, Keys::lightness, *ctx.logger);
        SassNumber* nr_a = getKwdArg(keywords, Keys::alpha, *ctx.logger);

        double r = nr_r ? nr_r->assertUnit(*ctx.logger, pstate, Strings::percent, Strings::red)->valueInRange(-100.0, 100.0, *ctx.logger, pstate, Strings::red) / 100.0 : 0.0;
        double g = nr_g ? nr_g->assertUnit(*ctx.logger, pstate, Strings::percent, Strings::green)->valueInRange(-100.0, 100.0, *ctx.logger, pstate, Strings::green) / 100.0 : 0.0;
        double b = nr_b ? nr_b->assertUnit(*ctx.logger, pstate, Strings::percent, Strings::blue)->valueInRange(-100.0, 100.0, *ctx.logger, pstate, Strings::blue) / 100.0 : 0.0;
        double s = nr_s ? nr_s->assertUnit(*ctx.logger, pstate, Strings::percent, Strings::saturation)->valueInRange(-100.0, 100.0, *ctx.logger, pstate, Strings::saturation) / 100.0 : 0.0;
        double l = nr_l ? nr_l->assertUnit(*ctx.logger, pstate, Strings::percent, Strings::lightness)->valueInRange(-100.0, 100.0, *ctx.logger, pstate, Strings::lightness) / 100.0 : 0.0;
        double a = nr_a ? nr_a->assertUnit(*ctx.logger, pstate, Strings::percent, Strings::alpha)->valueInRange(-100.0, 100.0, *ctx.logger, pstate, Strings::alpha) / 100.0 : 0.0;

        if (!keywords.empty()) {
          sass::vector<sass::string> keys;
          for (auto& kv : keywords) {
            keys.emplace_back(kv.first.orig());
          }
          sass::sstream msg;
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
          throw Exception::SassRuntimeException2(
            msg.str(), *ctx.logger, pstate);
        }

        bool hasRgb = nr_r || nr_g || nr_b;
        bool hasHsl = nr_s || nr_l;

        if (hasRgb && hasHsl) {
          throw Exception::InvalidSyntax(pstate, *ctx.logger,
            "RGB parameters may not be passed along with HSL parameters.");
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
        else if (nr_a) {
          Color_Obj c = SASS_MEMORY_COPY(color);
          if (nr_a) c->a(scaleValue(c->a(), a, 1.0));
          return c.detach();
        }
        return color.detach();
      }

      BUILT_IN_FN(mix)
      {
        SassColorObj color1 = arguments[0]->assertColor(*ctx.logger, "color1");
        SassColorObj color2 = arguments[1]->assertColor(*ctx.logger, "color2");
        SassNumber* weight = arguments[2]->assertNumber(*ctx.logger, "weight");
        return _mixColors(color1, color2, weight, pstate, *ctx.logger);
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
    double _percentageOrUnitless(Number* number, double max, sass::string name, Logger& traces) {
      double value = 0.0;
      if (!number->hasUnits()) {
        value = number->value();
      }
      else if (number->hasUnit(Strings::percent)) {
        value = max * number->value() / 100;
      }
      else {
        // callStackFrame frame(traces, Backtrace(number->pstate()));
        error(name + ": Expected " + number->to_css() +
          " to have no units or \"%\".", number->pstate(), traces);
      }
      if (value < 0.0) return 0.0;
      if (value > max) return max;
      return value;
    }

    Number* assertNumber(AST_Node* node, sass::string name, const SourceSpan& pstate, Backtraces traces)
    {
      if (node == nullptr) return nullptr;
      Number* nr = Cast<Number>(node);
      if (nr == nullptr) {
        error(name + ": " + node->to_string()
          + " is not a number.", pstate, traces);
      }
      return nr;
    }

    Value* _rgb(const sass::string& name, const sass::vector<ValueObj>& arguments, Signature sig, const SourceSpan& pstate, Logger& logger)
    {
      Value* _r = arguments[0];
      Value* _g = arguments[1];
      Value* _b = arguments[2];
      Value* _a = nullptr;
      if (arguments.size() > 3) {
        _a = arguments[3];
      }
      // Check if any `calc()` or `var()` are passed
      if (isSpecialNumber(_r) || isSpecialNumber(_g) || isSpecialNumber(_b) || isSpecialNumber(_a)) {
        sass::sstream fncall;
        fncall << name << "(";
        fncall << _r->to_css() << ", ";
        fncall << _g->to_css() << ", ";
        fncall << _b->to_css();
        if (_a) { fncall << ", " << _a->to_css(); }
        fncall << ")";
        return SASS_MEMORY_NEW(String_Constant, pstate, fncall.str());
      }

      Number* r = _r->assertNumber(logger, Strings::red);
      Number* g = _g->assertNumber(logger, Strings::green);
      Number* b = _b->assertNumber(logger, Strings::blue);
      Number* a = _a ? _a->assertNumber(logger, Strings::alpha) : nullptr;

      return SASS_MEMORY_NEW(Color_RGBA, pstate,
        fuzzyRound(_percentageOrUnitless(r, 255, "$red", logger), logger.epsilon),
        fuzzyRound(_percentageOrUnitless(g, 255, "$green", logger), logger.epsilon),
        fuzzyRound(_percentageOrUnitless(b, 255, "$blue", logger), logger.epsilon),
        _a ? _percentageOrUnitless(a, 1.0, "$alpha", logger) : 1.0);

    }

    Value* _hsl(const sass::string& name, const sass::vector<ValueObj>& arguments, Signature sig, const SourceSpan& pstate, Logger& logger)
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
        sass::sstream fncall;
        fncall << name << "(";
        fncall << _h->to_css() << ", ";
        fncall << _s->to_css() << ", ";
        fncall << _l->to_css();
        if (_a) { fncall << ", " << _a->to_css(); }
        fncall << ")";
        return SASS_MEMORY_NEW(String_Constant, pstate, fncall.str());
      }

      Number* h = _h->assertNumber(logger, "hue");
      Number* s = _s->assertNumber(logger, Strings::saturation);
      Number* l = _l->assertNumber(logger, Strings::lightness);
      Number* a = _a ? _a->assertNumber(logger, Strings::alpha) : nullptr;

      return SASS_MEMORY_NEW(Color_HSLA, pstate,
        h->value(),
        clamp(s->value(), 0.0, 100.0),
        clamp(l->value(), 0.0, 100.0),
        _a ? _percentageOrUnitless(a, 1.0, "$alpha", logger) : 1.0);

    }

  }

}
