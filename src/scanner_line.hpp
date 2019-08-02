#ifndef SASS_SCANNER_LINE_H
#define SASS_SCANNER_LINE_H

#include "source.hpp"
#include "position.hpp"
#include "scanner_string.hpp"

namespace Sass {

  class LineScanner
    : public StringScanner {

  public:

    LineScanner(
      Logger& logger,
      SourceDataObj source);

  };

}

#endif
