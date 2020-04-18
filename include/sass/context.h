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
enum Sass_Compiler_State {
  SASS_COMPILER_CREATED,
  SASS_COMPILER_PARSED,
  SASS_COMPILER_EXECUTED
};

ADDAPI struct SassCompiler322* ADDCALL sass_make_compiler3(struct SassImportCpp* entry);

// Create and initialize an option struct
ADDAPI struct SassOptionsCpp* ADDCALL sass_make_options (void);

ADDAPI struct SassContextReal* ADDCALL sass_make_context();

// Create and initialize a specific context
ADDAPI struct SassFileContextCpp* ADDCALL sass_make_file_context (const char* input_path);
ADDAPI struct SassDataContextCpp* ADDCALL sass_make_data_context (char* source_string);

ADDAPI struct SassImportCpp* ADDCALL sass_make_file_import(const char* input_path);
ADDAPI struct SassImportCpp* ADDCALL sass_make_data_import(char* source_string);

// Call the compilation step for the specific context
ADDAPI int ADDCALL sass_compile_file_context (struct SassFileContextCpp* ctx);
ADDAPI int ADDCALL sass_compile_data_context (struct SassDataContextCpp* ctx);

ADDAPI int ADDCALL sass_compile_file_context2(struct SassContextReal* ctx, const char* input_path);
ADDAPI int ADDCALL sass_compile_data_context2(struct SassContextReal* ctx, char* source_string);

// Create a sass compiler instance for more control
ADDAPI struct SassCompilerCpp* ADDCALL sass_make_file_compiler (struct SassFileContextCpp* file_ctx);
ADDAPI struct SassCompilerCpp* ADDCALL sass_make_data_compiler (struct SassDataContextCpp* data_ctx);

// Execute the different compilation steps individually
// Useful if you only want to query the included files
ADDAPI int ADDCALL sass_compiler_parse(struct SassCompilerCpp* compiler);
ADDAPI int ADDCALL sass_compiler_execute(struct SassCompilerCpp* compiler);

// Release all memory allocated with the compiler
// This does _not_ include any contexts or options
ADDAPI void ADDCALL sass_delete_compiler(struct SassCompilerCpp* compiler);
ADDAPI void ADDCALL sass_delete_options(struct SassOptionsCpp* options);
ADDAPI void ADDCALL sass_delete_context(struct SassContextReal* context);

// Release all memory allocated and also ourself
ADDAPI void ADDCALL sass_delete_file_context (struct SassFileContextCpp* ctx);
ADDAPI void ADDCALL sass_delete_data_context (struct SassDataContextCpp* ctx);

// Getters for context from specific implementation
ADDAPI struct SassContextCpp* ADDCALL sass_file_context_get_context (struct SassFileContextCpp* file_ctx);
ADDAPI struct SassContextCpp* ADDCALL sass_data_context_get_context (struct SassDataContextCpp* data_ctx);

// Getters for Context_Options from SassContextCpp
ADDAPI struct SassOptionsCpp* ADDCALL sass_context_get_options (struct SassContextCpp* ctx);
ADDAPI struct SassOptionsCpp* ADDCALL sass_file_context_get_options (struct SassFileContextCpp* file_ctx);
ADDAPI struct SassOptionsCpp* ADDCALL sass_data_context_get_options (struct SassDataContextCpp* data_ctx);
ADDAPI void ADDCALL sass_file_context_set_options (struct SassFileContextCpp* file_ctx, struct SassOptionsCpp* opt);
ADDAPI void ADDCALL sass_data_context_set_options (struct SassDataContextCpp* data_ctx, struct SassOptionsCpp* opt);

// Prints message to stderr with color for windows
ADDAPI void ADDCALL sass_print_stderr(const char* message);
ADDAPI void ADDCALL sass_context_print_stderr(struct SassContextCpp* ctx);
ADDAPI void ADDCALL sass_context_print_stderr2(struct SassContextReal* context);

