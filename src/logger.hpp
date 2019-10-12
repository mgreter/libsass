#ifndef SASS_LOGGER_H
#define SASS_LOGGER_H

// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include "backtrace.hpp"

namespace Sass {

  class Logger {

  public:

    // Epsilon for precision
    double epsilon;

    // The current callstack
    Backtraces callStack;

  public:

    Logger(size_t precision);

    static Logger* create(Sass_Options& options);

    virtual void warn(sass::string message) const = 0;
    virtual void debug(sass::string message) const = 0;
    virtual void error(sass::string message) const = 0;

  public:

    operator Backtraces&() {
      return callStack; }

    Backtraces& qwe() {
      return callStack;
    }

    virtual ~Logger() {};

  };

  class StdLogger :
    public Logger {

  public:

    StdLogger(size_t precision);

    void warn(sass::string message) const override final;
    void debug(sass::string message) const override final;
    void error(sass::string message) const override final;

    ~StdLogger() override {};

  };

  class NullLogger :
    public Logger {

  public:

    void warn(sass::string message) const override final {};
    void debug(sass::string message) const override final {};
    void error(sass::string message) const override final {};

    ~NullLogger() override {};

  };

  // Utility class to add a frame onto a call stack
  // Once this object goes out of scope, it is removed
  // We assume this happens in well defined order, as
  // we do not check if we actually remove ourself!
  class callStackFrame {

    // Reference to the managed callStack
    Backtraces& callStack;

    // The current stack frame
    Backtrace frame;

    // Are we invoked by `call`
    bool viaCall;

  public:

    // Create object and add frame to stack
    callStackFrame(Backtraces& callStack,
      const Backtrace& frame, bool viaCall = false);

    // Remove frame from stack on desctruction
    ~callStackFrame();

  };

}

#endif
