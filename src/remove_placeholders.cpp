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
            Statement_Ptr st = &b->at(i);
            // debug_ast(st);
            st->perform(this);
        }
    }

    CommaComplex_Selector_Ptr Remove_Placeholders::remove_placeholders(CommaComplex_Selector_Ptr sl)
    {
      CommaComplex_Selector_Ptr new_sl = SASS_MEMORY_NEW(ctx.mem, CommaComplex_Selector, sl->pstate());

      for (size_t i = 0, L = sl->length(); i < L; ++i) {
          if (!(*sl)[i]->contains_placeholder()) {
              *new_sl << (*sl)[i];
          }
      }

      return new_sl;

    }


    void Remove_Placeholders::operator()(Ruleset_Ptr r) {
        // Create a new selector group without placeholders
        CommaComplex_Selector_Ptr sl = SASS_MEMORY_CAST(CommaComplex_Selector, r->selector());

        if (sl) {
          // Set the new placeholder selector list
          r->selector(remove_placeholders(sl));
          // Remove placeholders in wrapped selectors
          for (Complex_Selector_Obj cs : sl->elements()) {
            while (cs) {
              if (cs->head()) {
                for (Simple_Selector_Obj ss : cs->head()->elements()) {
                  if (Wrapped_Selector_Ptr ws = SASS_MEMORY_CAST(Wrapped_Selector, ss)) {
                    if (CommaComplex_Selector_Ptr sl = SASS_MEMORY_CAST(CommaComplex_Selector, ws->selector())) {
                      CommaComplex_Selector_Ptr clean = remove_placeholders(sl);
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
        Block_Obj b = r->oblock();

        for (size_t i = 0, L = b->length(); i < L; ++i) {
			if (b->at(i)) {
				Statement_Obj st = b->at(i);
				st->perform(this);
			}
        }
    }

    void Remove_Placeholders::operator()(Media_Block_Ptr m) {
        operator()(&m->oblock());
    }
    void Remove_Placeholders::operator()(Supports_Block_Ptr m) {
        operator()(&m->oblock());
    }

    void Remove_Placeholders::operator()(Directive_Ptr a) {
        if (a->oblock()) a->oblock()->perform(this);
    }

}
