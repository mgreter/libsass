#ifndef SASS_SASS_CONTEXT_H
#define SASS_SASS_CONTEXT_H

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

  struct SassContextReal* context;

  // main entry point for compilation
  struct SassImportCpp* entry;

  // absolute paths to includes
  Sass::sass::vector<Sass::sass::string> included_files;
  // relative includes for sourcemap
  Sass::sass::vector<Sass::sass::string> srcmap_links;

  // Logging helper
  Sass::Logger logger;

  // Parsed ast-tree
  Sass::BlockObj parsed;
  // Evaluated ast-tree
  Sass::BlockObj compiled;

  Sass::sass::string output;
  Sass::sass::string srcmap;

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

  SassCompiler(struct SassContextReal* context,
    struct SassImportCpp* entry);

  void parse();
  void compile();

  Sass::OutputBuffer render23();

  // vectors above have same size
  // sass::string entry_path88;
};

// sass config options structure
struct SassOptionsCpp : SassOutputOptionsCpp {

  typedef Sass::sass::string string;
  typedef Sass::sass::vector<string> strings;

  // embed sourceMappingUrl as data URI
  bool source_map_embed;

  // embed include contents in maps
  bool source_map_contents;

  // create file URLs for sources
  bool source_map_file_urls;

  // Disable sourceMappingUrl in css output
  bool omit_source_map_url;

  // The input path is used for source map
  // generation. It can be used to define
  // something with string compilation or to
  // overload the input file path. It is
  // set to "stdin" for data contexts and
  // to the input file on file contexts.
  string input_path;

  // The output path is used for source map
  // generation. LibSass will not write to
  // this file, it is just used to create
  // information in source-maps etc.
  string output_path;

  // Colon-separated list of paths
  // Semicolon-separated on Windows
  // Maybe use array interface instead?
  string include_path;
  string plugin_path;

  // Include paths (linked string list)
  strings include_paths;
  // Plug-in paths (linked string list)
  strings plugin_paths;

  // Path to source map file
  // Enables source map generation
  // Used to create sourceMappingUrl
  string source_map_file;

  // Directly inserted in source maps
  string source_map_root;

  // Custom functions that can be called from SCSS code
  Sass::sass::vector<struct SassFunctionCpp*> c_functions;

  // List of custom importers
  Sass::sass::vector<struct SassImporterCpp*> c_importers;

  // List of custom headers
  Sass::sass::vector<struct SassImporterCpp*> c_headers;

};


// base for all contexts
struct SassContextCpp : SassOptionsCpp
{

  // store parser type info
  enum Sass_Import_Type parser;

  // logger style (color/unicode)
  enum Sass_Logger_Style logstyle;

  // generated output data
  string output_string;

  // generated stderr data
  // string stderr_string;

  // generated source map json
  // string source_map_string;

  // error status
  // int error_status;
  // string error_json;
  // string error_text;
  // string error_message;
  // error position
  // string error_file;
  // size_t error_line;
  // size_t error_column;

  // Should be real SourceData?
  // string error_src;

  // report imported files
  // strings included_files;

};

#endif
