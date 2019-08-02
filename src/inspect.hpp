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

    void visitBlockStatements(sass::vector<StatementObj> children);
    void visitQuotedString(const sass::string& text, uint8_t quotes = 0);

    virtual void operator()(CssValue*) override;
    virtual void operator()(Variable* node) override;
    virtual void operator()(CssString*) override;
    virtual void operator()(CssAtRule*) override;
    virtual void operator()(CssComment*) override;
    virtual void operator()(CssDeclaration*) override;

    // statements
    virtual void operator()(Block*) override;
    virtual void operator()(CssKeyframeBlock*) override;

    // virtual void operator()(Keyframe_Rule*);
    // virtual void operator()(Bubble*);
    // virtual void operator()(SupportsRule*);
    // virtual void operator()(CssAtRootRule*);
    // virtual void operator()(AtRule*) override;
    // virtual void operator()(Declaration*) override;
    // virtual void operator()(Assignment*);
    virtual void operator()(Import*) override;
    // virtual void operator()(Import_Stub*);

    virtual void operator()(ImportRule*) override;
    virtual void operator()(StaticImport*) override;
    // virtual void operator()(DynamicImport*);
    // virtual void operator()(WarnRule*);
    // virtual void operator()(ErrorRule*);
    // virtual void operator()(DebugRule*);
    void _writeMapElement(Expression* value);
    // virtual void operator()(If*);
    // virtual void operator()(For*);
    // virtual void operator()(Each*);
    // virtual void operator()(WhileRule*);
    // virtual void operator()(Return*);
    // virtual void operator()(ExtendRule*);
    // virtual void operator()(ContentRule*);
    // expressions
    virtual void operator()(Map*) override;
    virtual void operator()(SassFunction*) override;
    // virtual void operator()(Callable*) override;
    virtual void operator()(BuiltInCallable*) override;
    virtual void operator()(BuiltInCallables*) override;
    virtual void operator()(UserDefinedCallable*) override;
    virtual void operator()(SassList*) override;
    // virtual void operator()(ValueExpression* expr) override;
	  // virtual void operator()(ParenthesizedExpression* expr) override;
    // virtual void operator()(ParenthesizedExpression*);
    // virtual void operator()(Binary_Expression*) override;
    virtual void operator()(Unary_Expression*) override;
    // virtual void operator()(Custom_Warning*);
    // virtual void operator()(Custom_Error*);
    // virtual void operator()(Variable*) override;
    virtual void operator()(Number*) override;
    virtual void operator()(Color_RGBA*) override;
    virtual void operator()(Color_HSLA*) override;
    virtual void operator()(Boolean*) override;
    virtual void operator()(String_Constant*) override;
    virtual void operator()(StringLiteral*) override;
    virtual void operator()(Interpolation*) override;
    // virtual void operator()(StringExpression*) override;

    virtual void operator()(Custom_Error*) override;
    virtual void operator()(Custom_Warning*) override;
    // virtual void operator()(MediaRule*) override;
    virtual void operator()(CssMediaRule*) override;
    virtual void operator()(CssMediaQuery*) override;
    virtual void operator()(Null*) override;
    // virtual void operator()(Parent_Reference* p);
    // parameters and arguments
    // virtual void operator()(Argument*) override;
    // selectors
    virtual void operator()(PlaceholderSelector*) override;
    virtual void operator()(ClassSelector*) override;
    // virtual void operator()(SelectorComponent*);
    virtual void operator()(SelectorCombinator*) override;
    virtual void operator()(CompoundSelector*) override;
    virtual void operator()(SelectorList*) override;

    virtual void operator()(Trace*) override;


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



    void visitCssAtRule(CssAtRule* node);
    void visitCssComment(CssComment* node);
    void visitCssDeclaration(CssDeclaration* node);
    // T visitCssImport(CssImport node);
    // T visitCssKeyframeBlock(CssKeyframeBlock node);
    void visitCssMediaRule(CssMediaRule* node);
    void visitCssStyleRule(CssStyleRule* node);
    // T visitCssStylesheet(CssStylesheet node);
    // T visitCssSupportsRule(CssSupportsRule node);

    // Not part of css visitors (only used here)
    void visitCssMediaQuery(CssMediaQuery* query);

  };

}
#endif
