// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include "stylesheet.hpp"

namespace Sass {

  // Constructor
  Sass::StyleSheet::StyleSheet(SourceData* source, RootObj root) :
    source(source),
    plainCss(false),
    syntax(SASS_IMPORT_AUTO),
    root2(root)
  {
  }

}
