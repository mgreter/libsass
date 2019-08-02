#ifndef SASS_PARSER_CSS_H
#define SASS_PARSER_CSS_H

#include "parser_scss.hpp"
#include "parser_stylesheet.hpp"

namespace Sass {

  class CssParser : public ScssParser {

  public:
    CssParser(Context& context, SourceDataObj source) :
      ScssParser(context, source)
    {}

    // Whether this is a plain CSS stylesheet.
    bool plainCss() const override final { return true; }

    void silentComment() override;

    Statement* atRule(Statement* (StylesheetParser::* child)(), bool root = false) override;

    ImportRule* _cssImportRule(Position start);

    Expression* identifierLike() override final;


  };

}

#endif
