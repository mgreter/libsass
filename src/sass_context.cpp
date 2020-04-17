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

  // allow one error handler to throw another error
  // this can happen with invalid utf8 and json lib
  static int handle_errors(SassContextCpp* c_ctx) {
    try { return handle_error(c_ctx); }
    catch (...) { return handle_error(c_ctx); }
  }

  static Block_Obj sass_parse_block(SassCompilerCpp* compiler) throw()
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


#define IMPLEMENT_SASS_OPTION_STRING2_GETTER(option) \
    const char* ADDCALL sass_option_get_##option (struct SassOptionsCpp* options) \
    { return options->option.empty() ? 0 : options->option.c_str(); }

#define IMPLEMENT_SASS_OPTION_STRING2_SETTER(option) \
    void ADDCALL sass_option_set_##option (struct SassOptionsCpp* options, const char* value) \
    { options->option = value; }

#define IMPLEMENT_SASS_OPTION_STRING2_ACCESSOR(option) \
    IMPLEMENT_SASS_OPTION_STRING2_GETTER(option) \
    IMPLEMENT_SASS_OPTION_STRING2_SETTER(option)

  // TODO: return empty string too?
#define IMPLEMENT_SASS_CONTEXT_STRING2_GETTER(option) \
    const char* ADDCALL sass_context_get_##option (struct SassContextCpp* ctx) { return ctx->option.empty() ? 0 : ctx->option.c_str(); }


#define IMPLEMENT_SASS_CONTEXT_GETTER(type, option) \
    type ADDCALL sass_context_get_##option (struct SassContextCpp* ctx) { return ctx->option; }

