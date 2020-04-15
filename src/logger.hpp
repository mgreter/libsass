#ifndef SASS_LOGGER_H
#define SASS_LOGGER_H

// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include <sstream>
#include "strings.hpp"
#include "constants.hpp"
#include "backtrace.hpp"
#include "callstack.hpp"

namespace Sass {

  class Logger {

  public:

    // Epsilon for precision
    double epsilon;

    // output buffers
    sass::ostream errors;
    sass::ostream output;

    // The current callstack
    BackTraces callStack;

    enum Sass_Logger_Style style;

  private:

    // Split the line to three parts for error reporting.
    // The `lhs_len` is the position where the error reporting
    // should start and `mid_len` how many characters should be
    // highlighted. It will fill the strings `lhs`, `mid` and `rhs`
    // accordingly so the total length is not bigger than `columns`.
    // Strings might be shortened and ellipsis are added as needed.
    void splitLine(
      sass::string line,
      size_t lhs_len,
      size_t mid_len,
      size_t columns,
      sass::string& lhs,
      sass::string& mid,
      sass::string& rhs);

    // Helper function to ease color output. Returns the
    // passed color if color output is enable, otherwise
    // it will simply return an empty string.
    inline const char* getColor(const char* color) {
      if (style & SASS_LOGGER_COLOR) {
        return color;
      }
      return Constants::String::empty;
    }

    // Write warning header to error stream
    void writeWarnHead(
      bool deprecation = false);

  public:

    void writeStackTraces(sass::ostream& os,
													StackTraces traces,
													sass::string indent = "  ",
													bool showPos = true,
													size_t showTraces = sass::string::npos);



  public:

    Logger(size_t precision, enum Sass_Logger_Style style = SASS_LOGGER_AUTO);

    void format_pstate(sass::ostream& msg_stream, SourceSpan pstate, enum Sass_Logger_Style logstyle);

    static Logger* create(SassContextCpp& options);
    /*
    virtual void warn(sass::string message) const = 0;
    virtual void debug(sass::string message) const = 0;
    virtual void error(sass::string message) const = 0;
    void addWarn(const sass::string& message);
    */


    void addWarn43(const sass::string& message, bool deprecation = false);
    void addWarn33(const sass::string& message, const SourceSpan& pstate, bool deprecation = false);

  public:

    operator BackTraces&() {
      return callStack; }

    BackTraces& qwe() {
      return callStack;
    }

    virtual ~Logger() {};

  };
  /*
  class StdLogger :
    public Logger {

  public:

    StdLogger(size_t precision, enum Sass_Logger_Style style = SASS_LOGGER_AUTO);

    void warn(sass::string message) const override final;
    void debug(sass::string message) const override final;
    void error(sass::string message) const override final;

    ~StdLogger() override final {};

  };

  class NullLogger :
    public Logger {

  public:

    void warn(sass::string message) const override final {};
    void debug(sass::string message) const override final {};
    void error(sass::string message) const override final {};

    ~NullLogger() override final {};

  };
  */
}

#endif
