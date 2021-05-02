/*****************************************************************************/
/* Part of LibSass, released under the MIT license (See LICENSE.txt).        */
/*****************************************************************************/
#include "remove_placeholders.hpp"

#include "ast_selectors.hpp"

namespace Sass {

    bool isInvisibleCss(CssNode* stmt) {
      return stmt->isInvisibleCss();
    }

    void RemovePlaceholders::remove_placeholders(SimpleSelector* simple)
    {
      if (PseudoSelector * pseudo = simple->isaPseudoSelector()) {
        if (pseudo->selector()) remove_placeholders(pseudo->selector());
      }
    }

    void RemovePlaceholders::remove_placeholders(CompoundSelector* compound)
    {
      for (size_t i = 0, L = compound->size(); i < L; ++i) {
        if (compound->get(i)) remove_placeholders(compound->get(i));
      }
      // listEraseItemIf(compound->elements(), listIsInvisible<SimpleSelector>);
      // listEraseItemIf(compound->elements(), listIsEmpty<SimpleSelector>);
    }

    void RemovePlaceholders::remove_placeholders(ComplexSelector* complex)
    {
      for (const SelectorComponentObj& component : complex->elements()) {
        if (component->hasPlaceholder()) {
          complex->clear(); // remove all
          return;
        }
        if (CompoundSelector* compound = component->isaCompoundSelector()) {
          if (compound) remove_placeholders(compound);
        }
      }
      listEraseItemIf(complex->elements(), listIsEmpty<SelectorComponent>);
      // ToDo: describe what this means
      if (complex->hasInvisible()) {
        complex->clear(); // remove all
      }
    }

    void RemovePlaceholders::remove_placeholders(SelectorList* sl)
    {
      for(const ComplexSelectorObj& complex : sl->elements()) {
        if (complex) remove_placeholders(complex);
      }
      listEraseItemIf(sl->elements(), listIsEmpty<ComplexSelector>);
    }

    void RemovePlaceholders::visitCssRoot(CssRoot* b)
    {
      // Clean up all our children
      acceptCssParentNode(b);
      // Remove all invisible items
      listEraseItemIf(b->elements(), isInvisibleCss); // QQQQQQQQQQQQHHHHHH
    }

    void RemovePlaceholders::visitCssStyleRule(CssStyleRule* rule)
    {
      // Clean up all our children
      acceptCssParentNode(rule);

      if (SelectorList* sl = rule->selector()) {
        remove_placeholders(sl);
      }
    }

    void RemovePlaceholders::acceptCssParentNode(CssParentNode* m)
    {
      for (CssNodeObj& node : m->elements()) {
        node->accept(this);
      }
    }

}
