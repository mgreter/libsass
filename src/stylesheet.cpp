#include "stylesheet.hpp"

namespace Sass {

  // Constructor
  Sass::StyleSheet::StyleSheet(ImportObj import, RootObj root) :
    root2(root)
  {
    root->import = import;
  }

  // We expect more code here once we support modules

}
