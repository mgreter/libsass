// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"
#include "ast.hpp"

#include "sass_functions.hpp"
#include "sass_context.hpp"
#include "backtrace.hpp"
#include "compiler.hpp"
#include "terminal.hpp"
#include "source.hpp"
#include "json.hpp"
#include <iomanip>

using namespace Sass;

// Local helper to add streams to json objects
static inline JsonNode* json_mkstream(const sass::ostream& stream)
{
  // hold on to string on stack!
  sass::string str(stream.str());
  return json_mkstring(str.c_str());
}
// EO json_mkstream

static int handle_error(Sass::Compiler& compiler, StackTraces* traces, const char* msg, int severety)
{

  Logger& logger(*compiler.logger123);

  bool has_final_lf = false;
  logger.errors << "Error: ";
  // Add message and ensure it is
  // added with a final line-feed.
  if (msg != nullptr) {
    logger.errors << msg;
    while (*msg) {
      has_final_lf =
        *msg == '\r' ||
        *msg == '\n';
      ++msg;
    }
    if (!has_final_lf) {
      logger.errors << STRMLF;
    }
  }

  JsonNode* json_err = json_mkobject();

  // Some stuff is only logged if we have some traces
  // Otherwise we don't know where the error comes from
  if (traces && traces->size() > 0) {
    SourceSpan& pstate = traces->back().pstate;
    sass::string rel_path(File::abs2rel(pstate.getAbsPath(), CWD));
    logger.writeStackTraces(logger.errors, *traces, "  ");
    json_append_member(json_err, "file", json_mkstring(pstate.getAbsPath()));
    json_append_member(json_err, "line", json_mknumber((double)(pstate.getLine())));
    json_append_member(json_err, "column", json_mknumber((double)(pstate.getColumn())));
    compiler.error_src = pstate.getSource();
    compiler.error_file = pstate.getAbsPath();
    compiler.error_line = pstate.getLine();
    compiler.error_column = pstate.getColumn();
  }

  // Attach the generic error reporting items
  json_append_member(json_err, "status", json_mknumber(1));
  json_append_member(json_err, "message", json_mkstring(msg));
  json_append_member(json_err, "warnings", json_mkstream(logger.warnings));
  json_append_member(json_err, "formatted", json_mkstream(logger.errors));

  // The stringification may fail for whatever reason
  try { compiler.error_json = json_stringify(json_err, "  "); }
  // If it fails at least return a valid json with special status 9999
  catch (...) { compiler.error_json = sass_copy_c_string("{\"status\":9999}"); }

  // Delete json tree
  json_delete(json_err);

  compiler.warning_message = logger.warnings.str();
  compiler.error_message = logger.errors.str();
  compiler.error_status = severety;
  compiler.error_text = msg;

  return severety;

}

// 
static int handle_error(Sass::Compiler& compiler)
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
  return compiler.error_status;
}

// allow one error handler to throw another error
// this can happen with invalid utf8 and json lib
// Note: this might be obsolete but doesn't hurt!?
static int handle_errors(Sass::Compiler& compiler)
{
  try { return handle_error(compiler); }
  catch (...) { return handle_error(compiler); }
}

