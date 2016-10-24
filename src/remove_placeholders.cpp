#include "sass.hpp"
#include "remove_placeholders.hpp"
#include "context.hpp"
#include "inspect.hpp"
#include <iostream>

namespace Sass {

    Remove_Placeholders::Remove_Placeholders(Context& ctx)
    : ctx(ctx)
    { }

    void Remove_Placeholders::operator()(Block_Ptr b) {
        for (size_t i = 0, L = b->length(); i < L; ++i) {
            (*b)[i]->perform(this);
        }
    }

    CommaSequence_Selector_Ptr Remove_Placeholders::remove_placeholders(CommaSequence_Selector_Ptr sl)
    {
      CommaSequence_Selector_Ptr new_sl = SASS_MEMORY_NEW(ctx.mem, CommaSequence_Selector, sl->pstate());

      for (size_t i = 0, L = sl->length(); i < L; ++i) {
          if (!(*sl)[i]->contains_placeholder()) {
              *new_sl << (*sl)[i];
          }
      }

      return new_sl;

    }


    void Remove_Placeholders::operator()(Ruleset_Ptr r) {
        // Create a new selector group without placeholders
        CommaSequence_Selector_Ptr sl = static_cast<CommaSequence_Selector_Ptr>(r->selector());

        if (sl) {
          // Set the new placeholder selector list
          r->selector(remove_placeholders(sl));
          // Remove placeholders in wrapped selectors
          for (Sequence_Selector_Ptr cs : *sl) {
            while (cs) {
              if (cs->head()) {
                for (Simple_Selector* ss : *cs->head()) {
                  if (Wrapped_Selector_Ptr ws = dynamic_cast<Wrapped_Selector_Ptr>(ss)) {
                    if (CommaSequence_Selector_Ptr sl = dynamic_cast<CommaSequence_Selector_Ptr>(ws->selector())) {
                      CommaSequence_Selector_Ptr clean = remove_placeholders(sl);
                      // also clean superflous parent selectors
                      // probably not really the correct place
                      clean->remove_parent_selectors();
                      ws->selector(clean);
                    }
                  }
                }
              }
              cs = cs->tail();
            }
          }
        }

        // Iterate into child blocks
        Block_Obj b = r->block();

        for (size_t i = 0, L = b->length(); i < L; ++i) {
            if (b->at(i)) b->at(i)->perform(this);
        }
    }

    void Remove_Placeholders::operator()(Media_Block_Ptr m) {
        operator()(m->block());
    }
    void Remove_Placeholders::operator()(Supports_Block_Ptr m) {
        operator()(m->block());
    }

    void Remove_Placeholders::operator()(Directive_Ptr a) {
        if (a->block()) a->block()->perform(this);
    }

}
