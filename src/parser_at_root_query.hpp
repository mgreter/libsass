#ifndef SASS_PARSER_AT_ROOT_QUERY_H
#define SASS_PARSER_AT_ROOT_QUERY_H

#include "parser_scss.hpp"
#include "parser_stylesheet.hpp"

namespace Sass {

  class AtRootQueryParser : public Parser {

  public:
    AtRootQueryParser(Context& context, SourceDataObj source) :
      Parser(context, source)
    {}

    AtRootQuery* parse();


  };

}

#endif
