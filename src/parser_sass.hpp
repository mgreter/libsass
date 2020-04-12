#ifndef SASS_PARSER_SASS_H
#define SASS_PARSER_SASS_H

#include "parser_stylesheet.hpp"

namespace Sass {

  class SassParser : public StylesheetParser {

  public:

    enum Sass_Indent_Type {
      AUTO, TABS, SPACES,
    };

    SassParser(Context& context, SourceFileObj source) :
      StylesheetParser(context, source),
      _currentIndentation(0),
      _nextIndentation(sass::string::npos),
      _nextIndentationEnd({ source->begin(), Offset() }),
      _spaces(Sass_Indent_Type::AUTO)

    {}

    size_t _currentIndentation = 0;

    // The indentation level of the next source line after the scanner's
    // position, or `null` if that hasn't been computed yet.
    // A source line is any line that's not entirely whitespace.
    size_t _nextIndentation;

    // The beginning of the next source line after the scanner's
    // position, or `null` if that hasn't been computed yet.
    // A source line is any line that's not entirely whitespace.
    StringScannerState _nextIndentationEnd;

    // Whether the document is indented using spaces or tabs.
    // If this is `true`, the document is indented using spaces. If it's `false`,
    // the document is indented using tabs. If it's `null`, we haven't yet seen
    // the indentation character used by the document.
    Sass_Indent_Type _spaces = Sass_Indent_Type::AUTO;

    bool indentWithTabs() { return _spaces == Sass_Indent_Type::TABS; }
    bool indentWithSpaces() { return _spaces == Sass_Indent_Type::SPACES; }


    // Whether this is a plain CSS stylesheet.
    bool plainCss() const override final { return false; }

    bool isIndented() const override final { return true; };

    // void silentComment() override;

    // Statement* atRule(Statement* (StylesheetParser::* child)(), bool root = false) override;

    // ImportRule* _cssImportRule(Position start);

    // Expression* identifierLike() override final;

    Interpolation* styleRuleSelector() override final;

    void expectStatementSeparator(sass::string name) override final;

    bool atEndOfStatement() override final;

    bool lookingAtChildren() override final;

    ImportBase* importArgument(ImportRule* rule) override final;

    bool scanElse(size_t ifIndentation) override final;

    sass::vector<StatementObj> children(Statement* (StylesheetParser::* statement)()) override final;

    sass::vector<StatementObj> statements(Statement* (StylesheetParser::* statement)()) override final;

    Statement* _child(Statement* (StylesheetParser::* statement)());

    SilentComment* _silentComment();

    LoudComment* _loudComment();

    void whitespace() override final;

    void _expectNewline();

    bool _lookingAtDoubleNewline();

    void _whileIndentedLower(Statement* (StylesheetParser::* child)(), sass::vector<StatementObj>& children);

    size_t _readIndentation();

    size_t _peekIndentation();

    void _checkIndentationConsistency(bool containsTab, bool containsSpace);

  };

}

#endif