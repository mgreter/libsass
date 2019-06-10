#ifndef SASS_REMOVE_PLACEHOLDERS_H
#define SASS_REMOVE_PLACEHOLDERS_H

#pragma once

#include "ast.hpp"
#include "operation.hpp"

namespace Sass {


    class Remove_Placeholders : public Operation_CRTP<void, Remove_Placeholders> {

    public:
      Selector_List* remove_placeholders(Selector_List*);

      void remove_placeholders(Simple_Selector* simple);
      void remove_placeholders(CompoundSelector* complex);
      void remove_placeholders(ComplexSelector* complex);

      SelectorList* remove_placeholders(SelectorList*);

    public:
        Remove_Placeholders();
        ~Remove_Placeholders() { }

        void operator()(Block*);
        void operator()(Ruleset*);
        void operator()(Media_Block*);
        void operator()(Supports_Block*);
        void operator()(Directive*);

      // ignore missed types
        template <typename U>
      void fallback(U x) {}
    };

}

#endif