extern "C" {

 
  struct SassCompiler* ADDCALL sass_make_compiler()
  {
    return Compiler::wrap(new Sass::Compiler());
  }

  bool ADDCALL sass_compiler_parse(struct SassCompiler* sass_compiler)
  {
    Compiler& compiler(Compiler::unwrap(sass_compiler));
    try { compiler.parse(); return false; }
    catch (...) { handle_errors(compiler); }
    return true;
  }

  bool ADDCALL sass_compiler_compile(struct SassCompiler* sass_compiler)
  {
    Compiler& compiler(Compiler::unwrap(sass_compiler));
    try { compiler.compile(); return false; }
    catch (...) { handle_errors(compiler); }
    return true;
  }

  bool ADDCALL sass_compiler_render(struct SassCompiler* sass_compiler)
  {
   
    Sass::Compiler& compiler(Compiler::unwrap(sass_compiler));

    try {

      // We can always consume the logger streams
      compiler.error_message = compiler.logger123->errors.str();
      compiler.warning_message = compiler.logger123->warnings.str();

      if (compiler.compiled == nullptr) return true;

      OutputBuffer output(compiler.renderCss());

      compiler.output = output.buffer;

      // bool source_map_include = false;
      // bool source_map_file_urls = false;
      // const char* source_map_root = 0;

      struct SassSrcMapOptions options;
      options.source_map_mode = SASS_SRCMAP_EMBED_JSON;
      options.source_map_path = compiler.output_path + ".map";
      options.source_map_origin = compiler.entry_point->srcdata->getAbsPath();

      switch (options.source_map_mode) {
      case SASS_SRCMAP_NONE:
        compiler.srcmap = 0;
        compiler.footer = 0;
        break;
      case SASS_SRCMAP_CREATE:
        compiler.srcmap = compiler.renderSrcMapJson(options, output.smap);
        compiler.footer = 0; // Don't add any reference, just create it
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
      return false;
    }
    catch (...) { handle_errors(compiler); }
    return true;

  }

  ADDAPI const char* ADDCALL sass_compiler_get_output_string(struct SassCompiler* compiler)
  {
    if (reinterpret_cast<Sass::Compiler*>(compiler)->output == "stream://stdout") return nullptr;
    return reinterpret_cast<Sass::Compiler*>(compiler)->output.c_str();
  }

  ADDAPI const char* ADDCALL sass_compiler_get_footer_string(struct SassCompiler* compiler)
  {
    return reinterpret_cast<Sass::Compiler*>(compiler)->footer;
  }

  ADDAPI const char* ADDCALL sass_compiler_get_srcmap_string(struct SassCompiler* compiler)
  {
    return reinterpret_cast<Sass::Compiler*>(compiler)->srcmap;
  }

  void ADDCALL sass_compiler_set_entry_point(struct SassCompiler* compiler, struct SassImport* import)
  {
    reinterpret_cast<Sass::Compiler*>(compiler)->entry_point = import;
  }

  void ADDCALL sass_compiler_set_output_path(struct SassCompiler* compiler, const char* output_path)
  {
    reinterpret_cast<Sass::Compiler*>(compiler)->output_path = output_path ? output_path : "stream://stdout";
  }

  void ADDCALL sass_compiler_set_output_style(struct SassCompiler* compiler, enum Sass_Output_Style output_style)
  {
    reinterpret_cast<Sass::Compiler*>(compiler)->output_style = output_style;
  }

  int ADDCALL sass_compiler_get_error_status(struct SassCompiler* compiler) { return reinterpret_cast<Sass::Compiler*>(compiler)->error_status; }
  const char* ADDCALL sass_compiler_get_error_json(struct SassCompiler* compiler) { return reinterpret_cast<Sass::Compiler*>(compiler)->error_json.c_str(); }
  const char* ADDCALL sass_compiler_get_error_text(struct SassCompiler* compiler) { return reinterpret_cast<Sass::Compiler*>(compiler)->error_text.c_str(); }
  const char* ADDCALL sass_compiler_get_error_message(struct SassCompiler* compiler) { return reinterpret_cast<Sass::Compiler*>(compiler)->error_message.c_str(); }
  const char* ADDCALL sass_compiler_get_warning_message(struct SassCompiler* compiler) { return reinterpret_cast<Sass::Compiler*>(compiler)->warning_message.c_str(); }
  
  const char* ADDCALL sass_compiler_get_error_file(struct SassCompiler* compiler) { return reinterpret_cast<Sass::Compiler*>(compiler)->error_file.c_str(); }
  const char* ADDCALL sass_compiler_get_error_src(struct SassCompiler* compiler) { return reinterpret_cast<Sass::Compiler*>(compiler)->error_src->content(); }
  size_t ADDCALL sass_compiler_get_error_line(struct SassCompiler* compiler) { return reinterpret_cast<Sass::Compiler*>(compiler)->error_line; }
  size_t ADDCALL sass_compiler_get_error_column(struct SassCompiler* compiler) { return reinterpret_cast<Sass::Compiler*>(compiler)->error_column; }


  void ADDCALL sass_context_print_stderr2(struct SassContext* context)
  {
    // const char* message = sass_context_get_stderr_string2(context);
    // if (message != nullptr) Terminal::print(message, true);
  }
   
  void ADDCALL sass_chdir(const char* path)
  {
    if (path != nullptr) {
      CWD = File::rel2abs(path, CWD) + "/";
    }
  }

  void ADDCALL sass_print_stderr(const char* message)
  {
    Terminal::print(message, true);
  }

  void ADDCALL sass_print_stdout(const char* message)
  {
    Terminal::print(message, false);
  }



  void ADDCALL sass_delete_compiler(struct SassCompiler* context)
  {
    // delete context;
  }

  void ADDCALL sass_delete_context(SassContext* context)
  {
    // delete reinterpret_cast<Context*>(context);
  }


  // Push function for include paths (no manipulation support for now)
  void ADDCALL sass_compiler_add_include_paths(struct SassCompiler* compiler, const char* paths)
  {
    reinterpret_cast<Sass::Compiler*>(compiler)->addIncludePaths(paths);
  }

  // Push function for plugin paths (no manipulation support for now)
  void ADDCALL sass_compiler_load_plugins(struct SassCompiler* compiler, const char* paths)
  {
    reinterpret_cast<Sass::Compiler*>(compiler)->loadPlugins(paths);
  }

  void ADDCALL sass_compiler_set_precision(struct SassCompiler* compiler, int precision)
  {
    reinterpret_cast<Sass::Compiler*>(compiler)->precision = precision;
    reinterpret_cast<Sass::Compiler*>(compiler)->logger123->setPrecision(precision);
    
  }

  int ADDCALL sass_compiler_get_precision(struct SassCompiler* compiler)
  {
    return reinterpret_cast<Sass::Compiler*>(compiler)->precision;
  }

  bool ADDCALL sass_compiler_get_source_comments(struct SassCompiler* compiler)
  {
    return reinterpret_cast<Sass::Compiler*>(compiler)->source_comments;
  }

  void ADDCALL sass_compiler_set_source_comments(struct SassCompiler* compiler, bool source_comments)
  {
    reinterpret_cast<Sass::Compiler*>(compiler)->source_comments = source_comments;
  }

  struct SassImport* ADDCALL sass_compiler_get_last_import(struct SassCompiler* compiler)
  {

    return reinterpret_cast<Sass::Compiler*>(compiler)->import_stack.back();

  }

  void ADDCALL sass_compiler_set_logger_style(struct SassCompiler* compiler, enum Sass_Logger_Style log_style)
  {
    if (log_style == SASS_LOGGER_AUTO) log_style = SASS_LOGGER_ASCII_MONO;
    reinterpret_cast<Sass::Compiler*>(compiler)->logger123->style = log_style;
  }



}
