#ifndef SASS_INSPECT_H
#define SASS_INSPECT_H

#include "position.hpp"
#include "operation.hpp"
#include "emitter.hpp"
#include "visitor_selector.hpp"

namespace Sass {
  class Context;

  // public SelectorVisitor<void>
  class Inspect : public Operation_CRTP<void, Inspect>, public Emitter,
  public SelectorVisitor<void> {
  protected:
    // import all the class-specific methods and override as desired
    using Operation_CRTP<void, Inspect>::operator();


  public:

    // Whether quoted strings should be emitted with quotes.
    bool quotes;

    Inspect(const Emitter& emi);
    virtual ~Inspect();

    void visitQuotedString(const std::string& text, uint8_t quotes = 0);


    // statements
    virtual void operator()(Block*);
    // virtual void operator()(Keyframe_Rule*);
    // virtual void operator()(Bubble*);
    // virtual void operator()(SupportsRule*);
    // virtual void operator()(At_Root_Block*);
    virtual void operator()(AtRule*);
    virtual void operator()(Declaration*);
    // virtual void operator()(Assignment*);
    virtual void operator()(Import*);
    // virtual void operator()(Import_Stub*);

    virtual void operator()(ImportRule*);
    virtual void operator()(StaticImport*);
    // virtual void operator()(DynamicImport*);
    // virtual void operator()(Warning*);
    // virtual void operator()(Error*);
    // virtual void operator()(Debug*);
    virtual void operator()(CssComment*);
    void _writeMapElement(Expression* value);
    // virtual void operator()(If*);
    // virtual void operator()(For*);
    // virtual void operator()(Each*);
    // virtual void operator()(While*);
    // virtual void operator()(Return*);
    // virtual void operator()(ExtendRule*);
    // virtual void operator()(Definition*);
    // virtual void operator()(Mixin_Call*);
    // virtual void operator()(Content*);
    // expressions
    virtual void operator()(Map*);
    virtual void operator()(Function*);
    virtual void operator()(List*);
    virtual void operator()(SassList*);
    // virtual void operator()(ParenthesizedExpression*);
    virtual void operator()(Binary_Expression*);
    virtual void operator()(Unary_Expression*);
    virtual void operator()(FunctionExpression*);
    // virtual void operator()(Custom_Warning*);
    // virtual void operator()(Custom_Error*);
    virtual void operator()(Variable*);
    virtual void operator()(Number*);
    virtual void operator()(Color_RGBA*);
    virtual void operator()(Color_HSLA*);
    virtual void operator()(Boolean*);
    virtual void operator()(String_Constant*);
    virtual void operator()(String_Quoted*);

    virtual void operator()(StringLiteral*);
    virtual void operator()(Interpolation*);
    virtual void operator()(StringExpression*);

    virtual void operator()(Custom_Error*);
    virtual void operator()(Custom_Warning*);
    virtual void operator()(SupportsOperation*);
    virtual void operator()(SupportsNegation*);
    virtual void operator()(SupportsDeclaration*);
    virtual void operator()(SupportsInterpolation*);
    virtual void operator()(MediaRule*);
    virtual void operator()(CssMediaRule*);
    virtual void operator()(CssMediaQuery*);
    // virtual void operator()(At_Root_Query*);
    virtual void operator()(Null*);
    // virtual void operator()(Parent_Reference* p);
    // parameters and arguments
    // virtual void operator()(Parameter*);
    // virtual void operator()(Parameters*);
    virtual void operator()(Argument*);
    virtual void operator()(Arguments*);
    // selectors
    virtual void operator()(PlaceholderSelector*);
    virtual void operator()(ClassSelector*);
    // virtual void operator()(SelectorComponent*);
    virtual void operator()(SelectorCombinator*);
    virtual void operator()(CompoundSelector*);
    virtual void operator()(SelectorList*);
    virtual std::string lbracket(List*);
    virtual std::string rbracket(List*);
    virtual std::string lbracket(SassList*);
    virtual std::string rbracket(SassList*);

    // Implement those first without override
    void visitAttributeSelector(AttributeSelector* attribute)override;
    void visitClassSelector(ClassSelector* klass)override;
    void visitComplexSelector(ComplexSelector* complex)override;
    void visitCompoundSelector(CompoundSelector* compound)override;
    void visitSelectorCombinator(SelectorCombinator* combinator)override;
    void visitIDSelector(IDSelector* id)override;
    void visitPlaceholderSelector(PlaceholderSelector* placeholder)override;
    void visitPseudoSelector(PseudoSelector* pseudo)override;
    void visitSelectorList(SelectorList* list)override;
    void visitTypeSelector(TypeSelector* type)override;


  };

}
#endif
