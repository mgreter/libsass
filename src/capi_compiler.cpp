/*****************************************************************************/
/* Part of LibSass, released under the MIT license (See LICENSE.txt).        */
/*****************************************************************************/
#include "capi_compiler.hpp"

#include "compiler.hpp"
#include "exceptions.hpp"

namespace Sass {

  /////////////////////////////////////////////////////////////////////////
  // Some specific implementations that could also go into C++ part
  // ToDo: maybe we should guard all C-API functions this way!?
  /////////////////////////////////////////////////////////////////////////

  // Format the error and fill compiler error states
  static int handle_error(Compiler& compiler, StackTraces* traces, const char* what, int status)
  {

    sass::ostream formatted;
    bool has_final_lf = false;
    Logger& logger(compiler);
    formatted << "Error: ";
    // Add message and ensure it is
    // added with a final line-feed.
    const char* msg = what;
    if (msg != nullptr) {
      formatted << msg;
      while (*msg) {
        has_final_lf =
          *msg == '\r' ||
          *msg == '\n';
        ++msg;
      }
      if (!has_final_lf) {
        formatted << STRMLF;
      }
    }

    // Clear the previous array
    compiler.error.traces.clear();
    // Some stuff is only logged if we have some traces
    // Otherwise we don't know where the error comes from
    if (traces && traces->size() > 0) {
      // Write traces to string with some indentation
      logger.writeStackTraces(formatted, *traces, "  ");
      // Copy items over to error object
      compiler.error.traces = *traces;
    }

    // Attach stuff to error object
    compiler.error.what.clear();
    compiler.error.status = status;
    if (what) compiler.error.what = what;
    compiler.error.formatted = formatted.str();

    // Return status again
    return status;
  }
  // EO handle_error

  // Main entry point to catch errors
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
  // EO handle_error

  // allow one error handler to throw another error
  // this can happen with invalid utf8 and json lib
  // ToDo: this might be obsolete but doesn't hurt!?
  static int handle_errors(Compiler& compiler)
  {
    try { return handle_error(compiler); }
    catch (...) { return handle_error(compiler); }
  }
  // EO handle_errors

  // Main implementation (caller is wrapped in try/catch)
  void __sass_compiler_parse(Compiler& compiler)
  {
    compiler.parse();
  }
  // EO __sass_compiler_parse

  // Main implementation (caller is wrapped in try/catch)
  void __sass_compiler_compile(Compiler& compiler)
  {
    compiler.compile();
  }
  // EO __sass_compiler_compile

  // Main implementation (caller is wrapped in try/catch)
  void __sass_compiler_render(Compiler& compiler)
  {

    // Bail out if we had any previous errors
    if (compiler.error.status != 0) return;
    // Make sure compile step was called before
    if (compiler.compiled == nullptr) return;

    // This will hopefully use move semantics
    OutputBuffer output(compiler.renderCss());
    compiler.content = std::move(output.buffer);

    // Create options to render source map and footer.
    SrcMapOptions& options(compiler.srcmap_options);
    // Deduct some options always from original values.
    // ToDo: is there really any need to customize this?
    if (options.origin.empty() || options.origin == "stream://stdout") {
      options.origin = compiler.getOutputPath();
    }
    if (options.path.empty() || options.path == "stream://stdout") {
      if (!options.origin.empty() && options.origin != "stream://stdout") {
        options.path = options.origin + ".map";
      }
    }

    switch (options.mode) {
    case SASS_SRCMAP_NONE:
      compiler.srcmap = nullptr;
      compiler.footer = nullptr;
      break;
    case SASS_SRCMAP_CREATE:
      compiler.srcmap = compiler.renderSrcMapJson(options, *output.smap);
      compiler.footer = nullptr; // Don't add link, just create map file
      break;
    case SASS_SRCMAP_EMBED_LINK:
      compiler.srcmap = compiler.renderSrcMapJson(options, *output.smap);
      compiler.footer = compiler.renderSrcMapLink(options, *output.smap);
      break;
    case SASS_SRCMAP_EMBED_JSON:
      compiler.srcmap = compiler.renderSrcMapJson(options, *output.smap);
      compiler.footer = compiler.renderEmbeddedSrcMap(options, *output.smap);
      break;
    }

  }
  // EO __sass_compiler_render

