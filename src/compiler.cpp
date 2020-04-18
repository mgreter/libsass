// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include "ast.hpp"
#include "json.hpp"
#include "context.hpp"
#include "compiler.hpp"
#include "source_map.hpp"

SassCompiler::SassCompiler(struct SassContext* context, struct SassImportCpp* entry) :
  SassOutputOptionsCpp(),
  state(SassCompilerState::CREATED),
  context(context),
  entry(entry),
  logger(5, SASS_LOGGER_AUTO),
  error_status(0)
{}

void SassCompiler::parse()
{
  Sass::Context* ctx = reinterpret_cast<Sass::Context*>(context);
  parsed = ctx->parse2(entry);

  // Move over the includes from the main context
  // This effectively clears the original vector
  // Not the cleanest API but was easy to implement
  included_sources = std::move(ctx->included_sources);

  // Create source remapping lookup table
  // For source-maps we need to output sources in
  // consecutive manner, but we might have used various
  // different stylesheets from the prolonged context
  for (auto& source : included_sources) {
    idxremap.insert(std::make_pair(
      source->getSrcIdx(),
      idxremap.size()));
  }

  // update the compiler state
  state = SassCompilerState::PARSED;
}

void SassCompiler::compile()
{
  compiled = reinterpret_cast<Sass::Context*>(context)->compile(parsed, false);
  // update the compiler state
  state = SassCompilerState::COMPILED;
}

Sass::OutputBuffer SassCompiler::renderCss()
{
  // Create the emitter object
  Sass::Output emitter(*this);
  // start the render process
  compiled->perform(&emitter);
  // finish emitter stream
  emitter.finalize();
  // update the compiler state
  state = SassCompilerState::RENDERED;
  // get the resulting buffer from stream
  return std::move(emitter.get_buffer());
}
// EO renderCss

// Case 1) output to stdout, source map must be fully inline
// Case 2) output to path, source map output is deducted from it
char* SassCompiler::renderSrcMapLink(struct SassSrcMapOptions options,
  const Sass::SourceMap& source_map)
{
  // Source map json must already be there
  if (srcmap == nullptr) return nullptr;

  // Check if we output to stdout
  if (output_path.empty()) {
    // We can't generate a link without any reference
    return renderEmbeddedSrcMap(options, source_map);
  }

  // Create the relative include url
  const string& path(options.source_map_path);
  const char* base(entry->srcdata->getAbsPath());
  string url = Sass::File::abs2rel(path, base);

  // Create resulting footer and return a lasting copy
  string result("\n/*# sourceMappingURL=" + url + " */");
  return sass_copy_string(result);
}
// EO renderSrcMapLink

char* SassCompiler::renderEmbeddedSrcMap(struct SassSrcMapOptions options,
  const Sass::SourceMap& source_map)
{
  // Source map json must already be there
  if (srcmap == nullptr) return nullptr;
  // Encode json to base64
  Sass::sass::istream is(srcmap);
  Sass::sass::ostream buffer;
  buffer << "\n/*# sourceMappingURL=";
  buffer << "data:application/json;base64,";
  base64::encoder E;
  E.encode(is, buffer);
  buffer << " */";
  return sass_copy_string(buffer.str());
}
// EO renderEmbeddedSrcMap

char* SassCompiler::renderSrcMapJson(struct SassSrcMapOptions options,
  const Sass::SourceMap& source_map)
{
  // Create the emitter object
  // Sass::OutputBuffer buffer;

  /**********************************************/
  // Create main object to render json
  /**********************************************/
  JsonNode* json_srcmap = json_mkobject();

  /**********************************************/
  // Create the source-map version information
  /**********************************************/
  json_append_member(json_srcmap, "version", json_mknumber(3));

  /**********************************************/
  // Create file reference to whom our mappings apply
  /**********************************************/
  Sass::sass::string origin(options.source_map_origin);
  origin = Sass::File::abs2rel(origin, Sass::CWD);
  JsonNode* json_file_name = json_mkstring(origin.c_str());
  json_append_member(json_srcmap, "file", json_file_name);

  /**********************************************/
  // pass-through source_map_root option
  /**********************************************/
  if (!options.source_map_root.empty()) {
    json_append_member(json_srcmap, "sourceRoot",
      json_mkstring(options.source_map_root.c_str()));
  }

  /**********************************************/
  // Create the included sources array
  /**********************************************/
  JsonNode* json_sources = json_mkarray();
  for (size_t i = 0; i < included_sources.size(); ++i) {
    const Sass::SourceData* source(included_sources[i]);
    Sass::sass::string path(source->getAbsPath());
    path = Sass::File::rel2abs(path, ".", Sass::CWD);
    // Optionally convert to file urls
    if (options.source_map_file_urls) {
      if (path[0] == '/') {
        // ends up with three slashes
        path = "file://" + path;
      }
      else {
        // needs an additional slash
        path = "file:///" + path;
      }
      // Append item to json array
      json_append_element(json_sources,
        json_mkstring(path.c_str()));
    }
    else {
      path = Sass::File::abs2rel(path, ".", Sass::CWD);
      // Append item to json array
      json_append_element(json_sources,
        json_mkstring(path.c_str()));
    }
  }
  json_append_member(json_srcmap, "sources", json_sources);

  // add a relative link to the source map output file
  // srcmap_links88.emplace_back(abs2rel(abs_path, source_map_file88, CWD));

  /**********************************************/
  // Check if we have any includes to render
  /**********************************************/
  if (options.source_map_embed_contents) {
    JsonNode* json_contents = json_mkarray();
    for (size_t i = 0; i < included_sources.size(); ++i) {
      const Sass::SourceData* source = included_sources[i];
      JsonNode* json_content = json_mkstring(source->content());
      json_append_element(json_contents, json_content);
    }
    json_append_member(json_srcmap, "sourcesContent", json_contents);
  }

  /**********************************************/
  // So far we have no implementation for names
  /**********************************************/
  json_append_member(json_srcmap, "names", json_mkarray());

  /**********************************************/
  // Finally render the actual source mappings
  // Remap context srcidx to consecutive ordering
  /**********************************************/
  Sass::sass::string mappings(source_map.render(idxremap));
  JsonNode* json_mappings = json_mkstring(mappings.c_str());
  json_append_member(json_srcmap, "mappings", json_mappings);

  /**********************************************/
  // Render the json and return result
  // Memory must be freed by consumer!
  /**********************************************/
  char* data = json_stringify(json_srcmap, "\t");
  json_delete(json_srcmap);
  return data;

}
// EO renderSrcMapJson
