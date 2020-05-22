#ifndef SASS_CSSIZE_H
#define SASS_CSSIZE_H

#include "inspect.hpp"
#include "backtrace.hpp"
#include "error_handling.hpp"

namespace Sass {

  class Cssize : public Inspect {
  private:
    Logger& logger;
  protected:
    using Inspect::operator();
  public:
    Cssize(Logger& logger, SassOutputOptionsCpp& opt, bool srcmap_enabled)
      : Inspect(opt, srcmap_enabled), logger(logger)
    {}

    void operator()(Function* f) override;
    void operator()(Number* number) override;
    void operator()(List* f) override;
    void operator()(Map* f) override;

  };

}

#endif