#define IMPLEMENT_SASS_CONTEXT_TAKER(type, option) \
    type sass_context_take_##option (struct SassContextCpp* ctx) \
    { type foo = ctx->option; ctx->option = 0; return foo; }



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

  // generic compilation function (not exported, use file/data compile instead)
  static int sass_compile_context (Context* cpp_ctx)
  {

    SassContextCpp* c_ctx = &cpp_ctx->c_options;

    // prepare sass compiler with context and options
    SassCompilerCpp* compiler = sass_prepare_context(cpp_ctx);

    try {
      // call each compiler step
      // This is parse and execute now
      sass_compiler_parse(compiler);
      // This is only emitting stuff
      sass_compiler_execute(compiler);
    }
    // pass errors to generic error handler
    catch (...) { handle_errors(c_ctx); }

    sass_delete_compiler(compiler);

    return c_ctx->error_status;
  }

  inline void init_options (struct SassOptionsCpp* options)
  {
    options->precision = 10;
    options->indent = "  ";
    options->linefeed = LFEED;
  }

  SassOptionsCpp* ADDCALL sass_make_options (void)
  {
    struct SassOptionsCpp* options = new SassOptionsCpp{};
    if (options == 0) { std::cerr << "Error allocating memory for options" << STRMLF; return 0; }
    init_options(options);
    return options;
  }

  SassFileContextCpp* ADDCALL sass_make_file_context(const char* input_path)
  {
    #ifdef DEBUG_SHARED_PTR
    SharedObj::setTaint(true);
    #endif
    struct SassFileContextCpp* ctx = new SassFileContextCpp{};
    if (ctx == 0) { std::cerr << "Error allocating memory for file context" << STRMLF; return 0; }
    ctx->logstyle = IsConsoleRedirected() ? SASS_LOGGER_ASCII_MONO : SASS_LOGGER_UNICODE_COLOR;
    ctx->type = SASS_CONTEXT_FILE;
    init_options(ctx);
    try {
      if (input_path == 0) { throw(std::runtime_error("File context created without an input path")); }
      if (*input_path == 0) { throw(std::runtime_error("File context created with empty input path")); }
      sass_option_set_input_path(ctx, input_path);
    } catch (...) {
      handle_errors(ctx);
    }
    return ctx;
  }

  SassDataContextCpp* ADDCALL sass_make_data_context(char* source_string)
  {
    #ifdef DEBUG_SHARED_PTR
    SharedObj::setTaint(true);
    #endif
    struct SassDataContextCpp* ctx = new SassDataContextCpp{};
    if (ctx == 0) { std::cerr << "Error allocating memory for data context" << STRMLF; return 0; }
    ctx->logstyle = IsConsoleRedirected() ? SASS_LOGGER_ASCII_MONO : SASS_LOGGER_UNICODE_COLOR;
    ctx->type = SASS_CONTEXT_DATA;
    init_options(ctx);
    try {
      if (source_string == 0) { throw(std::runtime_error("Data context created without a source string")); }
      if (*source_string == 0) { throw(std::runtime_error("Data context created with empty source string")); }
      ctx->source_string = source_string;
    } catch (...) {
      handle_errors(ctx);
    }
    return ctx;
  }

  void ADDCALL sass_context_print_stderr(struct SassContextCpp* ctx)
  {
    const char* message = sass_context_get_stderr_string(ctx);
    if (message != nullptr) Terminal::print(message, true);
  }

  void ADDCALL sass_print_stderr(const char* message)
  {
    Terminal::print(message, true);
  }

  struct SassCompilerCpp* ADDCALL sass_make_data_compiler (struct SassDataContextCpp* data_ctx)
  {
    if (data_ctx == 0) return 0;
    Context* cpp_ctx = new Data_Context(*data_ctx);
    return sass_prepare_context(cpp_ctx);
  }

  struct SassCompilerCpp* ADDCALL sass_make_file_compiler (struct SassFileContextCpp* file_ctx)
  {
    if (file_ctx == 0) return 0;
    Context* cpp_ctx = new File_Context(*file_ctx);
    return sass_prepare_context(cpp_ctx);
  }

  int ADDCALL sass_compile_data_context(SassDataContextCpp* data_ctx)
  {
    if (data_ctx == 0) return 1;
    if (data_ctx->error_status)
      return data_ctx->error_status;
    try {
      // if (data_ctx->source_string.empty()) { throw(std::runtime_error("Data context has empty source string")); }
      // empty source string is a valid case, even if not really useful (different than with file context)
      // if (*data_ctx->source_string == 0) { throw(std::runtime_error("Data context has empty source string")); }
    }
    catch (...) { return handle_errors(data_ctx) | 1; }
    Context* cpp_ctx = new Data_Context(*data_ctx);
    return sass_compile_context(cpp_ctx);
  }

  int ADDCALL sass_compile_file_context(SassFileContextCpp* file_ctx)
  {
    if (file_ctx == 0) return 1;
    if (file_ctx->error_status)
      return file_ctx->error_status;
    try {
      if (file_ctx->input_path.empty()) { throw(std::runtime_error("File context has no input path")); }
    }
    catch (...) { return handle_errors(file_ctx) | 1; }
    Context* cpp_ctx = new File_Context(*file_ctx);
    return sass_compile_context(cpp_ctx);
  }

  int ADDCALL sass_compiler_parse(struct SassCompilerCpp* compiler)
  {
    if (compiler == 0) return 1;
    if (compiler->state == SASS_COMPILER_PARSED) return 0;
    if (compiler->state != SASS_COMPILER_CREATED) return -1;
    if (compiler->c_ctx == NULL) return 1;
    if (compiler->cpp_ctx == NULL) return 1;
    if (compiler->c_ctx->error_status)
      return compiler->c_ctx->error_status;
    // parse the context we have set up (file or data)
    compiler->root = sass_parse_block(compiler);
    // success
    return 0;
  }

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

  // helper function, not exported, only accessible locally
  static void sass_reset_options (struct SassOptionsCpp* options)
  {
    // free pointer before
    // or copy/move them
    options->c_importers = 0;
    options->c_headers = 0;
  }

  // helper function, not exported, only accessible locally
  static void sass_clear_options (struct SassOptionsCpp* options)
  {
    if (options == 0) return;
    // Deallocate custom functions, headers and imports
    sass_delete_importer_list(options->c_importers);
    sass_delete_importer_list(options->c_headers);
    // Reset our pointers
    options->c_importers = 0;
    options->c_headers = 0;
  }

  // helper function, not exported, only accessible locally
  // sass_free_context is also defined in old sass_interface
  static void sass_clear_context (struct SassContextCpp* ctx)
  {
    if (ctx == 0) return;
    // debug leaked memory
    #ifdef DEBUG_SHARED_PTR
      SharedObj::dumpMemLeaks();
      SharedObj::reportRefCounts();
    #endif
    // now clear the options
    sass_clear_options(ctx);
  }

  void ADDCALL sass_delete_compiler (struct SassCompilerCpp* compiler)
  {
    if (compiler == 0) {
      return;
    }
    Context* cpp_ctx = compiler->cpp_ctx;
    if (cpp_ctx) delete(cpp_ctx);
    compiler->cpp_ctx = NULL;
    compiler->c_ctx = NULL;
    compiler->root = {};
    delete compiler;
  }

  // Deallocate all associated memory with options
  void ADDCALL sass_delete_options (struct SassOptionsCpp* options)
  {
    sass_clear_options(options); delete options;
  }

  // Deallocate all associated memory with file context
  void ADDCALL sass_delete_file_context (struct SassFileContextCpp* ctx)
  {
    // clear the context and free it
    sass_clear_context(ctx); delete ctx;
  }
  // Deallocate all associated memory with data context
  void ADDCALL sass_delete_data_context (struct SassDataContextCpp* ctx)
  {
    // clear the context and free it
    sass_clear_context(ctx); delete ctx;
  }

  // Getters for sass context from specific implementations
  struct SassContextCpp* ADDCALL sass_file_context_get_context(struct SassFileContextCpp* ctx) { return ctx; }
  struct SassContextCpp* ADDCALL sass_data_context_get_context(struct SassDataContextCpp* ctx) { return ctx; }

  // Getters for context options from SassContextCpp
  struct SassOptionsCpp* ADDCALL sass_context_get_options(struct SassContextCpp* ctx) { return ctx; }
  struct SassOptionsCpp* ADDCALL sass_file_context_get_options(struct SassFileContextCpp* ctx) { return ctx; }
  struct SassOptionsCpp* ADDCALL sass_data_context_get_options(struct SassDataContextCpp* ctx) { return ctx; }
  void ADDCALL sass_file_context_set_options (struct SassFileContextCpp* ctx, struct SassOptionsCpp* opt) { copy_options(ctx, opt); }
  void ADDCALL sass_data_context_set_options (struct SassDataContextCpp* ctx, struct SassOptionsCpp* opt) { copy_options(ctx, opt); }

  // Getters for SassCompilerCpp options (get connected sass context)
  enum Sass_Compiler_State ADDCALL sass_compiler_get_state(struct SassCompilerCpp* compiler) { return compiler->state; }
  struct SassContextCpp* ADDCALL sass_compiler_get_context(struct SassCompilerCpp* compiler) { return compiler->c_ctx; }
  struct SassOptionsCpp* ADDCALL sass_compiler_get_options(struct SassCompilerCpp* compiler) { return compiler->c_ctx; }
  // Getters for SassCompilerCpp options (query import stack)
  size_t ADDCALL sass_compiler_get_import_stack_size(struct SassCompilerCpp* compiler) { return compiler->cpp_ctx->import_stack.size(); }
  SassImportPtr ADDCALL sass_compiler_get_last_import(struct SassCompilerCpp* compiler) { return compiler->cpp_ctx->import_stack.back(); }
  SassImportPtr ADDCALL sass_compiler_get_import_entry(struct SassCompilerCpp* compiler, size_t idx) { return compiler->cpp_ctx->import_stack[idx]; }
  // Getters for SassCompilerCpp options (query function stack)
  size_t ADDCALL sass_compiler_get_callee_stack_size(struct SassCompilerCpp* compiler) { return compiler->cpp_ctx->callee_stack.size(); }
  SassCalleePtr ADDCALL sass_compiler_get_last_callee(struct SassCompilerCpp* compiler) { return &compiler->cpp_ctx->callee_stack.back(); }
  SassCalleePtr ADDCALL sass_compiler_get_callee_entry(struct SassCompilerCpp* compiler, size_t idx) { return &compiler->cpp_ctx->callee_stack[idx]; }

  // Calculate the size of the stored null terminated array
  size_t ADDCALL sass_context_get_included_files(struct SassContextCpp* ctx) { return ctx->included_files.size(); }
  const char* ADDCALL sass_context_get_included_file(struct SassContextCpp* ctx, size_t i) { return ctx->included_files.at(i).c_str(); }

  // Create getter and setters for options
  IMPLEMENT_SASS_OPTION_ACCESSOR(int, precision);
  IMPLEMENT_SASS_OPTION_ACCESSOR(enum Sass_Output_Style, output_style);
  IMPLEMENT_SASS_OPTION_ACCESSOR(bool, source_comments);
  IMPLEMENT_SASS_OPTION_ACCESSOR(bool, source_map_embed);
  IMPLEMENT_SASS_OPTION_ACCESSOR(bool, source_map_contents);
  IMPLEMENT_SASS_OPTION_ACCESSOR(bool, source_map_file_urls);
  IMPLEMENT_SASS_OPTION_ACCESSOR(bool, omit_source_map_url);
