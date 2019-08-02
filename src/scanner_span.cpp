#include <iostream>

#include "sass.hpp"
#include "charcode.hpp"
#include "character.hpp"
#include "scanner_span.hpp"
#include "error_handling.hpp"

namespace Sass {

  SpanScanner::SpanScanner(
    Logger& logger,
    SourceDataObj source) :
    LineScanner(logger, source)
  {
  }

}
