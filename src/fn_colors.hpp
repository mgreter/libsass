#ifndef SASS_FN_COLORS_H
#define SASS_FN_COLORS_H

#include "fn_utils.hpp"

namespace Sass {

  namespace Functions {

    namespace Colors {
      BUILT_IN_FN(rgb_4);
      BUILT_IN_FN(rgb_3);
      BUILT_IN_FN(rgb_2);
      BUILT_IN_FN(rgb_1);
      BUILT_IN_FN(rgba_4);
      BUILT_IN_FN(rgba_3);
      BUILT_IN_FN(rgba_2);
      BUILT_IN_FN(rgba_1);
      BUILT_IN_FN(hsl_4);
      BUILT_IN_FN(hsl_3);
      BUILT_IN_FN(hsl_2);
      BUILT_IN_FN(hsl_1);
      BUILT_IN_FN(hsla_4);
      BUILT_IN_FN(hsla_3);
      BUILT_IN_FN(hsla_2);
      BUILT_IN_FN(hsla_1);

      BUILT_IN_FN(red);
      BUILT_IN_FN(green);
      BUILT_IN_FN(blue);
      BUILT_IN_FN(invert);
      BUILT_IN_FN(hue);
      BUILT_IN_FN(saturation);
      BUILT_IN_FN(lightness);
      BUILT_IN_FN(adjustHue);
      BUILT_IN_FN(complement);
      BUILT_IN_FN(grayscale);
      BUILT_IN_FN(lighten);
      BUILT_IN_FN(darken);
      BUILT_IN_FN(saturate_2);
      BUILT_IN_FN(saturate_1);
      BUILT_IN_FN(desaturate);
      BUILT_IN_FN(opacify);
      BUILT_IN_FN(transparentize);
      BUILT_IN_FN(alpha_one);
      BUILT_IN_FN(alpha_any);
      BUILT_IN_FN(opacity);
      BUILT_IN_FN(ieHexStr);
      
      BUILT_IN_FN(adjust);
      BUILT_IN_FN(change);
      BUILT_IN_FN(scale);
      BUILT_IN_FN(mix);

    }

    Value* _rgb(const sass::string& name, const sass::vector<ValueObj>& arguments, Signature sig, const SourceSpan& pstate, Logger& logger);
    Value* _hsl(const sass::string& name, const sass::vector<ValueObj>& arguments, Signature sig, const SourceSpan& pstate, Logger& logger);

  }

}

#endif
