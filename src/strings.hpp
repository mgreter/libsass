#ifndef SASS_STRINGS_H
#define SASS_STRINGS_H

#include "sass.hpp"

namespace Sass {

  namespace Strings {

    const sass::string empty("");

    const sass::string plus("+");
    const sass::string minus("-");
    const sass::string percent("%");

    const sass::string rgb("rgb");
    const sass::string hsl("hsl");
    const sass::string rgba("rgba");
    const sass::string hsla("hsla");

    const sass::string red("red");
    const sass::string hue("hue");
    const sass::string blue("blue");
    const sass::string green("green");
    const sass::string alpha("alpha");
    const sass::string color("color");
    const sass::string amount("amount");
    const sass::string lightness("lightness");
    const sass::string saturation("saturation");

    const sass::string contentRule("@content");

  }

  namespace Keys {

    const EnvString red(Strings::red);
    const EnvString hue(Strings::hue);
    const EnvString blue(Strings::blue);
    const EnvString green(Strings::green);
    const EnvString alpha(Strings::alpha);
    const EnvString lightness(Strings::lightness);
    const EnvString saturation(Strings::saturation);
    const EnvString contentRule(Strings::contentRule);

    const EnvString $condition("$condition");
    const EnvString $ifFalse("$if-false");
    const EnvString $ifTrue("$if-true");

  }

}


#endif
