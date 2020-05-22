// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include <iomanip>
#include "ast.hpp"
#include "fn_utils.hpp"
#include "fn_colors.hpp"
#include "util.hpp"
#include "util_string.hpp"
#include "string_utils.hpp"
#include "character.hpp"
#include "listize.hpp"
#include "logger.hpp"

namespace Sass {

  namespace Functions {

    /// Returns [color1] and [color2], mixed together and weighted by [weight].
    Color_RGBA* _mixColors(Color_RGBA* color1, Color_RGBA* color2,
      Number* weight, const SourceSpan& pstate, Logger& logger)
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
      double normalizedWeight = weightScale * 2.0 - 1.0;
      double alphaDistance = color1->a() - color2->a();

      double combinedWeight1 = normalizedWeight * alphaDistance == -1
        ? normalizedWeight : (normalizedWeight + alphaDistance) /
        (1.0 + normalizedWeight * alphaDistance);
      double weight1 = (combinedWeight1 + 1.0) / 2.0;
      double weight2 = 1.0 - weight1;

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
      if (obj == nullptr) return false;
      const String* s = obj->isString();
      if (s == nullptr) return false;
      if (s->hasQuotes()) return false;
      const sass::string& str = s->value();
      return StringUtils::startsWith(str, "var(");
    }

