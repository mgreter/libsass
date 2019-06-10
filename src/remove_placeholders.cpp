// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include "remove_placeholders.hpp"
#include "context.hpp"
#include "inspect.hpp"
#include "debugger.hpp"
#include <iostream>
#include <algorithm>

template<class T, class UnaryPredicate>
void Vec_RemoveAll_If(std::vector<T>& vec, UnaryPredicate* predicate)
{
  vec.erase(std::remove_if(vec.begin(), vec.end(), predicate), vec.end());
}


namespace Sass {

    Remove_Placeholders::Remove_Placeholders()
    { }

    void Remove_Placeholders::operator()(Block* b) {
        for (size_t i = 0, L = b->length(); i < L; ++i) {
            Statement* st = b->at(i);
            st->perform(this);
        }
    }

    Selector_List* Remove_Placeholders::remove_placeholders(Selector_List* sl)
    {
      Selector_List* new_sl = SASS_MEMORY_NEW(Selector_List, sl->pstate());

      for (size_t i = 0, L = sl->length(); i < L; ++i) {
          if (!sl->at(i)->contains_placeholder()) {
              new_sl->append(sl->at(i));
          }
      }

      return new_sl;

    }

// Remove empty pseudo/simple
// Remove empty compound
// Remove empty complex
// Leave empty lists

    template <class T>
    bool checkIsEmpty(T* cnt) {
      return cnt && cnt->empty();
    }

    void Remove_Placeholders::remove_placeholders(Simple_Selector* simple)
    {
      if (simple == nullptr) return;
      if (Pseudo_Selector * pseudo = simple->getPseudoSelector()) {
        if (pseudo->selector2()) remove_placeholders(pseudo->selector2());
      }
    }

    void Remove_Placeholders::remove_placeholders(CompoundSelector* compound)
    {
      if (compound == nullptr) return;
      for (size_t i = 0, L = compound->length(); i < L; ++i) {
        remove_placeholders(compound->get(i));
      }
      Vec_RemoveAll_If(compound->elements(), checkIsEmpty<Simple_Selector>);
    }

    void Remove_Placeholders::remove_placeholders(ComplexSelector* complex)
    {
      if (complex == nullptr) return;
      if (complex->contains_placeholder()) {
        complex->clear(); // remove all
      }
      for (size_t i = 0, L = complex->length(); i < L; ++i) {
        if (CompoundSelector* compound = complex->get(i)->getCompound()) {
          remove_placeholders(compound);
        }
      }
      Vec_RemoveAll_If(complex->elements(), checkIsEmpty<CompoundOrCombinator>);
    }

    SelectorList* Remove_Placeholders::remove_placeholders(SelectorList* sl)
    {
      for (size_t i = 0, L = sl->length(); i < L; ++i) {
        remove_placeholders(sl->at(i));
      }
      Vec_RemoveAll_If(sl->elements(), checkIsEmpty<ComplexSelector>);
      return sl;
    }

    void Remove_Placeholders::operator()(Ruleset* r) {
        // Create a new selector group without placeholders
        SelectorList_Obj sl = r->selector2();

        if (sl) {
          // Set the new placeholder selector list
          r->selector2((remove_placeholders(sl)));
        }

        // Iterate into child blocks
        Block_Obj b = r->block();

        for (size_t i = 0, L = b->length(); i < L; ++i) {
            if (b->at(i)) {
                Statement_Obj st = b->at(i);
                st->perform(this);
            }
        }
    }

    void Remove_Placeholders::operator()(Media_Block* m) {
        operator()(m->block());
    }
    void Remove_Placeholders::operator()(Supports_Block* m) {
        operator()(m->block());
    }

    void Remove_Placeholders::operator()(Directive* a) {
        if (a->block()) a->block()->perform(this);
    }

}
