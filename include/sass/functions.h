#ifndef SASS_C_FUNCTIONS_H
#define SASS_C_FUNCTIONS_H

#include <sass/base.h>

#ifdef __cplusplus
extern "C" {
#endif

// Creator for sass custom importer return argument list
ADDAPI SassImportListPtr ADDCALL sass_make_import_list();
ADDAPI void ADDCALL sass_delete_import_list(SassImportListPtr list);
ADDAPI size_t ADDCALL sass_import_list_size(SassImportListPtr list);
ADDAPI SassImportPtr ADDCALL sass_import_list_shift(SassImportListPtr list);
ADDAPI void ADDCALL sass_import_list_push(SassImportListPtr list, SassImportPtr);

// Creator for sass custom importer return argument list
ADDAPI SassImporterListPtr ADDCALL sass_make_importer_list ();
ADDAPI void ADDCALL sass_delete_importer_list(SassImporterListPtr list);
ADDAPI size_t ADDCALL sass_importer_list_size(SassImporterListPtr list);
ADDAPI SassImporterPtr ADDCALL sass_importer_list_shift(SassImporterListPtr list);
ADDAPI void ADDCALL sass_importer_list_push(SassImporterListPtr list, SassImporterPtr);

// Creators for custom importer callback (with some additional pointer)
// The pointer is mostly used to store the callback into the actual binding
ADDAPI SassImporterPtr ADDCALL sass_make_importer (SassImporterLambdaCpp importer, double priority, void* cookie);

// Getters for import function descriptors
ADDAPI SassImporterLambdaCpp ADDCALL sass_importer_get_function (SassImporterPtr cb);
ADDAPI double ADDCALL sass_importer_get_priority (SassImporterPtr cb);
ADDAPI void* ADDCALL sass_importer_get_cookie (SassImporterPtr cb);

// Deallocator for associated memory
ADDAPI void ADDCALL sass_delete_importer (SassImporterPtr cb); 

// Creator for a single import entry returned by the custom importer inside the list
ADDAPI SassImportPtr ADDCALL sass_make_import(const char* imp_path, const char* abs_base, char* source, char* srcmap, enum Sass_Import_Type type);
// set error message to abort import and to print out a message (path from existing object is used in output)
ADDAPI SassImportPtr ADDCALL sass_import_set_error(SassImportPtr import, const char* message, uint32_t line, uint32_t col);

// Getters for callee entry
ADDAPI const char* ADDCALL sass_callee_get_name (SassCalleePtr);
ADDAPI const char* ADDCALL sass_callee_get_path (SassCalleePtr);
ADDAPI uint32_t ADDCALL sass_callee_get_line (SassCalleePtr);
ADDAPI uint32_t ADDCALL sass_callee_get_column (SassCalleePtr);
ADDAPI enum Sass_Callee_Type ADDCALL sass_callee_get_type (SassCalleePtr);

// Getters and Setters for environments (lexical, local and global)
ADDAPI union Sass_Value* ADDCALL sass_env_get_lexical (struct SassCompilerCpp*, const char*);
ADDAPI void ADDCALL sass_env_set_lexical (struct SassCompilerCpp*, const char*, union Sass_Value*);
ADDAPI union Sass_Value* ADDCALL sass_env_get_local (struct SassCompilerCpp*, const char*);
ADDAPI void ADDCALL sass_env_set_local (struct SassCompilerCpp*, const char*, union Sass_Value*);
ADDAPI union Sass_Value* ADDCALL sass_env_get_global (struct SassCompilerCpp*, const char*);
ADDAPI void ADDCALL sass_env_set_global (struct SassCompilerCpp*, const char*, union Sass_Value*);

// Getters for import entry
ADDAPI const char* ADDCALL sass_import_get_imp_path (SassImportPtr);
ADDAPI const char* ADDCALL sass_import_get_abs_path (SassImportPtr);
ADDAPI const char* ADDCALL sass_import_get_source (SassImportPtr);
ADDAPI const char* ADDCALL sass_import_get_srcmap (SassImportPtr);
ADDAPI enum Sass_Import_Type ADDCALL sass_import_get_type(SassImportPtr);

// Getters from import error entry
ADDAPI uint32_t ADDCALL sass_import_get_error_line (SassImportPtr);
ADDAPI uint32_t ADDCALL sass_import_get_error_column (SassImportPtr);
ADDAPI const char* ADDCALL sass_import_get_error_message (SassImportPtr);

// Just in case we have some stray import structs
ADDAPI void ADDCALL sass_delete_import (SassImportPtr);

// Creators for sass function list and function descriptors
ADDAPI SassFunctionListPtr ADDCALL sass_make_function_list();
ADDAPI size_t ADDCALL sass_function_list_size(SassFunctionListPtr list);
ADDAPI SassFunctionPtr ADDCALL sass_function_list_shift(SassFunctionListPtr list);
ADDAPI void ADDCALL sass_function_list_push(SassFunctionListPtr list, SassFunctionPtr);

ADDAPI SassFunctionPtr ADDCALL sass_make_function (const char* signature, SassFunctionLambdaCpp cb, void* cookie);
ADDAPI void ADDCALL sass_delete_function (SassFunctionPtr entry);
ADDAPI void ADDCALL sass_delete_function_list (SassFunctionListPtr list);

// Getters for custom function descriptors
ADDAPI const char* ADDCALL sass_function_get_signature (SassFunctionPtr cb);
ADDAPI SassFunctionLambdaCpp ADDCALL sass_function_get_function (SassFunctionPtr cb);
ADDAPI void* ADDCALL sass_function_get_cookie (SassFunctionPtr cb);


#ifdef __cplusplus
} // __cplusplus defined.
#endif

#endif