    bool isSpecialNumber(const Value* obj) {
      if (obj == nullptr) return false;
      const String* s = obj->isString();
      if (s == nullptr) return false;
      if (s->hasQuotes()) return false;
      const sass::string& str = s->value();
      if (str.length() < 6) return false;
			return StringUtils::startsWith(str, "calc(", 5)
        || StringUtils::startsWith(str, "var(", 4)
        || StringUtils::startsWith(str, "env(", 4)
        || StringUtils::startsWith(str, "min(", 4)
        || StringUtils::startsWith(str, "max(", 4);
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
        // callStackFrame frame(traces, BackTrace(number->pstate()));
        error(name + ": Expected " + number->toValString(traces) +
          " to have no units or \"%\".", number->pstate(), traces);
      }
      if (value < 0.0) return 0.0;
      if (value > max) return max;
      return value;
    }

    /// Returns whether [value] is an unquoted string that start with `var(` and
    /// contains `/`.
    bool _isVarSlash(Value* value) {
      if (String * string = value->isString()) {
				return StringUtils::startsWith(string->value(), "var(", 4) &&
          string_constains(string->value(), '/');
      }
      return false;
    }

    Value* _parseChannels(sass::string name, sass::vector<sass::string> argumentNames, Value* channels, const SourceSpan& pstate, Logger& traces)
    {
      if (isVar(channels)) {
        sass::sstream fncall;
        fncall << name << "("
          << channels->toValString(traces) << ")";
        return SASS_MEMORY_NEW(String,
          pstate, fncall.str());
      }

      ListObj list = channels->isList();

      if (!list) {
        list = SASS_MEMORY_NEW(List, pstate);
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
            fncall << list->get(i)->inspect();
          }
          fncall << ")";
          return SASS_MEMORY_NEW(String,
            pstate, fncall.str());
        }
        else {
          sass::string argument = argumentNames[list->length()];
          error(
            "Missing element " + argument + ".",
            pstate, traces);
        }
      }

      Number* secondNumber = list->get(2)->isNumber();
      String* secondString = list->get(2)->isString();
      if (secondNumber && secondNumber->hasAsSlash()) {
        List* rv = SASS_MEMORY_NEW(List, pstate);
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
          fncall << list->get(i)->inspect();

        }
        fncall << ")";
        return SASS_MEMORY_NEW(String,
          pstate, fncall.str());
      }
      else {
        return list;
      }

    }

    String* _functionString(sass::string name, sass::vector<ValueObj> arguments, const SourceSpan& pstate)
    {
      bool addComma = false;
      sass::sstream fncall;
      fncall << name << "(";
      for (Value* arg : arguments) {
        if (addComma) fncall << ", ";
        fncall << arg->inspect();
        addComma = true;
      }
      fncall << ")";
      return SASS_MEMORY_NEW(String,
        pstate, fncall.str());
    }

    String* _functionRgbString(sass::string name, Color_RGBA* color, Value* alpha, const SourceSpan& pstate)
    {
      sass::sstream fncall;
      fncall << name << "(";
      fncall << color->r() << ", ";
      fncall << color->g() << ", ";
      fncall << color->b() << ", ";
      fncall << alpha->inspect() << ")";
      return SASS_MEMORY_NEW(String,
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
        if (Color* first = arguments[0]->isColor()) {
          Color_RGBA_Obj rgba = first->toRGBA();
          return _functionRgbString(name,
            rgba, arguments[1], pstate);
        }
        else {
          return _functionString(
            name, arguments, pstate);
        }
      }
      else if (isSpecialNumber(arguments[1])) {
        Color_RGBA_Obj color = arguments[0]->assertColor(logger, Strings::color);
        Color_RGBA_Obj rgba = color->toRGBA();
        return _functionRgbString(name,
          rgba, arguments[1], pstate);
      }

      Color_RGBA_Obj color = arguments[0]->assertColor(logger, Strings::color);
      Number* alpha = arguments[1]->assertNumber(logger, Strings::alpha);
      // callStackFrame frame(logger,
      //   BackTrace(pstate, name));
      color = SASS_MEMORY_COPY(color);
      color->a(_percentageOrUnitless(
        alpha, 1.0, "$alpha", logger));
      color->disp("");
      return color.detach();
    }

    namespace Colors {

      BUILT_IN_FN(rgb_4)
      {
        return _rgb(Strings::rgb, arguments, "", pstate, *ctx.logger123);
      }

      BUILT_IN_FN(rgb_3)
      {
        return _rgb(Strings::rgb, arguments, "", pstate, *ctx.logger123);
      }

      BUILT_IN_FN(rgb_2)
      {
        return _rgbTwoArg2(Strings::rgb, arguments, pstate, *ctx.logger123);
      }


      BUILT_IN_FN(rgb_1)
      {
        ValueObj parsed = _parseChannels(
          Strings::rgb, { "$red", "$green", "$blue" },
          arguments[0], pstate, *ctx.logger123);
        if (String * str = parsed->isString()) {
          parsed.detach();
          return str;
        }
        if (List * list = parsed->isList()) { // Ex
          return _rgb(Strings::rgb, list->elements(), "", pstate, *ctx.logger123);
        }
        return SASS_MEMORY_NEW(String, pstate, arguments[0]->toValString(*ctx.logger123));
      }


      BUILT_IN_FN(rgba_4)
      {
        return _rgb("rgba", arguments, "", pstate, *ctx.logger123);
      }


      BUILT_IN_FN(rgba_3)
      {
        return _rgb(Strings::rgba, arguments, "", pstate, *ctx.logger123);
      }


      BUILT_IN_FN(rgba_2)
      {
        return _rgbTwoArg2(Strings::rgba, arguments, pstate, *ctx.logger123);
      }


      BUILT_IN_FN(rgba_1)
      {
        ValueObj parsed = _parseChannels(
          Strings::rgba, { "$red", "$green", "$blue" },
          arguments[0], pstate, *ctx.logger123);
        if (String* str = parsed->isString()) {
          parsed.detach();
          return str;
        }
        if (List * list = parsed->isList()) { // Ex
          return _rgb(Strings::rgba, list->elements(), "", pstate, *ctx.logger123);
        }
        return SASS_MEMORY_NEW(String, pstate, arguments[0]->toValString(*ctx.logger123));
      }


      BUILT_IN_FN(hsl_4)
      {
        return _hsl(Strings::hsl, arguments, "", pstate, *ctx.logger123);
      }


      BUILT_IN_FN(hsl_3)
      {
        return _hsl(Strings::hsl, arguments, "", pstate, *ctx.logger123);
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
          *ctx.logger123, pstate);
      }


      BUILT_IN_FN(hsl_1)
      {
        ValueObj parsed = _parseChannels(
          Strings::hsl, { "$hue", "$saturation", "$lightness" },
          arguments[0], pstate, *ctx.logger123);
        if (String* str = parsed->isString()) { // Ex
          parsed.detach();
          return str;
        }
        if (List * list = parsed->isList()) { // Ex
          return _hsl(Strings::hsl, list->elements(), "", pstate, *ctx.logger123);
        }
        return SASS_MEMORY_NEW(String, pstate, arguments[0]->toValString(*ctx.logger123));
      }


      BUILT_IN_FN(hsla_4)
      {
        return _hsl("hsla", arguments, "", pstate, *ctx.logger123);
      }


      BUILT_IN_FN(hsla_3)
      {
        return _hsl(Strings::hsla, arguments, "", pstate, *ctx.logger123);
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
          *ctx.logger123, pstate);
      }


      BUILT_IN_FN(hsla_1)
      {
        ValueObj parsed = _parseChannels(
          Strings::hsla, { "$hue", "$saturation", "$lightness" },
          arguments[0], pstate, *ctx.logger123);
        if (String* str = parsed->isString()) { // Ex
          parsed.detach();
          return str;
        }
        if (List * list = parsed->isList()) { // Ex
          return _hsl(Strings::hsla, list->elements(), "", pstate, *ctx.logger123);
        }
        return SASS_MEMORY_NEW(String, pstate, arguments[0]->toValString(*ctx.logger123));
      }

      BUILT_IN_FN(red)
      {
        return SASS_MEMORY_NEW(Number, pstate,
          round(arguments[0]->assertColor(*ctx.logger123, Strings::color)->r()));
      }

      BUILT_IN_FN(green)
      {
        return SASS_MEMORY_NEW(Number, pstate,
          round(arguments[0]->assertColor(*ctx.logger123, Strings::color)->g()));
      }

      BUILT_IN_FN(blue)
      {
        return SASS_MEMORY_NEW(Number, pstate,
          round(arguments[0]->assertColor(*ctx.logger123, Strings::color)->b()));
      }

      BUILT_IN_FN(invert)
      {
        Number* weight = arguments[1]->assertNumber(*ctx.logger123, "weight");
        if (arguments[0]->isNumber()) {
          if (weight->value() != 100 || !weight->hasUnit(Strings::percent)) {
            throw Exception::SassRuntimeException2(
              "Only one argument may be passed "
              "to the plain-CSS invert() function.",
              *ctx.logger123);
          }
          return _functionString("invert", { arguments[0] }, pstate);
        }

        Color_RGBA_Obj color = arguments[0]->assertColor(*ctx.logger123, Strings::color);
        Color_RGBA_Obj inverse = color->copyAsRGBA();
        inverse->r(clamp(255.0 - color->r(), 0.0, 255.0));
        inverse->g(clamp(255.0 - color->g(), 0.0, 255.0));
        inverse->b(clamp(255.0 - color->b(), 0.0, 255.0));
        return _mixColors(inverse, color,
          weight, pstate, *ctx.logger123);
      }

      BUILT_IN_FN(hue)
      {
        Color_HSLA_Obj color = arguments[0]->assertColorHsla(*ctx.logger123, Strings::color);
        return SASS_MEMORY_NEW(Number, pstate, color->h(), "deg");
      }

      BUILT_IN_FN(saturation)
      {
        Color_HSLA_Obj color = arguments[0]->assertColorHsla(*ctx.logger123, Strings::color);
        return SASS_MEMORY_NEW(Number, pstate, color->s(), Strings::percent);
      }


      BUILT_IN_FN(lightness)
      {
        Color_HSLA_Obj color = arguments[0]->assertColorHsla(*ctx.logger123, Strings::color);
        return SASS_MEMORY_NEW(Number, pstate, color->l(), Strings::percent);
      }

      BUILT_IN_FN(adjustHue)
      {
        Color_RGBA_Obj color = arguments[0]->assertColor(*ctx.logger123, Strings::color);
        Number* degrees = arguments[1]->assertNumber(*ctx.logger123, "degrees");
        Color_HSLA_Obj copy = color->copyAsHSLA();
        copy->h(absmod(copy->h() + degrees->value(), 360.0));
        return copy.detach();
      }

      BUILT_IN_FN(complement)
      {
        Color_RGBA_Obj color = arguments[0]->assertColor(*ctx.logger123, Strings::color);
        Color_HSLA_Obj copy = color->copyAsHSLA();
        copy->h(absmod(copy->h() + 180.0, 360.0));
        return copy.detach();
      }

      BUILT_IN_FN(grayscale)
      {
        // Gracefully handle if number is passed
        if (arguments[0]->isNumber()) {
          return _functionString("grayscale",
            arguments, pstate);
        }
        Color_RGBA_Obj color = arguments[0]->assertColor(*ctx.logger123, Strings::color);
        Color_HSLA_Obj copy = color->copyAsHSLA();
        copy->s(0.0); // remove saturation
        return copy.detach();
      }

      BUILT_IN_FN(lighten)
      {
        Color_RGBA_Obj color = arguments[0]->assertColor(*ctx.logger123, Strings::color);
        Number* amount = arguments[1]->assertNumber(*ctx.logger123, Strings::amount);
        double nr = amount->valueInRange(0.0, 100.0, *ctx.logger123, pstate, Strings::amount);
        Color_HSLA_Obj copy = color->copyAsHSLA();
        copy->l(clamp(copy->l() + nr, 0.0, 100.0));
        return copy.detach();
      }

      BUILT_IN_FN(darken)
      {
        Color_RGBA_Obj color = arguments[0]->assertColor(*ctx.logger123, Strings::color);
        Number* amount = arguments[1]->assertNumber(*ctx.logger123, Strings::amount);
        double nr = amount->valueInRange(0.0, 100.0, *ctx.logger123, pstate, Strings::amount);
        Color_HSLA_Obj copy = color->copyAsHSLA();
        copy->l(clamp(copy->l() - nr, 0.0, 100.0));
        return copy.detach();
      }

      BUILT_IN_FN(saturate_2)
      {
        Color_RGBA_Obj color = arguments[0]->assertColor(*ctx.logger123, Strings::color);
        Number* amount = arguments[1]->assertNumber(*ctx.logger123, Strings::amount);
        double nr = amount->valueInRange(0.0, 100.0, *ctx.logger123, pstate, Strings::amount);
        Color_HSLA_Obj copy = color->copyAsHSLA();
        // ToDo: this was working without before?
        if (copy->h() == 0 && nr > 0.0) copy->h(100.0);
        copy->s(clamp(copy->s() + nr, 0.0, 100.0));
        return copy.detach();
      }

      BUILT_IN_FN(saturate_1)
      {
        Number* number = arguments[0]->assertNumber(*ctx.logger123, Strings::amount);
        return SASS_MEMORY_NEW(String, pstate,
          "saturate(" + number->toValString(*ctx.logger123) + ")");
      }

      BUILT_IN_FN(desaturate)
      {
        Color_RGBA_Obj color = arguments[0]->assertColor(*ctx.logger123, Strings::color);
        Number* amount = arguments[1]->assertNumber(*ctx.logger123, Strings::amount);
        double nr = amount->valueInRange(0.0, 100.0, *ctx.logger123, pstate, Strings::amount);
        Color_HSLA_Obj copy = color->copyAsHSLA();
        copy->s(clamp(copy->s() - nr, 0.0, 100.0));
        return copy.detach();
      }

      BUILT_IN_FN(opacify)
      {
        Color_RGBA_Obj color = arguments[0]->assertColor(*ctx.logger123, Strings::color);
        Number* amount = arguments[1]->assertNumber(*ctx.logger123, Strings::amount);
        double nr = amount->valueInRange(0.0, 1.0, *ctx.logger123, pstate, Strings::amount);
        Color_HSLA_Obj copy = color->copyAsHSLA();
        copy->a(clamp(copy->a() + nr, 0.0, 1.0));
        return copy.detach();
      }

      BUILT_IN_FN(transparentize)
      {
        Color_RGBA_Obj color = arguments[0]->assertColor(*ctx.logger123, Strings::color);
        Number* amount = arguments[1]->assertNumber(*ctx.logger123, Strings::amount);
        double nr = amount->valueInRange(0.0, 1.0, *ctx.logger123, pstate, Strings::amount);
        Color_HSLA_Obj copy = color->copyAsHSLA();
        copy->a(clamp(copy->a() - nr, 0.0, 1.0));
        return copy.detach();
      }


      // Implements regex check against /^[a-zA-Z]+\s*=/
      bool isMsFilterStart(const sass::string& text)
      {
        auto it = text.begin();
        // The filter must start with alpha
        if (!Character::isAlphabetic(*it)) return false;
        while (it != text.end() && Character::isAlphabetic(*it)) ++it;
        while (it != text.end() && Character::isWhitespace(*it)) ++it;
        return it != text.end() && *it == '=';
      }

      BUILT_IN_FN(alpha_one)
      {
        Value* argument = arguments[0];

        if (String * string = argument->isString()) {
          if (isMsFilterStart(string->value())) {
            return _functionString(Strings::alpha, arguments, pstate);
          }
        }

        /*
        if (argument is String &&
          !argument.hasQuotes &&
          argument.text.contains(_microsoftFilterStart)) {
          // Support the proprietary Microsoft alpha() function.
          return _functionString(Strings::alpha, arguments);
        }
        */
        Color_RGBA_Obj color = argument->assertColor(*ctx.logger123, Strings::color);
        return SASS_MEMORY_NEW(Number, pstate, color->a());
      }


      BUILT_IN_FN(alpha_any)
      {
        size_t size = arguments[0]->lengthAsList();

        if (size == 0) {
          throw Exception::SassRuntimeException2(
            "Missing argument $color.",
            *ctx.logger123);
        }

        bool isOnlyIeFilters = true;
        for (Value* value : arguments[0]->iterator()) {
          if (String* string = value->isString()) {
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
          // Support the proprietary Microsoft alpha() function.
          return _functionString(Strings::alpha, arguments, pstate);
        }

        sass::sstream strm;
        strm << "Only 1 argument allowed, but ";
        strm << size << " were passed.";
        throw Exception::SassRuntimeException2(
          strm.str(), *ctx.logger123);
      }


      BUILT_IN_FN(opacity)
      {
        // Gracefully handle if number is passed
        if (arguments[0]->isNumber()) {
          return _functionString("opacity",
            arguments, pstate);
        }
        Color_RGBA_Obj color = arguments[0]->assertColor(*ctx.logger123, Strings::color);
        return SASS_MEMORY_NEW(Number, pstate, color->a());
      }


      BUILT_IN_FN(ieHexStr)
      {
        Color_RGBA_Obj color = arguments[0]->assertColor(*ctx.logger123, Strings::color);
        // clip should not be needed here
        double r = clip(color->r(), 0.0, 255.0);
        double g = clip(color->g(), 0.0, 255.0);
        double b = clip(color->b(), 0.0, 255.0);
        double a = clip(color->a(), 0.0, 1.0) * 255.0;
        sass::sstream ss;
        ss << '#' << std::setw(2) << std::setfill('0') << std::uppercase;
        ss << std::hex << std::setw(2) << fuzzyRound(a, ctx.logger123->epsilon);
        ss << std::hex << std::setw(2) << fuzzyRound(r, ctx.logger123->epsilon);
        ss << std::hex << std::setw(2) << fuzzyRound(g, ctx.logger123->epsilon);
        ss << std::hex << std::setw(2) << fuzzyRound(b, ctx.logger123->epsilon);
        return SASS_MEMORY_NEW(String, pstate, ss.str());
      }

      Number* getKwdArg(EnvKeyFlatMap<ValueObj>& keywords, const EnvKey& name, Logger& logger)
      {
        EnvKey variable("$" + name.norm());
        auto kv = keywords.find(variable);
        // Return null since args are optional
        if (kv == keywords.end()) return nullptr;
        // Get the number object from found keyword
        Number* num = kv->second->assertNumber(logger, name.orig());
        // Only consume keyword once
        keywords.erase(kv);
        // Return the number
        return num;
      }

      BUILT_IN_FN(adjust)
      {
        Color_RGBA_Obj color = arguments[0]->assertColor(*ctx.logger123, Strings::color);
        ArgumentList* argumentList = arguments[1]
          ->assertArgumentList(*ctx.logger123, "kwargs");
        if (!argumentList->empty()) {
          SourceSpan span(color->pstate());
          callStackFrame frame(*ctx.logger123, BackTrace(
            span, Strings::colorAdjust));
          throw Exception::SassRuntimeException2(
            "Only one positional argument is allowed. All "
            "other arguments must be passed by name.",
            *ctx.logger123);
        }

        // ToDo: solve without erase ...
        EnvKeyFlatMap<ValueObj> keywords = argumentList->keywords();

        Number* nr_r = getKwdArg(keywords, Keys::red, *ctx.logger123);
        Number* nr_g = getKwdArg(keywords, Keys::green, *ctx.logger123);
        Number* nr_b = getKwdArg(keywords, Keys::blue, *ctx.logger123);
        Number* nr_h = getKwdArg(keywords, Keys::hue, *ctx.logger123);
        Number* nr_s = getKwdArg(keywords, Keys::saturation, *ctx.logger123);
        Number* nr_l = getKwdArg(keywords, Keys::lightness, *ctx.logger123);
        Number* nr_a = getKwdArg(keywords, Keys::alpha, *ctx.logger123);

        double r = nr_r ? nr_r->valueInRange(-255.0, 255.0, *ctx.logger123, pstate, Strings::red) : 0.0;
        double g = nr_g ? nr_g->valueInRange(-255.0, 255.0, *ctx.logger123, pstate, Strings::green) : 0.0;
        double b = nr_b ? nr_b->valueInRange(-255.0, 255.0, *ctx.logger123, pstate, Strings::blue) : 0.0;
        double s = nr_s ? nr_s->valueInRange(-100.0, 100.0, *ctx.logger123, pstate, Strings::saturation) : 0.0;
        double l = nr_l ? nr_l->valueInRange(-100.0, 100.0, *ctx.logger123, pstate, Strings::lightness) : 0.0;
        double a = nr_a ? nr_a->valueInRange(-1.0, 1.0, *ctx.logger123, pstate, Strings::alpha) : 0.0;
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
            msg.str(), *ctx.logger123);
        }

        bool hasRgb = nr_r || nr_g || nr_b;
        bool hasHsl = nr_h || nr_s || nr_l;

        if (hasRgb && hasHsl) {
          throw Exception::InvalidSyntax(*ctx.logger123,
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
        Color_RGBA_Obj color = arguments[0]->assertColor(*ctx.logger123, Strings::color);
        ArgumentList* argumentList = arguments[1]
          ->assertArgumentList(*ctx.logger123, "kwargs");
        if (!argumentList->empty()) {
          SourceSpan span(color->pstate());
          callStackFrame frame(*ctx.logger123, BackTrace(
            span, Strings::colorChange));
          throw Exception::SassRuntimeException2(
            "Only one positional argument is allowed. All "
            "other arguments must be passed by name.",
            *ctx.logger123);
        }

        // ToDo: solve without erase ...
        EnvKeyFlatMap<ValueObj> keywords = argumentList->keywords();

        Number* nr_r = getKwdArg(keywords, Keys::red, *ctx.logger123);
        Number* nr_g = getKwdArg(keywords, Keys::green, *ctx.logger123);
        Number* nr_b = getKwdArg(keywords, Keys::blue, *ctx.logger123);
        Number* nr_h = getKwdArg(keywords, Keys::hue, *ctx.logger123);
        Number* nr_s = getKwdArg(keywords, Keys::saturation, *ctx.logger123);
        Number* nr_l = getKwdArg(keywords, Keys::lightness, *ctx.logger123);
        Number* nr_a = getKwdArg(keywords, Keys::alpha, *ctx.logger123);

        double r = nr_r ? nr_r->valueInRange(0.0, 255.0, *ctx.logger123, pstate, Strings::red) : 0.0;
        double g = nr_g ? nr_g->valueInRange(0.0, 255.0, *ctx.logger123, pstate, Strings::green) : 0.0;
        double b = nr_b ? nr_b->valueInRange(0.0, 255.0, *ctx.logger123, pstate, Strings::blue) : 0.0;
        double s = nr_s ? nr_s->valueInRange(0.0, 100.0, *ctx.logger123, pstate, Strings::saturation) : 0.0;
        double l = nr_l ? nr_l->valueInRange(0.0, 100.0, *ctx.logger123, pstate, Strings::lightness) : 0.0;
        double a = nr_a ? nr_a->valueInRange(0.0, 1.0, *ctx.logger123, pstate, Strings::alpha) : 0.0;
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
            msg.str(), *ctx.logger123);
        }

        bool hasRgb = nr_r || nr_g || nr_b;
        bool hasHsl = nr_h || nr_s || nr_l;

        if (hasRgb && hasHsl) {
          throw Exception::InvalidSyntax(*ctx.logger123,
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
        Color_RGBA_Obj color = arguments[0]->assertColor(*ctx.logger123, Strings::color);
        ArgumentList* argumentList = arguments[1]
          ->assertArgumentList(*ctx.logger123, "kwargs");
        if (!argumentList->empty()) {
          SourceSpan span(color->pstate());
          callStackFrame frame(*ctx.logger123, BackTrace(
            span, Strings::scaleColor));
          throw Exception::SassRuntimeException2(
            "Only one positional argument is allowed. All "
            "other arguments must be passed by name.",
            *ctx.logger123);
        }

        // ToDo: solve without erase ...
        EnvKeyFlatMap<ValueObj> keywords = argumentList->keywords();

        Number* nr_r = getKwdArg(keywords, Keys::red, *ctx.logger123);
        Number* nr_g = getKwdArg(keywords, Keys::green, *ctx.logger123);
        Number* nr_b = getKwdArg(keywords, Keys::blue, *ctx.logger123);
        Number* nr_s = getKwdArg(keywords, Keys::saturation, *ctx.logger123);
        Number* nr_l = getKwdArg(keywords, Keys::lightness, *ctx.logger123);
        Number* nr_a = getKwdArg(keywords, Keys::alpha, *ctx.logger123);

        double r = nr_r ? nr_r->assertUnit(*ctx.logger123, pstate, Strings::percent, Strings::red)->valueInRange(-100.0, 100.0, *ctx.logger123, pstate, Strings::red) / 100.0 : 0.0;
        double g = nr_g ? nr_g->assertUnit(*ctx.logger123, pstate, Strings::percent, Strings::green)->valueInRange(-100.0, 100.0, *ctx.logger123, pstate, Strings::green) / 100.0 : 0.0;
        double b = nr_b ? nr_b->assertUnit(*ctx.logger123, pstate, Strings::percent, Strings::blue)->valueInRange(-100.0, 100.0, *ctx.logger123, pstate, Strings::blue) / 100.0 : 0.0;
        double s = nr_s ? nr_s->assertUnit(*ctx.logger123, pstate, Strings::percent, Strings::saturation)->valueInRange(-100.0, 100.0, *ctx.logger123, pstate, Strings::saturation) / 100.0 : 0.0;
        double l = nr_l ? nr_l->assertUnit(*ctx.logger123, pstate, Strings::percent, Strings::lightness)->valueInRange(-100.0, 100.0, *ctx.logger123, pstate, Strings::lightness) / 100.0 : 0.0;
        double a = nr_a ? nr_a->assertUnit(*ctx.logger123, pstate, Strings::percent, Strings::alpha)->valueInRange(-100.0, 100.0, *ctx.logger123, pstate, Strings::alpha) / 100.0 : 0.0;

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
            msg.str(), *ctx.logger123);
        }

        bool hasRgb = nr_r || nr_g || nr_b;
        bool hasHsl = nr_s || nr_l;

        if (hasRgb && hasHsl) {
          throw Exception::InvalidSyntax(*ctx.logger123,
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
        Color_RGBA_Obj color1 = arguments[0]->assertColor(*ctx.logger123, "color1");
        Color_RGBA_Obj color2 = arguments[1]->assertColor(*ctx.logger123, "color2");
        Number* weight = arguments[2]->assertNumber(*ctx.logger123, "weight");
        return _mixColors(color1, color2, weight, pstate, *ctx.logger123);
      }

      void registerFunctions(Context& ctx)
      {

        ctx.registerBuiltInOverloadFns("rgb", {
          std::make_pair("$red, $green, $blue, $alpha", rgb_4),
          std::make_pair("$red, $green, $blue", rgb_3),
          std::make_pair("$color, $alpha", rgb_2),
          std::make_pair("$channels", rgb_1),
          });
        ctx.registerBuiltInOverloadFns("rgba", {
          std::make_pair("$red, $green, $blue, $alpha", rgba_4),
          std::make_pair("$red, $green, $blue", rgba_3),
          std::make_pair("$color, $alpha", rgba_2),
          std::make_pair("$channels", rgba_1),
          });
        ctx.registerBuiltInOverloadFns("hsl", {
          std::make_pair("$hue, $saturation, $lightness, $alpha", hsl_4),
          std::make_pair("$hue, $saturation, $lightness", hsl_3),
          std::make_pair("$color, $alpha", hsl_2),
          std::make_pair("$channels", hsl_1),
          });
        ctx.registerBuiltInOverloadFns("hsla", {
          std::make_pair("$hue, $saturation, $lightness, $alpha", hsla_4),
          std::make_pair("$hue, $saturation, $lightness", hsla_3),
          std::make_pair("$color, $alpha", hsla_2),
          std::make_pair("$channels", hsla_1),
          });

        ctx.registerBuiltInFunction("red", "$color", red);
        ctx.registerBuiltInFunction("green", "$color", green);
        ctx.registerBuiltInFunction("blue", "$color", blue);
        ctx.registerBuiltInFunction("hue", "$color", hue);
        ctx.registerBuiltInFunction("lightness", "$color", lightness);
        ctx.registerBuiltInFunction("saturation", "$color", saturation);
        ctx.registerBuiltInFunction("invert", "$color, $weight: 100%", invert);
        ctx.registerBuiltInFunction("grayscale", "$color", grayscale);
        ctx.registerBuiltInFunction("complement", "$color", complement);
        ctx.registerBuiltInFunction("lighten", "$color, $amount", lighten);
        ctx.registerBuiltInFunction("darken", "$color, $amount", darken);
        ctx.registerBuiltInFunction("desaturate", "$color, $amount", desaturate);
        ctx.registerBuiltInOverloadFns("saturate", {
          std::make_pair("$amount", saturate_1),
          std::make_pair("$color, $amount", saturate_2),
          });

        ctx.registerBuiltInFunction("adjust-hue", "$color, $degrees", adjustHue);
        ctx.registerBuiltInFunction("adjust-color", "$color, $kwargs...", adjust);
        ctx.registerBuiltInFunction("change-color", "$color, $kwargs...", change);
        ctx.registerBuiltInFunction("scale-color", "$color, $kwargs...", scale);
        ctx.registerBuiltInFunction("mix", "$color1, $color2, $weight: 50%", mix);

        ctx.registerBuiltInFunction("opacify", "$color, $amount", opacify);
        ctx.registerBuiltInFunction("fade-in", "$color, $amount", opacify);
        ctx.registerBuiltInFunction("fade-out", "$color, $amount", transparentize);
        ctx.registerBuiltInFunction("transparentize", "$color, $amount", transparentize);
        ctx.registerBuiltInFunction("ie-hex-str", "$color", ieHexStr);
        ctx.registerBuiltInOverloadFns("alpha", {
          std::make_pair("$color", alpha_one),
          std::make_pair("$args...", alpha_any),
          });
        ctx.registerBuiltInFunction("opacity", "$color", opacity);

      }

    }


    // Who calls me, shouldn't we go from value? Why ast node?
    Number* assertNumber(AST_Node* node, sass::string name, const SourceSpan& pstate, Logger& logger)
    {
      if (node == nullptr) return nullptr;
      Number* nr = Cast<Number>(node);
      if (nr == nullptr) {
        error(name + ": " + node->to_string(logger)
          + " is not a number.", pstate, logger);
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
        fncall << _r->toValString(logger) << ", ";
        fncall << _g->toValString(logger) << ", ";
        fncall << _b->toValString(logger);
        if (_a) { fncall << ", " << _a->toValString(logger); }
        fncall << ")";
        return SASS_MEMORY_NEW(String, pstate, fncall.str());
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
        fncall << _h->toValString(logger) << ", ";
        fncall << _s->toValString(logger) << ", ";
        fncall << _l->toValString(logger);
        if (_a) { fncall << ", " << _a->toValString(logger); }
        fncall << ")";
        return SASS_MEMORY_NEW(String, pstate, fncall.str());
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