  void __sass_compiler_add_custom_function(Compiler& compiler, struct SassFunction* function)
  {
    compiler.registerCustomFunction(function);
  }
  // EO __sass_compiler_add_custom_function

  /////////////////////////////////////////////////////////////////////////
  // On windows we can improve the error handling by also catching
  // structured exceptions. In order for this to work we need a few
  // additional wrapper functions, which fortunately don't cost much.
  /////////////////////////////////////////////////////////////////////////

  #ifdef _MSC_VER
  int filter(unsigned int code, struct _EXCEPTION_POINTERS* ep)
  {
    // Handle exceptions we can't handle otherwise
    // Because these are not regular C++ exceptions
    if (code == EXCEPTION_ACCESS_VIOLATION)
    {
      return EXCEPTION_EXECUTE_HANDLER;
    }
    else if (code == EXCEPTION_STACK_OVERFLOW)
    {
      return EXCEPTION_EXECUTE_HANDLER;
    }
    else
    {
      // Do not handle any other exceptions here
      // Should be handled by regular try/catch
      return EXCEPTION_CONTINUE_SEARCH;
    }
  }

  // Convert exception codes to strings
  const char* seException(unsigned int code)
  {
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:         return "EXCEPTION_ACCESS_VIOLATION";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
    case EXCEPTION_BREAKPOINT:               return "EXCEPTION_BREAKPOINT";
    case EXCEPTION_DATATYPE_MISALIGNMENT:    return "EXCEPTION_DATATYPE_MISALIGNMENT";
    case EXCEPTION_FLT_DENORMAL_OPERAND:     return "EXCEPTION_FLT_DENORMAL_OPERAND";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
    case EXCEPTION_FLT_INEXACT_RESULT:       return "EXCEPTION_FLT_INEXACT_RESULT";
    case EXCEPTION_FLT_INVALID_OPERATION:    return "EXCEPTION_FLT_INVALID_OPERATION";
    case EXCEPTION_FLT_OVERFLOW:             return "EXCEPTION_FLT_OVERFLOW";
    case EXCEPTION_FLT_STACK_CHECK:          return "EXCEPTION_FLT_STACK_CHECK";
    case EXCEPTION_FLT_UNDERFLOW:            return "EXCEPTION_FLT_UNDERFLOW";
    case EXCEPTION_ILLEGAL_INSTRUCTION:      return "EXCEPTION_ILLEGAL_INSTRUCTION";
    case EXCEPTION_IN_PAGE_ERROR:            return "EXCEPTION_IN_PAGE_ERROR";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:       return "EXCEPTION_INT_DIVIDE_BY_ZERO";
    case EXCEPTION_INT_OVERFLOW:             return "EXCEPTION_INT_OVERFLOW";
    case EXCEPTION_INVALID_DISPOSITION:      return "EXCEPTION_INVALID_DISPOSITION";
    case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
    case EXCEPTION_PRIV_INSTRUCTION:         return "EXCEPTION_PRIV_INSTRUCTION";
    case EXCEPTION_SINGLE_STEP:              return "EXCEPTION_SINGLE_STEP";
    case EXCEPTION_STACK_OVERFLOW:           return "EXCEPTION_STACK_OVERFLOW";
    default: return "UNKNOWN EXCEPTION";
    }
  }
  #endif

  // Wrap Structured Exceptions for MSVC
  void _sass_compiler_parse(Compiler& compiler)
  {
    #ifdef _MSC_VER
    __try {
    #endif
      __sass_compiler_parse(compiler);
    #ifdef _MSC_VER
    }
    __except (filter(GetExceptionCode(), GetExceptionInformation())) {
      throw std::runtime_error(seException(GetExceptionCode()));
    }
    #endif
  }

  // Wrap Structured Exceptions for MSVC
  void _sass_compiler_compile(Compiler& compiler)
  {
    #ifdef _MSC_VER
    __try {
    #endif
      __sass_compiler_compile(compiler);
    #ifdef _MSC_VER
    }
    __except (filter(GetExceptionCode(), GetExceptionInformation())) {
      throw std::runtime_error(seException(GetExceptionCode()));
    }
    #endif
  }

  // Wrap Structured Exceptions for MSVC
  void _sass_compiler_render(Compiler& compiler)
  {
    #ifdef _MSC_VER
    __try {
    #endif
      __sass_compiler_render(compiler);
    #ifdef _MSC_VER
    }
    __except (filter(GetExceptionCode(), GetExceptionInformation())) {
      throw std::runtime_error(seException(GetExceptionCode()));
    }
    #endif
  }

  // Wrap Structured Exceptions for MSVC
  void _sass_compiler_add_custom_function(Compiler& compiler, struct SassFunction* function)
  {
    #ifdef _MSC_VER
    __try {
    #endif
      __sass_compiler_add_custom_function(compiler, function);
    #ifdef _MSC_VER
    }
    __except (filter(GetExceptionCode(), GetExceptionInformation())) {
      throw std::runtime_error(seException(GetExceptionCode()));
    }
    #endif
  }

  // Main entry point (wrap try/catch then MSVC)
  void sass_compiler_parse(Compiler& compiler)
  {
    Logger& logger(compiler);
    try { _sass_compiler_parse(compiler); }
    catch (...) { handle_errors(compiler); }
    compiler.warnings = logger.logstrm.str();
  }

  // Main entry point (wrap try/catch then MSVC)
  void sass_compiler_compile(Compiler& compiler)
  {
    Logger& logger(compiler);
    try { _sass_compiler_compile(compiler); }
    catch (...) { handle_errors(compiler); }
    compiler.warnings = logger.logstrm.str();
  }

  // Main entry point (wrap try/catch then MSVC)
  void sass_compiler_render(Compiler& compiler)
  {
    Logger& logger(compiler);
    try { _sass_compiler_render(compiler); }
    catch (...) { handle_errors(compiler); }
    compiler.warnings = logger.logstrm.str();
  }

  void sass_compiler_add_custom_function(Compiler& compiler, struct SassFunction* function)
  {
    Logger& logger(compiler);
    try { _sass_compiler_add_custom_function(compiler, function); }
    catch (...) { handle_errors(compiler); }
    compiler.warnings = logger.logstrm.str();
  }

}
// EO C++ helpers


