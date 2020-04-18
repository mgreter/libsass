// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"
#include "ast.hpp"

#include "sass_functions.hpp"
#include "sass_context.hpp"
#include "backtrace.hpp"
#include "terminal.hpp"
#include "source.hpp"
#include "json.hpp"
#include <iomanip>

#define LFEED "\n"

// C++ helper
namespace Sass {

  // see sass_copy_c_string(sass::string str)
  static inline JsonNode* json_mkstream(const sass::ostream& stream)
  {
    // hold on to string on stack!
    sass::string str(stream.str());
    return json_mkstring(str.c_str());
  }

  /*
    static void handle_string_error(SassContextCpp* c_ctx, const sass::string& msg, int severety)
  {
    sass::ostream msg_stream;
    JsonNode* json_err = json_mkobject();
    msg_stream << "Error: " << msg << STRMLF;
    json_append_member(json_err, "status", json_mknumber(severety));
    json_append_member(json_err, "message", json_mkstring(msg.c_str()));
    json_append_member(json_err, "formatted", json_mkstream(msg_stream));
    // try { c_ctx->error_json = json_stringify(json_err, "  "); }
    // catch (...) {}
    // c_ctx->error_message = msg_stream.str();
    // c_ctx->error_text = msg;
    // c_ctx->error_status = severety;
    json_delete(json_err);
  }

  static Block_Obj sass_parse_block(struct SassCompiler* compiler) throw()
  {

    // assert valid pointer
    if (compiler == 0) return {};
    // The CPP context must be set by now
    Context* cpp_ctx = compiler->cpp_ctx;
    SassContextCpp* c_ctx = compiler->c_ctx;
    // We will take care to wire up the rest
    compiler->cpp_ctx->c_compiler = compiler;
    compiler->state = SASS_COMPILER_PARSED;

    try {

      // get input/output path from options
      sass::string input_path = c_ctx->input_path;
      sass::string output_path = c_ctx->output_path;

      // maybe skip some entries of included files
      // we do not include stdin for data contexts
      bool skip = c_ctx->type == SASS_CONTEXT_DATA;

      // dispatch parse call
      Block_Obj root(cpp_ctx->parse(SASS_IMPORT_AUTO));
      // abort on errors
      if (!root) return {};

      // skip all prefixed files? (ToDo: check srcmap)
      // IMO source-maps should point to headers already
      // therefore don't skip it for now. re-enable or
      // remove completely once this is tested
      size_t headers = cpp_ctx->head_imports;

      // copy the included files on to the context (don't forget to free later)
      const sass::vector<sass::string>& add(cpp_ctx->get_included_files(skip, headers));
      c_ctx->included_files.insert(c_ctx->included_files.end(), add.begin(), add.end());

      // return parsed block
      return root;

    }
    // pass errors to generic error handler
    catch (...) { handle_errors(c_ctx); }

    // error
    return {};

  }
  */
}

