#ifndef SASS_PARSER_SELECTOR_H
#define SASS_PARSER_SELECTOR_H

#include "sass.hpp"
#include "ast.hpp"

#include "parser.hpp"
#include "stylesheet.hpp"
#include "interpolation.hpp"
#include "context.hpp"
#include "debugger.hpp"

namespace Sass {

  class SelectorParser : public Parser {

  public:

    // Whether this parser allows the parent selector `&`.
    bool _allowParent;

    // Whether this parser allows placeholder selectors beginning with `%`.
    bool _allowPlaceholder;

    SelectorParser(
      Context& context,
      const char* contents,
      const char* path,
      size_t srcid,
      bool allowParent = true,
      bool allowPlaceholder = true) :
      Parser(context, contents, path, srcid),
      _allowParent(allowParent),
      _allowPlaceholder(allowPlaceholder)
    {}

    SelectorListObj parse();

    CompoundSelectorObj parseCompoundSelector();

    SimpleSelectorObj parseSimpleSelector();

    SelectorListObj _selectorList();

    ComplexSelectorObj _complexSelector(bool lineBreak = false);

    CompoundSelectorObj _compoundSelector();

    SimpleSelectorObj _simpleSelector(bool allowParent);

    AttributeSelectorObj _attributeSelector();

    // QualifiedName _attributeName();
    std::string _attributeName();

    // AttributeOperator _attributeOperator();
    std::string _attributeOperator();

    ClassSelectorObj _classSelector();

    IDSelectorObj _idSelector();

    PlaceholderSelectorObj _placeholderSelector();

    // ParentSelectorObj _parentSelector();

    PseudoSelectorObj _pseudoSelector();

    std::string _aNPlusB();

    SimpleSelectorObj _typeOrUniversalSelector();

  };


}

#endif
