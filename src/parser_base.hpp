#ifndef SASS_PARSER_BASE_H
#define SASS_PARSER_BASE_H

#include "parser.hpp"

#include "interpolation.hpp"

namespace Sass {

  class BaseParser : public Parser {

  public:
    BaseParser(Context& context, SourceDataObj source) :
      Parser(context, source)
    {}

  };

}

#endif
