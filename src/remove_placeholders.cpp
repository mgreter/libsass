// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"
#include "ast.hpp"

#include "remove_placeholders.hpp"

namespace Sass {

    Remove_Placeholders::Remove_Placeholders()
    { }

    bool isInvisible(Statement* stmt) {
      return stmt->is_invisible();
    }

    void Remove_Placeholders::operator()(Root* b) {

      for (size_t i = 0, L = b->length(); i < L; ++i) {
        if (b->get(i)) b->get(i)->perform(this);
      }

      auto& foo = b->elements();

      foo.erase(std::remove_if(foo.begin(), foo.end(), isInvisible), foo.end());

    }

    void Remove_Placeholders::operator()(Block* b) {

      for (size_t i = 0, L = b->length(); i < L; ++i) {
        if (b->get(i)) b->get(i)->perform(this);
      }

      auto& foo = b->elements();

      foo.erase(std::remove_if(foo.begin(), foo.end(), isInvisible), foo.end());

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
      if (complex->has_placeholder()) {
        complex->clear(); // remove all
      }
      else {
        for (size_t i = 0, L = complex->length(); i < L; ++i) {
          if (CompoundSelector * compound = complex->get(i)->getCompound()) {
            if (compound) remove_placeholders(compound);
          }
        }
        listEraseItemIf(complex->elements(), listIsEmpty<SelectorComponent>);
      }
      if (complex->isImpossible()) {
        complex->clear(); // remove all
      }
    }

    SelectorList* Remove_Placeholders::remove_placeholders(SelectorList* sl)
    {
      for (size_t i = 0, L = sl->length(); i < L; ++i) {
        if (sl->get(i)) remove_placeholders(sl->get(i));
      }
      listEraseItemIf(sl->elements(), listIsEmpty<ComplexSelector>);
      return sl;
    }

    void Remove_Placeholders::operator()(CssMediaRule* rule)
    {
      for (auto stmt : rule->elements()) {
        stmt->perform(this);
      }
    }

    void Remove_Placeholders::operator()(CssStyleRule* r)
    {
      if (SelectorListObj sl = r->selector()) {
        // Set the new placeholder selector list
        r->selector((remove_placeholders(sl)));
      }
      // Iterate into child blocks
      for (size_t i = 0, L = r->length(); i < L; ++i) {
        if (r->get(i)) { r->get(i)->perform(this); }
      }
    }

    void Remove_Placeholders::operator()(SupportsRule* m)
    {
      if (m) m->Block::perform(this);
    }

    void Remove_Placeholders::operator()(CssSupportsRule* m)
    {
      for (auto stmt : m->elements()) {
        stmt->perform(this);
      }
    }

    void Remove_Placeholders::operator()(AtRule* a)
    {
    }

}
