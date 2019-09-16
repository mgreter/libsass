// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"
#include "ast.hpp"

#include "sass_functions.hpp"
#include "sass_context.hpp"
#include "backtrace.hpp"
#include "source.hpp"
#include "json.hpp"
#include <iomanip>

#define LFEED "\n"

// C++ helper
namespace Sass {

  // see sass_copy_c_string(sass::string str)
  static inline JsonNode* json_mkstream(const sass::sstream& stream)
  {
    // hold on to string on stack!
    sass::string str(stream.str());
    return json_mkstring(str.c_str());
  }

  static void handle_string_error(Sass_Context* c_ctx, const sass::string& msg, int severety)
  {
    sass::sstream msg_stream;
    JsonNode* json_err = json_mkobject();
    msg_stream << "Error: " << msg << std::endl;
    json_append_member(json_err, "status", json_mknumber(severety));
    json_append_member(json_err, "message", json_mkstring(msg.c_str()));
    json_append_member(json_err, "formatted", json_mkstream(msg_stream));
    try { c_ctx->error_json = json_stringify(json_err, "  "); }
    catch (...) {}
    c_ctx->error_message = sass_copy_string(msg_stream.str());
    c_ctx->error_text = sass_copy_c_string(msg.c_str());
    c_ctx->error_status = severety;
    c_ctx->output_string = 0;
    c_ctx->source_map_string = 0;
    json_delete(json_err);
  }

  static void format_pstate(sass::sstream& msg_stream, SourceSpan pstate)
  {
    // now create the code trace (ToDo: maybe have util functions?)
    if (pstate.position.line != sass::string::npos &&
      pstate.position.column != sass::string::npos &&
      pstate.source->begin() != nullptr) {
      // size_t lines = pstate.position.line;

      if (false && pstate.source) {

        SourceData* source = pstate.source;
        sass::vector<sass::string> lines;

        // Fetch all lines we need to print the state
        for (size_t i = 0; i != std::string::npos; i++) {
          lines.emplace_back(source->getLine(pstate.position.line + i));
        }

        msg_stream << lines[0] << "\n";

      }
      else {


        // scan through src until target line
        // move line_beg pointer to line start

        // Implement source->getLine

        sass::string line = pstate.source->getLine(pstate.position.line);

        size_t line_len = line.size();
        size_t move_in = 0; size_t shorten = 0;
        size_t left_chars = 42; size_t max_chars = 76;

        if (pstate.position.column > line_len) left_chars = pstate.position.column;
        if (pstate.position.column > left_chars) move_in = pstate.position.column - left_chars;
        if (line_len > max_chars + move_in) shorten = line_len - move_in - max_chars;

        auto line_beg = line.begin();
        auto line_end = line.end();

        utf8::advance(line_beg, move_in, line_end);
        utf8::retreat(line_end, shorten, line_beg);

        sass::string sanitized; sass::string marker(pstate.position.column - move_in, ' ');
        utf8::replace_invalid(line_beg, line_end, std::back_inserter(sanitized));

        Offset beg = pstate.position;
        Offset end = beg + pstate.length;

        sass::string filler;
        size_t len = pstate.position.column - move_in;
        for (size_t i = 0; i < len; i += 1) {
          if (sanitized[i] == '\t') {
            filler += "    ";
          }
          else {
            filler += " ";
          }
        }
        for (size_t i = sanitized.length(); i != std::string::npos; i -= 1) {
          if (sanitized[i] == '\t') {
            sanitized.replace(i, 1, "    ");
          }
        }

        /*
        const char* line_beg;
        for (line_beg = pstate.source->begin(); *line_beg != '\0'; ++line_beg) {
          if (lines == 0) break;
          if (*line_beg == '\n') --lines;
        }
        // move line_end before next newline character
        const char* line_end;
        for (line_end = line_beg; *line_end != '\0'; ++line_end) {
          if (*line_end == '\n' || *line_end == '\r') break;
        }
        if (*line_end != '\0') ++line_end;
        size_t line_len = line_end - line_beg;
        size_t move_in = 0; size_t shorten = 0;
        size_t left_chars = 42; size_t max_chars = 76;
        // reported excerpt should not exceed `max_chars` chars
        if (pstate.position.column > line_len) left_chars = pstate.position.column;
        if (pstate.position.column > left_chars) move_in = pstate.position.column - left_chars;
        if (line_len > max_chars + move_in) shorten = line_len - move_in - max_chars;
        utf8::advance(line_beg, move_in, line_end);
        utf8::retreat(line_end, shorten, line_beg);
        sass::string sanitized; sass::string marker(pstate.position.column - move_in, ' ');
        utf8::replace_invalid(line_beg, line_end, std::back_inserter(sanitized));

        // Make sure we dont print any newliness
        size_t found = sanitized.find_first_of("\r\n");
        if (found != sass::string::npos) {
          sanitized = sanitized.substr(0, found);
        }

        Offset beg = pstate.position;
        Offset end = beg + pstate.length;

        */

        size_t highlighter = 1;
        if (beg.line == end.line) {
          highlighter = end.column - beg.column;
          if (highlighter <= 0) highlighter = 1;
        }

        // Print them as utf8 encoded sequences
        sass::string /* `╷` */ upper("\xE2\x95\xB7");
        sass::string /* `│` */ middle("\xE2\x94\x82");
        sass::string /* `╵` */ lower("\xE2\x95\xB5");

        // Use ascii versions
        if (true) {
          upper = ",";
          middle = "|";
          lower = "'";
        }
        // sass::string upper("┬");  sass::string middle("│"); sass::string lower("┘");

        // Dart-sass claims this might fail due to float errors
        size_t leftPadding = (size_t)floor(log10(end.line + 1)) + 3;

        // Write the leading line
        msg_stream << std::right << std::setfill(' ')
          << std::setw(leftPadding) << upper << "\n";

        // Write the line number and the code
        msg_stream << (end.line + 1) << " " << middle;
        msg_stream << " " << (move_in > 0 ? "... " : "")
          << sanitized << (shorten > 0 ? " ..." : "") << "\n";

        // Write the prefix for the highlighter
        msg_stream << std::right << std::setfill(' ')
          << std::setw(leftPadding) << middle;

        // Append the repeated hightligher
        msg_stream << " " << (move_in > 0 ? "    " : "")
          // << sass::string(pstate.position.column - move_in, ' ')
          << filler
          << sass::string(highlighter, '^') << "\n";

        // Write the trailing line
        msg_stream << std::right << std::setfill(' ')
          << std::setw(leftPadding) << lower << "\n";

      }
    }

  }

