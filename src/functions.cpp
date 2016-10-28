#include "sass.hpp"
#include "functions.hpp"
#include "ast.hpp"
#include "context.hpp"
#include "backtrace.hpp"
#include "parser.hpp"
#include "constants.hpp"
#include "inspect.hpp"
#include "extend.hpp"
#include "eval.hpp"
#include "util.hpp"
#include "expand.hpp"
#include "utf8_string.hpp"
#include "sass/base.h"
#include "utf8.h"

#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <cctype>
#include <sstream>
#include <string>
#include <iomanip>
#include <iostream>
#include <random>
#include <set>

#ifdef __MINGW32__
#include "windows.h"
#include "wincrypt.h"
#endif

#define ARG(argname, argtype) get_arg<argtype>(argname, env, sig, pstate, backtrace)
#define ARGR(argname, argtype, lo, hi) get_arg_r(argname, env, sig, pstate, lo, hi, backtrace)
#define ARGM(argname, argtype, ctx) get_arg_m(argname, env, sig, pstate, backtrace, ctx)

namespace Sass {
  using std::stringstream;
  using std::endl;

  Definition_Ptr make_native_function(Signature sig, Native_Function func, Context& ctx)
  {
    Parser sig_parser = Parser::from_c_str(sig, ctx, ParserState("[built-in function]"));
    sig_parser.lex<Prelexer::identifier>();
    std::string name(Util::normalize_underscores(sig_parser.lexed));
    Parameters_Ptr params = &sig_parser.parse_parameters();
    return SASS_MEMORY_NEW(ctx.mem, Definition,
                           ParserState("[built-in function]"),
                           sig,
                           name,
                           params,
                           func,
                           false);
  }

  Definition_Ptr make_c_function(Sass_Function_Entry c_func, Context& ctx)
  {
    using namespace Prelexer;

    const char* sig = sass_function_get_signature(c_func);
    Parser sig_parser = Parser::from_c_str(sig, ctx, ParserState("[c function]"));
    // allow to overload generic callback plus @warn, @error and @debug with custom functions
    sig_parser.lex < alternatives < identifier, exactly <'*'>,
                                    exactly < Constants::warn_kwd >,
                                    exactly < Constants::error_kwd >,
                                    exactly < Constants::debug_kwd >
                   >              >();
    std::string name(Util::normalize_underscores(sig_parser.lexed));
    Parameters_Ptr params = &sig_parser.parse_parameters();
    return SASS_MEMORY_NEW(ctx.mem, Definition,
                           ParserState("[c function]"),
                           sig,
                           name,
                           params,
                           c_func,
                           false, true);
  }

  std::string function_name(Signature sig)
  {
    std::string str(sig);
    return str.substr(0, str.find('('));
  }

  namespace Functions {

    inline void handle_utf8_error (const ParserState& pstate, Backtrace* backtrace)
    {
      try {
       throw;
      }
      catch (utf8::invalid_code_point) {
        std::string msg("utf8::invalid_code_point");
        error(msg, pstate, backtrace);
      }
      catch (utf8::not_enough_room) {
        std::string msg("utf8::not_enough_room");
        error(msg, pstate, backtrace);
      }
      catch (utf8::invalid_utf8) {
        std::string msg("utf8::invalid_utf8");
        error(msg, pstate, backtrace);
      }
      catch (...) { throw; }
    }

    template <typename T>
    T* get_arg(const std::string& argname, Env& env, Signature sig, ParserState pstate, Backtrace* backtrace)
    {
      // Minimal error handling -- the expectation is that built-ins will be written correctly!
      T* val = dynamic_cast<T*>(env[argname]);
      if (!val) {
        std::string msg("argument `");
        msg += argname;
        msg += "` of `";
        msg += sig;
        msg += "` must be a ";
        msg += T::type_name();
        error(msg, pstate, backtrace);
      }
      return val;
    }

    Map_Ptr get_arg_m(const std::string& argname, Env& env, Signature sig, ParserState pstate, Backtrace* backtrace, Context& ctx)
    {
      // Minimal error handling -- the expectation is that built-ins will be written correctly!
      Map_Ptr val = dynamic_cast<Map_Ptr>(env[argname]);
      if (val) return val;

      List_Ptr lval = dynamic_cast<List_Ptr>(env[argname]);
      if (lval && lval->length() == 0) return SASS_MEMORY_NEW(ctx.mem, Map, pstate, 0);

      // fallback on get_arg for error handling
      val = get_arg<Map>(argname, env, sig, pstate, backtrace);
      return val;
    }

    Number_Ptr get_arg_r(const std::string& argname, Env& env, Signature sig, ParserState pstate, double lo, double hi, Backtrace* backtrace)
    {
      // Minimal error handling -- the expectation is that built-ins will be written correctly!
      Number_Ptr val = get_arg<Number>(argname, env, sig, pstate, backtrace);
      double v = val->value();
      if (!(lo <= v && v <= hi)) {
        std::stringstream msg;
        msg << "argument `" << argname << "` of `" << sig << "` must be between ";
        msg << lo << " and " << hi;
        error(msg.str(), pstate, backtrace);
      }
      return val;
    }

    #define ARGSEL(argname, seltype, contextualize) get_arg_sel<seltype>(argname, env, sig, pstate, backtrace, ctx)

    template <typename T>
    T* get_arg_sel(const std::string& argname, Env& env, Signature sig, ParserState pstate, Backtrace* backtrace, Context& ctx);

    template <>
    CommaSequence_Selector_Ptr get_arg_sel(const std::string& argname, Env& env, Signature sig, ParserState pstate, Backtrace* backtrace, Context& ctx) {
      Expression_Ptr exp = ARG(argname, Expression);
      if (exp->concrete_type() == Expression::NULL_VAL) {
        std::stringstream msg;
        msg << argname << ": null is not a valid selector: it must be a string,\n";
        msg << "a list of strings, or a list of lists of strings for `" << function_name(sig) << "'";
        error(msg.str(), pstate);
      }
      if (String_Constant_Ptr str =dynamic_cast<String_Constant_Ptr>(exp)) {
        str->quote_mark(0);
      }
      std::string exp_src = exp->to_string(ctx.c_options) + "{";
      return &Parser::parse_selector(exp_src.c_str(), ctx);
    }

    template <>
    Sequence_Selector_Ptr get_arg_sel(const std::string& argname, Env& env, Signature sig, ParserState pstate, Backtrace* backtrace, Context& ctx) {
      Expression_Ptr exp = ARG(argname, Expression);
      if (exp->concrete_type() == Expression::NULL_VAL) {
        std::stringstream msg;
        msg << argname << ": null is not a valid selector: it must be a string,\n";
        msg << "a list of strings, or a list of lists of strings for `" << function_name(sig) << "'";
        error(msg.str(), pstate);
      }
      if (String_Constant_Ptr str =dynamic_cast<String_Constant_Ptr>(exp)) {
        str->quote_mark(0);
      }
      std::string exp_src = exp->to_string(ctx.c_options) + "{";
      CommaSequence_Selector_Obj sel_list = Parser::parse_selector(exp_src.c_str(), ctx);
      return (sel_list->length() > 0) ? sel_list->first() : 0;
    }

    template <>
    SimpleSequence_Selector_Ptr get_arg_sel(const std::string& argname, Env& env, Signature sig, ParserState pstate, Backtrace* backtrace, Context& ctx) {
      Expression_Ptr exp = ARG(argname, Expression);
      if (exp->concrete_type() == Expression::NULL_VAL) {
        std::stringstream msg;
        msg << argname << ": null is not a string for `" << function_name(sig) << "'";
        error(msg.str(), pstate);
      }
      if (String_Constant_Ptr str =dynamic_cast<String_Constant_Ptr>(exp)) {
        str->quote_mark(0);
      }
      std::string exp_src = exp->to_string(ctx.c_options) + "{";
      CommaSequence_Selector_Obj sel_list = Parser::parse_selector(exp_src.c_str(), ctx);
      return (sel_list->length() > 0) ? sel_list->first()->tail()->head() : 0;
    }

