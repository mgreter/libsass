#ifndef SASS_PARSER_KEYFRAME_SELECTOR_H
#define SASS_PARSER_KEYFRAME_SELECTOR_H

#include "parser_scss.hpp"
#include "parser_stylesheet.hpp"

namespace Sass {

  // A parser for `@keyframes` block selectors.
  class KeyframeSelectorParser : public Parser {

  public:
    KeyframeSelectorParser(
      Context& context,
      SourceDataObj source) :
      Parser(context, source)
    {}

    sass::vector<sass::string> parse();

    sass::string _percentage();


  };

}

#endif
