#ifndef SASS_PARSER_MEDIA_QUERY_H
#define SASS_PARSER_MEDIA_QUERY_H

#include "sass.hpp"
#include "parser.hpp"

namespace Sass {

  class MediaQueryParser : public Parser {

  public:

    MediaQueryParser(
      Context& context,
      const char* contents,
      const char* path,
      size_t srcid) :
      Parser(context, contents, path, srcid)
    {}

    std::vector<CssMediaQueryObj> parse();

  private:

    CssMediaQuery* _mediaQuery();

  };

}

#endif
