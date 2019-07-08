/*****************************************************************************/
/* Part of LibSass, released under the MIT license (See LICENSE.txt).        */
/*****************************************************************************/
#ifndef SASS_OUTPUT_HPP
#define SASS_OUTPUT_HPP

// sass.hpp must go before all system headers
// to get the __EXTENSIONS__ fix on Solaris.
#include "capi_sass.hpp"

#include "cssize.hpp"

namespace Sass {

  class Output : public Cssize
  {
  protected:

    sass::string charset;
    CssNodeVector top_nodes;

  public:

    /////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////

    // Value constructor
    Output(
      SassOutputOptionsCpp& opt,
      bool srcmap_enabled);

    /////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////

    // Return buffer to compiler
    OutputBuffer getBuffer(void);

    /////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////

    virtual void visitCssImport(CssImport*) override;
    virtual void visitCssComment(CssComment*) override;
    virtual void visitCssMediaRule(CssMediaRule*) override;

    /////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////

    virtual void visitMap(Map* value) override;
    virtual void visitString(String* value) override;

    /////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////

  };

}

#endif