// Getters for Context_Option values
ADDAPI int ADDCALL sass_option_get_precision (struct SassOptionsCpp* options);
ADDAPI enum Sass_Output_Style ADDCALL sass_option_get_output_style (struct SassOptionsCpp* options);
ADDAPI bool ADDCALL sass_option_get_source_comments (struct SassOptionsCpp* options);
ADDAPI bool ADDCALL sass_option_get_source_map_embed (struct SassOptionsCpp* options);
ADDAPI bool ADDCALL sass_option_get_source_map_contents (struct SassOptionsCpp* options);
ADDAPI bool ADDCALL sass_option_get_source_map_file_urls (struct SassOptionsCpp* options);
ADDAPI bool ADDCALL sass_option_get_omit_source_map_url (struct SassOptionsCpp* options);
ADDAPI bool ADDCALL sass_option_get_is_indented_syntax_src (struct SassOptionsCpp* options);
ADDAPI const char* ADDCALL sass_option_get_indent (struct SassOptionsCpp* options);
ADDAPI const char* ADDCALL sass_option_get_linefeed (struct SassOptionsCpp* options);
ADDAPI const char* ADDCALL sass_option_get_input_path (struct SassOptionsCpp* options);
ADDAPI const char* ADDCALL sass_option_get_output_path (struct SassOptionsCpp* options);
ADDAPI const char* ADDCALL sass_option_get_source_map_file (struct SassOptionsCpp* options);
ADDAPI const char* ADDCALL sass_option_get_source_map_root (struct SassOptionsCpp* options);
// ADDAPI SassImporterListPtr ADDCALL sass_option_get_c_headers (struct SassOptionsCpp* options);
// ADDAPI SassImporterListPtr ADDCALL sass_option_get_c_importers (struct SassOptionsCpp* options);
// ADDAPI SassFunctionListPtr ADDCALL sass_option_get_c_functions (struct SassOptionsCpp* options);


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

// Setters for Context_Option values
ADDAPI void ADDCALL sass_option_set_precision (struct SassOptionsCpp* options, int precision);
ADDAPI void ADDCALL sass_option_set_output_style (struct SassOptionsCpp* options, enum Sass_Output_Style output_style);
ADDAPI void ADDCALL sass_option_set_source_comments (struct SassOptionsCpp* options, bool source_comments);
ADDAPI void ADDCALL sass_option_set_source_map_embed (struct SassOptionsCpp* options, bool source_map_embed);
ADDAPI void ADDCALL sass_option_set_source_map_contents (struct SassOptionsCpp* options, bool source_map_contents);
ADDAPI void ADDCALL sass_option_set_source_map_file_urls (struct SassOptionsCpp* options, bool source_map_file_urls);
ADDAPI void ADDCALL sass_option_set_omit_source_map_url (struct SassOptionsCpp* options, bool omit_source_map_url);
ADDAPI void ADDCALL sass_option_set_indent (struct SassOptionsCpp* options, const char* indent);
ADDAPI void ADDCALL sass_option_set_linefeed (struct SassOptionsCpp* options, const char* linefeed);
ADDAPI void ADDCALL sass_option_set_input_path (struct SassOptionsCpp* options, const char* input_path);
ADDAPI void ADDCALL sass_option_set_output_path (struct SassOptionsCpp* options, const char* output_path);
ADDAPI void ADDCALL sass_option_set_plugin_path (struct SassOptionsCpp* options, const char* plugin_path);
ADDAPI void ADDCALL sass_option_set_include_path (struct SassOptionsCpp* options, const char* include_path);
ADDAPI void ADDCALL sass_option_set_source_map_file (struct SassOptionsCpp* options, const char* source_map_file);
ADDAPI void ADDCALL sass_option_set_source_map_root (struct SassOptionsCpp* options, const char* source_map_root);
// ADDAPI void ADDCALL sass_option_set_c_headers (struct SassOptionsCpp* options, SassImporterListPtr c_headers);
// ADDAPI void ADDCALL sass_option_set_c_importers (struct SassOptionsCpp* options, SassImporterListPtr c_importers);
// ADDAPI void ADDCALL sass_option_set_c_functions (struct SassOptionsCpp* options, SassFunctionListPtr c_functions);


// Getters for SassContextCpp values
ADDAPI const char* ADDCALL sass_context_get_output_string (struct SassContextCpp* ctx);
ADDAPI const char* ADDCALL sass_context_get_stderr_string(struct SassContextCpp* ctx);
ADDAPI int ADDCALL sass_context_get_error_status (struct SassContextCpp* ctx);
ADDAPI const char* ADDCALL sass_context_get_error_json (struct SassContextCpp* ctx);
ADDAPI const char* ADDCALL sass_context_get_error_text (struct SassContextCpp* ctx);
ADDAPI const char* ADDCALL sass_context_get_error_message (struct SassContextCpp* ctx);
ADDAPI const char* ADDCALL sass_context_get_error_file (struct SassContextCpp* ctx);
ADDAPI const char* ADDCALL sass_context_get_error_src (struct SassContextCpp* ctx);
ADDAPI size_t ADDCALL sass_context_get_error_line (struct SassContextCpp* ctx);
ADDAPI size_t ADDCALL sass_context_get_error_column (struct SassContextCpp* ctx);
ADDAPI const char* ADDCALL sass_context_get_source_map_string (struct SassContextCpp* ctx);


