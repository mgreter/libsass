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

#ifdef __cplusplus
namespace Sass {
extern "C" {
#endif

  void ADDCALL sass_import_set_format(struct SassImport* import, enum SassImportFormat format)
  {
    import->format = format;
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

#ifdef __cplusplus
} // __cplusplus defined.
} // EP namespace Sass
#endif
