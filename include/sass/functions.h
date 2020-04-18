#ifndef SASS_C_FUNCTIONS_H
#define SASS_C_FUNCTIONS_H

#include <sass/base.h>

#ifdef __cplusplus
extern "C" {
#endif

// Creator for sass custom importer return argument list
ADDAPI struct SassImportListCpp* ADDCALL sass_make_import_list();
ADDAPI void ADDCALL sass_delete_import_list(struct SassImportListCpp* list);
ADDAPI size_t ADDCALL sass_import_list_size(struct SassImportListCpp* list);
ADDAPI struct SassImportCpp* ADDCALL sass_import_list_shift(struct SassImportListCpp* list);
ADDAPI void ADDCALL sass_import_list_push(struct SassImportListCpp* list, struct SassImportCpp*);

// Creator for sass custom importer return argument list
ADDAPI struct SassImporterListCpp* ADDCALL sass_make_importer_list ();
ADDAPI void ADDCALL sass_delete_importer_list(struct SassImporterListCpp* list);
ADDAPI size_t ADDCALL sass_importer_list_size(struct SassImporterListCpp* list);
ADDAPI struct SassImporterCpp* ADDCALL sass_importer_list_shift(struct SassImporterListCpp* list);
ADDAPI void ADDCALL sass_importer_list_push(struct SassImporterListCpp* list, struct SassImporterCpp*);

// Creators for custom importer callback (with some additional pointer)
// The pointer is mostly used to store the callback into the actual binding
ADDAPI struct SassImporterCpp* ADDCALL sass_make_importer (SassImporterLambdaCpp importer, double priority, void* cookie);

// Getters for import function descriptors
ADDAPI SassImporterLambdaCpp ADDCALL sass_importer_get_function (struct SassImporterCpp* cb);
ADDAPI double ADDCALL sass_importer_get_priority (struct SassImporterCpp* cb);
ADDAPI void* ADDCALL sass_importer_get_cookie (struct SassImporterCpp* cb);

// Deallocator for associated memory
ADDAPI void ADDCALL sass_delete_importer (struct SassImporterCpp* cb);

// Creator for a single import entry returned by the custom importer inside the list
ADDAPI struct SassImportCpp* ADDCALL sass_make_import(const char* imp_path, const char* abs_base, char* source, char* srcmap, enum Sass_Import_Type type);
// set error message to abort import and to print out a message (path from existing object is used in output)
ADDAPI struct SassImportCpp* ADDCALL sass_import_set_error(struct SassImportCpp* import, const char* message, uint32_t line, uint32_t col);

// Getters for callee entry
ADDAPI const char* ADDCALL sass_callee_get_name (struct SassCalleeCpp*);
ADDAPI const char* ADDCALL sass_callee_get_path (struct SassCalleeCpp*);
ADDAPI uint32_t ADDCALL sass_callee_get_line (struct SassCalleeCpp*);
ADDAPI uint32_t ADDCALL sass_callee_get_column (struct SassCalleeCpp*);
ADDAPI enum Sass_Callee_Type ADDCALL sass_callee_get_type (struct SassCalleeCpp*);

// Getters and Setters for environments (lexical, local and global)
ADDAPI struct SassValue* ADDCALL sass_env_get_lexical (struct SassCompiler*, const char*);
ADDAPI void ADDCALL sass_env_set_lexical (struct SassCompiler*, const char*, struct SassValue*);
ADDAPI struct SassValue* ADDCALL sass_env_get_local (struct SassCompiler*, const char*);
ADDAPI void ADDCALL sass_env_set_local (struct SassCompiler*, const char*, struct SassValue*);
ADDAPI struct SassValue* ADDCALL sass_env_get_global (struct SassCompiler*, const char*);
ADDAPI void ADDCALL sass_env_set_global (struct SassCompiler*, const char*, struct SassValue*);

// Getters for import entry
ADDAPI const char* ADDCALL sass_import_get_imp_path (struct SassImportCpp*);
ADDAPI const char* ADDCALL sass_import_get_abs_path (struct SassImportCpp*);
ADDAPI const char* ADDCALL sass_import_get_source (struct SassImportCpp*);
ADDAPI const char* ADDCALL sass_import_get_srcmap (struct SassImportCpp*);
ADDAPI enum Sass_Import_Type ADDCALL sass_import_get_type(struct SassImportCpp*);

// Getters from import error entry
ADDAPI uint32_t ADDCALL sass_import_get_error_line (struct SassImportCpp*);
ADDAPI uint32_t ADDCALL sass_import_get_error_column (struct SassImportCpp*);
ADDAPI const char* ADDCALL sass_import_get_error_message (struct SassImportCpp*);

// Just in case we have some stray import structs
ADDAPI void ADDCALL sass_delete_import (struct SassImportCpp*);

// Creators for sass function list and function descriptors
ADDAPI struct SassFunctionListCpp* ADDCALL sass_make_function_list();
ADDAPI size_t ADDCALL sass_function_list_size(struct SassFunctionListCpp* list);
ADDAPI struct SassFunctionCpp* ADDCALL sass_function_list_shift(struct SassFunctionListCpp* list);
ADDAPI void ADDCALL sass_function_list_push(struct SassFunctionListCpp* list, struct SassFunctionCpp*);

ADDAPI struct SassFunctionCpp* ADDCALL sass_make_function (const char* signature, SassFunctionLambdaCpp cb, void* cookie);
ADDAPI void ADDCALL sass_delete_function (struct SassFunctionCpp* entry);
ADDAPI void ADDCALL sass_delete_function_list (struct SassFunctionListCpp* list);

// Getters for custom function descriptors
ADDAPI const char* ADDCALL sass_function_get_signature (struct SassFunctionCpp* cb);
ADDAPI SassFunctionLambdaCpp ADDCALL sass_function_get_function (struct SassFunctionCpp* cb);
ADDAPI void* ADDCALL sass_function_get_cookie (struct SassFunctionCpp* cb);


#ifdef __cplusplus
} // __cplusplus defined.
#endif

#endif
