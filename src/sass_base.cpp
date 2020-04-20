// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include <sass/base.h>
#include "terminal.hpp"
#include "file.hpp"

#ifdef __cplusplus
namespace Sass {
extern "C" {
#endif

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

#ifdef __cplusplus
} // __cplusplus defined.
} // EO namespace Sass
#endif
