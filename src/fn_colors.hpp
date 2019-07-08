/*****************************************************************************/
/* Part of LibSass, released under the MIT license (See LICENSE.txt).        */
/*****************************************************************************/
#ifndef SASS_FN_COLORS_HPP
#define SASS_FN_COLORS_HPP

// sass.hpp must go before all system headers
// to get the __EXTENSIONS__ fix on Solaris.
#include "capi_sass.hpp"

#include "fn_utils.hpp"

namespace Sass {

  namespace Functions {

    /////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////

    Value* rgbFn(
      const sass::string& name,
      const ValueVector& arguments,
      const SourceSpan& pstate,
      Logger& logger);

    Value* hslFn(
      const sass::string& name,
      const ValueVector& arguments,
      const SourceSpan& pstate,
      Logger& logger);

    Value* hwbFn(
      const sass::string& name,
      const ValueVector& arguments,
      const SourceSpan& pstate,
      Logger& logger);

    /////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////

    namespace Colors {

      // Register all functions at compiler
      void registerFunctions(Compiler& ctx);

    }

    /////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////

  }

}

#endif
