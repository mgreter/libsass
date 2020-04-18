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
ADDAPI struct SassContextReal* ADDCALL sass_make_context();

// Create an import entry for the passed data from `stdin`
ADDAPI struct SassImportCpp* ADDCALL sass_make_data_import(char* source_string);

// Crate an import entry for the passed input path
ADDAPI struct SassImportCpp* ADDCALL sass_make_file_import(const char* input_path);

// Create a new compiler from the libsass context and the given entry point
ADDAPI struct SassCompiler* ADDCALL sass_make_compiler(struct SassContextReal* context, struct SassImportCpp* entry);

// Parse the entry point and potentially all imports within
ADDAPI void ADDCALL sass_compiler_parse(struct SassCompiler* compiler);
// Evaluate the parsed entry point and store resulting ast-tree
ADDAPI void ADDCALL sass_compiler_compile(struct SassCompiler* compiler);
// Render the evaluated ast-tree to get the final output string
ADDAPI void ADDCALL sass_compiler_render(struct SassCompiler* compiler);

// Return the rendered output content
ADDAPI const char* ADDCALL sass_compiler_get_output(struct SassCompiler* compiler);

// Return the rendered source map content
ADDAPI const char* ADDCALL sass_compiler_get_srcmap(struct SassCompiler* compiler);

// Release all memory allocated with the compiler
// This does _not_ include any contexts or options
ADDAPI void ADDCALL sass_delete_context(struct SassContextReal* context);



// Prints message to stderr with color for windows
ADDAPI void ADDCALL sass_print_stderr(const char* message);
ADDAPI void ADDCALL sass_context_print_stderr2(struct SassContextReal* context);


// Getters for context option values
ADDAPI int ADDCALL sass_context_get_precision(struct SassContextReal* options);
ADDAPI enum Sass_Output_Style ADDCALL sass_context_get_output_style(struct SassContextReal* options);

ADDAPI bool ADDCALL sass_context_get_source_comments(struct SassContextReal* context);
ADDAPI bool ADDCALL sass_context_get_source_map_embed(struct SassContextReal* context);
ADDAPI bool ADDCALL sass_context_get_source_map_contents(struct SassContextReal* context);
ADDAPI bool ADDCALL sass_context_get_source_map_file_urls(struct SassContextReal* context);
ADDAPI bool ADDCALL sass_context_get_omit_source_map_url(struct SassContextReal* context);

ADDAPI const char* ADDCALL sass_context_get_input_path(struct SassContextReal* context);
ADDAPI const char* ADDCALL sass_context_get_output_path(struct SassContextReal* context);
ADDAPI const char* ADDCALL sass_context_get_source_map_file(struct SassContextReal* context);
ADDAPI const char* ADDCALL sass_context_get_source_map_root(struct SassContextReal* context);

// Setters for context option values
ADDAPI void ADDCALL sass_context_set_precision(struct SassContextReal* context, int precision);
ADDAPI void ADDCALL sass_context_set_output_style(struct SassContextReal* context, enum Sass_Output_Style output_style);

ADDAPI void ADDCALL sass_context_set_source_comments(struct SassContextReal* context, bool source_comments);
ADDAPI void ADDCALL sass_context_set_source_map_embed(struct SassContextReal* context, bool source_map_embed);
ADDAPI void ADDCALL sass_context_set_source_map_contents(struct SassContextReal* context, bool source_map_contents);
ADDAPI void ADDCALL sass_context_set_source_map_file_urls(struct SassContextReal* context, bool source_map_file_urls);
ADDAPI void ADDCALL sass_context_set_omit_source_map_url(struct SassContextReal* context, bool omit_source_map_url);

ADDAPI void ADDCALL sass_context_set_entry_point(struct SassContextReal* context, struct SassImportCpp* entry);
ADDAPI void ADDCALL sass_context_set_input_path(struct SassContextReal* context, const char* input_path);
ADDAPI void ADDCALL sass_context_set_output_path(struct SassContextReal* context, const char* output_path);
ADDAPI void ADDCALL sass_context_set_source_map_file(struct SassContextReal* context, const char* source_map_file);
ADDAPI void ADDCALL sass_context_set_source_map_root(struct SassContextReal* context, const char* source_map_root);

// Getters for SassCompilerCpp options

// Push function for paths (no manipulation support for now)
ADDAPI void ADDCALL sass_option_push_plugin_path (struct SassOptionsCpp* options, const char* path);
ADDAPI void ADDCALL sass_option_push_include_path (struct SassOptionsCpp* options, const char* path);

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
