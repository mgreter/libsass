#ifndef SASS_EXCEPTION_H
#define SASS_EXCEPTION_H

#include <stdexcept>

#include "error_handling.hpp"

namespace Sass {

  /**
   * Exception thrown when a string or some other data does not have an expected
   * format and cannot be parsed or processed.
   */
  class FormatException :
    public std::runtime_error
  {
    FormatException();
  }

  class SourceSpanFormatException :
    public FormatException
  {
    SourceSpanFormatException();
  };

}

#endif
