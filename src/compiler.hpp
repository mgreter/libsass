#ifndef SASS_SASS_COMPILER_H
#define SASS_SASS_COMPILER_H

#include "sass/base.h"
#include "sass/context.h"
#include "source_map.hpp"
#include "ast_fwd_decl.hpp"
#include "backtrace.hpp"
#include "logger.hpp"
#include "output.hpp"
#include "sass.hpp"

struct SassCompiler : SassOutputOptionsCpp {

  SassCompilerState state;

  struct SassContext* context;

  // Where we want to store the output.
  // Source-map path is deducted from it.
  // Empty means we output to `stdout`.
  string output_path;

  // main entry point for compilation
  struct SassImportCpp* entry;

  // absolute paths to includes
  Sass::sass::vector<Sass::SourceDataObj> included_sources;

  // We need a lookup table to map between internal source indexes.
  // and what we actually render. Our main source indexes are related
  // to the context, which can be used for multiple compilations. The
  // files used for the actual result may only be a subset of all loaded
  // sheets. Therefore we need to re-map the indexes for source-maps to
  // make sure the items in resulting arrays are all consecutive.
  std::unordered_map<size_t, size_t> idxremap;

  // relative includes for sourcemap
  Sass::sass::vector<Sass::sass::string> srcmap_links;

  // Logging helper
  Sass::Logger logger;

  // Parsed ast-tree
  Sass::BlockObj parsed;
  // Evaluated ast-tree
  Sass::BlockObj compiled;

  Sass::sass::string output;

  // The rendered output footer. This includes the
  // rendered css comment footer for the source-map.
  char* footer;

  // The rendered source map. This is what an implementor
  // would normally write out to the `output.css.map` file
  char* srcmap;

  // error status
  int error_status;
  string error_json;
  string error_text;
  string error_message;
  // error position
  // Why not pstate?
  string error_file;
  size_t error_line;
  size_t error_column;
  Sass::SourceDataObj error_src;
  

  SassCompiler(struct SassContext* context,
    struct SassImportCpp* entry);

  void parse();
  void compile();

  Sass::OutputBuffer renderCss();

  char* renderSrcMapJson(struct SassSrcMapOptions options,
    const Sass::SourceMap& source_map);

  char* renderSrcMapLink(struct SassSrcMapOptions options,
    const Sass::SourceMap& source_map);

  char* renderEmbeddedSrcMap(struct SassSrcMapOptions options,
    const Sass::SourceMap& source_map);

private:



  // vectors above have same size
  // sass::string entry_path88;
};

#endif
