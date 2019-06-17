#ifndef SASS_INSPECT_H
#define SASS_INSPECT_H

#include "position.hpp"
#include "operation.hpp"
#include "emitter.hpp"

namespace Sass {
  class Context;

  class Inspect : public Operation_CRTP<void, Inspect>, public Emitter {
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
    // virtual void operator()(Supports_Block*);
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
    virtual void operator()(StringExpression2*);

    virtual void operator()(Custom_Error*);
    virtual void operator()(Custom_Warning*);
    virtual void operator()(Supports_Operator*);
    virtual void operator()(Supports_Negation*);
    virtual void operator()(Supports_Declaration*);
    virtual void operator()(Supports_Interpolation*);
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
    virtual void operator()(TypeSelector*);
    virtual void operator()(ClassSelector*);
    virtual void operator()(IDSelector*);
    virtual void operator()(AttributeSelector*);
    virtual void operator()(PseudoSelector*);
    // virtual void operator()(SelectorComponent*);
    virtual void operator()(SelectorCombinator*);
    virtual void operator()(CompoundSelector*);
    virtual void operator()(ComplexSelector*);
    virtual void operator()(SelectorList*);
    virtual std::string lbracket(List*);
    virtual std::string rbracket(List*);

  };

}
#endif
