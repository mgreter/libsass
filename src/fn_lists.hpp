#ifndef SASS_FN_LISTS_H
#define SASS_FN_LISTS_H

#include "fn_utils.hpp"

namespace Sass {

  namespace Functions {

    namespace Lists {
      Value* length(const std::vector<ValueObj>& arguments);
      Value* nth(const std::vector<ValueObj>& arguments);
      Value* setNth(const std::vector<ValueObj>& arguments);
      Value* join(const std::vector<ValueObj>& arguments);
      Value* append(const std::vector<ValueObj>& arguments);
      Value* zip(const std::vector<ValueObj>& arguments);
      Value* index(const std::vector<ValueObj>& arguments);
      Value* separator(const std::vector<ValueObj>& arguments);
      Value* isBracketed(const std::vector<ValueObj>& arguments);
    }

    extern Signature length_sig;
    extern Signature nth_sig;
    extern Signature index_sig;
    extern Signature join_sig;
    extern Signature append_sig;
    extern Signature zip_sig;
    extern Signature list_separator_sig;
    extern Signature is_bracketed_sig;
    extern Signature keywords_sig;

    BUILT_IN(length);
    BUILT_IN(nth);
    BUILT_IN(index);
    BUILT_IN(join);
    BUILT_IN(append);
    BUILT_IN(zip);
    BUILT_IN(list_separator);
    BUILT_IN(is_bracketed);
    BUILT_IN(keywords);

  }

}

#endif