/////////////////////////////////////////////////////////////////////////
// The actual C-API Implementations
/////////////////////////////////////////////////////////////////////////

extern "C" {

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  using namespace Sass;

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Create a new LibSass compiler context
  struct SassCompiler* ADDCALL sass_make_compiler()
  {
    return Compiler::wrap(new Compiler());
  }

  // Release all memory allocated with the compiler
  void ADDCALL sass_delete_compiler(struct SassCompiler* compiler)
  {
    delete& Compiler::unwrap(compiler);
    #ifdef DEBUG_SHARED_PTR
    RefCounted::dumpMemLeaks();
    #endif
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Parse the entry point and potentially all imports within.
  void ADDCALL sass_compiler_parse(struct SassCompiler* sass_compiler)
  {
    sass_compiler_parse(Compiler::unwrap(sass_compiler));
  }

  // Evaluate the parsed entry point and store resulting ast-tree.
  void ADDCALL sass_compiler_compile(struct SassCompiler* sass_compiler)
  {
    sass_compiler_compile(Compiler::unwrap(sass_compiler));
  }

  // Render the evaluated ast-tree to get the final output string.
  void ADDCALL sass_compiler_render(struct SassCompiler* sass_compiler)
  {
    sass_compiler_render(Compiler::unwrap(sass_compiler));
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Add additional include paths where LibSass will look for includes.
  // Note: the passed in `paths` can be path separated (`;` on windows, `:` otherwise).
  void ADDCALL sass_compiler_add_include_paths(struct SassCompiler* compiler, const char* paths)
  {
    Compiler::unwrap(compiler).addIncludePaths(paths);
  }

  // Load dynamic loadable plugins from `paths`. Plugins are only supported on certain OSs and
  // are still in experimental state. This will look for `*.dll`, `*.so` or `*.dynlib` files.
  // It then tries to load the found libraries and does a few checks to see if the library
  // is actually a LibSass plugin. We then call its init hook if the library is compatible.
  // Note: the passed in `paths` can be path separated (`;` on windows, `:` otherwise).
  void ADDCALL sass_compiler_load_plugins(struct SassCompiler* compiler, const char* paths)
  {
    Compiler::unwrap(compiler).loadPlugins(paths);
  }

  // Add a custom header importer that will always be executed before any other 
  // compilations takes place. Useful to prepend a shared copyright header or to
  // provide global variables or functions. This feature is still in experimental state.
  // Note: With the adaption of Sass Modules this might be completely replaced in the future.
  void ADDCALL sass_compiler_add_custom_header(struct SassCompiler* compiler, struct SassImporter* header)
  {
    Compiler::unwrap(compiler).addCustomHeader(header);
  }

  // Add a custom importer that will be executed when a sass `@import` rule is found.
  // This is useful to e.g. rewrite import locations or to load content from remote.
  // For more please check https://github.com/sass/libsass/blob/master/docs/api-importer.md
  // Note: The importer will not be called for regular css `@import url()` rules.
  void ADDCALL sass_compiler_add_custom_importer(struct SassCompiler* compiler, struct SassImporter* importer)
  {
    Compiler::unwrap(compiler).addCustomImporter(importer);
  }

  // Add a custom function that will be executed when the corresponding function call is
  // requested from any sass code. This is useful to provide custom functions in your code.
  // For more please check https://github.com/sass/libsass/blob/master/docs/api-function.md
  // Note: since we need to parse the function signature this may throw an error!
  void ADDCALL sass_compiler_add_custom_function(struct SassCompiler* compiler, struct SassFunction* function)
  {
    // Wrap to correctly trap errors and to fill the error object if needed
    sass_compiler_add_custom_function(Compiler::unwrap(compiler), function);
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Setter for output style (see `enum SassOutputStyle` for possible options).
  void ADDCALL sass_compiler_set_output_style(struct SassCompiler* compiler, enum SassOutputStyle output_style)
  {
    Compiler::unwrap(compiler).output_style = output_style;
  }

  // Setter for logging style (see `enum SassLoggerStyle` for possible options).
  void ADDCALL sass_compiler_set_logger_style(struct SassCompiler* compiler, enum SassLoggerStyle log_style)
  {
    Compiler::unwrap(compiler).setLogStyle(log_style);
  }

  // Getter for compiler precision (how floating point numbers are truncated).
  int ADDCALL sass_compiler_get_precision(struct SassCompiler* compiler)
  {
    return Compiler::unwrap(compiler).precision;
  }

  // Setter for compiler precision (how floating point numbers are truncated).
  void ADDCALL sass_compiler_set_precision(struct SassCompiler* context, int precision)
  {
    Compiler& compiler(Compiler::unwrap(context));
    compiler.setPrecision(precision);
    compiler.setLogPrecision(precision);
  }

  // Getter for compiler entry point (which file or data to parse first).
  struct SassImport* ADDCALL sass_compiler_get_entry_point(struct SassCompiler* compiler)
  {
    return Compiler::unwrap(compiler).entry_point->wrap();
  }

  // Setter for compiler entry point (which file or data to parse first).
  void ADDCALL sass_compiler_set_entry_point(struct SassCompiler* compiler, struct SassImport* import)
  {
    Compiler::unwrap(compiler).entry_point = &Import93::unwrap(import);
  }

  // Getter for compiler output path (where to store the result)
  // Note: LibSass does not write the file, implementers should write to this path.
  const char* ADDCALL sass_compiler_get_output_path(struct SassCompiler* compiler)
  {
    return Compiler::unwrap(compiler).output_path.c_str();
  }

  // Setter for compiler output path (where to store the result)
  // Note: LibSass does not write the file, implementers should write to this path.
  void ADDCALL sass_compiler_set_output_path(struct SassCompiler* compiler, const char* output_path)
  {
    Compiler::unwrap(compiler).output_path = output_path ? output_path : "stream://stdout";
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Getter for warnings that occurred during any step.
  const char* ADDCALL sass_compiler_get_warn_string(struct SassCompiler* compiler)
  {
    if (Compiler::unwrap(compiler).warnings.empty()) return nullptr;
    return Compiler::unwrap(compiler).warnings.c_str();
  }

  // Getter for output after parsing, compilation and rendering.
  const char* ADDCALL sass_compiler_get_output_string(struct SassCompiler* compiler)
  {
    if (Compiler::unwrap(compiler).content.empty()) return nullptr;
    return Compiler::unwrap(compiler).content.c_str();
  }

  // Getter for footer string containing optional source-map (embedded or link).
  const char* ADDCALL sass_compiler_get_footer_string(struct SassCompiler* compiler)
  {
    return Compiler::unwrap(compiler).footer;
  }

  // Getter for string containing the optional source-mapping.
  const char* ADDCALL sass_compiler_get_srcmap_string(struct SassCompiler* compiler)
  {
    return Compiler::unwrap(compiler).srcmap;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Setter for source-map mode (how to embed or not embed the source-map).
  void ADDCALL sass_compiler_set_srcmap_mode(struct SassCompiler* compiler, enum SassSrcMapMode mode)
  {
    Compiler::unwrap(compiler).srcmap_options.mode = mode;
  }

  // Setter for source-map path (where to store the source-mapping).
  // Note: if path is not explicitly given, we will deduct one from input path.
  // Note: LibSass does not write the file, implementers should write to this path.
  void ADDCALL sass_compiler_set_srcmap_path(struct SassCompiler* compiler, const char* path)
  {
    Compiler::unwrap(compiler).srcmap_options.path = path;
  }

  // Getter for source-map path (where to store the source-mapping).
  // Note: if path is not explicitly given, we will deduct one from input path.
  // Note: the value will only be deducted after the main render phase is completed.
  // Note: LibSass does not write the file, implementers should write to this path.
  const char* ADDCALL sass_compiler_get_srcmap_path(struct SassCompiler* compiler)
  {
    const sass::string& path(Compiler::unwrap(compiler).srcmap_options.path);
    return path.empty() ? nullptr : path.c_str();
  }

  // Setter for source-map root (simply passed to the resulting srcmap info).
  // Note: if not given, no root attribute will be added to the srcmap info object.
  void ADDCALL sass_compiler_set_srcmap_root(struct SassCompiler* compiler, const char* root)
  {
    Compiler::unwrap(compiler).srcmap_options.root = root;
  }

  // Setter for source-map file-url option (renders urls in srcmap as `file://` urls)
  void ADDCALL sass_compiler_set_srcmap_file_urls(struct SassCompiler* compiler, bool enable)
  {
    Compiler::unwrap(compiler).srcmap_options.file_urls = enable;
  }

  // Setter for source-map embed-contents option (includes full sources in the srcmap info)
  void ADDCALL sass_compiler_set_srcmap_embed_contents(struct SassCompiler* compiler, bool enable)
  {
    Compiler::unwrap(compiler).srcmap_options.embed_contents = enable;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Getter to return the number of all included files.
  size_t ADDCALL sass_compiler_get_included_files_count(struct SassCompiler* compiler)
  {
    return Compiler::unwrap(compiler).included_sources.size();
  }

  // Getter to return path to the included file at position `n`.
  const char* ADDCALL sass_compiler_get_included_file_path(struct SassCompiler* compiler, size_t n)
  {
    return Compiler::unwrap(compiler).included_sources.at(n)->getAbsPath();
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Getter for current import context. Use `SassImport` functions to query the state.
  const struct SassImport* ADDCALL sass_compiler_get_last_import(struct SassCompiler* compiler)
  {
    return Compiler::unwrap(compiler).import_stack.back()->wrap();
  }

  // Returns status code for compiler (0 meaning success, anything else is an error)
  int ADDCALL sass_compiler_get_status(struct SassCompiler* compiler)
  {
    return Compiler::unwrap(compiler).error.status;
  }

  // Returns pointer to error object associated with compiler.
  // Will be valid until the associated compiler is destroyed.
  const struct SassError* ADDCALL sass_compiler_get_error(struct SassCompiler* compiler)
  {
    return &Compiler::unwrap(compiler).error;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Resolve an include relative to last import or include paths in the sass option struct.
  // This will do a lookup as LibSass would do internally (partials, different extensions).
  // ToDo: Check if we should add `includeIndex` option to check for directory index files!?
  char* ADDCALL sass_compiler_find_include(const char* file, struct SassCompiler* compiler)
  {
    std::cerr << "YEP, find include\n";
    /*
    // get the last import entry to get current base directory
    SassImportPtr import = sass_compiler_get_last_import(compiler);
    const sass::vector<sass::string>& incs = compiler->cpp_ctx->includePaths;
    // create the vector with paths to lookup
    sass::vector<sass::string> paths(1 + incs.size());
    paths.emplace_back(File::dir_name(import->srcdata->getAbsPath()));
    paths.insert( paths.end(), incs.begin(), incs.end() );
    // now resolve the file path relative to lookup paths
    sass::string resolved(File::find_include(file,
      compiler->cpp_ctx->CWD, paths, compiler->cpp_ctx->fileExistsCache));
    return sass_copy_c_string(resolved.c_str());
    */
    return 0;
  }

  // Resolve a file relative to last import or include paths in the sass option struct.
  char* ADDCALL sass_compiler_find_file(const char* file, struct SassCompiler* compiler)
  {
    sass::string path(Compiler::unwrap(compiler).findFile(file));
    return path.empty() ? nullptr : sass_copy_c_string(path.c_str());
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////



  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  //size_t ADDCALL sass_compiler_count_traces(struct SassCompiler* compiler)
  //{
  //  Logger& logger(Compiler::unwrap(compiler));
  //  return logger.callStack.size();
  //}
  //
  //const struct SassTrace* ADDCALL sass_compiler_get_trace(struct SassCompiler* compiler, size_t i)
  //{
  //  Logger& logger(Compiler::unwrap(compiler));
  //  return logger.callStack.at(i).wrap();
  //}
  //
  //const struct SassTrace* ADDCALL sass_compiler_last_trace(struct SassCompiler* compiler)
  //{
  //  Logger& logger(Compiler::unwrap(compiler));
  //  return logger.callStack.back().wrap();
  //}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  //   bool ADDCALL sass_compiler_get_source_comments(struct SassCompiler* compiler)
  //   {
  //     return Compiler::unwrap(compiler).source_comments;
  //   }
  // 
  //   void ADDCALL sass_compiler_set_source_comments(struct SassCompiler* compiler, bool source_comments)
  //   {
  //     Compiler::unwrap(compiler).source_comments = source_comments;
  //   }

}
