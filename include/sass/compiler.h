#ifndef SASS_C_COMPILER_H
#define SASS_C_COMPILER_H

#include <sass/base.h>

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

    // Returns pointer to error object associated with compiler.
    // Will be valid until the associated compiler is destroyed.
    ADDAPI struct SassError* ADDCALL sass_compiler_get_error(struct SassCompiler* compiler);

    // Push function for paths (no manipulation support for now)
    ADDAPI void ADDCALL sass_compiler_load_plugins(struct SassCompiler* compiler, const char* paths);
    ADDAPI void ADDCALL sass_compiler_add_include_paths(struct SassCompiler* compiler, const char* paths);
    ADDAPI void ADDCALL sass_compiler_add_custom_header(struct SassCompiler* compiler, struct SassImporter* header);
    ADDAPI void ADDCALL sass_compiler_add_custom_importer(struct SassCompiler* compiler, struct SassImporter* importer);
    ADDAPI void ADDCALL sass_compiler_add_custom_function(struct SassCompiler* compiler, struct SassFunction* function);

    // Setters for compiler option values
    ADDAPI void ADDCALL sass_compiler_set_precision(struct SassCompiler* compiler, int precision);
    ADDAPI void ADDCALL sass_compiler_set_output_path(struct SassCompiler* compiler, const char* output_path);
    ADDAPI void ADDCALL sass_compiler_set_entry_point(struct SassCompiler* compiler, struct SassImport* import);
    ADDAPI void ADDCALL sass_compiler_set_output_style(struct SassCompiler* compiler, enum SassOutputStyle style);
    ADDAPI void ADDCALL sass_compiler_set_logger_style(struct SassCompiler* compiler, enum SassLoggerStyle style);

    ADDAPI int ADDCALL sass_compiler_get_precision(struct SassCompiler* compiler);

    // ADDAPI void ADDCALL sass_compiler_set_input_path(struct SassCompiler* compiler, const char* input_path);
    // ADDAPI void ADDCALL sass_compiler_set_source_map_file(struct SassCompiler* compiler, const char* source_map_file);
    // ADDAPI void ADDCALL sass_compiler_set_source_map_root(struct SassCompiler* compiler, const char* source_map_root);

    // Getter for sass compiler results
    ADDAPI const char* ADDCALL sass_compiler_get_output_string(struct SassCompiler* compiler);
    ADDAPI const char* ADDCALL sass_compiler_get_footer_string(struct SassCompiler* compiler);
    ADDAPI const char* ADDCALL sass_compiler_get_srcmap_string(struct SassCompiler* compiler);

    ADDAPI void ADDCALL sass_compiler_set_srcmap_path(struct SassCompiler* compiler, const char* path);
    ADDAPI void ADDCALL sass_compiler_set_srcmap_root(struct SassCompiler* compiler, const char* root);
    ADDAPI void ADDCALL sass_compiler_set_srcmap_mode(struct SassCompiler* compiler, enum SassSrcMapMode mode);
    ADDAPI void ADDCALL sass_compiler_set_srcmap_file_urls(struct SassCompiler* compiler, bool enable);
    ADDAPI void ADDCALL sass_compiler_set_srcmap_embed_contents(struct SassCompiler* compiler, bool enable);


    ADDAPI struct SassValue* ADDCALL sass_compiler_get_local_variable(struct SassCompiler* compiler, const char* name);
    ADDAPI struct SassValue* ADDCALL sass_compiler_get_global_variable(struct SassCompiler* compiler, const char* name);
    ADDAPI struct SassValue* ADDCALL sass_compiler_get_lexical_variable(struct SassCompiler* compiler, const char* name);

#ifdef __cplusplus
} // EO extern "C".
} // EO namespace Sass
#endif

#endif
