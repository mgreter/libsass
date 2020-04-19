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
    static void handle_string_error(SassOptionsCpp* c_ctx, const sass::string& msg, int severety)
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
    SassOptionsCpp* c_ctx = compiler->c_ctx;
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

  
  struct SassCompiler* ADDCALL sass_make_compiler()
  {
    Sass::Compiler* compiler = new Sass::Compiler();
    return reinterpret_cast<struct SassCompiler*>(compiler); //
  }

  static int handle_error(struct SassCompiler* compiler2)
  {

    Sass::Compiler* compiler = reinterpret_cast<Sass::Compiler*>(compiler2);

    try {
      throw;
    }
    catch (Exception::Base & e) {

      Logger& logger(*compiler->logger123);

      sass::ostream& msg_stream = logger.errors;
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

      sass::string rel_path(Sass::File::abs2rel(pstate.getAbsPath(), CWD, CWD));
      logger.writeStackTraces(logger.errors, e.traces, "  ");

      JsonNode* json_err = json_mkobject();
      json_append_member(json_err, "status", json_mknumber(1));
      json_append_member(json_err, "file", json_mkstring(pstate.getAbsPath()));
      json_append_member(json_err, "line", json_mknumber((double)(pstate.getLine())));
      json_append_member(json_err, "column", json_mknumber((double)(pstate.getColumn())));
      json_append_member(json_err, "message", json_mkstring(e.what()));
      json_append_member(json_err, "formatted", json_mkstream(msg_stream));
      try { compiler->error_json = json_stringify(json_err, "  "); }
      catch (...) { compiler->error_json = sass_copy_c_string("{\"status\":99}"); }
      json_delete(json_err);

      compiler->error_message = msg_stream.str();
      compiler->error_text = e.what();
      compiler->error_status = 1;
      compiler->error_file = pstate.getAbsPath();
      compiler->error_line = pstate.getLine();
      compiler->error_column = pstate.getColumn();
      compiler->error_src = pstate.getSource();

    }
    catch (std::bad_alloc &) {
      std::cerr << "Error std::bad_alloc\n";
    }
    catch (std::exception &) {
      std::cerr << "Error std::exception\n";
    }
    catch (sass::string &) {
      std::cerr << "Error sass::string\n";
    }
    catch (const char*) {
      std::cerr << "Error const char*\n";
    }
    catch (...) {
      std::cerr << "Error any else\n";
    }

    return 0;
  }

  // allow one error handler to throw another error
  // this can happen with invalid utf8 and json lib
  static int handle_errors(struct SassCompiler* compiler) {
    try { return handle_error(compiler); }
    catch (...) { return handle_error(compiler); }
  }

  void ADDCALL sass_compiler_parse(struct SassCompiler* compiler2)
  {
    Sass::Compiler* compiler = reinterpret_cast<Sass::Compiler*>(compiler2);
    try { compiler->parse(); }
    catch (...) { handle_errors(compiler2); }

  }

  void ADDCALL sass_compiler_compile(struct SassCompiler* compiler2)
  {
    Sass::Compiler* compiler = reinterpret_cast<Sass::Compiler*>(compiler2);
    try { compiler->compile(); }
    catch (...) { handle_errors(compiler2); }
  }

  void ADDCALL sass_compiler_render(struct SassCompiler* compiler2)
  {
    Sass::Compiler* compiler = reinterpret_cast<Sass::Compiler*>(compiler2);
    // Render the output css
    // Render the source map json
    // Embed the source map after output
    OutputBuffer output(compiler->renderCss());

    compiler->output = output.buffer;

    bool source_map_include = false;
    bool source_map_file_urls = false;
    const char* source_map_root = 0;

    struct SassSrcMapOptions options;
    options.source_map_mode = SASS_SRCMAP_EMBED_JSON;
    options.source_map_path = compiler->output_path + ".map";

    options.source_map_origin = compiler->entry->srcdata->getAbsPath();

    switch (options.source_map_mode) {
    case SASS_SRCMAP_NONE:
      compiler->srcmap = 0;
      compiler->footer = 0;
      break;
    case SASS_SRCMAP_CREATE:
      compiler->srcmap = compiler->renderSrcMapJson(options, output.smap);
      compiler->footer = 0; // Don't add any reference, just create it
      break;
    case SASS_SRCMAP_EMBED_LINK:
      compiler->srcmap = compiler->renderSrcMapJson(options, output.smap);
      compiler->footer = compiler->renderSrcMapLink(options, output.smap);
      break;
    case SASS_SRCMAP_EMBED_JSON:
      compiler->srcmap = compiler->renderSrcMapJson(options, output.smap);
      compiler->footer = compiler->renderEmbeddedSrcMap(options, output.smap);
      break;
    }

  }


  ADDAPI const char* ADDCALL sass_compiler_get_output_string(struct SassCompiler* compiler)
  {
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

  void ADDCALL sass_compiler_set_entry_point(struct SassCompiler* compiler, struct SassImportCpp* import)
  {
    reinterpret_cast<Sass::Compiler*>(compiler)->entry = import;
  }

  void ADDCALL sass_compiler_set_output_path(struct SassCompiler* compiler, const char* output_path)
  {
    reinterpret_cast<Sass::Compiler*>(compiler)->output_path = output_path ? output_path : "";
  }

  int ADDCALL sass_compiler_get_error_status(struct SassCompiler* compiler) { return reinterpret_cast<Sass::Compiler*>(compiler)->error_status; }
  const char* ADDCALL sass_compiler_get_error_json(struct SassCompiler* compiler) { return reinterpret_cast<Sass::Compiler*>(compiler)->error_json.c_str(); }
  const char* ADDCALL sass_compiler_get_error_text(struct SassCompiler* compiler) { return reinterpret_cast<Sass::Compiler*>(compiler)->error_text.c_str(); }
  const char* ADDCALL sass_compiler_get_error_message(struct SassCompiler* compiler) { return reinterpret_cast<Sass::Compiler*>(compiler)->error_message.c_str(); }
  const char* ADDCALL sass_compiler_get_error_file(struct SassCompiler* compiler) { return reinterpret_cast<Sass::Compiler*>(compiler)->error_file.c_str(); }
  const char* ADDCALL sass_compiler_get_error_src(struct SassCompiler* compiler) { return reinterpret_cast<Sass::Compiler*>(compiler)->error_src->content(); }
  size_t ADDCALL sass_compiler_get_error_line(struct SassCompiler* compiler) { return reinterpret_cast<Sass::Compiler*>(compiler)->error_line; }
  size_t ADDCALL sass_compiler_get_error_column(struct SassCompiler* compiler) { return reinterpret_cast<Sass::Compiler*>(compiler)->error_column; }


  void ADDCALL sass_context_print_stderr2(struct SassContext* context)
  {
    // const char* message = sass_context_get_stderr_string2(context);
    // if (message != nullptr) Terminal::print(message, true);
  }
   

  void ADDCALL sass_print_stderr(const char* message)
  {
    Terminal::print(message, true);
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
  void ADDCALL sass_compiler_load_plugins(struct SassCompiler* compiler, const char* path)
  {
    reinterpret_cast<Sass::Compiler*>(compiler)->include_paths.push_back(path); // method addIncludePath
  }

  // Push function for plugin paths (no manipulation support for now)
  void ADDCALL sass_compiler_push_include_path(struct SassCompiler* compiler, const char* path)
  {
    reinterpret_cast<Sass::Compiler*>(compiler)->loadPlugins(path);
  }

  void ADDCALL sass_compiler_set_precision(struct SassCompiler* compiler, int precision)
  {
    reinterpret_cast<Sass::Compiler*>(compiler)->precision = precision;
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



}
