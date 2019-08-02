#ifndef SASS_SCANNER_SPAN_H
#define SASS_SCANNER_SPAN_H

#include "scanner_line.hpp"

namespace Sass {

  class SpanScanner
    : public LineScanner {

  public:

    SpanScanner(
      Logger& logger,
      SourceDataObj source);

  };

}

#endif
