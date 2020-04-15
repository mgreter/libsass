#ifndef SASS_C_FUNCTIONS_H
#define SASS_C_FUNCTIONS_H

#include <sass/base.h>

#ifdef __cplusplus
extern "C" {
#endif


// Forward declaration
struct SassCalleeCpp;
struct SassImportCpp;
struct SassOptionsCpp;
struct SassCompilerCpp;
struct SassImporterCpp;
struct SassFunctionCpp;

// Typedef helpers for callee lists
typedef struct SassCalleeCpp* (SassCalleePtr);
// Typedef helpers for import lists
typedef struct SassImportCpp* (SassImportPtr);
//typedef struct SassImportCpp* (*SassImportListPtr);
// Typedef helpers for custom importer lists
typedef struct SassImporterCpp* (SassImporterPtr);
typedef struct Sass_Importer_List2* (SassImporterListPtr);
typedef struct Sass_Import_List2* (SassImportListPtr);
// Typedef defining importer signature and return type
typedef SassImportListPtr (*SassImporterFnCpp)
  (const char* url, SassImporterPtr cb, struct SassCompilerCpp* compiler);

// Typedef helpers for custom functions lists
typedef struct SassFunctionCpp (*SassFunctionPtr);

typedef struct Sass_Function_List2 (*SassFunctionListPtr);
// Typedef defining function signature and return type
typedef union Sass_Value* (*Sass_Function_Fn)
  (const union Sass_Value*, SassFunctionPtr cb, struct SassCompilerCpp* compiler);

// Type of parser to use
enum Sass_Import_Type {
  SASS_IMPORT_AUTO,
  SASS_IMPORT_SCSS,
  SASS_IMPORT_SASS,
  SASS_IMPORT_CSS,
};

// Type of function calls
enum Sass_Callee_Type {
  SASS_CALLEE_MIXIN,
  SASS_CALLEE_FUNCTION,
  SASS_CALLEE_C_FUNCTION,
};

// Creator for sass custom importer return argument list
ADDAPI SassImporterListPtr ADDCALL sass_make_importer_list ();
ADDAPI void ADDCALL sass_delete_importer_list(SassImporterListPtr list);
ADDAPI size_t ADDCALL sass_importer_list_size(SassImporterListPtr list);
ADDAPI SassImporterPtr ADDCALL sass_importer_list_shift(SassImporterListPtr list);
ADDAPI void ADDCALL sass_importer_list_push(SassImporterListPtr list, SassImporterPtr);

// ADDAPI SassImporterPtr ADDCALL sass_importer_get_list_entry (SassImporterListPtr list, uint32_t idx);
// ADDAPI void ADDCALL sass_importer_set_list_entry (SassImporterListPtr list, uint32_t idx, SassImporterPtr entry);




// Creators for custom importer callback (with some additional pointer)
// The pointer is mostly used to store the callback into the actual binding
ADDAPI SassImporterPtr ADDCALL sass_make_importer (SassImporterFnCpp importer, double priority, void* cookie);

// Getters for import function descriptors
ADDAPI SassImporterFnCpp ADDCALL sass_importer_get_function (SassImporterPtr cb);
ADDAPI double ADDCALL sass_importer_get_priority (SassImporterPtr cb);
ADDAPI void* ADDCALL sass_importer_get_cookie (SassImporterPtr cb);

// Deallocator for associated memory
ADDAPI void ADDCALL sass_delete_importer (SassImporterPtr cb); 

// Creator for sass custom importer return argument list
ADDAPI SassImportListPtr ADDCALL sass_make_import_list();
ADDAPI void ADDCALL sass_delete_import_list(SassImportListPtr list);
ADDAPI size_t ADDCALL sass_import_list_size(SassImportListPtr list);
ADDAPI SassImportPtr ADDCALL sass_import_list_shift(SassImportListPtr list);
ADDAPI void ADDCALL sass_import_list_push(SassImportListPtr list, SassImportPtr);


// Creator for a single import entry returned by the custom importer inside the list
ADDAPI SassImportPtr ADDCALL sass_make_import_entry (const char* path, char* source, char* srcmap);
ADDAPI SassImportPtr ADDCALL sass_make_import(const char* imp_path, const char* abs_base, char* source, char* srcmap);
ADDAPI SassImportPtr ADDCALL sass_make_import2(const char* imp_path, const char* abs_base, char* source, char* srcmap, enum Sass_Import_Type type);
// set error message to abort import and to print out a message (path from existing object is used in output)
ADDAPI SassImportPtr ADDCALL sass_import_set_error(SassImportPtr import, const char* message, uint32_t line, uint32_t col);

// Setters to insert an entry into the import list (you may also use [] access directly)
// Since we are dealing with pointers they should have a guaranteed and fixed size
// ADDAPI void ADDCALL sass_import_set_list_entry (SassImportListPtr list, uint32_t idx, SassImportPtr entry);
// ADDAPI SassImportPtr ADDCALL sass_import_get_list_entry (SassImportListPtr list, uint32_t idx);

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

// Explicit functions to take ownership of these items
// The property on our struct will be reset to NULL
//ADDAPI char* ADDCALL sass_import_take_source (SassImportPtr);
//ADDAPI char* ADDCALL sass_import_take_srcmap (SassImportPtr);
// Getters from import error entry
ADDAPI uint32_t ADDCALL sass_import_get_error_line (SassImportPtr);
ADDAPI uint32_t ADDCALL sass_import_get_error_column (SassImportPtr);
ADDAPI const char* ADDCALL sass_import_get_error_message (SassImportPtr);

// Deallocator for associated memory (incl. entries)
// ADDAPI void ADDCALL sass_delete_import_list (SassImportListPtr);
// Just in case we have some stray import structs
ADDAPI void ADDCALL sass_delete_import (SassImportPtr);



// Creators for sass function list and function descriptors
ADDAPI SassFunctionListPtr ADDCALL sass_make_function_list();
ADDAPI size_t ADDCALL sass_function_list_size(SassFunctionListPtr list);
ADDAPI SassFunctionPtr ADDCALL sass_function_list_shift(SassFunctionListPtr list);
ADDAPI void ADDCALL sass_function_list_push(SassFunctionListPtr list, SassFunctionPtr);

ADDAPI SassFunctionPtr ADDCALL sass_make_function (const char* signature, Sass_Function_Fn cb, void* cookie);
ADDAPI void ADDCALL sass_delete_function (SassFunctionPtr entry);
ADDAPI void ADDCALL sass_delete_function_list (SassFunctionListPtr list);


// Setters and getters for callbacks on function lists
// ADDAPI SassFunctionPtr ADDCALL sass_function_get_list_entry(SassFunctionListPtr list, uint32_t pos);
// ADDAPI void ADDCALL sass_function_set_list_entry(SassFunctionListPtr list, uint32_t pos, SassFunctionPtr cb);

// Getters for custom function descriptors
ADDAPI const char* ADDCALL sass_function_get_signature (SassFunctionPtr cb);
ADDAPI Sass_Function_Fn ADDCALL sass_function_get_function (SassFunctionPtr cb);
ADDAPI void* ADDCALL sass_function_get_cookie (SassFunctionPtr cb);


#ifdef __cplusplus
} // __cplusplus defined.
#endif

#endif
