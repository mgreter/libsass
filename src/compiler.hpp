#ifndef SASS_COMPILER_H
#define SASS_COMPILER_H

#include "sass.hpp"

#include "sass/base.h"
#include "sass/error.h"
#include "sass/context.h"
#include "ast_def_macros.hpp"
#include "sass_error.hpp"

#include "context.hpp"

namespace Sass {

  // The split between Context and Compiler is technically not really needed.
  // But it helps a little to organize the different aspects of the compilation.
  // The Context mainly stores all the read-only values (e.g. settings).
  class Compiler : public Context {

  public:

    // The current state the compiler is in.
    enum SassCompilerState state;

    struct SassSrcMapOptions srcmap_options;

    // Where we want to store the output.
    // Source-map path is deducted from it.
    // Defaults to `stream://stdout`.
    sass::string output_path;

    // main entry point for compilation
    struct SassImport* entry_point;

    // Parsed ast-tree
    RootObj parsed;
    // Evaluated ast-tree
    CssRootObj compiled;

    // The rendered css content.
    sass::string content;

    // Rendered warnings and debugs.
    sass::string warnings;

    // The rendered output footer. This includes the
    // rendered css comment footer for the source-map.
    // Append after output for the full output document.
    char* footer;

    // The rendered source map. This is what an implementor
    // would normally write out to the `output.css.map` file
    char* srcmap;

    // Runtime error
    SassError error;


    /*
    // error status
    int error_status;

    // Detail position of error
    SourceSpan error_pstate;


    // Traces leading up to error
    StackTraces error_traces;

    // Specific error message
    sass::string error_text;

    // This is the formatted output
    sass::string error_message;
    sass::string warning_message;

    // All details in a json object
    sass::string error_json;
    */
    // Constructor
    Compiler();

    void parse();
    void compile();

    OutputBuffer renderCss();

    sass::string getInputPath() const;
    sass::string getOutputPath() const;

    // ToDO, maybe belongs to compiler?
    CssRootObj compile2(RootObj root, bool plainCss);

    char* renderSrcMapJson(struct SassSrcMapOptions options,
      const SourceMap& source_map);

    char* renderSrcMapLink(struct SassSrcMapOptions options,
      const SourceMap& source_map);

    char* renderEmbeddedSrcMap(struct SassSrcMapOptions options,
      const SourceMap& source_map);

    DECLARE_CAPI_WRAPPER(Compiler, SassCompiler);

  };

}

#endif
