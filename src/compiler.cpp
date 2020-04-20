// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include "ast.hpp"
#include "json.hpp"
#include "context.hpp"
#include "compiler.hpp"
#include "source_map.hpp"
#include "sass_functions.hpp"


namespace Sass {


  Compiler::Compiler() :
    Context(),
    state(SASS_COMPILER_CREATED),
    output_path("stream://stdout"),
    entry_point(nullptr),
    footer(nullptr),
    srcmap(nullptr),
    error()
  {}

  sass::string Compiler::getInputPath() const
  {
    if (entry_point->srcdata->getAbsPath()) {
      return entry_point->srcdata->getAbsPath();
    }
    return "";
  }

  sass::string Compiler::getOutputPath() const
  {
    if (!output_path.empty()) {
      return output_path;
    }
    sass::string path(getInputPath());
    size_t dotpos = path.find_last_of('.');
    if (dotpos != sass::string::npos) {
      path.erase(dotpos);
    }
    path += ".css";
    return path;
  }

  void Compiler::parse()
  {
    // Check if anything was actually loaded
    // This should only happen with SourceFile
    // SourceString will always return empty string
    if (entry_point->srcdata->content() == nullptr) {
      throw std::runtime_error(
        "File to read not found or unreadable: " +
        std::string(entry_point->srcdata->getImpPath()));
    }
    // Now parse the entry point into ast-tree
    parsed = Context::parseImport(entry_point);
    // Update the compiler state
    state = SASS_COMPILER_PARSED;
  }

  void Compiler::compile()
  {
    // Evaluate parsed ast-tree to new ast-tree
    if (parsed != nullptr) {
      compiled = Context::compile(parsed, false);
    }
    // Update the compiler state
    state = SASS_COMPILER_COMPILED;
  }

  OutputBuffer Compiler::renderCss()
  {
    // Create the emitter object
    Output emitter(*this);
    // Start the render process
    if (compiled != nullptr) {
      compiled->perform(&emitter);
    }
    // Finish emitter stream
    emitter.finalize();
    // Update the compiler state
    state = SASS_COMPILER_RENDERED;
    // Move the resulting buffer from stream
    return emitter.get_buffer();
  }
  // EO renderCss

  // Case 1) output to stdout, source map must be fully inline
  // Case 2) output to path, source map output is deducted from it
  char* Compiler::renderSrcMapLink(struct SassSrcMapOptions options,
    const SourceMap& source_map)
  {
    // Source map json must already be there
    if (srcmap == nullptr) return nullptr;
    // Check if we output to stdout (any link would be useless)
    if (output_path.empty() || output_path == "stream://stdout") {
      // Instead always embed the source-map on stdout
      return renderEmbeddedSrcMap(options, source_map);
    }
    // Create resulting footer and return a copy
    return sass_copy_string("\n/*# sourceMappingURL=" +
      File::abs2rel(options.path, options.origin) + " */");

  }
  // EO renderSrcMapLink

  char* Compiler::renderEmbeddedSrcMap(struct SassSrcMapOptions options,
    const SourceMap& source_map)
  {
    // Source map json must already be there
    if (srcmap == nullptr) return nullptr;
    // Encode json to base64
    sass::istream is(srcmap);
    sass::ostream buffer;
    buffer << "\n/*# sourceMappingURL=";
    buffer << "data:application/json;base64,";
    base64::encoder E;
    E.encode(is, buffer);
    buffer << " */";
    return sass_copy_string(buffer.str());
  }
  // EO renderEmbeddedSrcMap

  char* Compiler::renderSrcMapJson(struct SassSrcMapOptions options,
    const SourceMap& source_map)
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
    sass::string origin(options.origin);
    origin = File::abs2rel(origin, CWD);
    JsonNode* json_file_name = json_mkstring(origin.c_str());
    json_append_member(json_srcmap, "file", json_file_name);

    /**********************************************/
    // pass-through source_map_root option
    /**********************************************/
    if (!options.root.empty()) {
      json_append_member(json_srcmap, "sourceRoot",
        json_mkstring(options.root.c_str()));
    }

    /**********************************************/
    // Create the included sources array
    /**********************************************/
    JsonNode* json_sources = json_mkarray();
    for (size_t i = 0; i < included_sources.size(); ++i) {
      const SourceData* source(included_sources[i]);
      sass::string path(source->getAbsPath());
      path = File::rel2abs(path, ".", CWD);
      // Optionally convert to file urls
      if (options.file_urls) {
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
        path = File::abs2rel(path, ".", CWD);
        // Append item to json array
        json_append_element(json_sources,
          json_mkstring(path.c_str()));
      }
    }
    json_append_member(json_srcmap, "sources", json_sources);

    // add a relative link to the source map output file
    // srcmap_links88.emplace_back(abs2rel(abs_path, file88, CWD));

    /**********************************************/
    // Check if we have any includes to render
    /**********************************************/
    if (options.embed_contents) {
      JsonNode* json_contents = json_mkarray();
      for (size_t i = 0; i < included_sources.size(); ++i) {
        const SourceData* source = included_sources[i];
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
    // Create source remapping lookup table
    // For source-maps we need to output sources in
    // consecutive manner, but we might have used various
    // different stylesheets from the prolonged context
    /**********************************************/
    std::unordered_map<size_t, size_t> idxremap;
    for (auto& source : included_sources) {
      idxremap.insert(std::make_pair(
        source->getSrcIdx(),
        idxremap.size()));
    }

    /**********************************************/
    // Finally render the actual source mappings
    // Remap context srcidx to consecutive ordering
    /**********************************************/
    sass::string mappings(source_map.render(idxremap));
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

}