  sass::string traces_to_string(Backtraces traces, sass::string indent, size_t showTraces) {

    sass::sstream ss;
    sass::sstream strm;
    sass::string cwd(File::get_cwd());

    // bool first = true;
    size_t max = 0;
    size_t i_beg = traces.size() - 1;
    size_t i_end = sass::string::npos;

    std::vector<std::pair<sass::string, sass::string>> traced;
    for (size_t i = i_beg; i != i_end; i--) {

      // Skip entry if parent is a call invocation
      // if (i > 0 && traces[i - 1].caller == "call") continue;

      const Backtrace& trace = traces[i];


      // make path relative to the current directory
      sass::string rel_path(File::abs2rel(trace.pstate.getPath(), cwd, cwd));

      strm.str(sass::string());
      strm << rel_path << " ";
      strm << trace.pstate.getLine();
      strm << ":" << trace.pstate.getColumn();

      sass::string str(strm.str());
      max = std::max(max, str.length());

      if (i == 0) {
        traced.emplace_back(std::make_pair(
          str, "root stylesheet"));
      }
      else if (!traces[i - 1].name.empty()) {
        sass::string name(traces[i - 1].name);
        if (traces[i - 1].fn) name += "()";
        traced.emplace_back(std::make_pair(str, name));
      }
      else {
        traced.emplace_back(std::make_pair(
          str, "#{}"));
      }

    }

    i_beg = traces.size() - 1;
    i_end = sass::string::npos;
    for (size_t i = i_beg, n = 0; i != i_end; i--, n++) {

      // Skip entry if parent is a call invocation
      // if (i > 0 && traces[i - 1].caller == "call") continue;

      const Backtrace& trace = traces[i];

      // make path relative to the current directory
      sass::string rel_path(File::abs2rel(trace.pstate.getPath(), cwd, cwd));

      // skip functions on error cases (unsure why ruby sass does this)
      // if (trace.caller.substr(0, 6) == ", in f") continue;

      if (showTraces == sass::string::npos || showTraces > 0) {
        format_pstate(ss, trace.pstate);
        if (showTraces > 0) --showTraces;
      }

      ss << indent;
      ss << std::left << std::setfill(' ')
        << std::setw(max + 2) << traced[n].first;
      ss << traced[n].second;
      ss << std::endl << std::endl;

    }

    return ss.str();

  }




