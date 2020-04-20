#ifndef SASS_C_CONTEXT_H
#define SASS_C_CONTEXT_H

#include <sass/base.h>
#include <sass/fwdecl.h>

#ifdef __cplusplus
extern "C" {
#endif

  // Change the virtual current working directory
  // You should probably not really use this function!
  ADDAPI void ADDCALL sass_chdir(const char* path);

  // Prints message to stderr with color for windows
  ADDAPI void ADDCALL sass_print_stdout(const char* message);
  ADDAPI void ADDCALL sass_print_stderr(const char* message);

// Resolve a file via the given include paths in the sass option struct
// find_file looks for the exact file name while find_include does a regular sass include
// ADDAPI char* ADDCALL sass_find_file (const char* path, struct SassOptionsCpp* opt);
// ADDAPI char* ADDCALL sass_find_include (const char* path, struct SassOptionsCpp* opt);

// Resolve a file relative to last import or include paths in the sass option struct
// find_file looks for the exact file name while find_include does a regular sass include
ADDAPI char* ADDCALL sass_compiler_find_file (const char* path, struct SassCompiler* compiler);
ADDAPI char* ADDCALL sass_compiler_find_include (const char* path, struct SassCompiler* compiler);

#ifdef __cplusplus
} // __cplusplus defined.
#endif

#endif
