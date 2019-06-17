// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include "stylesheet.hpp"

namespace Sass {

  // Constructor
  Sass::StyleSheet::StyleSheet(const Resource& res, Block_Obj root) :
    Resource(res),
    plainCss(false),
    syntax(SASS_IMPORT_AUTO),
    root(root)
  {
  }

  StyleSheet::StyleSheet(const StyleSheet& sheet) :
    Resource(sheet),
    plainCss(sheet.plainCss),
    syntax(sheet.syntax),
    root(sheet.root)
  {
  }

}