    #ifdef __MINGW32__
    uint64_t GetSeed()
    {
      HCRYPTPROV hp = 0;
      BYTE rb[8];
      CryptAcquireContext(&hp, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
      CryptGenRandom(hp, sizeof(rb), rb);
      CryptReleaseContext(hp, 0);

      uint64_t seed;
      memcpy(&seed, &rb[0], sizeof(seed));

      return seed;
    }
    #else
    uint64_t GetSeed()
    {
      std::random_device rd;
      return rd();
    }
    #endif

    // note: the performance of many  implementations of
    // random_device degrades sharply once the entropy pool
    // is exhausted. For practical use, random_device is
    // generally only used to seed a PRNG such as mt19937.
    static std::mt19937 rand(static_cast<unsigned int>(GetSeed()));

    // features
    static std::set<std::string> features {
      "global-variable-shadowing",
      "extend-selector-pseudoclass",
      "at-error",
      "units-level-3"
    };

    ////////////////
    // RGB FUNCTIONS
    ////////////////

    inline double color_num(Number_Ptr n) {
      if (n->unit() == "%") {
        return std::min(std::max(n->value() * 255 / 100.0, 0.0), 255.0);
      } else {
        return std::min(std::max(n->value(), 0.0), 255.0);
      }
    }

    inline double alpha_num(Number_Ptr n) {
      if (n->unit() == "%") {
        return std::min(std::max(n->value(), 0.0), 100.0);
      } else {
        return std::min(std::max(n->value(), 0.0), 1.0);
      }
    }

    Signature rgb_sig = "rgb($red, $green, $blue)";
    BUILT_IN(rgb)
    {
      return SASS_MEMORY_NEW(ctx.mem, Color,
                             pstate,
                             color_num(ARG("$red",   Number)),
                             color_num(ARG("$green", Number)),
                             color_num(ARG("$blue",  Number)));
    }

    Signature rgba_4_sig = "rgba($red, $green, $blue, $alpha)";
    BUILT_IN(rgba_4)
    {
      return SASS_MEMORY_NEW(ctx.mem, Color,
                             pstate,
                             color_num(ARG("$red",   Number)),
                             color_num(ARG("$green", Number)),
                             color_num(ARG("$blue",  Number)),
                             alpha_num(ARG("$alpha", Number)));
    }

    Signature rgba_2_sig = "rgba($color, $alpha)";
    BUILT_IN(rgba_2)
    {
      Color_Ptr c_arg = ARG("$color", Color);
      Color_Ptr new_c = SASS_MEMORY_NEW(ctx.mem, Color, *c_arg);
      new_c->a(alpha_num(ARG("$alpha", Number)));
      new_c->disp("");
      return new_c;
    }

    Signature red_sig = "red($color)";
    BUILT_IN(red)
    { return SASS_MEMORY_NEW(ctx.mem, Number, pstate, ARG("$color", Color)->r()); }

    Signature green_sig = "green($color)";
    BUILT_IN(green)
    { return SASS_MEMORY_NEW(ctx.mem, Number, pstate, ARG("$color", Color)->g()); }

    Signature blue_sig = "blue($color)";
    BUILT_IN(blue)
    { return SASS_MEMORY_NEW(ctx.mem, Number, pstate, ARG("$color", Color)->b()); }

    Signature mix_sig = "mix($color-1, $color-2, $weight: 50%)";
    BUILT_IN(mix)
    {
      Color_Ptr  color1 = ARG("$color-1", Color);
      Color_Ptr  color2 = ARG("$color-2", Color);
      Number_Ptr weight = ARGR("$weight", Number, 0, 100);

      double p = weight->value()/100;
      double w = 2*p - 1;
      double a = color1->a() - color2->a();

      double w1 = (((w * a == -1) ? w : (w + a)/(1 + w*a)) + 1)/2.0;
      double w2 = 1 - w1;

      return SASS_MEMORY_NEW(ctx.mem, Color,
                             pstate,
                             Sass::round(w1*color1->r() + w2*color2->r(), ctx.c_options.precision),
                             Sass::round(w1*color1->g() + w2*color2->g(), ctx.c_options.precision),
                             Sass::round(w1*color1->b() + w2*color2->b(), ctx.c_options.precision),
                             color1->a()*p + color2->a()*(1-p));
    }

    ////////////////
    // HSL FUNCTIONS
    ////////////////

    // RGB to HSL helper function
    struct HSL { double h; double s; double l; };
    HSL rgb_to_hsl(double r, double g, double b)
    {

      // Algorithm from http://en.wikipedia.org/wiki/wHSL_and_HSV#Conversion_from_RGB_to_HSL_or_HSV
      r /= 255.0; g /= 255.0; b /= 255.0;

      double max = std::max(r, std::max(g, b));
      double min = std::min(r, std::min(g, b));
      double delta = max - min;

      double h = 0, s = 0, l = (max + min) / 2.0;

      if (max == min) {
        h = s = 0; // achromatic
      }
      else {
        if (l < 0.5) s = delta / (max + min);
        else         s = delta / (2.0 - max - min);

        if      (r == max) h = (g - b) / delta + (g < b ? 6 : 0);
        else if (g == max) h = (b - r) / delta + 2;
        else if (b == max) h = (r - g) / delta + 4;
      }

      HSL hsl_struct;
      hsl_struct.h = h / 6 * 360;
      hsl_struct.s = s * 100;
      hsl_struct.l = l * 100;

      return hsl_struct;
    }

    // hue to RGB helper function
    double h_to_rgb(double m1, double m2, double h) {
      while (h < 0) h += 1;
      while (h > 1) h -= 1;
      if (h*6.0 < 1) return m1 + (m2 - m1)*h*6;
      if (h*2.0 < 1) return m2;
      if (h*3.0 < 2) return m1 + (m2 - m1) * (2.0/3.0 - h)*6;
      return m1;
    }

    Color_Ptr hsla_impl(double h, double s, double l, double a, Context& ctx, ParserState pstate)
    {
      h /= 360.0;
      s /= 100.0;
      l /= 100.0;

      if (l < 0) l = 0;
      if (s < 0) s = 0;
      if (l > 1) l = 1;
      if (s > 1) s = 1;
      while (h < 0) h += 1;
      while (h > 1) h -= 1;

      // if saturation is exacly zero, we loose
      // information for hue, since it will evaluate
      // to zero if converted back from rgb. Setting
      // saturation to a very tiny number solves this.
      if (s == 0) s = 1e-10;

      // Algorithm from the CSS3 spec: http://www.w3.org/TR/css3-color/#hsl-color.
      double m2;
      if (l <= 0.5) m2 = l*(s+1.0);
      else m2 = (l+s)-(l*s);
      double m1 = (l*2.0)-m2;
      // round the results -- consider moving this into the Color constructor
      double r = (h_to_rgb(m1, m2, h + 1.0/3.0) * 255.0);
      double g = (h_to_rgb(m1, m2, h) * 255.0);
      double b = (h_to_rgb(m1, m2, h - 1.0/3.0) * 255.0);

      return SASS_MEMORY_NEW(ctx.mem, Color, pstate, r, g, b, a);
    }

    Signature hsl_sig = "hsl($hue, $saturation, $lightness)";
    BUILT_IN(hsl)
    {
      return hsla_impl(ARG("$hue", Number)->value(),
                       ARG("$saturation", Number)->value(),
                       ARG("$lightness",  Number)->value(),
                       1.0,
                       ctx,
                       pstate);
    }

    Signature hsla_sig = "hsla($hue, $saturation, $lightness, $alpha)";
    BUILT_IN(hsla)
    {
      return hsla_impl(ARG("$hue", Number)->value(),
                       ARG("$saturation", Number)->value(),
                       ARG("$lightness",  Number)->value(),
                       ARG("$alpha",  Number)->value(),
                       ctx,
                       pstate);
    }

    Signature hue_sig = "hue($color)";
    BUILT_IN(hue)
    {
      Color_Ptr rgb_color = ARG("$color", Color);
      HSL hsl_color = rgb_to_hsl(rgb_color->r(),
                                 rgb_color->g(),
                                 rgb_color->b());
      return SASS_MEMORY_NEW(ctx.mem, Number, pstate, hsl_color.h, "deg");
    }

    Signature saturation_sig = "saturation($color)";
    BUILT_IN(saturation)
    {
      Color_Ptr rgb_color = ARG("$color", Color);
      HSL hsl_color = rgb_to_hsl(rgb_color->r(),
                                 rgb_color->g(),
                                 rgb_color->b());
      return SASS_MEMORY_NEW(ctx.mem, Number, pstate, hsl_color.s, "%");
    }

    Signature lightness_sig = "lightness($color)";
    BUILT_IN(lightness)
    {
      Color_Ptr rgb_color = ARG("$color", Color);
      HSL hsl_color = rgb_to_hsl(rgb_color->r(),
                                 rgb_color->g(),
                                 rgb_color->b());
      return SASS_MEMORY_NEW(ctx.mem, Number, pstate, hsl_color.l, "%");
    }

    Signature adjust_hue_sig = "adjust-hue($color, $degrees)";
    BUILT_IN(adjust_hue)
    {
      Color_Ptr rgb_color = ARG("$color", Color);
      Number_Ptr degrees = ARG("$degrees", Number);
      HSL hsl_color = rgb_to_hsl(rgb_color->r(),
                                 rgb_color->g(),
                                 rgb_color->b());
      return hsla_impl(hsl_color.h + degrees->value(),
                       hsl_color.s,
                       hsl_color.l,
                       rgb_color->a(),
                       ctx,
                       pstate);
    }

    Signature lighten_sig = "lighten($color, $amount)";
    BUILT_IN(lighten)
    {
      Color_Ptr rgb_color = ARG("$color", Color);
      Number_Ptr amount = ARGR("$amount", Number, 0, 100);
      HSL hsl_color = rgb_to_hsl(rgb_color->r(),
                                 rgb_color->g(),
                                 rgb_color->b());
      //Check lightness is not negative before lighten it
      double hslcolorL = hsl_color.l;
      if (hslcolorL < 0) {
        hslcolorL = 0;
      }

      return hsla_impl(hsl_color.h,
                       hsl_color.s,
                       hslcolorL + amount->value(),
                       rgb_color->a(),
                       ctx,
                       pstate);
    }

    Signature darken_sig = "darken($color, $amount)";
    BUILT_IN(darken)
    {
      Color_Ptr rgb_color = ARG("$color", Color);
      Number_Ptr amount = ARGR("$amount", Number, 0, 100);
      HSL hsl_color = rgb_to_hsl(rgb_color->r(),
                                 rgb_color->g(),
                                 rgb_color->b());

      //Check lightness if not over 100, before darken it
      double hslcolorL = hsl_color.l;
      if (hslcolorL > 100) {
        hslcolorL = 100;
      }

      return hsla_impl(hsl_color.h,
                       hsl_color.s,
                       hslcolorL - amount->value(),
                       rgb_color->a(),
                       ctx,
                       pstate);
    }

    Signature saturate_sig = "saturate($color, $amount: false)";
    BUILT_IN(saturate)
    {
      // CSS3 filter function overload: pass literal through directly
      Number_Ptr amount = dynamic_cast<Number_Ptr>(env["$amount"]);
      if (!amount) {
        return SASS_MEMORY_NEW(ctx.mem, String_Quoted, pstate, "saturate(" + env["$color"]->to_string(ctx.c_options) + ")");
      }

      ARGR("$amount", Number, 0, 100);
      Color_Ptr rgb_color = ARG("$color", Color);
      HSL hsl_color = rgb_to_hsl(rgb_color->r(),
                                 rgb_color->g(),
                                 rgb_color->b());

      double hslcolorS = hsl_color.s + amount->value();

      // Saturation cannot be below 0 or above 100
      if (hslcolorS < 0) {
        hslcolorS = 0;
      }
      if (hslcolorS > 100) {
        hslcolorS = 100;
      }

      return hsla_impl(hsl_color.h,
                       hslcolorS,
                       hsl_color.l,
                       rgb_color->a(),
                       ctx,
                       pstate);
    }

    Signature desaturate_sig = "desaturate($color, $amount)";
    BUILT_IN(desaturate)
    {
      Color_Ptr rgb_color = ARG("$color", Color);
      Number_Ptr amount = ARGR("$amount", Number, 0, 100);
      HSL hsl_color = rgb_to_hsl(rgb_color->r(),
                                 rgb_color->g(),
                                 rgb_color->b());

      double hslcolorS = hsl_color.s - amount->value();

      // Saturation cannot be below 0 or above 100
      if (hslcolorS <= 0) {
        hslcolorS = 0;
      }
      if (hslcolorS > 100) {
        hslcolorS = 100;
      }

      return hsla_impl(hsl_color.h,
                       hslcolorS,
                       hsl_color.l,
                       rgb_color->a(),
                       ctx,
                       pstate);
    }

    Signature grayscale_sig = "grayscale($color)";
    BUILT_IN(grayscale)
    {
      // CSS3 filter function overload: pass literal through directly
      Number_Ptr amount = dynamic_cast<Number_Ptr>(env["$color"]);
      if (amount) {
        return SASS_MEMORY_NEW(ctx.mem, String_Quoted, pstate, "grayscale(" + amount->to_string(ctx.c_options) + ")");
      }

      Color_Ptr rgb_color = ARG("$color", Color);
      HSL hsl_color = rgb_to_hsl(rgb_color->r(),
                                 rgb_color->g(),
                                 rgb_color->b());
      return hsla_impl(hsl_color.h,
                       0.0,
                       hsl_color.l,
                       rgb_color->a(),
                       ctx,
                       pstate);
    }

    Signature complement_sig = "complement($color)";
    BUILT_IN(complement)
    {
      Color_Ptr rgb_color = ARG("$color", Color);
      HSL hsl_color = rgb_to_hsl(rgb_color->r(),
                                 rgb_color->g(),
                                 rgb_color->b());
      return hsla_impl(hsl_color.h - 180.0,
                       hsl_color.s,
                       hsl_color.l,
                       rgb_color->a(),
                       ctx,
                       pstate);
    }

    Signature invert_sig = "invert($color)";
    BUILT_IN(invert)
    {
      // CSS3 filter function overload: pass literal through directly
      Number_Ptr amount = dynamic_cast<Number_Ptr>(env["$color"]);
      if (amount) {
        return SASS_MEMORY_NEW(ctx.mem, String_Quoted, pstate, "invert(" + amount->to_string(ctx.c_options) + ")");
      }

      Color_Ptr rgb_color = ARG("$color", Color);
      return SASS_MEMORY_NEW(ctx.mem, Color,
                             pstate,
                             255 - rgb_color->r(),
                             255 - rgb_color->g(),
                             255 - rgb_color->b(),
                             rgb_color->a());
    }

    ////////////////////
    // OPACITY FUNCTIONS
    ////////////////////
    Signature alpha_sig = "alpha($color)";
    Signature opacity_sig = "opacity($color)";
    BUILT_IN(alpha)
    {
      String_Constant_Ptr ie_kwd = dynamic_cast<String_Constant_Ptr>(env["$color"]);
      if (ie_kwd) {
        return SASS_MEMORY_NEW(ctx.mem, String_Quoted, pstate, "alpha(" + ie_kwd->value() + ")");
      }

      // CSS3 filter function overload: pass literal through directly
      Number_Ptr amount = dynamic_cast<Number_Ptr>(env["$color"]);
      if (amount) {
        return SASS_MEMORY_NEW(ctx.mem, String_Quoted, pstate, "opacity(" + amount->to_string(ctx.c_options) + ")");
      }

      return SASS_MEMORY_NEW(ctx.mem, Number, pstate, ARG("$color", Color)->a());
    }

    Signature opacify_sig = "opacify($color, $amount)";
    Signature fade_in_sig = "fade-in($color, $amount)";
    BUILT_IN(opacify)
    {
      Color_Ptr color = ARG("$color", Color);
      double amount = ARGR("$amount", Number, 0, 1)->value();
      double alpha = std::min(color->a() + amount, 1.0);
      return SASS_MEMORY_NEW(ctx.mem, Color,
                             pstate,
                             color->r(),
                             color->g(),
                             color->b(),
                             alpha);
    }

    Signature transparentize_sig = "transparentize($color, $amount)";
    Signature fade_out_sig = "fade-out($color, $amount)";
    BUILT_IN(transparentize)
    {
      Color_Ptr color = ARG("$color", Color);
      double amount = ARGR("$amount", Number, 0, 1)->value();
      double alpha = std::max(color->a() - amount, 0.0);
      return SASS_MEMORY_NEW(ctx.mem, Color,
                             pstate,
                             color->r(),
                             color->g(),
                             color->b(),
                             alpha);
    }

    ////////////////////////
    // OTHER COLOR FUNCTIONS
    ////////////////////////

    Signature adjust_color_sig = "adjust-color($color, $red: false, $green: false, $blue: false, $hue: false, $saturation: false, $lightness: false, $alpha: false)";
    BUILT_IN(adjust_color)
    {
      Color_Ptr color = ARG("$color", Color);
      Number_Ptr r = dynamic_cast<Number_Ptr>(env["$red"]);
      Number_Ptr g = dynamic_cast<Number_Ptr>(env["$green"]);
      Number_Ptr b = dynamic_cast<Number_Ptr>(env["$blue"]);
      Number_Ptr h = dynamic_cast<Number_Ptr>(env["$hue"]);
      Number_Ptr s = dynamic_cast<Number_Ptr>(env["$saturation"]);
      Number_Ptr l = dynamic_cast<Number_Ptr>(env["$lightness"]);
      Number_Ptr a = dynamic_cast<Number_Ptr>(env["$alpha"]);

      bool rgb = r || g || b;
      bool hsl = h || s || l;

      if (rgb && hsl) {
        error("cannot specify both RGB and HSL values for `adjust-color`", pstate);
      }
      if (rgb) {
        double rr = r ? ARGR("$red",   Number, -255, 255)->value() : 0;
        double gg = g ? ARGR("$green", Number, -255, 255)->value() : 0;
        double bb = b ? ARGR("$blue",  Number, -255, 255)->value() : 0;
        double aa = a ? ARGR("$alpha", Number, -1, 1)->value() : 0;
        return SASS_MEMORY_NEW(ctx.mem, Color,
                               pstate,
                               color->r() + rr,
                               color->g() + gg,
                               color->b() + bb,
                               color->a() + aa);
      }
      if (hsl) {
        HSL hsl_struct = rgb_to_hsl(color->r(), color->g(), color->b());
        double ss = s ? ARGR("$saturation", Number, -100, 100)->value() : 0;
        double ll = l ? ARGR("$lightness",  Number, -100, 100)->value() : 0;
        double aa = a ? ARGR("$alpha",      Number, -1, 1)->value() : 0;
        return hsla_impl(hsl_struct.h + (h ? h->value() : 0),
                         hsl_struct.s + ss,
                         hsl_struct.l + ll,
                         color->a() + aa,
                         ctx,
                         pstate);
      }
      if (a) {
        return SASS_MEMORY_NEW(ctx.mem, Color,
                               pstate,
                               color->r(),
                               color->g(),
                               color->b(),
                               color->a() + (a ? a->value() : 0));
      }
      error("not enough arguments for `adjust-color`", pstate);
      // unreachable
      return color;
    }

    Signature scale_color_sig = "scale-color($color, $red: false, $green: false, $blue: false, $hue: false, $saturation: false, $lightness: false, $alpha: false)";
    BUILT_IN(scale_color)
    {
      Color_Ptr color = ARG("$color", Color);
      Number_Ptr r = dynamic_cast<Number_Ptr>(env["$red"]);
      Number_Ptr g = dynamic_cast<Number_Ptr>(env["$green"]);
      Number_Ptr b = dynamic_cast<Number_Ptr>(env["$blue"]);
      Number_Ptr h = dynamic_cast<Number_Ptr>(env["$hue"]);
      Number_Ptr s = dynamic_cast<Number_Ptr>(env["$saturation"]);
      Number_Ptr l = dynamic_cast<Number_Ptr>(env["$lightness"]);
      Number_Ptr a = dynamic_cast<Number_Ptr>(env["$alpha"]);

      bool rgb = r || g || b;
      bool hsl = h || s || l;

      if (rgb && hsl) {
        error("cannot specify both RGB and HSL values for `scale-color`", pstate);
      }
      if (rgb) {
        double rscale = (r ? ARGR("$red",   Number, -100.0, 100.0)->value() : 0.0) / 100.0;
        double gscale = (g ? ARGR("$green", Number, -100.0, 100.0)->value() : 0.0) / 100.0;
        double bscale = (b ? ARGR("$blue",  Number, -100.0, 100.0)->value() : 0.0) / 100.0;
        double ascale = (a ? ARGR("$alpha", Number, -100.0, 100.0)->value() : 0.0) / 100.0;
        return SASS_MEMORY_NEW(ctx.mem, Color,
                               pstate,
                               color->r() + rscale * (rscale > 0.0 ? 255 - color->r() : color->r()),
                               color->g() + gscale * (gscale > 0.0 ? 255 - color->g() : color->g()),
                               color->b() + bscale * (bscale > 0.0 ? 255 - color->b() : color->b()),
                               color->a() + ascale * (ascale > 0.0 ? 1.0 - color->a() : color->a()));
      }
      if (hsl) {
        double hscale = (h ? ARGR("$hue",        Number, -100.0, 100.0)->value() : 0.0) / 100.0;
        double sscale = (s ? ARGR("$saturation", Number, -100.0, 100.0)->value() : 0.0) / 100.0;
        double lscale = (l ? ARGR("$lightness",  Number, -100.0, 100.0)->value() : 0.0) / 100.0;
        double ascale = (a ? ARGR("$alpha",      Number, -100.0, 100.0)->value() : 0.0) / 100.0;
        HSL hsl_struct = rgb_to_hsl(color->r(), color->g(), color->b());
        hsl_struct.h += hscale * (hscale > 0.0 ? 360.0 - hsl_struct.h : hsl_struct.h);
        hsl_struct.s += sscale * (sscale > 0.0 ? 100.0 - hsl_struct.s : hsl_struct.s);
        hsl_struct.l += lscale * (lscale > 0.0 ? 100.0 - hsl_struct.l : hsl_struct.l);
        double alpha = color->a() + ascale * (ascale > 0.0 ? 1.0 - color->a() : color->a());
        return hsla_impl(hsl_struct.h, hsl_struct.s, hsl_struct.l, alpha, ctx, pstate);
      }
      if (a) {
        double ascale = (a ? ARGR("$alpha", Number, -100.0, 100.0)->value() : 0.0) / 100.0;
        return SASS_MEMORY_NEW(ctx.mem, Color,
                               pstate,
                               color->r(),
                               color->g(),
                               color->b(),
                               color->a() + ascale * (ascale > 0.0 ? 1.0 - color->a() : color->a()));
      }
      error("not enough arguments for `scale-color`", pstate);
      // unreachable
      return color;
    }

    Signature change_color_sig = "change-color($color, $red: false, $green: false, $blue: false, $hue: false, $saturation: false, $lightness: false, $alpha: false)";
    BUILT_IN(change_color)
    {
      Color_Ptr color = ARG("$color", Color);
      Number_Ptr r = dynamic_cast<Number_Ptr>(env["$red"]);
      Number_Ptr g = dynamic_cast<Number_Ptr>(env["$green"]);
      Number_Ptr b = dynamic_cast<Number_Ptr>(env["$blue"]);
      Number_Ptr h = dynamic_cast<Number_Ptr>(env["$hue"]);
      Number_Ptr s = dynamic_cast<Number_Ptr>(env["$saturation"]);
      Number_Ptr l = dynamic_cast<Number_Ptr>(env["$lightness"]);
      Number_Ptr a = dynamic_cast<Number_Ptr>(env["$alpha"]);

      bool rgb = r || g || b;
      bool hsl = h || s || l;

      if (rgb && hsl) {
        error("cannot specify both RGB and HSL values for `change-color`", pstate);
      }
      if (rgb) {
        return SASS_MEMORY_NEW(ctx.mem, Color,
                               pstate,
                               r ? ARGR("$red",   Number, 0, 255)->value() : color->r(),
                               g ? ARGR("$green", Number, 0, 255)->value() : color->g(),
                               b ? ARGR("$blue",  Number, 0, 255)->value() : color->b(),
                               a ? ARGR("$alpha", Number, 0, 255)->value() : color->a());
      }
      if (hsl) {
        HSL hsl_struct = rgb_to_hsl(color->r(), color->g(), color->b());
        if (h) hsl_struct.h = std::fmod(h->value(), 360.0);
        if (s) hsl_struct.s = ARGR("$saturation", Number, 0, 100)->value();
        if (l) hsl_struct.l = ARGR("$lightness",  Number, 0, 100)->value();
        double alpha = a ? ARGR("$alpha", Number, 0, 1.0)->value() : color->a();
        return hsla_impl(hsl_struct.h, hsl_struct.s, hsl_struct.l, alpha, ctx, pstate);
      }
      if (a) {
        double alpha = a ? ARGR("$alpha", Number, 0, 1.0)->value() : color->a();
        return SASS_MEMORY_NEW(ctx.mem, Color,
                               pstate,
                               color->r(),
                               color->g(),
                               color->b(),
                               alpha);
      }
      error("not enough arguments for `change-color`", pstate);
      // unreachable
      return color;
    }

    template <size_t range>
    static double cap_channel(double c) {
      if      (c > range) return range;
      else if (c < 0)     return 0;
      else                return c;
    }

    Signature ie_hex_str_sig = "ie-hex-str($color)";
    BUILT_IN(ie_hex_str)
    {
      Color_Ptr c = ARG("$color", Color);
      double r = cap_channel<0xff>(c->r());
      double g = cap_channel<0xff>(c->g());
      double b = cap_channel<0xff>(c->b());
      double a = cap_channel<1>   (c->a()) * 255;

      std::stringstream ss;
      ss << '#' << std::setw(2) << std::setfill('0');
      ss << std::hex << std::setw(2) << static_cast<unsigned long>(Sass::round(a, ctx.c_options.precision));
      ss << std::hex << std::setw(2) << static_cast<unsigned long>(Sass::round(r, ctx.c_options.precision));
      ss << std::hex << std::setw(2) << static_cast<unsigned long>(Sass::round(g, ctx.c_options.precision));
      ss << std::hex << std::setw(2) << static_cast<unsigned long>(Sass::round(b, ctx.c_options.precision));

      std::string result(ss.str());
      for (size_t i = 0, L = result.length(); i < L; ++i) {
        result[i] = std::toupper(result[i]);
      }
      return SASS_MEMORY_NEW(ctx.mem, String_Quoted, pstate, result);
    }

    ///////////////////
    // STRING FUNCTIONS
    ///////////////////

    Signature unquote_sig = "unquote($string)";
    BUILT_IN(sass_unquote)
    {
      AST_Node_Ptr arg = env["$string"];
      if (String_Quoted_Ptr string_quoted = dynamic_cast<String_Quoted_Ptr>(arg)) {
        String_Constant_Ptr result = SASS_MEMORY_NEW(ctx.mem, String_Constant, pstate, string_quoted->value());
        // remember if the string was quoted (color tokens)
        result->is_delayed(true); // delay colors
        return result;
      }
      else if (dynamic_cast<String_Constant_Ptr>(arg)) {
        return (Expression_Ptr) arg;
      }
      else {
        Sass_Output_Style oldstyle = ctx.c_options.output_style;
        ctx.c_options.output_style = SASS_STYLE_NESTED;
        std::string val(arg->to_string(ctx.c_options));
        val = dynamic_cast<Null_Ptr>(arg) ? "null" : val;
        ctx.c_options.output_style = oldstyle;

        deprecated_function("Passing " + val + ", a non-string value, to unquote()", pstate);
        return (Expression_Ptr) arg;
      }
    }

    Signature quote_sig = "quote($string)";
    BUILT_IN(sass_quote)
    {
      AST_Node_Ptr arg = env["$string"];
      // only set quote mark to true if already a string
      if (String_Quoted_Ptr qstr = dynamic_cast<String_Quoted_Ptr>(arg)) {
        qstr->quote_mark('*');
        return qstr;
      }
      // all other nodes must be converted to a string node
      std::string str(quote(arg->to_string(ctx.c_options), String_Constant::double_quote()));
      String_Quoted_Ptr result = SASS_MEMORY_NEW(ctx.mem, String_Quoted, pstate, str);
      result->quote_mark('*');
      return result;
    }


    Signature str_length_sig = "str-length($string)";
    BUILT_IN(str_length)
    {
      size_t len = std::string::npos;
      try {
        String_Constant_Ptr s = ARG("$string", String_Constant);
        len = UTF_8::code_point_count(s->value(), 0, s->value().size());

      }
      // handle any invalid utf8 errors
      // other errors will be re-thrown
      catch (...) { handle_utf8_error(pstate, backtrace); }
      // return something even if we had an error (-1)
      return SASS_MEMORY_NEW(ctx.mem, Number, pstate, (double)len);
    }

    Signature str_insert_sig = "str-insert($string, $insert, $index)";
    BUILT_IN(str_insert)
    {
      std::string str;
      try {
        String_Constant_Ptr s = ARG("$string", String_Constant);
        str = s->value();
        str = unquote(str);
        String_Constant_Ptr i = ARG("$insert", String_Constant);
        std::string ins = i->value();
        ins = unquote(ins);
        Number_Ptr ind = ARG("$index", Number);
        double index = ind->value();
        size_t len = UTF_8::code_point_count(str, 0, str.size());

        if (index > 0 && index <= len) {
          // positive and within string length
          str.insert(UTF_8::offset_at_position(str, static_cast<size_t>(index) - 1), ins);
        }
        else if (index > len) {
          // positive and past string length
          str += ins;
        }
        else if (index == 0) {
          str = ins + str;
        }
        else if (std::abs(index) <= len) {
          // negative and within string length
          index += len + 1;
          str.insert(UTF_8::offset_at_position(str, static_cast<size_t>(index)), ins);
        }
        else {
          // negative and past string length
          str = ins + str;
        }

        if (String_Quoted_Ptr ss = dynamic_cast<String_Quoted_Ptr>(s)) {
          if (ss->quote_mark()) str = quote(str);
        }
      }
      // handle any invalid utf8 errors
      // other errors will be re-thrown
      catch (...) { handle_utf8_error(pstate, backtrace); }
      return SASS_MEMORY_NEW(ctx.mem, String_Quoted, pstate, str);
    }

    Signature str_index_sig = "str-index($string, $substring)";
    BUILT_IN(str_index)
    {
      size_t index = std::string::npos;
      try {
        String_Constant_Ptr s = ARG("$string", String_Constant);
        String_Constant_Ptr t = ARG("$substring", String_Constant);
        std::string str = s->value();
        str = unquote(str);
        std::string substr = t->value();
        substr = unquote(substr);

        size_t c_index = str.find(substr);
        if(c_index == std::string::npos) {
          return SASS_MEMORY_NEW(ctx.mem, Null, pstate);
        }
        index = UTF_8::code_point_count(str, 0, c_index) + 1;
      }
      // handle any invalid utf8 errors
      // other errors will be re-thrown
      catch (...) { handle_utf8_error(pstate, backtrace); }
      // return something even if we had an error (-1)
      return SASS_MEMORY_NEW(ctx.mem, Number, pstate, (double)index);
    }

    Signature str_slice_sig = "str-slice($string, $start-at, $end-at:-1)";
    BUILT_IN(str_slice)
    {
      std::string newstr;
      try {
        String_Constant_Ptr s = ARG("$string", String_Constant);
        double start_at = ARG("$start-at", Number)->value();
        double end_at = ARG("$end-at", Number)->value();

        std::string str = unquote(s->value());

        size_t size = utf8::distance(str.begin(), str.end());
        if (end_at <= size * -1.0 && size > 1) {
          end_at += size;
          if (end_at == 0) end_at = 1;
        }
        if (end_at < 0) {
          end_at += size + 1;
          if (end_at == 0) end_at = 1;
        }
        if (end_at > size) { end_at = (double)size; }
        if (start_at < 0) { start_at += size + 1; }
        else if (start_at == 0) { ++ start_at; }

        if (start_at <= end_at)
        {
          std::string::iterator start = str.begin();
          utf8::advance(start, start_at - 1, str.end());
          std::string::iterator end = start;
          utf8::advance(end, end_at - start_at + 1, str.end());
          newstr = std::string(start, end);
        }
        if (String_Quoted_Ptr ss = dynamic_cast<String_Quoted_Ptr>(s)) {
          if(ss->quote_mark()) newstr = quote(newstr);
        }
      }
      // handle any invalid utf8 errors
      // other errors will be re-thrown
      catch (...) { handle_utf8_error(pstate, backtrace); }
      return SASS_MEMORY_NEW(ctx.mem, String_Quoted, pstate, newstr);
    }

    Signature to_upper_case_sig = "to-upper-case($string)";
    BUILT_IN(to_upper_case)
    {
      String_Constant_Ptr s = ARG("$string", String_Constant);
      std::string str = s->value();

      for (size_t i = 0, L = str.length(); i < L; ++i) {
        if (Sass::Util::isAscii(str[i])) {
          str[i] = std::toupper(str[i]);
        }
      }

      if (String_Quoted_Ptr ss = dynamic_cast<String_Quoted_Ptr>(s)) {
        String_Quoted_Ptr cpy = SASS_MEMORY_NEW(ctx.mem, String_Quoted, *ss);
        cpy->value(str);
        return cpy;
      } else {
        return SASS_MEMORY_NEW(ctx.mem, String_Quoted, pstate, str);
      }
    }

    Signature to_lower_case_sig = "to-lower-case($string)";
    BUILT_IN(to_lower_case)
    {
      String_Constant_Ptr s = ARG("$string", String_Constant);
      std::string str = s->value();

      for (size_t i = 0, L = str.length(); i < L; ++i) {
        if (Sass::Util::isAscii(str[i])) {
          str[i] = std::tolower(str[i]);
        }
      }

      if (String_Quoted_Ptr ss = dynamic_cast<String_Quoted_Ptr>(s)) {
        String_Quoted_Ptr cpy = SASS_MEMORY_NEW(ctx.mem, String_Quoted, *ss);
        cpy->value(str);
        return cpy;
      } else {
        return SASS_MEMORY_NEW(ctx.mem, String_Quoted, pstate, str);
      }
    }

    ///////////////////
    // NUMBER FUNCTIONS
    ///////////////////

    Signature percentage_sig = "percentage($number)";
    BUILT_IN(percentage)
    {
      Number_Ptr n = ARG("$number", Number);
      if (!n->is_unitless()) error("argument $number of `" + std::string(sig) + "` must be unitless", pstate);
      return SASS_MEMORY_NEW(ctx.mem, Number, pstate, n->value() * 100, "%");
    }

    Signature round_sig = "round($number)";
    BUILT_IN(round)
    {
      Number_Ptr n = ARG("$number", Number);
      Number_Ptr r = SASS_MEMORY_NEW(ctx.mem, Number, *n);
      r->pstate(pstate);
      r->value(Sass::round(r->value(), ctx.c_options.precision));
      return r;
    }

    Signature ceil_sig = "ceil($number)";
    BUILT_IN(ceil)
    {
      Number_Ptr n = ARG("$number", Number);
      Number_Ptr r = SASS_MEMORY_NEW(ctx.mem, Number, *n);
      r->pstate(pstate);
      r->value(std::ceil(r->value()));
      return r;
    }

    Signature floor_sig = "floor($number)";
    BUILT_IN(floor)
    {
      Number_Ptr n = ARG("$number", Number);
      Number_Ptr r = SASS_MEMORY_NEW(ctx.mem, Number, *n);
      r->pstate(pstate);
      r->value(std::floor(r->value()));
      return r;
    }

    Signature abs_sig = "abs($number)";
    BUILT_IN(abs)
    {
      Number_Ptr n = ARG("$number", Number);
      Number_Ptr r = SASS_MEMORY_NEW(ctx.mem, Number, *n);
      r->pstate(pstate);
      r->value(std::abs(r->value()));
      return r;
    }

    Signature min_sig = "min($numbers...)";
    BUILT_IN(min)
    {
      List_Ptr arglist = ARG("$numbers", List);
      Number_Ptr least = 0;
      for (size_t i = 0, L = arglist->length(); i < L; ++i) {
        Expression_Ptr val = arglist->value_at_index(i);
        Number_Ptr xi = dynamic_cast<Number_Ptr>(val);
        if (!xi) {
          error("\"" + val->to_string(ctx.c_options) + "\" is not a number for `min'", pstate);
        }
        if (least) {
          if (*xi < *least) least = xi;
        } else least = xi;
      }
      return least;
    }

    Signature max_sig = "max($numbers...)";
    BUILT_IN(max)
    {
      List_Ptr arglist = ARG("$numbers", List);
      Number_Ptr greatest = 0;
      for (size_t i = 0, L = arglist->length(); i < L; ++i) {
        Expression_Ptr val = arglist->value_at_index(i);
        Number_Ptr xi = dynamic_cast<Number_Ptr>(val);
        if (!xi) {
          error("\"" + val->to_string(ctx.c_options) + "\" is not a number for `max'", pstate);
        }
        if (greatest) {
          if (*greatest < *xi) greatest = xi;
        } else greatest = xi;
      }
      return greatest;
    }

    Signature random_sig = "random($limit:false)";
    BUILT_IN(random)
    {
      AST_Node_Ptr arg = env["$limit"];
      Value_Ptr v = dynamic_cast<Value_Ptr>(arg);
      Number_Ptr l = dynamic_cast<Number_Ptr>(arg);
      Boolean_Ptr b = dynamic_cast<Boolean_Ptr>(arg);
      if (l) {
        double v = l->value();
        if (v < 1) {
          stringstream err;
          err << "$limit " << v << " must be greater than or equal to 1 for `random`";
          error(err.str(), pstate);
        }
        bool eq_int = std::fabs(trunc(v) - v) < NUMBER_EPSILON;
        if (!eq_int) {
          stringstream err;
          err << "Expected $limit to be an integer but got `" << v << "` for `random`";
          error(err.str(), pstate);
        }
        std::uniform_real_distribution<> distributor(1, v + 1);
        uint_fast32_t distributed = static_cast<uint_fast32_t>(distributor(rand));
        return SASS_MEMORY_NEW(ctx.mem, Number, pstate, (double)distributed);
      }
      else if (b) {
        std::uniform_real_distribution<> distributor(0, 1);
        double distributed = static_cast<double>(distributor(rand));
        return SASS_MEMORY_NEW(ctx.mem, Number, pstate, distributed);
      } else if (v) {
        throw Exception::InvalidArgumentType(pstate, "random", "$limit", "number", v);
      } else {
        throw Exception::InvalidArgumentType(pstate, "random", "$limit", "number");
      }
      return 0;
    }

    /////////////////
    // LIST FUNCTIONS
    /////////////////

    Signature length_sig = "length($list)";
    BUILT_IN(length)
    {
      if (CommaSequence_Selector_Ptr sl = dynamic_cast<CommaSequence_Selector_Ptr>(env["$list"])) {
        return SASS_MEMORY_NEW(ctx.mem, Number, pstate, (double)sl->length());
      }
      Expression_Ptr v = ARG("$list", Expression);
      if (v->concrete_type() == Expression::MAP) {
        Map_Ptr map = dynamic_cast<Map_Ptr>(env["$list"]);
        return SASS_MEMORY_NEW(ctx.mem, Number, pstate, (double)(map ? map->length() : 1));
      }
      if (v->concrete_type() == Expression::SELECTOR) {
        if (SimpleSequence_Selector_Ptr h = dynamic_cast<SimpleSequence_Selector_Ptr>(v)) {
          return SASS_MEMORY_NEW(ctx.mem, Number, pstate, (double)h->length());
        } else if (CommaSequence_Selector_Ptr ls = dynamic_cast<CommaSequence_Selector_Ptr>(v)) {
          return SASS_MEMORY_NEW(ctx.mem, Number, pstate, (double)ls->length());
        } else {
          return SASS_MEMORY_NEW(ctx.mem, Number, pstate, 1);
        }
      }

      List_Ptr list = dynamic_cast<List_Ptr>(env["$list"]);
      return SASS_MEMORY_NEW(ctx.mem, Number,
                             pstate,
                             (double)(list ? list->size() : 1));
    }

    Signature nth_sig = "nth($list, $n)";
    BUILT_IN(nth)
    {
      Number_Ptr n = ARG("$n", Number);
      Map_Ptr m = dynamic_cast<Map_Ptr>(env["$list"]);
      if (CommaSequence_Selector_Ptr sl = dynamic_cast<CommaSequence_Selector_Ptr>(env["$list"])) {
        size_t len = m ? m->length() : sl->length();
        bool empty = m ? m->empty() : sl->empty();
        if (empty) error("argument `$list` of `" + std::string(sig) + "` must not be empty", pstate);
        double index = std::floor(n->value() < 0 ? len + n->value() : n->value() - 1);
        if (index < 0 || index > len - 1) error("index out of bounds for `" + std::string(sig) + "`", pstate);
        // return (*sl)[static_cast<int>(index)];
        Listize listize(ctx.mem);
        return (*sl)[static_cast<int>(index)]->perform(&listize);
      }
      List_Ptr l = dynamic_cast<List_Ptr>(env["$list"]);
      if (n->value() == 0) error("argument `$n` of `" + std::string(sig) + "` must be non-zero", pstate);
      // if the argument isn't a list, then wrap it in a singleton list
      if (!m && !l) {
        l = SASS_MEMORY_NEW(ctx.mem, List, pstate, 1);
        *l << ARG("$list", Expression);
      }
      size_t len = m ? m->length() : l->length();
      bool empty = m ? m->empty() : l->empty();
      if (empty) error("argument `$list` of `" + std::string(sig) + "` must not be empty", pstate);
      double index = std::floor(n->value() < 0 ? len + n->value() : n->value() - 1);
      if (index < 0 || index > len - 1) error("index out of bounds for `" + std::string(sig) + "`", pstate);

      if (m) {
        l = SASS_MEMORY_NEW(ctx.mem, List, pstate, 1);
        *l << m->keys()[static_cast<unsigned int>(index)];
        *l << m->at(m->keys()[static_cast<unsigned int>(index)]);
        return l;
      }
      else {
        Expression_Ptr rv = l->value_at_index(static_cast<int>(index));
        rv->set_delayed(false);
        return rv;
      }
    }

    Signature set_nth_sig = "set-nth($list, $n, $value)";
    BUILT_IN(set_nth)
    {
      Map_Ptr m = dynamic_cast<Map_Ptr>(env["$list"]);
      List_Ptr l = dynamic_cast<List_Ptr>(env["$list"]);
      Number_Ptr n = ARG("$n", Number);
      Expression_Ptr v = ARG("$value", Expression);
      if (!l) {
        l = SASS_MEMORY_NEW(ctx.mem, List, pstate, 1);
        *l << ARG("$list", Expression);
      }
      if (m) {
        l = m->to_list(ctx, pstate);
      }
      if (l->empty()) error("argument `$list` of `" + std::string(sig) + "` must not be empty", pstate);
      double index = std::floor(n->value() < 0 ? l->length() + n->value() : n->value() - 1);
      if (index < 0 || index > l->length() - 1) error("index out of bounds for `" + std::string(sig) + "`", pstate);
      List_Ptr result = SASS_MEMORY_NEW(ctx.mem, List, pstate, l->length(), l->separator());
      for (size_t i = 0, L = l->length(); i < L; ++i) {
        *result << ((i == index) ? v : (*l)[i]);
      }
      return result;
    }

    Signature index_sig = "index($list, $value)";
    BUILT_IN(index)
    {
      Map_Ptr m = dynamic_cast<Map_Ptr>(env["$list"]);
      List_Ptr l = dynamic_cast<List_Ptr>(env["$list"]);
      Expression_Ptr v = ARG("$value", Expression);
      if (!l) {
        l = SASS_MEMORY_NEW(ctx.mem, List, pstate, 1);
        *l << ARG("$list", Expression);
      }
      if (m) {
        l = m->to_list(ctx, pstate);
      }
      for (size_t i = 0, L = l->length(); i < L; ++i) {
        if (Eval::eq(l->value_at_index(i), v)) return SASS_MEMORY_NEW(ctx.mem, Number, pstate, (double)(i+1));
      }
      return SASS_MEMORY_NEW(ctx.mem, Null, pstate);
    }

    Signature join_sig = "join($list1, $list2, $separator: auto)";
    BUILT_IN(join)
    {
      Map_Ptr m1 = dynamic_cast<Map_Ptr>(env["$list1"]);
      Map_Ptr m2 = dynamic_cast<Map_Ptr>(env["$list2"]);
      List_Ptr l1 = dynamic_cast<List_Ptr>(env["$list1"]);
      List_Ptr l2 = dynamic_cast<List_Ptr>(env["$list2"]);
      String_Constant_Ptr sep = ARG("$separator", String_Constant);
      enum Sass_Separator sep_val = (l1 ? l1->separator() : SASS_SPACE);
      if (!l1) {
        l1 = SASS_MEMORY_NEW(ctx.mem, List, pstate, 1);
        *l1 << ARG("$list1", Expression);
        sep_val = (l2 ? l2->separator() : SASS_SPACE);
      }
      if (!l2) {
        l2 = SASS_MEMORY_NEW(ctx.mem, List, pstate, 1);
        *l2 << ARG("$list2", Expression);
      }
      if (m1) {
        l1 = m1->to_list(ctx, pstate);
        sep_val = SASS_COMMA;
      }
      if (m2) {
        l2 = m2->to_list(ctx, pstate);
      }
      size_t len = l1->length() + l2->length();
      std::string sep_str = unquote(sep->value());
      if (sep_str == "space") sep_val = SASS_SPACE;
      else if (sep_str == "comma") sep_val = SASS_COMMA;
      else if (sep_str != "auto") error("argument `$separator` of `" + std::string(sig) + "` must be `space`, `comma`, or `auto`", pstate);
      List_Ptr result = SASS_MEMORY_NEW(ctx.mem, List, pstate, len, sep_val);
      *result += l1;
      *result += l2;
      return result;
    }

    Signature append_sig = "append($list, $val, $separator: auto)";
    BUILT_IN(append)
    {
      Map_Ptr m = dynamic_cast<Map_Ptr>(env["$list"]);
      List_Ptr l = dynamic_cast<List_Ptr>(env["$list"]);
      Expression_Ptr v = ARG("$val", Expression);
      if (CommaSequence_Selector_Ptr sl = dynamic_cast<CommaSequence_Selector_Ptr>(env["$list"])) {
        Listize listize(ctx.mem);
        l = dynamic_cast<List_Ptr>(sl->perform(&listize));
      }
      String_Constant_Ptr sep = ARG("$separator", String_Constant);
      if (!l) {
        l = SASS_MEMORY_NEW(ctx.mem, List, pstate, 1);
        *l << ARG("$list", Expression);
      }
      if (m) {
        l = m->to_list(ctx, pstate);
      }
      List_Ptr result = SASS_MEMORY_NEW(ctx.mem, List, pstate, l->length() + 1, l->separator());
      std::string sep_str(unquote(sep->value()));
      if (sep_str == "space") result->separator(SASS_SPACE);
      else if (sep_str == "comma") result->separator(SASS_COMMA);
      else if (sep_str != "auto") error("argument `$separator` of `" + std::string(sig) + "` must be `space`, `comma`, or `auto`", pstate);
      *result += l;
      bool is_arglist = l->is_arglist();
      result->is_arglist(is_arglist);
      if (is_arglist) {
        *result << SASS_MEMORY_NEW(ctx.mem, Argument,
                                   v->pstate(),
                                   v,
                                   "",
                                   false,
                                   false);

      } else {
        *result << v;
      }
      return result;
    }

    Signature zip_sig = "zip($lists...)";
    BUILT_IN(zip)
    {
      List_Obj arglist = SASS_MEMORY_NEW(ctx.mem, List, *ARG("$lists", List));
      size_t shortest = 0;
      for (size_t i = 0, L = arglist->length(); i < L; ++i) {
        List_Obj ith = SASS_MEMORY_CAST_PTR(List, arglist->value_at_index(i));
        Map_Obj mith = SASS_MEMORY_CAST_PTR(Map, arglist->value_at_index(i));
        if (!ith) {
          if (mith) {
            ith = mith->to_list(ctx, pstate);
          } else {
            ith = SASS_MEMORY_NEW(ctx.mem, List, pstate, 1);
            ith->append(arglist->value_at_index(i));
          }
          if (arglist->is_arglist()) {
            Argument_Obj arg = (Argument_Ptr)(&arglist->at(i));
            arg->value(&ith);
          } else {
            (*arglist)[i] = &ith;
          }
        }
        shortest = (i ? std::min(shortest, ith->length()) : ith->length());
      }
      List_Ptr zippers = SASS_MEMORY_NEW(ctx.mem, List, pstate, shortest, SASS_COMMA);
      size_t L = arglist->length();
      for (size_t i = 0; i < shortest; ++i) {
        List_Ptr zipper = SASS_MEMORY_NEW(ctx.mem, List, pstate, L);
        for (size_t j = 0; j < L; ++j) {
          *zipper << (*static_cast<List_Ptr>(arglist->value_at_index(j)))[i];
        }
        *zippers << zipper;
      }
      return zippers;
    }

    Signature list_separator_sig = "list_separator($list)";
    BUILT_IN(list_separator)
    {
      List_Ptr l = dynamic_cast<List_Ptr>(env["$list"]);
      if (!l) {
        l = SASS_MEMORY_NEW(ctx.mem, List, pstate, 1);
        *l << ARG("$list", Expression);
      }
      return SASS_MEMORY_NEW(ctx.mem, String_Quoted,
                               pstate,
                               l->separator() == SASS_COMMA ? "comma" : "space");
    }

    /////////////////
    // MAP FUNCTIONS
    /////////////////

    Signature map_get_sig = "map-get($map, $key)";
    BUILT_IN(map_get)
    {
      Map_Ptr m = ARGM("$map", Map, ctx);
      Expression_Ptr v = ARG("$key", Expression);
      try {
        return m->at(v);
      } catch (const std::out_of_range&) {
        return SASS_MEMORY_NEW(ctx.mem, Null, pstate);
      }
      catch (...) { throw; }
    }

    Signature map_has_key_sig = "map-has-key($map, $key)";
    BUILT_IN(map_has_key)
    {
      Map_Ptr m = ARGM("$map", Map, ctx);
      Expression_Ptr v = ARG("$key", Expression);
      return SASS_MEMORY_NEW(ctx.mem, Boolean, pstate, m->has(v));
    }

    Signature map_keys_sig = "map-keys($map)";
    BUILT_IN(map_keys)
    {
      Map_Ptr m = ARGM("$map", Map, ctx);
      List_Ptr result = SASS_MEMORY_NEW(ctx.mem, List, pstate, m->length(), SASS_COMMA);
      for ( auto key : m->keys()) {
        *result << key;
      }
      return result;
    }

    Signature map_values_sig = "map-values($map)";
    BUILT_IN(map_values)
    {
      Map_Ptr m = ARGM("$map", Map, ctx);
      List_Ptr result = SASS_MEMORY_NEW(ctx.mem, List, pstate, m->length(), SASS_COMMA);
      for ( auto key : m->keys()) {
        *result << m->at(key);
      }
      return result;
    }

    Signature map_merge_sig = "map-merge($map1, $map2)";
    BUILT_IN(map_merge)
    {
      Map_Ptr m1 = ARGM("$map1", Map, ctx);
      Map_Ptr m2 = ARGM("$map2", Map, ctx);

      size_t len = m1->length() + m2->length();
      Map_Ptr result = SASS_MEMORY_NEW(ctx.mem, Map, pstate, len);
      *result += m1;
      *result += m2;
      return result;
    }

    Signature map_remove_sig = "map-remove($map, $keys...)";
    BUILT_IN(map_remove)
    {
      bool remove;
      Map_Ptr m = ARGM("$map", Map, ctx);
      List_Ptr arglist = ARG("$keys", List);
      Map_Ptr result = SASS_MEMORY_NEW(ctx.mem, Map, pstate, 1);
      for (auto key : m->keys()) {
        remove = false;
        for (size_t j = 0, K = arglist->length(); j < K && !remove; ++j) {
          remove = Eval::eq(key, arglist->value_at_index(j));
        }
        if (!remove) *result << std::make_pair(key, m->at(key));
      }
      return result;
    }

    Signature keywords_sig = "keywords($args)";
    BUILT_IN(keywords)
    {
      List_Ptr arglist = SASS_MEMORY_NEW(ctx.mem, List, *ARG("$args", List));
      Map_Ptr result = SASS_MEMORY_NEW(ctx.mem, Map, pstate, 1);
      for (size_t i = arglist->size(), L = arglist->length(); i < L; ++i) {
        Expression_Obj obj = arglist->at(i);
        Argument_Obj arg = (Argument_Ptr)&obj;
        std::string name = std::string(arg->name());
        name = name.erase(0, 1); // sanitize name (remove dollar sign)
        *result << std::make_pair(SASS_MEMORY_NEW(ctx.mem, String_Quoted,
                 pstate, name),
                 arg->value());
      }
      return result;
    }

    //////////////////////////
    // INTROSPECTION FUNCTIONS
    //////////////////////////

    Signature type_of_sig = "type-of($value)";
    BUILT_IN(type_of)
    {
      Expression_Ptr v = ARG("$value", Expression);
      return SASS_MEMORY_NEW(ctx.mem, String_Quoted, pstate, v->type());
    }

    Signature unit_sig = "unit($number)";
    BUILT_IN(unit)
    { return SASS_MEMORY_NEW(ctx.mem, String_Quoted, pstate, quote(ARG("$number", Number)->unit(), '"')); }

    Signature unitless_sig = "unitless($number)";
    BUILT_IN(unitless)
    { return SASS_MEMORY_NEW(ctx.mem, Boolean, pstate, ARG("$number", Number)->is_unitless()); }

    Signature comparable_sig = "comparable($number-1, $number-2)";
    BUILT_IN(comparable)
    {
      Number_Ptr n1 = ARG("$number-1", Number);
      Number_Ptr n2 = ARG("$number-2", Number);
      if (n1->is_unitless() || n2->is_unitless()) {
        return SASS_MEMORY_NEW(ctx.mem, Boolean, pstate, true);
      }
      Number tmp_n2(*n2);
      tmp_n2.normalize(n1->find_convertible_unit());
      return SASS_MEMORY_NEW(ctx.mem, Boolean, pstate, n1->unit() == tmp_n2.unit());
    }

    Signature variable_exists_sig = "variable-exists($name)";
    BUILT_IN(variable_exists)
    {
      std::string s = Util::normalize_underscores(unquote(ARG("$name", String_Constant)->value()));

      if(d_env.has("$"+s)) {
        return SASS_MEMORY_NEW(ctx.mem, Boolean, pstate, true);
      }
      else {
        return SASS_MEMORY_NEW(ctx.mem, Boolean, pstate, false);
      }
    }

    Signature global_variable_exists_sig = "global-variable-exists($name)";
    BUILT_IN(global_variable_exists)
    {
      std::string s = Util::normalize_underscores(unquote(ARG("$name", String_Constant)->value()));

      if(d_env.has_global("$"+s)) {
        return SASS_MEMORY_NEW(ctx.mem, Boolean, pstate, true);
      }
      else {
        return SASS_MEMORY_NEW(ctx.mem, Boolean, pstate, false);
      }
    }

    Signature function_exists_sig = "function-exists($name)";
    BUILT_IN(function_exists)
    {
      std::string s = Util::normalize_underscores(unquote(ARG("$name", String_Constant)->value()));

      if(d_env.has_global(s+"[f]")) {
        return SASS_MEMORY_NEW(ctx.mem, Boolean, pstate, true);
      }
      else {
        return SASS_MEMORY_NEW(ctx.mem, Boolean, pstate, false);
      }
    }

    Signature mixin_exists_sig = "mixin-exists($name)";
    BUILT_IN(mixin_exists)
    {
      std::string s = Util::normalize_underscores(unquote(ARG("$name", String_Constant)->value()));

      if(d_env.has_global(s+"[m]")) {
        return SASS_MEMORY_NEW(ctx.mem, Boolean, pstate, true);
      }
      else {
        return SASS_MEMORY_NEW(ctx.mem, Boolean, pstate, false);
      }
    }

    Signature feature_exists_sig = "feature-exists($name)";
    BUILT_IN(feature_exists)
    {
      std::string s = unquote(ARG("$name", String_Constant)->value());

      if(features.find(s) == features.end()) {
        return SASS_MEMORY_NEW(ctx.mem, Boolean, pstate, false);
      }
      else {
        return SASS_MEMORY_NEW(ctx.mem, Boolean, pstate, true);
      }
    }

    Signature call_sig = "call($name, $args...)";
    BUILT_IN(call)
    {
      std::string name = Util::normalize_underscores(unquote(ARG("$name", String_Constant)->value()));
      List_Ptr arglist = SASS_MEMORY_NEW(ctx.mem, List, *ARG("$args", List));

      Arguments_Obj args = SASS_MEMORY_NEW(ctx.mem, Arguments, pstate);
      // std::string full_name(name + "[f]");
      // Definition_Ptr def = d_env.has(full_name) ? static_cast<Definition_Ptr>((d_env)[full_name]) : 0;
      // Parameters_Ptr params = def ? def->parameters() : 0;
      // size_t param_size = params ? params->length() : 0;
      for (size_t i = 0, L = arglist->length(); i < L; ++i) {
        Expression_Ptr expr = arglist->value_at_index(i);
        // if (params && params->has_rest_parameter()) {
        //   Parameter_Obj p = param_size > i ? (*params)[i] : 0;
        //   List_Ptr list = dynamic_cast<List_Ptr>(expr);
        //   if (list && p && !p->is_rest_parameter()) expr = (*list)[0];
        // }
        if (arglist->is_arglist()) {
          Expression_Obj obj = arglist->at(i);
          Argument_Obj arg = (Argument_Ptr)&obj;
          *args << SASS_MEMORY_NEW(ctx.mem, Argument,
                                   pstate,
                                   expr,
                                   arg ? arg->name() : "",
                                   arg ? arg->is_rest_argument() : false,
                                   arg ? arg->is_keyword_argument() : false);
        } else {
          *args << SASS_MEMORY_NEW(ctx.mem, Argument, pstate, expr);
        }
      }
      Function_Call_Ptr func = SASS_MEMORY_NEW(ctx.mem, Function_Call, pstate, name, &args);
      Expand expand(ctx, &d_env, backtrace, &selector_stack);
      return func->perform(&expand.eval);

    }

    ////////////////////
    // BOOLEAN FUNCTIONS
    ////////////////////

    Signature not_sig = "not($value)";
    BUILT_IN(sass_not)
    { return SASS_MEMORY_NEW(ctx.mem, Boolean, pstate, ARG("$value", Expression)->is_false()); }

    Signature if_sig = "if($condition, $if-true, $if-false)";
    // BUILT_IN(sass_if)
    // { return ARG("$condition", Expression)->is_false() ? ARG("$if-false", Expression) : ARG("$if-true", Expression); }
    BUILT_IN(sass_if)
    {
      Expand expand(ctx, &d_env, backtrace, &selector_stack);
      bool is_true = !ARG("$condition", Expression)->perform(&expand.eval)->is_false();
      Expression_Ptr res = ARG(is_true ? "$if-true" : "$if-false", Expression);
      res = res->perform(&expand.eval);
      res->set_delayed(false); // clone?
      return res;
    }

    ////////////////
    // URL FUNCTIONS
    ////////////////

    Signature image_url_sig = "image-url($path, $only-path: false, $cache-buster: false)";
    BUILT_IN(image_url)
    {
      error("`image_url` has been removed from libsass because it's not part of the Sass spec", pstate);
      return 0; // suppress warning, error will exit anyway
    }

    //////////////////////////
    // MISCELLANEOUS FUNCTIONS
    //////////////////////////

    // value.check_deprecated_interp if value.is_a?(Sass::Script::Value::String)
    // unquoted_string(value.to_sass)

    Signature inspect_sig = "inspect($value)";
    BUILT_IN(inspect)
    {
      Expression_Ptr v = ARG("$value", Expression);
      if (v->concrete_type() == Expression::NULL_VAL) {
        return SASS_MEMORY_NEW(ctx.mem, String_Quoted, pstate, "null");
      } else if (v->concrete_type() == Expression::BOOLEAN && *v == 0) {
        return SASS_MEMORY_NEW(ctx.mem, String_Quoted, pstate, "false");
      } else if (v->concrete_type() == Expression::STRING) {
        return v;
      } else {
        // ToDo: fix to_sass for nested parentheses
        Sass_Output_Style old_style;
        old_style = ctx.c_options.output_style;
        ctx.c_options.output_style = TO_SASS;
        Emitter emitter(ctx.c_options);
        Inspect i(emitter);
        i.in_declaration = false;
        v->perform(&i);
        ctx.c_options.output_style = old_style;
        return SASS_MEMORY_NEW(ctx.mem, String_Quoted, pstate, i.get_buffer());
      }
      // return v;
    }
    Signature selector_nest_sig = "selector-nest($selectors...)";
    BUILT_IN(selector_nest)
    {
      List_Ptr arglist = ARG("$selectors", List);

      // Not enough parameters
      if( arglist->length() == 0 )
        error("$selectors: At least one selector must be passed", pstate);

      // Parse args into vector of selectors
      std::vector<CommaSequence_Selector_Ptr> parsedSelectors;
      for (size_t i = 0, L = arglist->length(); i < L; ++i) {
        Expression_Ptr exp = dynamic_cast<Expression_Ptr>(arglist->value_at_index(i));
        if (exp->concrete_type() == Expression::NULL_VAL) {
          std::stringstream msg;
          msg << "$selectors: null is not a valid selector: it must be a string,\n";
          msg << "a list of strings, or a list of lists of strings for 'selector-nest'";
          error(msg.str(), pstate);
        }
        if (String_Constant_Ptr str =dynamic_cast<String_Constant_Ptr>(exp)) {
          str->quote_mark(0);
        }
        std::string exp_src = exp->to_string(ctx.c_options) + "{";
        CommaSequence_Selector_Obj sel = Parser::parse_selector(exp_src.c_str(), ctx);
        parsedSelectors.push_back(&sel);
      }

      // Nothing to do
      if( parsedSelectors.empty() ) {
        return SASS_MEMORY_NEW(ctx.mem, Null, pstate);
      }

      // Set the first element as the `result`, keep appending to as we go down the parsedSelector vector.
      std::vector<CommaSequence_Selector_Ptr>::iterator itr = parsedSelectors.begin();
      CommaSequence_Selector_Ptr result = *itr;
      ++itr;

      for(;itr != parsedSelectors.end(); ++itr) {
        CommaSequence_Selector_Ptr child = *itr;
        std::vector<Sequence_Selector_Ptr> exploded;
        CommaSequence_Selector_Ptr rv = child->resolve_parent_refs(ctx, result);
        for (size_t m = 0, mLen = rv->length(); m < mLen; ++m) {
          exploded.push_back((*rv)[m]);
        }
        result->elements(exploded);
      }

      Listize listize(ctx.mem);
      return result->perform(&listize);
    }

    Signature selector_append_sig = "selector-append($selectors...)";
    BUILT_IN(selector_append)
    {
      List_Ptr arglist = ARG("$selectors", List);

      // Not enough parameters
      if( arglist->length() == 0 )
        error("$selectors: At least one selector must be passed", pstate);

      // Parse args into vector of selectors
      std::vector<CommaSequence_Selector_Ptr> parsedSelectors;
      for (size_t i = 0, L = arglist->length(); i < L; ++i) {
        Expression_Ptr exp = dynamic_cast<Expression_Ptr>(arglist->value_at_index(i));
        if (exp->concrete_type() == Expression::NULL_VAL) {
          std::stringstream msg;
          msg << "$selectors: null is not a valid selector: it must be a string,\n";
          msg << "a list of strings, or a list of lists of strings for 'selector-append'";
          error(msg.str(), pstate);
        }
        if (String_Constant_Ptr str =dynamic_cast<String_Constant_Ptr>(exp)) {
          str->quote_mark(0);
        }
        std::string exp_src = exp->to_string() + "{";
        CommaSequence_Selector_Obj sel = Parser::parse_selector(exp_src.c_str(), ctx);
        parsedSelectors.push_back(&sel);
      }

      // Nothing to do
      if( parsedSelectors.empty() ) {
        return SASS_MEMORY_NEW(ctx.mem, Null, pstate);
      }

      // Set the first element as the `result`, keep appending to as we go down the parsedSelector vector.
      std::vector<CommaSequence_Selector_Ptr>::iterator itr = parsedSelectors.begin();
      CommaSequence_Selector_Ptr result = *itr;
      ++itr;

      for(;itr != parsedSelectors.end(); ++itr) {
        CommaSequence_Selector_Ptr child = *itr;
        std::vector<Sequence_Selector_Ptr> newElements;

        // For every COMPLEX_SELECTOR in `result`
        // For every COMPLEX_SELECTOR in `child`
          // let parentSeqClone equal a copy of result->elements[i]
          // let childSeq equal child->elements[j]
          // Append all of childSeq head elements into parentSeqClone
          // Set the innermost tail of parentSeqClone, to childSeq's tail
        // Replace result->elements with newElements
        for (size_t i = 0, resultLen = result->length(); i < resultLen; ++i) {
          for (size_t j = 0, childLen = child->length(); j < childLen; ++j) {
            Sequence_Selector_Ptr parentSeqClone = (*result)[i]->cloneFully(ctx);
            Sequence_Selector_Ptr childSeq = (*child)[j];
            Sequence_Selector_Ptr base = childSeq->tail();

            // Must be a simple sequence
            if( childSeq->combinator() != Sequence_Selector_Ref::Combinator::ANCESTOR_OF ) {
              std::string msg("Can't append  `");
              msg += childSeq->to_string();
              msg += "` to `";
              msg += parentSeqClone->to_string();
              msg += "`";
              error(msg, pstate, backtrace);
            }

            // Cannot be a Universal selector
            Element_Selector_Ptr pType = dynamic_cast<Element_Selector_Ptr>(childSeq->head()->first());
            if(pType && pType->name() == "*") {
              std::string msg("Can't append  `");
              msg += childSeq->to_string();
              msg += "` to `";
              msg += parentSeqClone->to_string();
              msg += "`";
              error(msg, pstate, backtrace);
            }

            // TODO: Add check for namespace stuff

            // append any selectors in childSeq's head
            *(parentSeqClone->innermost()->head()) += (base->head());

            // Set parentSeqClone new tail
            parentSeqClone->innermost()->tail( base->tail() );

            newElements.push_back(parentSeqClone);
          }
        }

        result->elements(newElements);
      }

      Listize listize(ctx.mem);
      return result->perform(&listize);
    }

    Signature selector_unify_sig = "selector-unify($selector1, $selector2)";
    BUILT_IN(selector_unify)
    {
      CommaSequence_Selector_Ptr selector1 = ARGSEL("$selector1", CommaSequence_Selector, p_contextualize);
      CommaSequence_Selector_Ptr selector2 = ARGSEL("$selector2", CommaSequence_Selector, p_contextualize);

      CommaSequence_Selector_Ptr result = selector1->unify_with(selector2, ctx);
      Listize listize(ctx.mem);
      return result->perform(&listize);
    }

    Signature simple_selectors_sig = "simple-selectors($selector)";
    BUILT_IN(simple_selectors)
    {
      SimpleSequence_Selector_Ptr sel = ARGSEL("$selector", SimpleSequence_Selector, p_contextualize);

      List_Ptr l = SASS_MEMORY_NEW(ctx.mem, List, sel->pstate(), sel->length(), SASS_COMMA);

      for (size_t i = 0, L = sel->length(); i < L; ++i) {
        Simple_Selector* ss = (*sel)[i];
        std::string ss_string = ss->to_string() ;

        *l << SASS_MEMORY_NEW(ctx.mem, String_Quoted, ss->pstate(), ss_string);
      }

      return l;
    }

    Signature selector_extend_sig = "selector-extend($selector, $extendee, $extender)";
    BUILT_IN(selector_extend)
    {
      CommaSequence_Selector_Ptr  selector = ARGSEL("$selector", CommaSequence_Selector, p_contextualize);
      CommaSequence_Selector_Ptr  extendee = ARGSEL("$extendee", CommaSequence_Selector, p_contextualize);
      CommaSequence_Selector_Ptr  extender = ARGSEL("$extender", CommaSequence_Selector, p_contextualize);

      ExtensionSubsetMap subset_map;
      extender->populate_extends(extendee, ctx, subset_map);

      CommaSequence_Selector_Ptr result = Extend::extendSelectorList(selector, ctx, subset_map, false);

      Listize listize(ctx.mem);
      return result->perform(&listize);
    }

    Signature selector_replace_sig = "selector-replace($selector, $original, $replacement)";
    BUILT_IN(selector_replace)
    {
      CommaSequence_Selector_Ptr  selector = ARGSEL("$selector", CommaSequence_Selector, p_contextualize);
      CommaSequence_Selector_Ptr  original = ARGSEL("$original", CommaSequence_Selector, p_contextualize);
      CommaSequence_Selector_Ptr  replacement = ARGSEL("$replacement", CommaSequence_Selector, p_contextualize);

      ExtensionSubsetMap subset_map;
      replacement->populate_extends(original, ctx, subset_map);

      CommaSequence_Selector_Ptr result = Extend::extendSelectorList(selector, ctx, subset_map, true);

      Listize listize(ctx.mem);
      return result->perform(&listize);
    }

    Signature selector_parse_sig = "selector-parse($selector)";
    BUILT_IN(selector_parse)
    {
      CommaSequence_Selector_Ptr sel = ARGSEL("$selector", CommaSequence_Selector, p_contextualize);

      Listize listize(ctx.mem);
      return sel->perform(&listize);
    }

    Signature is_superselector_sig = "is-superselector($super, $sub)";
    BUILT_IN(is_superselector)
    {
      CommaSequence_Selector_Ptr  sel_sup = ARGSEL("$super", CommaSequence_Selector, p_contextualize);
      CommaSequence_Selector_Ptr  sel_sub = ARGSEL("$sub", CommaSequence_Selector, p_contextualize);
      bool result = sel_sup->is_superselector_of(sel_sub);
      return SASS_MEMORY_NEW(ctx.mem, Boolean, pstate, result);
    }

    Signature unique_id_sig = "unique-id()";
    BUILT_IN(unique_id)
    {
      std::stringstream ss;
      std::uniform_real_distribution<> distributor(0, 4294967296); // 16^8
      uint_fast32_t distributed = static_cast<uint_fast32_t>(distributor(rand));
      ss << "u" << std::setfill('0') << std::setw(8) << std::hex << distributed;
      return SASS_MEMORY_NEW(ctx.mem, String_Quoted, pstate, ss.str());
    }

  }
}
