#ifndef SASS_SASS_CONTEXT_H
#define SASS_SASS_CONTEXT_H

#include "sass/base.h"
#include "sass/context.h"
#include "source_map.hpp"
#include "ast_fwd_decl.hpp"
#include "backtrace.hpp"
#include "compiler.hpp"
#include "logger.hpp"
#include "output.hpp"
#include "sass.hpp"

// Case 1: create no source-maps
// Case 2: create source-maps, but no reference in css
// Case 3: create source-maps, reference to file in css
// Case 4: create source-maps, embed the json in the css
// Note: Writing source-maps to disk depends on implementor

enum SassSrcMapMode {
  SASS_SRCMAP_NONE,
  SASS_SRCMAP_CREATE,
  SASS_SRCMAP_EMBED_LINK,
  SASS_SRCMAP_EMBED_JSON,

};

struct SassSrcMapOptions {

  // Use string implementation from sass
  typedef Sass::sass::string string;

  enum SassSrcMapMode source_map_mode;

  // Flag to embed full sources
  // Ignored for SASS_SRCMAP_NONE
  bool source_map_embed_contents;

  // create file URLs for sources
  bool source_map_file_urls;

  // Path to source map file
  // Enables source map generation
  // Used to create sourceMappingUrl
  string source_map_origin;

  // Directly inserted in source maps
  string source_map_root;

  // Path where source map is saved
  string source_map_path;

  // Init everything to false
  SassSrcMapOptions() :
    source_map_mode(SASS_SRCMAP_CREATE),
    source_map_embed_contents(true),
    source_map_file_urls(false)
  {}

};

// sass config options structure
struct SassOptionsCpp : SassOutputOptionsCpp {

  typedef Sass::sass::string string;
  typedef Sass::sass::vector<string> strings;

  // embed sourceMappingUrl as data URI
  // bool source_map_embed;

  // embed include contents in maps
  // bool source_map_contents;

  // create file URLs for sources
  // bool source_map_file_urls;

  // Disable sourceMappingUrl in css output
  // bool omit_source_map_url;

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

  // Vectors with paths
  strings include_paths;
  strings plugin_paths;

  // Path to source map file
  // Enables source map generation
  // Used to create sourceMappingUrl
  // string source_map_file;

  // Directly inserted in source maps
  // string source_map_root;

  // Custom functions that can be called from SCSS code
  Sass::sass::vector<struct SassFunctionCpp*> c_functions;

  // List of custom importers
  Sass::sass::vector<struct SassImporterCpp*> c_importers;

  // List of custom headers
  Sass::sass::vector<struct SassImporterCpp*> c_headers;

};

#endif
