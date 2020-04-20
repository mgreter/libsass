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
#include <iomanip>

using namespace Sass;


extern "C" {



  void ADDCALL sass_import_set_format(struct SassImport* import, enum SassImportFormat format)
  {
    import->format = format;
  }

  int ADDCALL sass_error_get_status(struct SassError* error) { return error->status; }
  char* ADDCALL sass_error_get_json(struct SassError* error) { return error->getJson(true); }
  const char* ADDCALL sass_error_get_what(struct SassError* error) { return error->what.c_str(); }
  const char* ADDCALL sass_error_get_messages(struct SassError* error) { return error->messages.c_str(); }
  const char* ADDCALL sass_error_get_warnings(struct SassError* error) { return error->warnings.c_str(); }
  const char* ADDCALL sass_error_get_formatted(struct SassError* error) { return error->formatted.c_str(); }


  // int ADDCALL sass_compiler_get_error_status(struct SassCompiler* compiler) { return reinterpret_cast<Compiler*>(compiler)->error.status; }
  // char* ADDCALL sass_compiler_get_error_json(struct SassCompiler* compiler) { return reinterpret_cast<Compiler*>(compiler)->error.getJson(true); }
  // const char* ADDCALL sass_compiler_get_error_what(struct SassCompiler* compiler) { return reinterpret_cast<Compiler*>(compiler)->error.what.c_str(); }
  // const char* ADDCALL sass_compiler_get_error_message(struct SassCompiler* compiler) { return reinterpret_cast<Compiler*>(compiler)->error.message.c_str(); }
  // const char* ADDCALL sass_compiler_get_warning_message(struct SassCompiler* compiler) { return reinterpret_cast<Compiler*>(compiler)->error.warnings.c_str(); }
  
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



  struct SassImport* ADDCALL sass_make_file_import(const char* input_path88)
  {
    // check if entry file is given
    if (input_path88 == nullptr) return {};

    // create absolute path from input filename
    // ToDo: this should be resolved via custom importers

    sass::string abs_path(File::rel2abs(input_path88, CWD, CWD));

    // try to load the entry file
    char* contents = File::slurp_file(abs_path, CWD);
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
      input_path88,
      abs_path.c_str(),
      contents, 0,
      SASS_IMPORT_AUTO
    );

  }

  struct SassImport* ADDCALL sass_make_stdin_import()
  {
    std::istreambuf_iterator<char> begin(std::cin), end;
    sass::string text(begin, end);
    return sass_make_import(
      "stdin", "stdin",
      sass_copy_string(text), 0,
      SASS_IMPORT_AUTO
    );
  }


}
