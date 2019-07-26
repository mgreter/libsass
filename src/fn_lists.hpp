#ifndef SASS_FN_LISTS_H
#define SASS_FN_LISTS_H

#include "fn_utils.hpp"

namespace Sass {

  namespace Functions {

    namespace Lists {
      BUILT_IN_FN(length);
      BUILT_IN_FN(nth);
      BUILT_IN_FN(setNth);
      BUILT_IN_FN(join);
      BUILT_IN_FN(append);
      BUILT_IN_FN(zip);
      BUILT_IN_FN(index);
      BUILT_IN_FN(separator);
      BUILT_IN_FN(isBracketed);
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
