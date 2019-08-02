#ifndef SASS_PARSER_MEDIA_QUERY_H
#define SASS_PARSER_MEDIA_QUERY_H

#include "sass.hpp"
#include "parser.hpp"

namespace Sass {

  class MediaQueryParser : public Parser {

  public:

    MediaQueryParser(
      Context& context,
      SourceDataObj source) :
      Parser(context, source)
    {}

    sass::vector<CssMediaQueryObj> parse();

  private:

    CssMediaQuery* _mediaQuery();

  };

}

#endif
