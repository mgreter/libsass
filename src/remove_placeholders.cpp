// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include "remove_placeholders.hpp"
#include "context.hpp"
#include "inspect.hpp"
#include <iostream>

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
    /*
    Pseudo_Selector* Remove_Placeholders::remove_placeholders(Pseudo_Selector* sl)
    {
      SelectorList* new_sl = SASS_MEMORY_NEW(SelectorList, sl->pstate());

      for (size_t i = 0, L = sl->length(); i < L; ++i) {
        if (!sl->at(i)->hasPlaceholder()) {
          new_sl->append(sl->at(i));
        }
      }

      return new_sl;

    }

    CompoundSelector* Remove_Placeholders::remove_placeholders(CompoundSelector* sl)
    {
      SelectorList* new_sl = SASS_MEMORY_NEW(CompoundSelector, sl->pstate());

      for (size_t i = 0, L = sl->length(); i < L; ++i) {
        if (!sl->at(i)->hasPlaceholder()) {
          new_sl->append(sl->at(i));
        }
      }

      return new_sl;

    }


    ComplexSelector* Remove_Placeholders::remove_placeholders(ComplexSelector* sl)
    {
      SelectorList* new_sl = SASS_MEMORY_NEW(SelectorList, sl->pstate());

      for (size_t i = 0, L = sl->length(); i < L; ++i) {
        if (!sl->at(i)->hasPlaceholder()) {
          new_sl->append(sl->at(i));
        }
      }

      return new_sl;

    }
    */
    SelectorList* Remove_Placeholders::remove_placeholders(SelectorList* sl)
    {
      SelectorList* new_sl = SASS_MEMORY_NEW(SelectorList, sl->pstate());

      for (size_t i = 0, L = sl->length(); i < L; ++i) {
        // new_sl->append(remove_placeholders(sl->at(i)));
      }

      return new_sl;

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
