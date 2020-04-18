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

  static void handle_string_error(SassContextCpp* c_ctx, const sass::string& msg, int severety)
  {
    sass::ostream msg_stream;
    JsonNode* json_err = json_mkobject();
    msg_stream << "Error: " << msg << STRMLF;
    json_append_member(json_err, "status", json_mknumber(severety));
    json_append_member(json_err, "message", json_mkstring(msg.c_str()));
    json_append_member(json_err, "formatted", json_mkstream(msg_stream));
    try { c_ctx->error_json = json_stringify(json_err, "  "); }
    catch (...) {}
    c_ctx->error_message = msg_stream.str();
    c_ctx->error_text = msg;
    c_ctx->error_status = severety;
    json_delete(json_err);
  }





  static int handle_error(SassContextCpp* c_ctx) {
    try {
      throw;
    }
    catch (Exception::Base& e) {

      Logger logger(10, c_ctx->logstyle);

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
      try { c_ctx->error_json = json_stringify(json_err, "  "); }
      catch (...) {} // silently ignore this error?
      c_ctx->error_message = msg_stream.str();
      c_ctx->error_text = e.what();
      c_ctx->error_status = 1;
      c_ctx->error_file = pstate.getAbsPath();
      c_ctx->error_line = pstate.getLine();
      c_ctx->error_column = pstate.getColumn();
      c_ctx->error_src = pstate.getContent();
      json_delete(json_err);
    }
    catch (std::bad_alloc& ba) {
      sass::sstream msg_stream;
      msg_stream << "Unable to allocate memory: " << ba.what();
      handle_string_error(c_ctx, msg_stream.str(), 2);
    }
    catch (std::exception& e) {
      handle_string_error(c_ctx, e.what(), 3);
    }
    catch (sass::string& e) {
      handle_string_error(c_ctx, e, 4);
    }
    catch (const char* e) {
      handle_string_error(c_ctx, e, 4);
    }
    catch (...) {
      handle_string_error(c_ctx, "unknown", 5);
    }
    return c_ctx->error_status;
  }



  static int handle_error(SassContextReal* a_ctx) {

    Context* c_ctx = reinterpret_cast<Context*>(a_ctx);

    try {
      throw;
    }
    catch (Exception::Base & e) {

      Logger logger(10, c_ctx->logstyle);

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
      try { c_ctx->error_json = json_stringify(json_err, "  "); }
      catch (...) {} // silently ignore this error?
      c_ctx->error_message = msg_stream.str();
      c_ctx->error_text = e.what();
      c_ctx->error_status = 1;
      c_ctx->error_file = pstate.getAbsPath();
      c_ctx->error_line = pstate.getLine();
      c_ctx->error_column = pstate.getColumn();
      c_ctx->error_src = pstate.getContent();
      json_delete(json_err);
    }
    catch (std::bad_alloc & ba) {
      sass::sstream msg_stream;
      msg_stream << "Unable to allocate memory: " << ba.what();
      handle_string_error(c_ctx, msg_stream.str(), 2);
    }
    catch (std::exception & e) {
      handle_string_error(c_ctx, e.what(), 3);
    }
    catch (sass::string & e) {
      handle_string_error(c_ctx, e, 4);
    }
    catch (const char* e) {
      handle_string_error(c_ctx, e, 4);
    }
    catch (...) {
      handle_string_error(c_ctx, "unknown", 5);
    }
    return c_ctx->error_status;
  }

  // allow one error handler to throw another error
  // this can happen with invalid utf8 and json lib
  static int handle_errors(SassContextCpp* c_ctx) {
    try { return handle_error(c_ctx); }
    catch (...) { return handle_error(c_ctx); }
  }

  // allow one error handler to throw another error
  // this can happen with invalid utf8 and json lib
  static int handle_errors(SassContextReal* c_ctx) {
    try { return handle_error(c_ctx); }
    catch (...) { return handle_error(c_ctx); }
  }
  /*
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
  
  inline void init_options (struct SassOptionsCpp* options)
  {
    options->precision = 10;
    options->indent = "  ";
    options->linefeed = LFEED;
  }

  struct SassCompiler* ADDCALL sass_make_compiler(struct SassContextReal* context, struct SassImportCpp* entry)
  {
    return new SassCompiler(context, entry);
  }

  void ADDCALL sass_compiler_parse(struct SassCompiler* compiler)
  {
    compiler->parse();
  }

  void ADDCALL sass_compiler_compile322(struct SassCompiler* compiler)
  {
    compiler->compile();
  }

  void ADDCALL sass_compiler_render322(struct SassCompiler* compiler)
  {
    OutputBuffer output(compiler->render23());
    compiler->output = output.buffer;
  }

  ADDAPI const char* ADDCALL sass_compiler_get_output(struct SassCompiler* compiler)
  {
    return compiler->output.c_str();
  }


  void ADDCALL sass_context_print_stderr2(struct SassContextReal* context)
  {
    const char* message = sass_context_get_stderr_string2(context);
    if (message != nullptr) Terminal::print(message, true);
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

  void ADDCALL sass_delete_context(SassContextReal* context)
  {
    // delete reinterpret_cast<Context*>(context);
  }


  // Push function for include paths (no manipulation support for now)
  void ADDCALL sass_option_push_include_path(struct SassOptionsCpp* options, const char* path)
  {
    options->include_paths.push_back(path);
  }

  // Push function for plugin paths (no manipulation support for now)
  void ADDCALL sass_option_push_plugin_path(struct SassOptionsCpp* options, const char* path)
  {
    options->plugin_paths.push_back(path);
  }

}
