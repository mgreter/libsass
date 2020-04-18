#ifndef SASS_C_CONTEXT_H
#define SASS_C_CONTEXT_H

#include <stddef.h>
#include <stdbool.h>
#include <sass/base.h>
#include <sass/values.h>
#include <sass/functions.h>

#ifdef __cplusplus
extern "C" {
#endif

// Compiler states
enum SassCompilerState {
  CREATED,
  PARSED,
  COMPILED,
  RENDERED,
  DESTROYED
};

// Create and initialize the main libsass context
ADDAPI struct SassContext* ADDCALL sass_make_context();

// Create an import entry by reading from `stdin`
ADDAPI struct SassImportCpp* ADDCALL sass_make_stdin_import();

// Crate an import entry for the passed data content
ADDAPI struct SassImportCpp* ADDCALL sass_make_data_import(char* content);

// Crate an import entry for the passed input path
ADDAPI struct SassImportCpp* ADDCALL sass_make_file_import(const char* imp_path);

// Create a new compiler from the libsass context and the given entry point
ADDAPI struct SassCompiler* ADDCALL sass_make_compiler(struct SassContext* context, struct SassImportCpp* entry);

// Parse the entry point and potentially all imports within
ADDAPI void ADDCALL sass_compiler_parse(struct SassCompiler* compiler);
// Evaluate the parsed entry point and store resulting ast-tree
ADDAPI void ADDCALL sass_compiler_compile(struct SassCompiler* compiler);
// Render the evaluated ast-tree to get the final output string
ADDAPI void ADDCALL sass_compiler_render(struct SassCompiler* compiler);

// Getter for sass compiler results
ADDAPI const char* ADDCALL sass_compiler_get_output_string(struct SassCompiler* compiler);
ADDAPI const char* ADDCALL sass_compiler_get_footer_string(struct SassCompiler* compiler);
ADDAPI const char* ADDCALL sass_compiler_get_srcmap_string(struct SassCompiler* compiler);

ADDAPI int ADDCALL sass_compiler_get_error_status(struct SassCompiler* compiler);
ADDAPI const char* ADDCALL sass_compiler_get_error_json(struct SassCompiler* compiler);
ADDAPI const char* ADDCALL sass_compiler_get_error_text(struct SassCompiler* compiler);
ADDAPI const char* ADDCALL sass_compiler_get_error_message(struct SassCompiler* compiler);
ADDAPI const char* ADDCALL sass_compiler_get_error_file(struct SassCompiler* compiler);
ADDAPI const char* ADDCALL sass_compiler_get_error_src(struct SassCompiler* compiler);
ADDAPI size_t ADDCALL sass_compiler_get_error_line(struct SassCompiler* compiler);
ADDAPI size_t ADDCALL sass_compiler_get_error_column(struct SassCompiler* compiler);

// Release all memory allocated with the structures
ADDAPI void ADDCALL sass_delete_context(struct SassContext* context);
ADDAPI void ADDCALL sass_delete_compiler(struct SassCompiler* context);



// Prints message to stderr with color for windows
ADDAPI void ADDCALL sass_print_stderr(const char* message);
ADDAPI void ADDCALL sass_context_print_stderr2(struct SassContext* context);


// Getters for context option values
ADDAPI int ADDCALL sass_context_get_precision(struct SassContext* options);
ADDAPI enum Sass_Output_Style ADDCALL sass_context_get_output_style(struct SassContext* options);

ADDAPI bool ADDCALL sass_context_get_source_comments(struct SassContext* context);
ADDAPI bool ADDCALL sass_compiler_get_source_map_embed(struct SassCompiler* compiler);
ADDAPI bool ADDCALL sass_compiler_get_source_map_contents(struct SassCompiler* compiler);
ADDAPI bool ADDCALL sass_compiler_get_source_map_file_urls(struct SassCompiler* compiler);
ADDAPI bool ADDCALL sass_compiler_get_omit_source_map_url(struct SassCompiler* compiler);

ADDAPI const char* ADDCALL sass_context_get_input_path(struct SassContext* context);
ADDAPI const char* ADDCALL sass_context_get_output_path(struct SassContext* context);
ADDAPI const char* ADDCALL sass_context_get_source_map_file(struct SassContext* context);
ADDAPI const char* ADDCALL sass_context_get_source_map_root(struct SassContext* context);

// Setters for context option values
ADDAPI void ADDCALL sass_context_set_precision(struct SassContext* context, int precision);
ADDAPI void ADDCALL sass_context_set_output_style(struct SassContext* context, enum Sass_Output_Style output_style);

ADDAPI void ADDCALL sass_context_set_source_comments(struct SassContext* context, bool source_comments);
ADDAPI void ADDCALL sass_compiler_set_source_map_embed(struct SassCompiler* compiler, bool source_map_embed);
ADDAPI void ADDCALL sass_compiler_set_source_map_contents(struct SassCompiler* compiler, bool source_map_contents);
ADDAPI void ADDCALL sass_compiler_set_source_map_file_urls(struct SassCompiler* compiler, bool source_map_file_urls);
ADDAPI void ADDCALL sass_compiler_set_omit_source_map_url(struct SassCompiler* compiler, bool omit_source_map_url);

ADDAPI void ADDCALL sass_context_set_entry_point(struct SassContext* context, struct SassImportCpp* entry);
ADDAPI void ADDCALL sass_context_set_input_path(struct SassContext* context, const char* input_path);
ADDAPI void ADDCALL sass_context_set_output_path(struct SassContext* context, const char* output_path);
ADDAPI void ADDCALL sass_context_set_source_map_file(struct SassContext* context, const char* source_map_file);
ADDAPI void ADDCALL sass_context_set_source_map_root(struct SassContext* context, const char* source_map_root);

// Getters for SassCompilerCpp options

// Push function for paths (no manipulation support for now)
ADDAPI void ADDCALL sass_context_push_plugin_path (struct SassContext* context, const char* path);
ADDAPI void ADDCALL sass_context_push_include_path (struct SassContext* context, const char* path);

// Resolve a file via the given include paths in the sass option struct
// find_file looks for the exact file name while find_include does a regular sass include
ADDAPI char* ADDCALL sass_find_file (const char* path, struct SassOptionsCpp* opt);
ADDAPI char* ADDCALL sass_find_include (const char* path, struct SassOptionsCpp* opt);

// Resolve a file relative to last import or include paths in the sass option struct
// find_file looks for the exact file name while find_include does a regular sass include
ADDAPI char* ADDCALL sass_compiler_find_file (const char* path, struct SassCompiler* compiler);
ADDAPI char* ADDCALL sass_compiler_find_include (const char* path, struct SassCompiler* compiler);

#ifdef __cplusplus
} // __cplusplus defined.
#endif

#endif
