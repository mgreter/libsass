#ifndef SASS_OUTPUT_H
#define SASS_OUTPUT_H

#include <string>
#include <vector>

#include "util.hpp"
#include "inspect.hpp"
#include "operation.hpp"

namespace Sass {
  class Context;

  class Output : public Inspect {
  protected:
    using Inspect::operator();

  public:
    Output(SassOutputOptionsCpp& opt, bool srcmap_enabled);
    virtual ~Output();

  protected:
    sass::string charset;
    sass::vector<CssNodeObj> top_nodes;

  public:
    OutputBuffer get_buffer(void);

    virtual void operator()(Map*);
    virtual void operator()(CssStyleRule*);
    virtual void operator()(SupportsRule*);
    virtual void operator()(CssSupportsRule*);
    virtual void operator()(CssMediaRule*);
    virtual void operator()(StaticImport*);
    virtual void operator()(CssImport*);
    virtual void operator()(CssComment*);
    virtual void operator()(LoudComment*);
    virtual void operator()(SilentComment*);
    virtual void operator()(Number*);

    // virtual void operator()(ItplString*);
    virtual void operator()(String*);

    void fallback_impl(AST_Node* n);

  };

}

#endif
