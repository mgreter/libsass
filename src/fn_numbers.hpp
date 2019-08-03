#ifndef SASS_FN_NUMBERS_H
#define SASS_FN_NUMBERS_H

#include "fn_utils.hpp"

namespace Sass {

  namespace Functions {

    namespace Math {
      BUILT_IN_FN(round);
      BUILT_IN_FN(ceil);
      BUILT_IN_FN(floor);
      BUILT_IN_FN(abs);
      BUILT_IN_FN(max);
      BUILT_IN_FN(min);
      BUILT_IN_FN(random);
      BUILT_IN_FN(unit);
      BUILT_IN_FN(isUnitless);
      BUILT_IN_FN(percentage);
      BUILT_IN_FN(compatible);   
    }


    // return a number object (copied since we want to have reduced units)
    #define ARGN(argname) get_arg_n(argname, env, sig, pstate, traces) // Number copy

    extern Signature percentage_sig;
    extern Signature round_sig;
    extern Signature ceil_sig;
    extern Signature floor_sig;
    extern Signature abs_sig;
    extern Signature min_sig;
    extern Signature max_sig;
    extern Signature inspect_sig;
    extern Signature random_sig;
    extern Signature unique_id_sig;
    extern Signature unit_sig;
    extern Signature unitless_sig;
    extern Signature comparable_sig;

    BUILT_IN(percentage);
    BUILT_IN(round);
    BUILT_IN(ceil);
    BUILT_IN(floor);
    BUILT_IN(abs);
    BUILT_IN(min);
    BUILT_IN(max);
    BUILT_IN(inspect);
    BUILT_IN(random);
    BUILT_IN(unique_id);
    BUILT_IN(unit);
    BUILT_IN(unitless);
    BUILT_IN(comparable);

  }

}

#endif
