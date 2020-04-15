#ifndef SASS_SASS_CONTEXT_H
#define SASS_SASS_CONTEXT_H

#include "sass/base.h"
#include "sass/context.h"
#include "ast_fwd_decl.hpp"
#include "backtrace.hpp"
#include "sass.hpp"

// sass config options structure
struct SassOptionsCpp : SassOutputOptionsCpp {

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
  Sass::sass::string input_path;

  // The output path is used for source map
  // generation. LibSass will not write to
  // this file, it is just used to create
  // information in source-maps etc.
  Sass::sass::string output_path;

  // Colon-separated list of paths
  // Semicolon-separated on Windows
  // Maybe use array interface instead?
  Sass::sass::string include_path;
  Sass::sass::string plugin_path;

  // Include paths (linked string list)
  Sass::sass::vector<Sass::sass::string> include_paths;
  // Plug-in paths (linked string list)
  Sass::sass::vector<Sass::sass::string> plugin_paths;

  // Path to source map file
  // Enables source map generation
  // Used to create sourceMappingUrl
  Sass::sass::string source_map_file;

  // Directly inserted in source maps
  Sass::sass::string source_map_root;

  // Custom functions that can be called from SCSS code
  SassFunctionListPtr c_functions;

  // List of custom importers
  SassImporterListPtr c_importers;

  // List of custom headers
  SassImporterListPtr c_headers;

};


// base for all contexts
struct SassContextCpp : SassOptionsCpp
{

  // store context type info
  enum Sass_Input_Style type;

  // store parser type info
  enum Sass_Import_Type parser;

  // logger style (color/unicode)
  enum Sass_Logger_Style logstyle;

  // generated output data
  Sass::sass::string output_string;

  // generated stderr data
  Sass::sass::string stderr_string;

  // generated source map json
  Sass::sass::string source_map_string;

  // error status
  int error_status;
  Sass::sass::string error_json;
  Sass::sass::string error_text;
  Sass::sass::string error_message;
  // error position
  Sass::sass::string error_file;
  size_t error_line;
  size_t error_column;

  // Should be real SourceData?
  Sass::sass::string error_src;

  // report imported files
  Sass::sass::vector<Sass::sass::string> included_files;

};

// struct for file compilation
struct SassFileContextCpp : SassContextCpp {

  // no additional fields required
  // input_path is already on options

};

// struct for data compilation
struct SassDataContextCpp : SassContextCpp {

  // provided source string
  Sass::sass::string source_string;
  Sass::sass::string srcmap_string;

};

// link C and CPP context
struct SassCompilerCpp {
  // progress status
  Sass_Compiler_State state;
  // original c context
  SassContextCpp* c_ctx;
  // Sass::Context
  Sass::Context* cpp_ctx;
  // Sass::Block
  Sass::Block_Obj root;
};

#endif