  static int handle_error(Sass_Context* c_ctx) {
    try {
      throw;
    }
    catch (Exception::Base& e) {

      sass::sstream msg_stream;
      sass::string cwd(Sass::File::get_cwd());
      sass::string msg_prefix(e.errtype());
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

      if (e.traces.empty()) {
        Backtraces traces = e.traces;
        traces.emplace_back(e.pstate);
        sass::string rel_path(Sass::File::abs2rel(e.pstate.getPath(), cwd, cwd));
        msg_stream << traces_to_string(traces, "  ");
      }
      else {
        sass::string rel_path(Sass::File::abs2rel(e.pstate.getPath(), cwd, cwd));
        msg_stream << traces_to_string(e.traces, "  ");
      }

      JsonNode* json_err = json_mkobject();
      json_append_member(json_err, "status", json_mknumber(1));
      json_append_member(json_err, "file", json_mkstring(e.pstate.getPath()));
      json_append_member(json_err, "line", json_mknumber((double)(e.pstate.getLine())));
      json_append_member(json_err, "column", json_mknumber((double)(e.pstate.getColumn())));
      json_append_member(json_err, "message", json_mkstring(e.what()));
      json_append_member(json_err, "formatted", json_mkstream(msg_stream));
      try { c_ctx->error_json = json_stringify(json_err, "  "); }
      catch (...) {} // silently ignore this error?
      c_ctx->error_message = sass_copy_string(msg_stream.str());
      c_ctx->error_text = sass_copy_c_string(e.what());
      c_ctx->error_status = 1;
      c_ctx->error_file = sass_copy_c_string(e.pstate.getPath());
      c_ctx->error_line = e.pstate.getLine();
      c_ctx->error_column = e.pstate.getColumn();
      c_ctx->error_src = e.pstate.getRawData();
      c_ctx->output_string = 0;
      c_ctx->source_map_string = 0;
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
  static int handle_errors(Sass_Context* c_ctx) {
    try { return handle_error(c_ctx); }
    catch (...) { return handle_error(c_ctx); }
  }

  static Block_Obj sass_parse_block(Sass_Compiler* compiler) throw()
  {

    // assert valid pointer
    if (compiler == 0) return {};
    // The cpp context must be set by now
    Context* cpp_ctx = compiler->cpp_ctx;
    Sass_Context* c_ctx = compiler->c_ctx;
    // We will take care to wire up the rest
    compiler->cpp_ctx->c_compiler = compiler;
    compiler->state = SASS_COMPILER_PARSED;

    try {

      // get input/output path from options
      sass::string input_path = safe_str(c_ctx->input_path);
      sass::string output_path = safe_str(c_ctx->output_path);

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

      // copy the included files on to the context (dont forget to free later)
      if (copy_strings(cpp_ctx->get_included_files(skip, headers), &c_ctx->included_files) == NULL)
        throw(std::bad_alloc());

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

  static void sass_clear_options (struct Sass_Options* options);
  static void sass_reset_options (struct Sass_Options* options);
  static void copy_options(struct Sass_Options* to, struct Sass_Options* from) {
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
    type ADDCALL sass_option_get_##option (struct Sass_Options* options) { return options->option; } \
    void ADDCALL sass_option_set_##option (struct Sass_Options* options, type option) { options->option = option; }
  #define IMPLEMENT_SASS_OPTION_STRING_GETTER(type, option, def) \
    type ADDCALL sass_option_get_##option (struct Sass_Options* options) { return safe_str(options->option, def); }
  #define IMPLEMENT_SASS_OPTION_STRING_SETTER(type, option, def) \
    void ADDCALL sass_option_set_##option (struct Sass_Options* options, type option) \
    { free(options->option); options->option = option || def ? sass_copy_c_string(option ? option : def) : 0; }
  #define IMPLEMENT_SASS_OPTION_STRING_ACCESSOR(type, option, def) \
    IMPLEMENT_SASS_OPTION_STRING_GETTER(type, option, def) \
    IMPLEMENT_SASS_OPTION_STRING_SETTER(type, option, def)

  #define IMPLEMENT_SASS_CONTEXT_GETTER(type, option) \
    type ADDCALL sass_context_get_##option (struct Sass_Context* ctx) { return ctx->option; }
  #define IMPLEMENT_SASS_CONTEXT_TAKER(type, option) \
    type sass_context_take_##option (struct Sass_Context* ctx) \
    { type foo = ctx->option; ctx->option = 0; return foo; }


  // generic compilation function (not exported, use file/data compile instead)
  static Sass_Compiler* sass_prepare_context (Sass_Context* c_ctx, Context* cpp_ctx) throw()
  {
    try {
      // register our custom functions
      if (c_ctx->c_functions) {
        auto this_func_data = c_ctx->c_functions;
        while (this_func_data && *this_func_data) {
          cpp_ctx->add_c_function(*this_func_data);
          ++this_func_data;
        }
      }

      // register our custom headers
      if (c_ctx->c_headers) {
        auto this_head_data = c_ctx->c_headers;
        while (this_head_data && *this_head_data) {
          cpp_ctx->add_c_header(*this_head_data);
          ++this_head_data;
        }
      }

      // register our custom importers
      if (c_ctx->c_importers) {
        auto this_imp_data = c_ctx->c_importers;
        while (this_imp_data && *this_imp_data) {
          cpp_ctx->add_c_importer(*this_imp_data);
          ++this_imp_data;
        }
      }

      // reset error status
      c_ctx->error_json = 0;
      c_ctx->error_text = 0;
      c_ctx->error_message = 0;
      c_ctx->error_status = 0;
      // reset error position
      c_ctx->error_src = 0;
      c_ctx->error_file = 0;
      c_ctx->error_line = sass::string::npos;
      c_ctx->error_column = sass::string::npos;

      // allocate a new compiler instance
      void* ctxmem = calloc(1, sizeof(struct Sass_Compiler));
      if (ctxmem == 0) { std::cerr << "Error allocating memory for context" << std::endl; return 0; }
      Sass_Compiler* compiler = (struct Sass_Compiler*) ctxmem;
      compiler->state = SASS_COMPILER_CREATED;

      // store in sass compiler
      compiler->c_ctx = c_ctx;
      compiler->cpp_ctx = cpp_ctx;
      cpp_ctx->c_compiler = compiler;

      // use to parse block
      return compiler;

    }
    // pass errors to generic error handler
    catch (...) { handle_errors(c_ctx); }

    // error
    return 0;

  }

  // generic compilation function (not exported, use file/data compile instead)
  static int sass_compile_context (Sass_Context* c_ctx, Context* cpp_ctx)
  {

    // prepare sass compiler with context and options
    Sass_Compiler* compiler = sass_prepare_context(c_ctx, cpp_ctx);

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

  inline void init_options (struct Sass_Options* options)
  {
    options->precision = 10;
    options->indent = "  ";
    options->linefeed = LFEED;
  }

  Sass_Options* ADDCALL sass_make_options (void)
  {
    struct Sass_Options* options = (struct Sass_Options*) calloc(1, sizeof(struct Sass_Options));
    if (options == 0) { std::cerr << "Error allocating memory for options" << std::endl; return 0; }
    init_options(options);
    return options;
  }

  Sass_File_Context* ADDCALL sass_make_file_context(const char* input_path)
  {
    #ifdef DEBUG_SHARED_PTR
    SharedObj::setTaint(true);
    #endif
    struct Sass_File_Context* ctx = (struct Sass_File_Context*) calloc(1, sizeof(struct Sass_File_Context));
    if (ctx == 0) { std::cerr << "Error allocating memory for file context" << std::endl; return 0; }
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

  Sass_Data_Context* ADDCALL sass_make_data_context(char* source_string)
  {
    #ifdef DEBUG_SHARED_PTR
    SharedObj::setTaint(true);
    #endif
    struct Sass_Data_Context* ctx = (struct Sass_Data_Context*) calloc(1, sizeof(struct Sass_Data_Context));
    if (ctx == 0) { std::cerr << "Error allocating memory for data context" << std::endl; return 0; }
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

  struct Sass_Compiler* ADDCALL sass_make_data_compiler (struct Sass_Data_Context* data_ctx)
  {
    if (data_ctx == 0) return 0;
    Context* cpp_ctx = new Data_Context(*data_ctx);
    return sass_prepare_context(data_ctx, cpp_ctx);
  }

  struct Sass_Compiler* ADDCALL sass_make_file_compiler (struct Sass_File_Context* file_ctx)
  {
    if (file_ctx == 0) return 0;
    Context* cpp_ctx = new File_Context(*file_ctx);
    return sass_prepare_context(file_ctx, cpp_ctx);
  }

  int ADDCALL sass_compile_data_context(Sass_Data_Context* data_ctx)
  {
    if (data_ctx == 0) return 1;
    if (data_ctx->error_status)
      return data_ctx->error_status;
    try {
      if (data_ctx->source_string == 0) { throw(std::runtime_error("Data context has no source string")); }
      // empty source string is a valid case, even if not really usefull (different than with file context)
      // if (*data_ctx->source_string == 0) { throw(std::runtime_error("Data context has empty source string")); }
    }
    catch (...) { return handle_errors(data_ctx) | 1; }
    Context* cpp_ctx = new Data_Context(*data_ctx);
    return sass_compile_context(data_ctx, cpp_ctx);
  }

  int ADDCALL sass_compile_file_context(Sass_File_Context* file_ctx)
  {
    if (file_ctx == 0) return 1;
    if (file_ctx->error_status)
      return file_ctx->error_status;
    try {
      if (file_ctx->input_path == 0) { throw(std::runtime_error("File context has no input path")); }
      if (*file_ctx->input_path == 0) { throw(std::runtime_error("File context has empty input path")); }
    }
    catch (...) { return handle_errors(file_ctx) | 1; }
    Context* cpp_ctx = new File_Context(*file_ctx);
    return sass_compile_context(file_ctx, cpp_ctx);
  }

  int ADDCALL sass_compiler_parse(struct Sass_Compiler* compiler)
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

  int ADDCALL sass_compiler_execute(struct Sass_Compiler* compiler)
  {
    if (compiler == 0) return 1;
    if (compiler->state == SASS_COMPILER_EXECUTED) return 0;
    if (compiler->state != SASS_COMPILER_PARSED) return -1;
    if (compiler->c_ctx == NULL) return 1;
    if (compiler->cpp_ctx == NULL) return 1;
    if (compiler->root.isNull()) return 1;
    if (compiler->c_ctx->error_status)
      return compiler->c_ctx->error_status;
    compiler->state = SASS_COMPILER_EXECUTED;
    Context* cpp_ctx = compiler->cpp_ctx;
    Block_Obj root = compiler->root;
    // compile the parsed root block
    try { compiler->c_ctx->output_string = cpp_ctx->render(root); }
    // pass catched errors to generic error handler
    catch (...) { return handle_errors(compiler->c_ctx) | 1; }
    // generate source map json and store on context
    compiler->c_ctx->source_map_string = cpp_ctx->render_srcmap();
    // success
    return 0;
  }

  // helper function, not exported, only accessible locally
  static void sass_reset_options (struct Sass_Options* options)
  {
    // free pointer before
    // or copy/move them
    options->input_path = 0;
    options->output_path = 0;
    options->plugin_path = 0;
    options->include_path = 0;
    options->source_map_file = 0;
    options->source_map_root = 0;
    options->c_functions = 0;
    options->c_importers = 0;
    options->c_headers = 0;
    options->plugin_paths = 0;
    options->include_paths = 0;
  }

  // helper function, not exported, only accessible locally
  static void sass_clear_options (struct Sass_Options* options)
  {
    if (options == 0) return;
    // Deallocate custom functions, headers and importes
    sass_delete_function_list(options->c_functions);
    sass_delete_importer_list(options->c_importers);
    sass_delete_importer_list(options->c_headers);
    // Deallocate inc paths
    if (options->plugin_paths) {
      struct string_list* cur;
      struct string_list* next;
      cur = options->plugin_paths;
      while (cur) {
        next = cur->next;
        free(cur->string);
        free(cur);
        cur = next;
      }
    }
    // Deallocate inc paths
    if (options->include_paths) {
      struct string_list* cur;
      struct string_list* next;
      cur = options->include_paths;
      while (cur) {
        next = cur->next;
        free(cur->string);
        free(cur);
        cur = next;
      }
    }
    // Free options strings
    free(options->input_path);
    free(options->output_path);
    free(options->plugin_path);
    free(options->include_path);
    free(options->source_map_file);
    free(options->source_map_root);
    // Reset our pointers
    options->input_path = 0;
    options->output_path = 0;
    options->plugin_path = 0;
    options->include_path = 0;
    options->source_map_file = 0;
    options->source_map_root = 0;
    options->c_functions = 0;
    options->c_importers = 0;
    options->c_headers = 0;
    options->plugin_paths = 0;
    options->include_paths = 0;
  }

  // helper function, not exported, only accessible locally
  // sass_free_context is also defined in old sass_interface
  static void sass_clear_context (struct Sass_Context* ctx)
  {
    if (ctx == 0) return;
    // release the allocated memory (mostly via sass_copy_c_string)
    if (ctx->output_string)     free(ctx->output_string);
    if (ctx->source_map_string) free(ctx->source_map_string);
    if (ctx->error_message)     free(ctx->error_message);
    if (ctx->error_text)        free(ctx->error_text);
    if (ctx->error_json)        free(ctx->error_json);
    if (ctx->error_file)        free(ctx->error_file);
    free_string_array(ctx->included_files);
    // play safe and reset properties
    ctx->output_string = 0;
    ctx->source_map_string = 0;
    ctx->error_message = 0;
    ctx->error_text = 0;
    ctx->error_json = 0;
    ctx->error_file = 0;
    ctx->included_files = 0;
    // debug leaked memory
    #ifdef DEBUG_SHARED_PTR
      SharedObj::dumpMemLeaks();
      SharedObj::reportRefCounts();
    #endif
    // now clear the options
    sass_clear_options(ctx);
  }

  void ADDCALL sass_delete_compiler (struct Sass_Compiler* compiler)
  {
    if (compiler == 0) {
      return;
    }
    Context* cpp_ctx = compiler->cpp_ctx;
    if (cpp_ctx) delete(cpp_ctx);
    compiler->cpp_ctx = NULL;
    compiler->c_ctx = NULL;
    compiler->root = {};
    free(compiler);
  }

  void ADDCALL sass_delete_options (struct Sass_Options* options)
  {
    sass_clear_options(options); free(options);
  }

  // Deallocate all associated memory with file context
  void ADDCALL sass_delete_file_context (struct Sass_File_Context* ctx)
  {
    // clear the context and free it
    sass_clear_context(ctx); free(ctx);
  }
  // Deallocate all associated memory with data context
  void ADDCALL sass_delete_data_context (struct Sass_Data_Context* ctx)
  {
    // clean the source string if it was not passed
    // we reset this member once we start parsing
    if (ctx->source_string) free(ctx->source_string);
    // clear the context and free it
    sass_clear_context(ctx); free(ctx);
  }

  // Getters for sass context from specific implementations
  struct Sass_Context* ADDCALL sass_file_context_get_context(struct Sass_File_Context* ctx) { return ctx; }
  struct Sass_Context* ADDCALL sass_data_context_get_context(struct Sass_Data_Context* ctx) { return ctx; }

  // Getters for context options from Sass_Context
  struct Sass_Options* ADDCALL sass_context_get_options(struct Sass_Context* ctx) { return ctx; }
  struct Sass_Options* ADDCALL sass_file_context_get_options(struct Sass_File_Context* ctx) { return ctx; }
  struct Sass_Options* ADDCALL sass_data_context_get_options(struct Sass_Data_Context* ctx) { return ctx; }
  void ADDCALL sass_file_context_set_options (struct Sass_File_Context* ctx, struct Sass_Options* opt) { copy_options(ctx, opt); }
  void ADDCALL sass_data_context_set_options (struct Sass_Data_Context* ctx, struct Sass_Options* opt) { copy_options(ctx, opt); }

  // Getters for Sass_Compiler options (get conected sass context)
  enum Sass_Compiler_State ADDCALL sass_compiler_get_state(struct Sass_Compiler* compiler) { return compiler->state; }
  struct Sass_Context* ADDCALL sass_compiler_get_context(struct Sass_Compiler* compiler) { return compiler->c_ctx; }
  struct Sass_Options* ADDCALL sass_compiler_get_options(struct Sass_Compiler* compiler) { return compiler->c_ctx; }
  // Getters for Sass_Compiler options (query import stack)
  size_t ADDCALL sass_compiler_get_import_stack_size(struct Sass_Compiler* compiler) { return compiler->cpp_ctx->import_stack.size(); }
  Sass_Import_Entry ADDCALL sass_compiler_get_last_import(struct Sass_Compiler* compiler) { return compiler->cpp_ctx->import_stack.back(); }
  Sass_Import_Entry ADDCALL sass_compiler_get_import_entry(struct Sass_Compiler* compiler, size_t idx) { return compiler->cpp_ctx->import_stack[idx]; }
  // Getters for Sass_Compiler options (query function stack)
  size_t ADDCALL sass_compiler_get_callee_stack_size(struct Sass_Compiler* compiler) { return compiler->cpp_ctx->callee_stack.size(); }
  Sass_Callee_Entry ADDCALL sass_compiler_get_last_callee(struct Sass_Compiler* compiler) { return &compiler->cpp_ctx->callee_stack.back(); }
  Sass_Callee_Entry ADDCALL sass_compiler_get_callee_entry(struct Sass_Compiler* compiler, size_t idx) { return &compiler->cpp_ctx->callee_stack[idx]; }

  // Calculate the size of the stored null terminated array
  size_t ADDCALL sass_context_get_included_files_size (struct Sass_Context* ctx)
  { size_t l = 0; auto i = ctx->included_files; while (i && *i) { ++i; ++l; } return l; }

  // Create getter and setters for options
  IMPLEMENT_SASS_OPTION_ACCESSOR(int, precision);
  IMPLEMENT_SASS_OPTION_ACCESSOR(enum Sass_Output_Style, output_style);
  IMPLEMENT_SASS_OPTION_ACCESSOR(bool, source_comments);
  IMPLEMENT_SASS_OPTION_ACCESSOR(bool, source_map_embed);
  IMPLEMENT_SASS_OPTION_ACCESSOR(bool, source_map_contents);
  IMPLEMENT_SASS_OPTION_ACCESSOR(bool, source_map_file_urls);
  IMPLEMENT_SASS_OPTION_ACCESSOR(bool, omit_source_map_url);
  IMPLEMENT_SASS_OPTION_ACCESSOR(bool, is_indented_syntax_src);
  IMPLEMENT_SASS_OPTION_ACCESSOR(Sass_Function_List, c_functions);
  IMPLEMENT_SASS_OPTION_ACCESSOR(Sass_Importer_List, c_importers);
  IMPLEMENT_SASS_OPTION_ACCESSOR(Sass_Importer_List, c_headers);
  IMPLEMENT_SASS_OPTION_ACCESSOR(const char*, indent);
  IMPLEMENT_SASS_OPTION_ACCESSOR(const char*, linefeed);
  IMPLEMENT_SASS_OPTION_STRING_SETTER(const char*, plugin_path, 0);
  IMPLEMENT_SASS_OPTION_STRING_SETTER(const char*, include_path, 0);
  IMPLEMENT_SASS_OPTION_STRING_ACCESSOR(const char*, input_path, 0);
  IMPLEMENT_SASS_OPTION_STRING_ACCESSOR(const char*, output_path, 0);
  IMPLEMENT_SASS_OPTION_STRING_ACCESSOR(const char*, source_map_file, 0);
  IMPLEMENT_SASS_OPTION_STRING_ACCESSOR(const char*, source_map_root, 0);

  // Create getter and setters for context
  IMPLEMENT_SASS_CONTEXT_GETTER(int, error_status);
  IMPLEMENT_SASS_CONTEXT_GETTER(const char*, error_json);
  IMPLEMENT_SASS_CONTEXT_GETTER(const char*, error_message);
  IMPLEMENT_SASS_CONTEXT_GETTER(const char*, error_text);
  IMPLEMENT_SASS_CONTEXT_GETTER(const char*, error_file);
  IMPLEMENT_SASS_CONTEXT_GETTER(size_t, error_line);
  IMPLEMENT_SASS_CONTEXT_GETTER(size_t, error_column);
  IMPLEMENT_SASS_CONTEXT_GETTER(const char*, error_src);
  IMPLEMENT_SASS_CONTEXT_GETTER(const char*, output_string);
  IMPLEMENT_SASS_CONTEXT_GETTER(const char*, source_map_string);
  IMPLEMENT_SASS_CONTEXT_GETTER(char**, included_files);

  // Take ownership of memory (value on context is set to 0)
  IMPLEMENT_SASS_CONTEXT_TAKER(char*, error_json);
  IMPLEMENT_SASS_CONTEXT_TAKER(char*, error_message);
  IMPLEMENT_SASS_CONTEXT_TAKER(char*, error_text);
  IMPLEMENT_SASS_CONTEXT_TAKER(char*, error_file);
  IMPLEMENT_SASS_CONTEXT_TAKER(char*, output_string);
  IMPLEMENT_SASS_CONTEXT_TAKER(char*, source_map_string);
  IMPLEMENT_SASS_CONTEXT_TAKER(char**, included_files);

  // Push function for include paths (no manipulation support for now)
  void ADDCALL sass_option_push_include_path(struct Sass_Options* options, const char* path)
  {

    struct string_list* include_path = (struct string_list*) calloc(1, sizeof(struct string_list));
    if (include_path == 0) return;
    include_path->string = path ? sass_copy_c_string(path) : 0;
    struct string_list* last = options->include_paths;
    if (!options->include_paths) {
      options->include_paths = include_path;
    } else {
      while (last->next)
        last = last->next;
      last->next = include_path;
    }

  }

  // Push function for include paths (no manipulation support for now)
  size_t ADDCALL sass_option_get_include_path_size(struct Sass_Options* options)
  {
    size_t len = 0;
    struct string_list* cur = options->include_paths;
    while (cur) { len ++; cur = cur->next; }
    return len;
  }

  // Push function for include paths (no manipulation support for now)
  const char* ADDCALL sass_option_get_include_path(struct Sass_Options* options, size_t i)
  {
    struct string_list* cur = options->include_paths;
    while (i) { i--; cur = cur->next; }
    return cur->string;
  }

  // Push function for plugin paths (no manipulation support for now)
  void ADDCALL sass_option_push_plugin_path(struct Sass_Options* options, const char* path)
  {

    struct string_list* plugin_path = (struct string_list*) calloc(1, sizeof(struct string_list));
    if (plugin_path == 0) return;
    plugin_path->string = path ? sass_copy_c_string(path) : 0;
    struct string_list* last = options->plugin_paths;
    if (!options->plugin_paths) {
      options->plugin_paths = plugin_path;
    } else {
      while (last->next)
        last = last->next;
      last->next = plugin_path;
    }

  }

}
