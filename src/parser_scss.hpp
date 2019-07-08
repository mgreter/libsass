#ifndef SASS_PARSER_SCSS_H
#define SASS_PARSER_SCSS_H

#include "parser_stylesheet.hpp"

namespace Sass {

  class ScssParser : public StylesheetParser {

  public:
    ScssParser(Context& context, SourceDataObj source) :
      StylesheetParser(context, source)
    {}

    virtual bool plainCss() const override;

    bool isIndented() const override final;

    Interpolation* styleRuleSelector() override;

    void expectStatementSeparator(sass::string name) override;
    
    bool atEndOfStatement() override;

    bool lookingAtChildren() override;

    bool scanElse(size_t ifIndentation) override;

    sass::vector<StatementObj> children(Statement* (StylesheetParser::* child)()) override;

    sass::vector<StatementObj> statements(Statement* (StylesheetParser::* statement)()) override;

    virtual SilentComment* _silentComment();

    LoudComment* _loudComment();

  };

}

#endif
