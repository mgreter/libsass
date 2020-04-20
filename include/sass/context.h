#ifndef SASS_C_CONTEXT_H
#define SASS_C_CONTEXT_H

#include <stddef.h>
#include <stdbool.h>
#include <sass/base.h>
#include <sass/fwdecl.h>
#include <sass/values.h>
#include <sass/functions.h>

#ifdef __cplusplus
extern "C" {
#endif

// Compiler states
  // Don't expose that one!

// Create and initialize the main libsass context
// ADDAPI struct SassContext* ADDCALL sass_make_context();

// Create an import entry by reading from `stdin`
ADDAPI struct SassImport* ADDCALL sass_make_stdin_import();

// Crate an import entry for the passed data content
ADDAPI struct SassImport* ADDCALL sass_make_data_import(char* content);

// Crate an import entry for the passed input path
ADDAPI struct SassImport* ADDCALL sass_make_file_import(const char* imp_path);

ADDAPI void ADDCALL sass_import_set_format(struct SassImport* import, enum SassImportFormat format);



// Error related getters (use after compiler was rendered)
ADDAPI int ADDCALL sass_error_get_status(struct SassError* error);
ADDAPI char* ADDCALL sass_error_get_json(struct SassError* error);
ADDAPI const char* ADDCALL sass_error_get_what(struct SassError* error);
ADDAPI const char* ADDCALL sass_error_get_messages(struct SassError* error);
ADDAPI const char* ADDCALL sass_error_get_warnings(struct SassError* error);
ADDAPI const char* ADDCALL sass_error_get_formatted(struct SassError* error);

// Error related getters for compiler (use after render is executed)
// ADDAPI int ADDCALL sass_compiler_get_error_status(struct SassCompiler* compiler);
// ADDAPI char* ADDCALL sass_compiler_get_error_json(struct SassCompiler* compiler);
// ADDAPI const char* ADDCALL sass_compiler_get_error_what(struct SassCompiler* compiler);
// ADDAPI const char* ADDCALL sass_compiler_get_error_message(struct SassCompiler* compiler);
// ADDAPI const char* ADDCALL sass_compiler_get_warning_message(struct SassCompiler* compiler);




// Change the virtual current working directory
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
