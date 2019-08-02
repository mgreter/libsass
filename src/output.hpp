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
    Output(Sass_Output_Options& opt);
    virtual ~Output();

  protected:
    sass::string charset;
    sass::vector<AST_Node*> top_nodes;

  public:
    OutputBuffer get_buffer(void);

    virtual void operator()(Map*);
    virtual void operator()(CssStyleRule*);
    virtual void operator()(SupportsRule*);
    virtual void operator()(CssSupportsRule*);
    virtual void operator()(CssMediaRule*);
    virtual void operator()(Keyframe_Rule*);
    virtual void operator()(Import*);
    virtual void operator()(StaticImport*);
    virtual void operator()(CssComment*);
    virtual void operator()(LoudComment*);
    virtual void operator()(SilentComment*);
    virtual void operator()(Number*);

    virtual void operator()(StringLiteral*);
    virtual void operator()(String_Constant*);

    void fallback_impl(AST_Node* n);

  };

}

#endif
