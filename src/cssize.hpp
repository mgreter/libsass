#ifndef SASS_CSSIZE_H
#define SASS_CSSIZE_H

#include "inspect.hpp"
#include "backtrace.hpp"
#include "error_handling.hpp"

namespace Sass {

  class Cssize : public Inspect {
  protected:
    using Inspect::operator();
  public:
    Cssize(SassOutputOptionsCpp& opt, bool srcmap_enabled)
      : Inspect(opt, srcmap_enabled)
    {}

    void operator()(Function* f) override;
    void operator()(List* f) override;
    void operator()(Map* f) override;

  };

}

#endif
