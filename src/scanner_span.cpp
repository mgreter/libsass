#include <iostream>

#include "sass.hpp"
#include "charcode.hpp"
#include "character.hpp"
#include "scanner_span.hpp"
#include "error_handling.hpp"

namespace Sass {

  SpanScanner::SpanScanner(const char* content, const char* path, size_t srcid) :
    LineScanner(content, path, srcid)
  {
  }

}
