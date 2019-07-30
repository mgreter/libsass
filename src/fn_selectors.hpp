#ifndef SASS_FN_SELECTORS_H
#define SASS_FN_SELECTORS_H

#include "fn_utils.hpp"

namespace Sass {

  namespace Functions {

    namespace Selectors {
      BUILT_IN_FN(nest);
      BUILT_IN_FN(append);
      BUILT_IN_FN(extend);
      BUILT_IN_FN(replace);
      BUILT_IN_FN(unify);
      BUILT_IN_FN(isSuper);
      BUILT_IN_FN(simple);
      BUILT_IN_FN(parse);
    }

#define ARGSEL(argname) get_arg_sel(argname, env, sig, pstate, traces, ctx)
    #define ARGSELS(argname) get_arg_sels(argname, env, sig, pstate, traces, ctx)

    BUILT_IN(selector_nest);
    BUILT_IN(selector_append);
    BUILT_IN(selector_extend);
    BUILT_IN(selector_replace);
    BUILT_IN(selector_unify);
    BUILT_IN(is_superselector);
    BUILT_IN(simple_selectors);
    BUILT_IN(selector_parse);

    extern Signature selector_nest_sig;
    extern Signature selector_append_sig;
    extern Signature selector_extend_sig;
    extern Signature selector_replace_sig;
    extern Signature selector_unify_sig;
    extern Signature is_superselector_sig;
    extern Signature simple_selectors_sig;
    extern Signature selector_parse_sig;

  }

}

#endif
