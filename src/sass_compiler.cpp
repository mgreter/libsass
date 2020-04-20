// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include "sass_compiler.hpp"

namespace Sass {

  static int handle_error(Compiler& compiler, StackTraces* traces, const char* what, int status)
  {

    Logger& logger(*compiler.logger123);

    bool has_final_lf = false;
    logger.formatted << "Error: ";
    // Add message and ensure it is
    // added with a final line-feed.
    if (what != nullptr) {
      logger.formatted << what;
      while (*what) {
        has_final_lf =
          *what == '\r' ||
          *what == '\n';
        ++what;
      }
      if (!has_final_lf) {
        logger.formatted << STRMLF;
      }
    }

    // Clear the previous array
    compiler.error.traces.clear();
    // Some stuff is only logged if we have some traces
    // Otherwise we don't know where the error comes from
    if (traces && traces->size() > 0) {
      logger.writeStackTraces(logger.formatted, *traces, "  ");
      // Copy items over to error object
      compiler.error.traces = *traces;
    }

    // Attach stuff to error object
    compiler.error.what = what;
    compiler.error.status = status;
    compiler.error.messages = logger.errors.str();
    compiler.error.warnings = logger.warnings.str();
    compiler.error.formatted = logger.formatted.str();

    return status;

  }

  // 
  static int handle_error(Compiler& compiler)
  {
    // Re-throw last error
    try { throw; }
    // Catch LibSass specific error cases
    catch (Exception::Base & e) { handle_error(compiler, &e.traces, e.what(), 1); }
    // Bad allocations can always happen, maybe we should exit in this case?
    catch (std::bad_alloc & e) { handle_error(compiler, nullptr, e.what(), 2); }
    // Other errors should not really happen and indicate more severe issues!
    catch (std::exception & e) { handle_error(compiler, nullptr, e.what(), 3); }
    catch (sass::string & e) { handle_error(compiler, nullptr, e.c_str(), 4); }
    catch (const char* e) { handle_error(compiler, nullptr, e, 4); }
    catch (...) { handle_error(compiler, nullptr, "unknown", 5); }
    // Return the error state
    return compiler.error.status;
  }

  // allow one error handler to throw another error
  // this can happen with invalid utf8 and json lib
  // Note: this might be obsolete but doesn't hurt!?
  static int handle_errors(Compiler& compiler)
  {
    try { return handle_error(compiler); }
    catch (...) { return handle_error(compiler); }
  }