ADDAPI const char* ADDCALL sass_context_get_output_string2(struct SassContextReal* context);
ADDAPI const char* ADDCALL sass_context_get_stderr_string2(struct SassContextReal* context);
ADDAPI int ADDCALL sass_context_get_error_status2(struct SassContextReal* context);
ADDAPI const char* ADDCALL sass_context_get_error_json2(struct SassContextReal* context);
ADDAPI const char* ADDCALL sass_context_get_error_text2(struct SassContextReal* context);
ADDAPI const char* ADDCALL sass_context_get_error_message2(struct SassContextReal* context);
ADDAPI const char* ADDCALL sass_context_get_error_file2(struct SassContextReal* context);
ADDAPI const char* ADDCALL sass_context_get_error_src2(struct SassContextReal* context);
ADDAPI size_t ADDCALL sass_context_get_error_line2(struct SassContextReal* context);
ADDAPI size_t ADDCALL sass_context_get_error_column2(struct SassContextReal* context);
ADDAPI const char* ADDCALL sass_context_get_source_map_string2(struct SassContextReal* context);



// Getters for options include path array
ADDAPI size_t ADDCALL sass_option_get_include_path_size(struct SassOptionsCpp* options);
ADDAPI const char* ADDCALL sass_option_get_include_path(struct SassOptionsCpp* options, size_t i);

// Calculate the size of the stored null terminated array
ADDAPI size_t ADDCALL sass_context_get_included_files(struct SassContextCpp* ctx);
ADDAPI const char* ADDCALL sass_context_get_included_file(struct SassContextCpp* ctx, size_t i);

ADDAPI void ADDCALL sass_option_add_c_function(struct SassOptionsCpp* options, SassFunctionPtr function);
ADDAPI void ADDCALL sass_option_add_c_header(struct SassOptionsCpp* options, SassImporterPtr header);

// Take ownership of memory (value on context is set to 0)
// ADDAPI char* ADDCALL sass_context_take_error_json (struct SassContextCpp* ctx);
// ADDAPI char* ADDCALL sass_context_take_error_text (struct SassContextCpp* ctx);
// ADDAPI char* ADDCALL sass_context_take_error_message (struct SassContextCpp* ctx);
// ADDAPI char* ADDCALL sass_context_take_error_file (struct SassContextCpp* ctx);
// ADDAPI char* ADDCALL sass_context_take_error_src (struct SassContextCpp* ctx);
// ADDAPI char* ADDCALL sass_context_take_output_string (struct SassContextCpp* ctx);
// ADDAPI char* ADDCALL sass_context_take_source_map_string (struct SassContextCpp* ctx);

// Getters for SassCompilerCpp options
ADDAPI enum Sass_Compiler_State ADDCALL sass_compiler_get_state(struct SassCompilerCpp* compiler);
ADDAPI struct SassContextCpp* ADDCALL sass_compiler_get_context(struct SassCompilerCpp* compiler);
ADDAPI struct SassOptionsCpp* ADDCALL sass_compiler_get_options(struct SassCompilerCpp* compiler);
ADDAPI size_t ADDCALL sass_compiler_get_import_stack_size(struct SassCompilerCpp* compiler);
ADDAPI SassImportPtr ADDCALL sass_compiler_get_last_import(struct SassCompilerCpp* compiler);
ADDAPI SassImportPtr ADDCALL sass_compiler_get_import_entry(struct SassCompilerCpp* compiler, size_t idx);
ADDAPI size_t ADDCALL sass_compiler_get_callee_stack_size(struct SassCompilerCpp* compiler);
ADDAPI SassCalleePtr ADDCALL sass_compiler_get_last_callee(struct SassCompilerCpp* compiler);
ADDAPI SassCalleePtr ADDCALL sass_compiler_get_callee_entry(struct SassCompilerCpp* compiler, size_t idx);

// Push function for paths (no manipulation support for now)
ADDAPI void ADDCALL sass_option_push_plugin_path (struct SassOptionsCpp* options, const char* path);
ADDAPI void ADDCALL sass_option_push_include_path (struct SassOptionsCpp* options, const char* path);

// Resolve a file via the given include paths in the sass option struct
// find_file looks for the exact file name while find_include does a regular sass include
ADDAPI char* ADDCALL sass_find_file (const char* path, struct SassOptionsCpp* opt);
ADDAPI char* ADDCALL sass_find_include (const char* path, struct SassOptionsCpp* opt);

// Resolve a file relative to last import or include paths in the sass option struct
// find_file looks for the exact file name while find_include does a regular sass include
ADDAPI char* ADDCALL sass_compiler_find_file (const char* path, struct SassCompilerCpp* compiler);
ADDAPI char* ADDCALL sass_compiler_find_include (const char* path, struct SassCompilerCpp* compiler);

#ifdef __cplusplus
} // __cplusplus defined.
#endif

#endif
