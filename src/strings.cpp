#include "strings.hpp"

namespace Sass
{

	namespace Strings
	{

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

		const sass::string key("key");
		const sass::string map("map");
		const sass::string map1("map1");
		const sass::string map2("map2");
		const sass::string args("args");
		const sass::string name("name");
		const sass::string media("media");
		const sass::string module("module");
		const sass::string keyframes("keyframes");

		const sass::string forRule("@for");
		const sass::string warnRule("@warn");
		const sass::string errorRule("@error");
		const sass::string debugRule("@debug");
		const sass::string extendRule("@extend");
		const sass::string importRule("@import");
		const sass::string contentRule("@content");

		const sass::string scaleColor("scale-color");
		const sass::string colorAdjust("color-adjust");
		const sass::string colorChange("color-change");

		const sass::string $condition("$condition");
		const sass::string $ifFalse("$if-false");
		const sass::string $ifTrue("$if-true");

		const sass::string utf8bom("\xEF\xBB\xBF");

    const sass::string argument("argument");
    const sass::string _or_("or");

  } // namespace Strings

  namespace Keys {

    const EnvKey red(Strings::red);
		const EnvKey hue(Strings::hue);
		const EnvKey blue(Strings::blue);
		const EnvKey green(Strings::green);
		const EnvKey alpha(Strings::alpha);
		const EnvKey lightness(Strings::lightness);
		const EnvKey saturation(Strings::saturation);

		const EnvKey warnRule(Strings::warnRule);
		const EnvKey errorRule(Strings::errorRule);
		const EnvKey debugRule(Strings::debugRule);
		const EnvKey contentRule(Strings::contentRule);

		const EnvKey $condition(Strings::$condition);
		const EnvKey $ifFalse(Strings::$ifFalse);
		const EnvKey $ifTrue(Strings::$ifTrue);

	} // namespace Keys

} // namespace Sass
