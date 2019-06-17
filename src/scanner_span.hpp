#ifndef SASS_SCANNER_SPAN_H
#define SASS_SCANNER_SPAN_H

#include "scanner_line.hpp"

namespace Sass {

  class SpanScanner
    : public LineScanner {

  public:

    SpanScanner(
      const char* content,
      const char* path,
      size_t srcid);

  };

}

#endif
