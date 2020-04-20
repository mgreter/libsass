#ifndef SASS_SASS_ERROR_H
#define SASS_SASS_ERROR_H

// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"
#include "sass/base.h"

#include "memory.hpp"
#include "source_span.hpp"
#include "sass_functions.hpp"

struct SassError {

public:

  // Error status
  int status;

  // Specific error message
  sass::string what;

  // Traces leading up to error
  Sass::StackTraces traces;

  // Streams from logger
  // Also when status is 0
  sass::string messages;
  sass::string warnings;
  sass::string formatted;

  // Constructor
  SassError() :
    status(0)
  {}

  char* getJson(bool include_sources);

  int SassError::set(Sass::Logger& logger, Sass::StackTraces* traces, const char* msg, int status);

};

#endif
