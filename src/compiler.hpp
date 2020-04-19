#ifndef SASS_COMPILER_H
#define SASS_COMPILER_H

#include "sass.hpp"

#include "sass/base.h"
#include "sass/context.h"

#include "context.hpp"

namespace Sass {

  // The split between Context and Compiler is technically not really needed.
  // But it helps a little to organize the different aspects of the compilation.
  // The Context mainly stores all the read-only values (e.g. settings).
  class Compiler : public Context {

  public:

    // The current state the compiler is in.
    enum SassCompilerState state;

    // main entry point for compilation
    struct SassImportCpp* entry_point;

    // Where we want to store the output.
    // Source-map path is deducted from it.
    // Defaults to `stream://stdout`.
    sass::string output_path;

    // Parsed ast-tree
    BlockObj parsed;
    // Evaluated ast-tree
    BlockObj compiled;

    // The rendered css content.
    sass::string output;

    // The rendered output footer. This includes the
    // rendered css comment footer for the source-map.
    // Append after output for the full output document.
    char* footer;

    // The rendered source map. This is what an implementor
    // would normally write out to the `output.css.map` file
    char* srcmap;

    // error status
    int error_status;
    sass::string error_json;
    sass::string error_text;
    sass::string error_message;

    // error position
    // Why not pstate?
    sass::string error_file;
    size_t error_line;
    size_t error_column;
    SourceDataObj error_src;

    // Constructor
    Compiler();

    void parse();
    void compile();

    OutputBuffer renderCss();

    char* renderSrcMapJson(struct SassSrcMapOptions options,
      const SourceMap& source_map);

    char* renderSrcMapLink(struct SassSrcMapOptions options,
      const SourceMap& source_map);

    char* renderEmbeddedSrcMap(struct SassSrcMapOptions options,
      const SourceMap& source_map);

  };

}

#endif
