#ifndef SASS_C_COMPILER_H
#define SASS_C_COMPILER_H

#include <stddef.h>
#include <stdbool.h>
#include <sass/base.h>
#include <sass/fwdecl.h>
#include <sass/values.h>
#include <sass/functions.h>

#ifdef __cplusplus
namespace Sass {
extern "C" {
#endif

  // Create a new compiler from the libsass context and the given entry point
  ADDAPI struct SassCompiler* ADDCALL sass_make_compiler();

  // Release all memory allocated with the structures
  ADDAPI void ADDCALL sass_delete_compiler(struct SassCompiler* compiler);

  // Parse the entry point and potentially all imports within
  ADDAPI bool ADDCALL sass_compiler_parse(struct SassCompiler* compiler);

  // Evaluate the parsed entry point and store resulting ast-tree
  ADDAPI bool ADDCALL sass_compiler_compile(struct SassCompiler* compiler);

  // Render the evaluated ast-tree to get the final output string
  ADDAPI bool ADDCALL sass_compiler_render(struct SassCompiler* compiler);

  // Setters for compiler option values
  ADDAPI void ADDCALL sass_compiler_set_precision(struct SassCompiler* compiler, int precision);

  ADDAPI void ADDCALL sass_compiler_set_entry_point(struct SassCompiler* compiler, struct SassImport* entry);
  // ADDAPI void ADDCALL sass_compiler_set_input_path(struct SassCompiler* compiler, const char* input_path);
  // ADDAPI void ADDCALL sass_compiler_set_output_path(struct SassCompiler* compiler, const char* output_path);
  // ADDAPI void ADDCALL sass_compiler_set_source_map_file(struct SassCompiler* compiler, const char* source_map_file);
  // ADDAPI void ADDCALL sass_compiler_set_source_map_root(struct SassCompiler* compiler, const char* source_map_root);

  // Getters for SassCompilerCpp options

  // Push function for paths (no manipulation support for now)
  ADDAPI void ADDCALL sass_compiler_load_plugins(struct SassCompiler* compiler, const char* paths);
  ADDAPI void ADDCALL sass_compiler_add_include_paths(struct SassCompiler* compiler, const char* paths);


  // Getter for sass compiler results
  ADDAPI const char* ADDCALL sass_compiler_get_output_string(struct SassCompiler* compiler);
  ADDAPI const char* ADDCALL sass_compiler_get_footer_string(struct SassCompiler* compiler);
  ADDAPI const char* ADDCALL sass_compiler_get_srcmap_string(struct SassCompiler* compiler);

  ADDAPI int ADDCALL sass_compiler_get_precision(struct SassCompiler* compiler);
  

  ADDAPI void ADDCALL sass_compiler_set_output_path(struct SassCompiler* compiler, const char* output_path);
  ADDAPI void ADDCALL sass_compiler_set_output_style(struct SassCompiler* compiler, enum SassOutputStyle output_style);

  // ADDAPI void ADDCALL sass_compiler_set_input_format(struct SassCompiler* compiler, enum Sass_Import_Type input_format);


  ADDAPI void ADDCALL sass_compiler_set_entry_point(struct SassCompiler* compiler, struct SassImport* import);
  ADDAPI void ADDCALL sass_compiler_set_logger_style(struct SassCompiler* compiler, enum SassLoggerStyle log_style);

  ADDAPI void ADDCALL sass_compiler_set_srcmap_mode(struct SassCompiler* compiler, enum SassSrcMapMode mode);
  ADDAPI void ADDCALL sass_compiler_set_srcmap_root(struct SassCompiler* compiler, const char* root);


  // Returns pointer to error object associated with compiler.
  // Will be valid until the associated compiler is destroyed.
  ADDAPI struct SassError* ADDCALL sass_compiler_get_error(struct SassCompiler* compiler);


#ifdef __cplusplus
} // __cplusplus defined.
} // EO namespace Sass
#endif

#endif
