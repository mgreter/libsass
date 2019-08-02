#ifndef SASS_POSITION_H
#define SASS_POSITION_H

#include <string>
#include <cstring>
#include "offset.hpp"
#include "ast_fwd_decl.hpp"
#include "ast_def_macros.hpp"
// #include <iostream>

namespace Sass {

  class LineScanner;


  // Position is LineScannerState2
  class Position : public Offset {

    public: // c-tor
      Position(LineScanner& scanner);
      const char* position;
  };




}

#endif
