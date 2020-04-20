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

  void ADDCALL sass_compiler_set_output_path(struct SassCompiler* compiler, const char* output_path)
  {
    reinterpret_cast<Compiler*>(compiler)->output_path = output_path ? output_path : "stream://stdout";
  }

  void ADDCALL sass_compiler_set_output_style(struct SassCompiler* compiler, enum SassOutputStyle output_style)
  {
    reinterpret_cast<Compiler*>(compiler)->output_style = output_style;
  }

  // Returns pointer to error object associated with compiler.
  // Will be valid until the associated compiler is destroyed.
  struct SassError* ADDCALL sass_compiler_get_error(struct SassCompiler* sass_compiler)
  {
    return &reinterpret_cast<Compiler*>(sass_compiler)->error;
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



}
