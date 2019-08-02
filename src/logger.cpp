#include "logger.hpp"
// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include <iostream>
#include <typeinfo>
#include <string>
#include <cmath>

#include "logger.hpp"

namespace Sass {

  Logger::Logger(size_t precision) :
    epsilon(std::pow(0.1, precision + 1))
  {}

  Logger* Logger::create(Sass_Options& options)
  {
    int precision = sass_option_get_precision(&options);
    // Nothing implemented yet beside std loggger.
    // ToDo: add options to C-API to choose here.
    return new StdLogger(precision);
  }

  StdLogger::StdLogger(size_t precision) :
    Logger(precision)
  {
  }

  void StdLogger::warn(sass::string message) const
  {
  }

  void StdLogger::debug(sass::string message) const
  {
  }

  void StdLogger::error(sass::string message) const
  {
  }

  // Create object and add frame to stack
  callStackFrame::callStackFrame(
    Backtraces& callStack,
    const Backtrace& frame,
    bool viaCall) :
    callStack(callStack),
    frame(frame),
    viaCall(viaCall)
  {
    // Append frame to stack
    if (!viaCall) callStack.push_back(frame);
  }

  // Remove frame from stack on desctruction
  callStackFrame::~callStackFrame()
  {
    // Pop frame from stack
    if (!viaCall) callStack.pop_back();
  }

}
