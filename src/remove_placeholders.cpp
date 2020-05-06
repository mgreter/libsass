// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"
#include "ast.hpp"

#include "remove_placeholders.hpp"

namespace Sass {

    Remove_Placeholders::Remove_Placeholders()
    { }

    bool isInvisibleCss(CssNode* stmt) {
      return stmt->isInvisibleCss();
    }

    void Remove_Placeholders::remove_placeholders(SimpleSelector* simple)
    {
      if (PseudoSelector * pseudo = simple->getPseudoSelector()) {
        if (pseudo->selector()) remove_placeholders(pseudo->selector());
      }
    }

    void Remove_Placeholders::remove_placeholders(CompoundSelector* compound)
    {
      for (size_t i = 0, L = compound->length(); i < L; ++i) {
        if (compound->get(i)) remove_placeholders(compound->get(i));
      }
      listEraseItemIf(compound->elements(), listIsInvisible<SimpleSelector>);
      // listEraseItemIf(compound->elements(), listIsEmpty<SimpleSelector>);
    }

    void Remove_Placeholders::remove_placeholders(ComplexSelector* complex)
    {
      if (complex->hasPlaceholderCplxSel()) {
        complex->clear(); // remove all
      }
      else {
        for (SelectorComponentObj& selector : complex->elements()) {
          if (CompoundSelector * compound = selector->getCompound()) {
            if (compound) remove_placeholders(compound);
          }
        }
        listEraseItemIf(complex->elements(), listIsEmpty<SelectorComponent>);
      }
      // ToDo: describe what this means
      if (complex->isImpossible()) {
        complex->clear(); // remove all
      }
    }

    void Remove_Placeholders::remove_placeholders(SelectorList* sl)
    {
      for(const ComplexSelectorObj& complex : sl->elements()) {
        if (complex) remove_placeholders(complex);
      }
      listEraseItemIf(sl->elements(), listIsEmpty<ComplexSelector>);
    }

    void Remove_Placeholders::operator()(CssRoot* b)
    {
      // Clean up all our children
      b->CssParentNode::perform(this);
      // Remove all invisible items
      listEraseItemIf(b->elements(), isInvisibleCss);
    }

    void Remove_Placeholders::operator()(CssStyleRule* rule)
    {
      // Clean up all our children
      rule->CssParentNode::perform(this);

      if (SelectorList* sl = rule->selector()) {
        remove_placeholders(sl);
      }
    }

    void Remove_Placeholders::operator()(CssParentNode* m)
    {
      for (CssNodeObj& stmt : m->elements()) {
        stmt->perform(this);
      }
    }

}
