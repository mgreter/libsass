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
    virtual void operator()(Block*) override;
    // virtual void operator()(Keyframe_Rule*);
    // virtual void operator()(Bubble*);
    // virtual void operator()(SupportsRule*);
    // virtual void operator()(At_Root_Block*);
    virtual void operator()(AtRule*) override;
    virtual void operator()(Declaration*) override;
    // virtual void operator()(Assignment*);
    virtual void operator()(Import*) override;
    // virtual void operator()(Import_Stub*);

    virtual void operator()(ImportRule*) override;
    virtual void operator()(StaticImport*) override;
    // virtual void operator()(DynamicImport*);
    // virtual void operator()(Warning*);
    // virtual void operator()(Error*);
    // virtual void operator()(Debug*);
    virtual void operator()(CssComment*) override;
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
    virtual void operator()(Map*) override;
    virtual void operator()(Function*) override;
    virtual void operator()(SassFunction*) override;
    virtual void operator()(Callable*) override;
    virtual void operator()(BuiltInCallable*) override;
    virtual void operator()(UserDefinedCallable*) override;
    virtual void operator()(List*) override;
    virtual void operator()(SassList*) override;
    virtual void operator()(ValueExpression* expr) override;
	  virtual void operator()(ParenthesizedExpression* expr) override;
    // virtual void operator()(ParenthesizedExpression*);
    virtual void operator()(Binary_Expression*) override;
    virtual void operator()(Unary_Expression*) override;
    virtual void operator()(FunctionExpression*) override;
    // virtual void operator()(Custom_Warning*);
    // virtual void operator()(Custom_Error*);
    virtual void operator()(Variable*) override;
    virtual void operator()(Number*) override;
    virtual void operator()(Color_RGBA*) override;
    virtual void operator()(Color_HSLA*) override;
    virtual void operator()(Boolean*) override;
    virtual void operator()(String_Constant*) override;
    virtual void operator()(String_Quoted*) override;

    virtual void operator()(StringLiteral*) override;
    virtual void operator()(Interpolation*) override;
    virtual void operator()(StringExpression*) override;

    virtual void operator()(Custom_Error*) override;
    virtual void operator()(Custom_Warning*) override;
    virtual void operator()(MediaRule*) override;
    virtual void operator()(CssMediaRule*) override;
    virtual void operator()(CssMediaQuery*) override;
    // virtual void operator()(At_Root_Query*);
    virtual void operator()(Null*) override;
    // virtual void operator()(Parent_Reference* p);
    // parameters and arguments
    // virtual void operator()(Parameter*);
    // virtual void operator()(Parameters*);
    virtual void operator()(Argument*) override;
    virtual void operator()(Arguments*) override;
    // selectors
    virtual void operator()(PlaceholderSelector*) override;
    virtual void operator()(ClassSelector*) override;
    // virtual void operator()(SelectorComponent*);
    virtual void operator()(SelectorCombinator*) override;
    virtual void operator()(CompoundSelector*) override;
    virtual void operator()(SelectorList*) override;
    virtual std::string lbracket(List*);
    virtual std::string rbracket(List*);
    virtual std::string lbracket(SassList*);
    virtual std::string rbracket(SassList*);

    // Implement those first without override
    void visitAttributeSelector(AttributeSelector* attribute) override;
    void visitClassSelector(ClassSelector* klass) override;
    void visitComplexSelector(ComplexSelector* complex) override;
    void visitCompoundSelector(CompoundSelector* compound) override;
    void visitSelectorCombinator(SelectorCombinator* combinator) override;
    void visitIDSelector(IDSelector* id) override;
    void visitPlaceholderSelector(PlaceholderSelector* placeholder) override;
    void visitPseudoSelector(PseudoSelector* pseudo) override;
    void visitSelectorList(SelectorList* list) override;
    void visitTypeSelector(TypeSelector* type) override;


  };

}
#endif
