// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include "extend.hpp"
#include "extension.hpp"
#include "context.hpp"
#include "backtrace.hpp"
#include "paths.hpp"
#include "parser.hpp"
#include "expand.hpp"
#include "node.hpp"
#include "sass_util.hpp"
#include "remove_placeholders.hpp"
#include "debugger.hpp"
#include "debug.hpp"
#include <iostream>
#include <deque>
#include <set>

namespace Sass {

  Extension Extension::withExtender(ComplexSelector_Obj newExtender)
  {
    Extension extension(newExtender);
    extension.specificity = specificity;
    extension.isOptional = isOptional;
    extension.target = target;
    return extension;
  }

}