//   IMPLEMENT_SASS_OPTION_ACCESSOR(SassFunctionListPtr, c_functions);
  IMPLEMENT_SASS_OPTION_ACCESSOR(SassImporterListPtr, c_importers);
  IMPLEMENT_SASS_OPTION_ACCESSOR(SassImporterListPtr, c_headers);
  IMPLEMENT_SASS_OPTION_ACCESSOR(const char*, indent);
  IMPLEMENT_SASS_OPTION_ACCESSOR(const char*, linefeed);
  IMPLEMENT_SASS_OPTION_STRING2_SETTER(plugin_path);
  IMPLEMENT_SASS_OPTION_STRING2_SETTER(include_path);
  IMPLEMENT_SASS_OPTION_STRING2_ACCESSOR(input_path);
  IMPLEMENT_SASS_OPTION_STRING2_ACCESSOR(output_path);
  IMPLEMENT_SASS_OPTION_STRING2_ACCESSOR(source_map_file);
  IMPLEMENT_SASS_OPTION_STRING2_ACCESSOR(source_map_root);

  // Create getter and setters for context
  IMPLEMENT_SASS_CONTEXT_GETTER(int, error_status);
  IMPLEMENT_SASS_CONTEXT_STRING2_GETTER(error_json);
  IMPLEMENT_SASS_CONTEXT_STRING2_GETTER(error_message);
  IMPLEMENT_SASS_CONTEXT_STRING2_GETTER(error_text);
  IMPLEMENT_SASS_CONTEXT_STRING2_GETTER(error_file);
  IMPLEMENT_SASS_CONTEXT_STRING2_GETTER(error_src);
  IMPLEMENT_SASS_CONTEXT_GETTER(size_t, error_line);
  IMPLEMENT_SASS_CONTEXT_GETTER(size_t, error_column);
  IMPLEMENT_SASS_CONTEXT_STRING2_GETTER(output_string);
  IMPLEMENT_SASS_CONTEXT_STRING2_GETTER(stderr_string);
  IMPLEMENT_SASS_CONTEXT_STRING2_GETTER(source_map_string);

  // Push function for include paths (no manipulation support for now)
  void ADDCALL sass_option_push_include_path(struct SassOptionsCpp* options, const char* path)
  {
    options->include_paths.push_back(path);
  }

  // Push function for include paths (no manipulation support for now)
  size_t ADDCALL sass_option_get_include_path_size(struct SassOptionsCpp* options)
  {
    return options->include_paths.size();
  }

  // Push function for include paths (no manipulation support for now)
  const char* ADDCALL sass_option_get_include_path(struct SassOptionsCpp* options, size_t i)
  {
    return options->include_paths.at(i).c_str();
  }

  // Push function for plugin paths (no manipulation support for now)
  void ADDCALL sass_option_push_plugin_path(struct SassOptionsCpp* options, const char* path)
  {
    options->plugin_paths.push_back(path);
  }

}