  extern "C" {

    struct SassCompiler* ADDCALL sass_make_compiler()
    {
      return Compiler::wrap(new Compiler());
    }

    bool ADDCALL sass_compiler_parse(struct SassCompiler* sass_compiler)
    {
      Compiler& compiler(Compiler::unwrap(sass_compiler));
      try { compiler.parse(); return true; }
      catch (...) { handle_errors(compiler); }
      return false;
    }

    bool ADDCALL sass_compiler_compile(struct SassCompiler* sass_compiler)
    {
      Compiler& compiler(Compiler::unwrap(sass_compiler));
      try { compiler.compile(); return true; }
      catch (...) { handle_errors(compiler); }
      return false;
    }

    bool ADDCALL sass_compiler_render(struct SassCompiler* sass_compiler)
    {

      Compiler& compiler(Compiler::unwrap(sass_compiler));
      if (compiler.compiled == nullptr) return false;

      try {

        // This will hopefully use move semantics
        OutputBuffer output(compiler.renderCss());

        compiler.content = output.buffer;

        // Create options to render source map and footer.
        struct SassSrcMapOptions options(compiler.srcmap_options);
        // Deduct some options always from original values.
        // ToDo: is there really any need to customize this?
        if (options.origin.empty() || options.origin == "stream://stdout") {
          options.origin = compiler.getOutputPath();
        }
        if (options.path.empty() || options.path == "stream://stdout") {
          options.path = options.origin + ".map";
        }

        switch (options.mode) {
        case SASS_SRCMAP_NONE:
          compiler.srcmap = 0;
          compiler.footer = 0;
          break;
        case SASS_SRCMAP_CREATE:
          compiler.srcmap = compiler.renderSrcMapJson(options, output.smap);
          compiler.footer = nullptr; // Don't add link, just create map file
          break;
        case SASS_SRCMAP_EMBED_LINK:
          compiler.srcmap = compiler.renderSrcMapJson(options, output.smap);
          compiler.footer = compiler.renderSrcMapLink(options, output.smap);
          break;
        case SASS_SRCMAP_EMBED_JSON:
          compiler.srcmap = compiler.renderSrcMapJson(options, output.smap);
          compiler.footer = compiler.renderEmbeddedSrcMap(options, output.smap);
          break;
        }

        // Success
        return true;
      }
      catch (...) { handle_errors(compiler); }

      // Failed
      return false;

    }
    // EO sass_compiler_render



    struct SassImport* ADDCALL sass_make_file_import(struct SassCompiler* sass_compiler, const char* imp_path)
    {
      Compiler& compiler(Compiler::unwrap(sass_compiler));

      try {

        // check if entry file is given
        if (imp_path == nullptr) return {};

        // create absolute path from input filename
        // ToDo: this should be resolved via custom importers

        sass::string abs_path(File::rel2abs(imp_path, CWD));

        // try to load the entry file
        char* contents = File::slurp_file(abs_path, CWD);

        if (contents == nullptr) {
          // throw std::runtime_error("File to read not found or unreadable: " + std::string(imp_path));
        }
        /*
            // alternatively also look inside each include path folder
            // I think this differs from ruby sass (IMO too late to remove)
            for (size_t i = 0, S = include_paths.size(); contents == 0 && i < S; ++i) {
              // build absolute path for this include path entry
              abs_path = rel2abs(input_path88, include_paths[i], CWD);
              // try to load the resulting path
              contents = slurp_file(abs_path, CWD);
            }
            */
            // abort early if no content could be loaded (various reasons)
        // if (!contents) throw std::runtime_error("File to read not found or unreadable: " + std::string(input_path88));

        // store entry path
        // entry_path88 = abs_path;

        return sass_make_import(
          imp_path,
          abs_path.c_str(),
          contents, 0,
          SASS_IMPORT_AUTO
        );

      }
      catch (...) { handle_errors(compiler); }
      return nullptr;

    }

    void ADDCALL sass_compiler_set_entry_point(struct SassCompiler* compiler, struct SassImport* import)
    {
      Compiler::unwrap(compiler).entry_point = import;
    }

    ADDAPI const char* ADDCALL sass_compiler_get_output_string(struct SassCompiler* compiler)
    {
      if (Compiler::unwrap(compiler).content.empty()) return nullptr;
      return Compiler::unwrap(compiler).content.c_str();
    }

    ADDAPI const char* ADDCALL sass_compiler_get_footer_string(struct SassCompiler* compiler)
    {
      return Compiler::unwrap(compiler).footer;
    }

    ADDAPI const char* ADDCALL sass_compiler_get_srcmap_string(struct SassCompiler* compiler)
    {
      return Compiler::unwrap(compiler).srcmap;
    }

    void ADDCALL sass_delete_compiler(struct SassCompiler* context)
    {
      // delete context;
    }

    // Push function for include paths (no manipulation support for now)
    void ADDCALL sass_compiler_add_include_paths(struct SassCompiler* compiler, const char* paths)
    {
      Compiler::unwrap(compiler).addIncludePaths(paths);
    }

    // Push function for plugin paths (no manipulation support for now)
    void ADDCALL sass_compiler_load_plugins(struct SassCompiler* compiler, const char* paths)
    {
      Compiler::unwrap(compiler).loadPlugins(paths);
    }

    void ADDCALL sass_compiler_set_precision(struct SassCompiler* compiler, int precision)
    {
      Compiler::unwrap(compiler).precision = precision;
      Compiler::unwrap(compiler).logger123->setPrecision(precision);

    }

    int ADDCALL sass_compiler_get_precision(struct SassCompiler* compiler)
    {
      return Compiler::unwrap(compiler).precision;
    }

    bool ADDCALL sass_compiler_get_source_comments(struct SassCompiler* compiler)
    {
      return Compiler::unwrap(compiler).source_comments;
    }

    // void ADDCALL sass_compiler_set_source_comments(struct SassCompiler* compiler, bool source_comments)
    // {
    //   Compiler::unwrap(compiler).source_comments = source_comments;
    // }

    struct SassImport* ADDCALL sass_compiler_get_last_import(struct SassCompiler* compiler)
    {

      return Compiler::unwrap(compiler).import_stack.back();

    }

    void ADDCALL sass_compiler_set_logger_style(struct SassCompiler* compiler, enum SassLoggerStyle log_style)
    {
      if (log_style == SASS_LOGGER_AUTO) log_style = SASS_LOGGER_ASCII_MONO;
      Compiler::unwrap(compiler).logger123->style = log_style;
    }

    void ADDCALL sass_compiler_set_srcmap_mode(struct SassCompiler* compiler, enum SassSrcMapMode mode)
    {
      Compiler::unwrap(compiler).srcmap_options.mode = mode;
    }

    void ADDCALL sass_compiler_set_srcmap_root(struct SassCompiler* compiler, const char* root)
    {
      Compiler::unwrap(compiler).srcmap_options.root = root;
    }

    void ADDCALL sass_compiler_set_srcmap_path(struct SassCompiler* compiler, const char* path)
    {
      Compiler::unwrap(compiler).srcmap_options.path = path;
    }

    void ADDCALL sass_compiler_set_srcmap_file_urls(struct SassCompiler* compiler, bool enable)
    {
      Compiler::unwrap(compiler).srcmap_options.file_urls = enable;
    }

    void ADDCALL sass_compiler_set_srcmap_embed_contents(struct SassCompiler* compiler, bool enable)
    {
      Compiler::unwrap(compiler).srcmap_options.embed_contents = enable;
    }

    void ADDCALL sass_compiler_set_output_path(struct SassCompiler* compiler, const char* output_path)
    {
      Compiler::unwrap(compiler).output_path = output_path ? output_path : "stream://stdout";
    }

    void ADDCALL sass_compiler_set_output_style(struct SassCompiler* compiler, enum SassOutputStyle output_style)
    {
      Compiler::unwrap(compiler).output_style = output_style;
    }

    // Returns pointer to error object associated with compiler.
    // Will be valid until the associated compiler is destroyed.
    struct SassError* ADDCALL sass_compiler_get_error(struct SassCompiler* compiler)
    {
      return &Compiler::unwrap(compiler).error;
    }

  }

}