extern "C" {
  using namespace Sass;

  static void sass_clear_options (struct SassOptionsCpp* options);
  static void sass_reset_options (struct SassOptionsCpp* options);
  static void copy_options(struct SassOptionsCpp* to, struct SassOptionsCpp* from) {
    // do not overwrite ourself
    if (to == from) return;
    // free assigned memory
    sass_clear_options(to);
    // move memory
    *to = *from;
    // Reset pointers on source
    sass_reset_options(from);
  }

  #define IMPLEMENT_SASS_OPTION_ACCESSOR(type, option) \
    type ADDCALL sass_option_get_##option (struct SassOptionsCpp* options) { return options->option; } \
    void ADDCALL sass_option_set_##option (struct SassOptionsCpp* options, type option) { options->option = option; }
  #define IMPLEMENT_SASS_OPTION_STRING_GETTER(type, option, def) \
    type ADDCALL sass_option_get_##option (struct SassOptionsCpp* options) { return safe_str(options->option, def); }
  #define IMPLEMENT_SASS_OPTION_STRING_SETTER(type, option, def) \
    void ADDCALL sass_option_set_##option (struct SassOptionsCpp* options, type option) \
    { free(options->option); options->option = option || def ? sass_copy_c_string(option ? option : def) : 0; }
  #define IMPLEMENT_SASS_OPTION_STRING_ACCESSOR(type, option, def) \
    IMPLEMENT_SASS_OPTION_STRING_GETTER(type, option, def) \
    IMPLEMENT_SASS_OPTION_STRING_SETTER(type, option, def)




  bool IsConsoleRedirected() {
    HANDLE handle = GetStdHandle(STD_ERROR_HANDLE);
    if (handle != INVALID_HANDLE_VALUE) {
      DWORD filetype = GetFileType(handle);
      if (!((filetype == FILE_TYPE_UNKNOWN) && (GetLastError() != ERROR_SUCCESS))) {
        DWORD mode;
        filetype &= ~(FILE_TYPE_REMOTE);
        if (filetype == FILE_TYPE_CHAR) {
          bool retval = GetConsoleMode(handle, &mode);
          if ((retval == false) && (GetLastError() == ERROR_INVALID_HANDLE)) {
            return true;
          }
          else {
            return false;
          }
        }
        else {
          return true;
        }
      }
    }
    // TODO: Not even a stdout so this is not even a console?
    return false;
  }
  /*
  // generic compilation function (not exported, use file/data compile instead)
  static SassCompilerCpp* sass_prepare_context (Context* cpp_ctx) throw()
  {

    SassContextCpp* c_ctx = &cpp_ctx->c_options;

    try {

      // register our custom handlers
      cpp_ctx->addCustomHeaders(c_ctx->c_headers);
      cpp_ctx->addCustomFunctions(c_ctx->c_functions);
      cpp_ctx->addCustomImporters(c_ctx->c_importers);

      // reset error status
      c_ctx->error_status = 0;
      // reset error position
      c_ctx->error_line = sass::string::npos;
      c_ctx->error_column = sass::string::npos;

      // allocate a new compiler instance
      void* ctxmem = new SassCompilerCpp{};
      if (ctxmem == 0) { std::cerr << "Error allocating memory for context" << STRMLF; return 0; }
      SassCompilerCpp* compiler = (struct SassCompilerCpp*) ctxmem;
      compiler->state = SASS_COMPILER_CREATED;

      // store in sass compiler
      compiler->c_ctx = c_ctx;
      compiler->cpp_ctx = cpp_ctx;
      cpp_ctx->c_compiler = compiler;
      cpp_ctx->logger->style = c_ctx->logstyle;

      // use to parse block
      return compiler;

    }
    // pass errors to generic error handler
    catch (...) { handle_errors(c_ctx); }

    // error
    return 0;

  }
  */
  
  struct SassCompiler* ADDCALL sass_make_compiler(struct SassContext* context, struct SassImportCpp* entry)
  {
    return new SassCompiler(context, entry);
  }


  static int handle_error(struct SassCompiler* compiler)
  {
    try {
      throw;
    }
    catch (Exception::Base & e) {

      Logger& logger(compiler->logger);

      sass::ostream& msg_stream = logger.errors;
      sass::string cwd(Sass::File::get_cwd());
      sass::string msg_prefix("Error");
      bool got_newline = false;
      msg_stream << msg_prefix << ": ";
      const char* msg = e.what();
      while (msg && *msg) {
        if (*msg == '\r') {
          got_newline = true;
        }
        else if (*msg == '\n') {
          got_newline = true;
        }
        else if (got_newline) {
          // msg_stream << sass::string(msg_prefix.size() + 2, ' ');
          got_newline = false;
        }
        msg_stream << *msg;
        ++msg;
      }
      if (!got_newline) msg_stream << "\n";

      auto pstate = SourceSpan::tmp("[NOPE]");
      if (e.traces.empty()) {
        std::cerr << "THIS IS OLD AND NOT YET\n";
      }
      else {
        pstate = e.traces.back().pstate;
      }

      sass::string rel_path(Sass::File::abs2rel(pstate.getAbsPath(), cwd, cwd));
      logger.writeStackTraces(logger.errors, e.traces, "  ");
      JsonNode* json_err = json_mkobject();
      json_append_member(json_err, "status", json_mknumber(1));
      json_append_member(json_err, "file", json_mkstring(pstate.getAbsPath()));
      json_append_member(json_err, "line", json_mknumber((double)(pstate.getLine())));
      json_append_member(json_err, "column", json_mknumber((double)(pstate.getColumn())));
      json_append_member(json_err, "message", json_mkstring(e.what()));
      json_append_member(json_err, "formatted", json_mkstream(msg_stream));
      try { compiler->error_json = json_stringify(json_err, "  "); }
      catch (...) {} // silently ignore this error?
      compiler->error_message = msg_stream.str();
      compiler->error_text = e.what();
      compiler->error_status = 1;
      compiler->error_file = pstate.getAbsPath();
      compiler->error_line = pstate.getLine();
      compiler->error_column = pstate.getColumn();
      compiler->error_src = pstate.getSource();
      json_delete(json_err);

    }
    catch (std::bad_alloc & ba) {
      std::cerr << "Error std::bad_alloc\n";
    }
    catch (std::exception & e) {
      std::cerr << "Error std::exception\n";
    }
    catch (sass::string & e) {
      std::cerr << "Error sass::string\n";
    }
    catch (const char* e) {
      std::cerr << "Error const char*\n";
    }
    catch (...) {
      std::cerr << "Error any else\n";
    }
  }

  // allow one error handler to throw another error
  // this can happen with invalid utf8 and json lib
  static int handle_errors(struct SassCompiler* compiler) {
    try { return handle_error(compiler); }
    catch (...) { return handle_error(compiler); }
  }

  void ADDCALL sass_compiler_parse(struct SassCompiler* compiler)
  {
    try { compiler->parse(); }
    catch (...) { handle_errors(compiler); }

  }

  void ADDCALL sass_compiler_compile(struct SassCompiler* compiler)
  {
    try { compiler->compile(); }
    catch (...) { handle_errors(compiler); }
  }

  void ADDCALL sass_compiler_render(struct SassCompiler* compiler)
  {
    OutputBuffer output(compiler->render23());
    compiler->output = output.buffer;
  }


  ADDAPI const char* ADDCALL sass_compiler_get_output_string(struct SassCompiler* compiler)
  {
    return compiler->output.c_str();
  }

  ADDAPI const char* ADDCALL sass_compiler_get_srcmap_string(struct SassCompiler* compiler)
  {
    return compiler->srcmap.c_str();
  }

  int ADDCALL sass_compiler_get_error_status(struct SassCompiler* compiler) { return compiler->error_status; }
  const char* ADDCALL sass_compiler_get_error_json(struct SassCompiler* compiler) { return compiler->error_json.c_str(); }
  const char* ADDCALL sass_compiler_get_error_text(struct SassCompiler* compiler) { return compiler->error_text.c_str(); }
  const char* ADDCALL sass_compiler_get_error_message(struct SassCompiler* compiler) { return compiler->error_message.c_str(); }
  const char* ADDCALL sass_compiler_get_error_file(struct SassCompiler* compiler) { return compiler->error_file.c_str(); }
  const char* ADDCALL sass_compiler_get_error_src(struct SassCompiler* compiler) { return compiler->error_src->content(); }
  size_t ADDCALL sass_compiler_get_error_line(struct SassCompiler* compiler) { return compiler->error_line; }
  size_t ADDCALL sass_compiler_get_error_column(struct SassCompiler* compiler) { return compiler->error_column; }


  void ADDCALL sass_context_print_stderr2(struct SassContext* context)
  {
    // const char* message = sass_context_get_stderr_string2(context);
    // if (message != nullptr) Terminal::print(message, true);
  }
   

  void ADDCALL sass_print_stderr(const char* message)
  {
    Terminal::print(message, true);
  }



  /*
  int ADDCALL sass_compiler_execute(struct SassCompilerCpp* compiler)
  {
    if (compiler == 0) return 1;
    if (compiler->state == SASS_COMPILER_EXECUTED) return 0;
    if (compiler->state != SASS_COMPILER_PARSED) return -1;
    if (compiler->c_ctx == NULL) return 1;
    if (compiler->cpp_ctx == NULL) return 1;
    Context* cpp_ctx = compiler->cpp_ctx;
    if (compiler->root.isNull()) {
      compiler->c_ctx->stderr_string = cpp_ctx->render_stderr();
      return 1;
    }
    if (compiler->c_ctx->error_status) {
      compiler->c_ctx->stderr_string = cpp_ctx->render_stderr();
      return compiler->c_ctx->error_status;
    }
    compiler->state = SASS_COMPILER_EXECUTED;
    Block_Obj root = compiler->root;
    // compile the parsed root block
    try { compiler->c_ctx->output_string = cpp_ctx->render(root); }
    // pass caught errors to generic error handler
    catch (...) {
      compiler->c_ctx->stderr_string = cpp_ctx->render_stderr();
      return handle_errors(compiler->c_ctx) | 1;
    }
    // generate source map json and store on context
    compiler->c_ctx->source_map_string = cpp_ctx->render_srcmap();
    compiler->c_ctx->stderr_string = cpp_ctx->render_stderr();
    // success
    return 0;
  }
  */

  // helper function, not exported, only accessible locally
  static void sass_reset_options (struct SassOptionsCpp* options)
  {
  }

  // helper function, not exported, only accessible locally
  static void sass_clear_options (struct SassOptionsCpp* options)
  {
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
  void ADDCALL sass_context_push_include_path(struct SassContext* context, const char* path)
  {
    reinterpret_cast<Context*>(context)->include_paths.push_back(path);
  }

  // Push function for plugin paths (no manipulation support for now)
  void ADDCALL sass_context_push_plugin_path(struct SassContext* context, const char* path)
  {
    reinterpret_cast<Context*>(context)->plugin_paths.push_back(path);
  }

  struct SassContext* ADDCALL sass_make_context()
  {
    return reinterpret_cast<SassContext*>(new Sass::Context());
  }

  void ADDCALL sass_context_set_precision(struct SassContext* context, int precision)
  {
    reinterpret_cast<Sass::Context*>(context)->precision = precision;
  }
  void ADDCALL sass_context_set_output_style(struct SassContext* context, enum Sass_Output_Style output_style)
  {
    reinterpret_cast<Sass::Context*>(context)->c_options.output_style = output_style;
  }

  int ADDCALL sass_context_get_precision(struct SassContext* context)
  {
    return reinterpret_cast<Sass::Context*>(context)->precision;
  }
  enum Sass_Output_Style ADDCALL sass_context_get_output_style(struct SassContext* context)
  {
    return reinterpret_cast<Sass::Context*>(context)->c_options.output_style;
  }

  bool ADDCALL sass_context_get_source_comments(struct SassContext* context)
  {
    return reinterpret_cast<Sass::Context*>(context)->source_comments;
  }

  bool ADDCALL sass_context_get_source_map_embed(struct SassContext* context)
  {
    return reinterpret_cast<Sass::Context*>(context)->source_map_embed;
  }
  bool ADDCALL sass_context_get_source_map_contents(struct SassContext* context)
  {
    return reinterpret_cast<Sass::Context*>(context)->source_map_contents;
  }

  bool ADDCALL sass_context_get_source_map_file_urls(struct SassContext* context)
  {
    return reinterpret_cast<Sass::Context*>(context)->source_comments;
  }

  bool ADDCALL sass_context_get_omit_source_map_url(struct SassContext* context)
  {
    return reinterpret_cast<Sass::Context*>(context)->omit_source_map_url;
  }

  void ADDCALL sass_context_set_source_comments(struct SassContext* context, bool source_comments)
  {
    reinterpret_cast<Sass::Context*>(context)->source_comments = source_comments;
  }
  void ADDCALL sass_context_set_source_map_embed(struct SassContext* context, bool source_map_embed)
  {
    reinterpret_cast<Sass::Context*>(context)->source_map_embed = source_map_embed;
  }
  void ADDCALL sass_context_set_source_map_contents(struct SassContext* context, bool source_map_contents)
  {
    reinterpret_cast<Sass::Context*>(context)->source_map_contents = source_map_contents;
  }
  void ADDCALL sass_context_set_source_map_file_urls(struct SassContext* context, bool source_map_file_urls)
  {
    reinterpret_cast<Sass::Context*>(context)->source_map_file_urls = source_map_file_urls;
  }
  void ADDCALL sass_context_set_omit_source_map_url(struct SassContext* context, bool omit_source_map_url)
  {
    reinterpret_cast<Sass::Context*>(context)->omit_source_map_url = omit_source_map_url;
  }

  const char* ADDCALL sass_context_get_input_path(struct SassContext* context)
  {
    return reinterpret_cast<Sass::Context*>(context)->input_path.c_str();
  }

  const char* ADDCALL sass_context_get_output_path(struct SassContext* context)
  {
    return reinterpret_cast<Sass::Context*>(context)->output_path.c_str();
  }

  const char* ADDCALL sass_context_get_source_map_file(struct SassContext* context)
  {
    return reinterpret_cast<Sass::Context*>(context)->source_map_file.c_str();
  }
  const char* ADDCALL sass_context_get_source_map_root(struct SassContext* context)
  {
    return reinterpret_cast<Sass::Context*>(context)->source_map_root.c_str();
  }

  void ADDCALL sass_context_set_entry_point(struct SassContext* context, struct SassImportCpp* entry)
  {
    reinterpret_cast<Sass::Context*>(context)->entry = entry;
  }

  void ADDCALL sass_context_set_input_path(struct SassContext* context, const char* input_path)
  {
    reinterpret_cast<Sass::Context*>(context)->input_path = input_path;
    reinterpret_cast<Sass::Context*>(context)->input_path88 = input_path;
  }
  void ADDCALL sass_context_set_output_path(struct SassContext* context, const char* output_path)
  {
    reinterpret_cast<Sass::Context*>(context)->output_path = output_path;
    reinterpret_cast<Sass::Context*>(context)->output_path88 = output_path;
  }

  void ADDCALL sass_context_set_source_map_file(struct SassContext* context, const char* source_map_file)
  {
    reinterpret_cast<Sass::Context*>(context)->source_map_file = source_map_file;
    reinterpret_cast<Sass::Context*>(context)->source_map_file88 = source_map_file;
  }
  void ADDCALL sass_context_set_source_map_root(struct SassContext* context, const char* source_map_root)
  {
    reinterpret_cast<Sass::Context*>(context)->source_map_root = source_map_root;
    reinterpret_cast<Sass::Context*>(context)->source_map_root88 = source_map_root;
  }


}
