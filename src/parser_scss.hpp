#ifndef SASS_PARSER_SCSS_H
#define SASS_PARSER_SCSS_H

#include "parser_stylesheet.hpp"

namespace Sass {

  class ScssParser : public StylesheetParser {

  public:
    ScssParser(Context& context, const char* content, const char* path, size_t srcid) :
      StylesheetParser(context, content, path, srcid)
    {}

    virtual bool plainCss() const override;

    bool isIndented() const override final;

    Interpolation* styleRuleSelector() override;

    void expectStatementSeparator(std::string name) override;
    
    bool atEndOfStatement() override;

    bool lookingAtChildren() override;

    bool scanElse(size_t ifIndentation) override;

    std::vector<StatementObj> children(Statement* (StylesheetParser::* child)()) override;

    std::vector<StatementObj> statements(Statement* (StylesheetParser::* statement)()) override;

    virtual SilentComment* _silentComment();

    LoudComment* _loudComment();

  };

}

#endif
