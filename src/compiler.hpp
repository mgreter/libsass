#ifndef SASS_COMPILER_H
#define SASS_COMPILER_H

#include "sass.hpp"

#include "sass/base.h"
#include "sass/context.h"

#include "context.hpp"

namespace Sass {

  class Compiler : public Context {

  public:

    SassCompilerState state;

    // Where we want to store the output.
    // Source-map path is deducted from it.
    // Empty means we output to `stdout`.
    sass::string output_path;

    // main entry point for compilation
    struct SassImportCpp* entry;

    // We need a lookup table to map between internal source indexes.
    // and what we actually render. Our main source indexes are related
    // to the context, which can be used for multiple compilations. The
    // files used for the actual result may only be a subset of all loaded
    // sheets. Therefore we need to re-map the indexes for source-maps to
    // make sure the items in resulting arrays are all consecutive.
    std::unordered_map<size_t, size_t> idxremap;

    // relative includes for sourcemap
    sass::vector<sass::string> srcmap_links;

    // Parsed ast-tree
    BlockObj parsed;
    // Evaluated ast-tree
    BlockObj compiled;

    sass::string output;

    // The rendered output footer. This includes the
    // rendered css comment footer for the source-map.
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

    Compiler(enum Sass_Logger_Style logstyle);

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
