#ifndef SASS_FN_COLORS_H
#define SASS_FN_COLORS_H

#include "fn_utils.hpp"

namespace Sass {

  namespace Functions {

    // macros for common ranges (u mean unsigned or upper, r for full range)
    #define DARG_U_FACT(argname) get_arg_r(argname, env, sig, pstate, traces,   0.0, 1.0) // double
    #define DARG_R_FACT(argname) get_arg_r(argname, env, sig, pstate, traces, - 1.0, 1.0) // double
    #define DARG_U_BYTE(argname) get_arg_r(argname, env, sig, pstate, traces,   0.0, 255.0) // double
    #define DARG_R_BYTE(argname) get_arg_r(argname, env, sig, pstate, traces, - 255.0, 255.0) // double
    #define DARG_U_PRCT(argname) get_arg_r(argname, env, sig, pstate, traces,   0.0, 100.0) // double
    #define DARG_R_PRCT(argname) get_arg_r(argname, env, sig, pstate, traces, - 100.0, 100.0, "%") // double
    #define DARG_R_CENT(argname) get_arg_r(argname, env, sig, pstate, traces, - 100.0, 100.0) // double

    extern Signature rgb_4_sig;
    extern Signature rgb_3_sig;
    extern Signature rgb_2_sig;
    extern Signature rgb_1_sig;
    extern Signature hsl_1_sig;
    extern Signature hsl_2_sig;
    extern Signature hsl_3_sig;
    extern Signature hsl_4_sig;
    extern Signature rgba_4_sig;
    extern Signature rgba_3_sig;
    extern Signature rgba_2_sig;
    extern Signature rgba_1_sig;
    extern Signature hsla_4_sig;
    extern Signature hsla_3_sig;
    extern Signature hsla_2_sig;
    extern Signature hsla_1_sig;
    extern Signature red_sig;
    extern Signature green_sig;
    extern Signature blue_sig;
    extern Signature mix_sig;
    extern Signature hue_sig;
    extern Signature saturation_sig;
    extern Signature lightness_sig;
    extern Signature adjust_hue_sig;
    extern Signature lighten_sig;
    extern Signature darken_sig;
    extern Signature saturate_1_sig;
    extern Signature saturate_2_sig;
    extern Signature desaturate_sig;
    extern Signature grayscale_sig;
    extern Signature complement_sig;
    extern Signature invert_sig;
    extern Signature alpha_sig;
    extern Signature opacity_sig;
    extern Signature opacify_sig;
    extern Signature fade_in_sig;
    extern Signature transparentize_sig;
    extern Signature fade_out_sig;
    extern Signature adjust_color_sig;
    extern Signature scale_color_sig;
    extern Signature change_color_sig;
    extern Signature ie_hex_str_sig;

    BUILT_IN(rgb_1);
    BUILT_IN(rgb_2);
    BUILT_IN(rgb_3);
    BUILT_IN(rgb_4);
    BUILT_IN(hsl_1);
    BUILT_IN(hsl_2);
    BUILT_IN(hsl_3);
    BUILT_IN(hsl_4);
    BUILT_IN(rgba_4);
    BUILT_IN(rgba_3);
    BUILT_IN(rgba_2);
    BUILT_IN(rgba_1);
    BUILT_IN(hsla_1);
    BUILT_IN(hsla_2);
    BUILT_IN(hsla_3);
    BUILT_IN(hsla_4);

    BUILT_IN(red);
    BUILT_IN(green);
    BUILT_IN(blue);
    BUILT_IN(mix);
    BUILT_IN(hue);
    BUILT_IN(saturation);
    BUILT_IN(lightness);
    BUILT_IN(adjust_hue);
    BUILT_IN(lighten);
    BUILT_IN(darken);
    BUILT_IN(saturate_1);
    BUILT_IN(saturate_2);
    BUILT_IN(desaturate);
    BUILT_IN(grayscale);
    BUILT_IN(complement);
    BUILT_IN(invert);
    BUILT_IN(alpha);
    BUILT_IN(alpha_ie);
    BUILT_IN(opacify);
    BUILT_IN(transparentize);
    BUILT_IN(adjust_color);
    BUILT_IN(scale_color);
    BUILT_IN(change_color);
    BUILT_IN(ie_hex_str);

    Value* _rgb(std::string name, std::vector<ValueObj> arguments, Signature sig, ParserState pstate, Backtraces traces);

    Value* _hsl(std::string name, std::vector<ValueObj> arguments, Signature sig, ParserState pstate, Backtraces traces);

  }

}

#endif
