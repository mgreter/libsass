#ifndef SASS_C_FUNCTIONS_H
#define SASS_C_FUNCTIONS_H

#include <sass/base.h>

#ifdef __cplusplus
extern "C" {
#endif


// Forward declaration
struct SassCalleeCpp;
struct SassImportCpp;
struct Sass_Options;
struct Sass_Compiler;
struct SassImporterCpp;
struct Sass_Function;

// Typedef helpers for callee lists
typedef struct SassCalleeCpp* (Sass_Callee_Entry);
// Typedef helpers for import lists
typedef struct SassImportCpp* (Sass_Import_Entry);
//typedef struct SassImportCpp* (*Sass_Import_List);
// Typedef helpers for custom importer lists
typedef struct SassImporterCpp* (Sass_Importer_Entry);
typedef struct Sass_Importer_List2* (Sass_Importer_List);
typedef struct Sass_Import_List2* (Sass_Import_List);
// Typedef defining importer signature and return type
typedef Sass_Import_List (*Sass_Importer_Fn)
  (const char* url, Sass_Importer_Entry cb, struct Sass_Compiler* compiler);

// Typedef helpers for custom functions lists
typedef struct Sass_Function (*Sass_Function_Entry);

typedef struct Sass_Function_List2 (*Sass_Function_List);
// Typedef defining function signature and return type
typedef union Sass_Value* (*Sass_Function_Fn)
  (const union Sass_Value*, Sass_Function_Entry cb, struct Sass_Compiler* compiler);

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
ADDAPI Sass_Importer_List ADDCALL sass_make_importer_list ();
ADDAPI void ADDCALL sass_delete_importer_list(Sass_Importer_List list);
ADDAPI size_t ADDCALL sass_importer_list_size(Sass_Importer_List list);
ADDAPI Sass_Importer_Entry ADDCALL sass_importer_list_shift(Sass_Importer_List list);
ADDAPI void ADDCALL sass_importer_list_push(Sass_Importer_List list, Sass_Importer_Entry);

// ADDAPI Sass_Importer_Entry ADDCALL sass_importer_get_list_entry (Sass_Importer_List list, uint32_t idx);
// ADDAPI void ADDCALL sass_importer_set_list_entry (Sass_Importer_List list, uint32_t idx, Sass_Importer_Entry entry);




// Creators for custom importer callback (with some additional pointer)
// The pointer is mostly used to store the callback into the actual binding
ADDAPI Sass_Importer_Entry ADDCALL sass_make_importer (Sass_Importer_Fn importer, double priority, void* cookie);

// Getters for import function descriptors
ADDAPI Sass_Importer_Fn ADDCALL sass_importer_get_function (Sass_Importer_Entry cb);
ADDAPI double ADDCALL sass_importer_get_priority (Sass_Importer_Entry cb);
ADDAPI void* ADDCALL sass_importer_get_cookie (Sass_Importer_Entry cb);

// Deallocator for associated memory
ADDAPI void ADDCALL sass_delete_importer (Sass_Importer_Entry cb); 

// Creator for sass custom importer return argument list
ADDAPI Sass_Import_List ADDCALL sass_make_import_list();
ADDAPI void ADDCALL sass_delete_import_list(Sass_Import_List list);
ADDAPI size_t ADDCALL sass_import_list_size(Sass_Import_List list);
ADDAPI Sass_Import_Entry ADDCALL sass_import_list_shift(Sass_Import_List list);
ADDAPI void ADDCALL sass_import_list_push(Sass_Import_List list, Sass_Import_Entry);


// Creator for a single import entry returned by the custom importer inside the list
ADDAPI Sass_Import_Entry ADDCALL sass_make_import_entry (const char* path, char* source, char* srcmap);
ADDAPI Sass_Import_Entry ADDCALL sass_make_import(const char* imp_path, const char* abs_base, char* source, char* srcmap);
ADDAPI Sass_Import_Entry ADDCALL sass_make_import2(const char* imp_path, const char* abs_base, char* source, char* srcmap, enum Sass_Import_Type type);
// set error message to abort import and to print out a message (path from existing object is used in output)
ADDAPI Sass_Import_Entry ADDCALL sass_import_set_error(Sass_Import_Entry import, const char* message, uint32_t line, uint32_t col);

// Setters to insert an entry into the import list (you may also use [] access directly)
// Since we are dealing with pointers they should have a guaranteed and fixed size
// ADDAPI void ADDCALL sass_import_set_list_entry (Sass_Import_List list, uint32_t idx, Sass_Import_Entry entry);
// ADDAPI Sass_Import_Entry ADDCALL sass_import_get_list_entry (Sass_Import_List list, uint32_t idx);

// Getters for callee entry
ADDAPI const char* ADDCALL sass_callee_get_name (Sass_Callee_Entry);
ADDAPI const char* ADDCALL sass_callee_get_path (Sass_Callee_Entry);
ADDAPI uint32_t ADDCALL sass_callee_get_line (Sass_Callee_Entry);
ADDAPI uint32_t ADDCALL sass_callee_get_column (Sass_Callee_Entry);
ADDAPI enum Sass_Callee_Type ADDCALL sass_callee_get_type (Sass_Callee_Entry);

// Getters and Setters for environments (lexical, local and global)
ADDAPI union Sass_Value* ADDCALL sass_env_get_lexical (struct Sass_Compiler*, const char*);
ADDAPI void ADDCALL sass_env_set_lexical (struct Sass_Compiler*, const char*, union Sass_Value*);
ADDAPI union Sass_Value* ADDCALL sass_env_get_local (struct Sass_Compiler*, const char*);
ADDAPI void ADDCALL sass_env_set_local (struct Sass_Compiler*, const char*, union Sass_Value*);
ADDAPI union Sass_Value* ADDCALL sass_env_get_global (struct Sass_Compiler*, const char*);
ADDAPI void ADDCALL sass_env_set_global (struct Sass_Compiler*, const char*, union Sass_Value*);

// Getters for import entry
ADDAPI const char* ADDCALL sass_import_get_imp_path (Sass_Import_Entry);
ADDAPI const char* ADDCALL sass_import_get_abs_path (Sass_Import_Entry);
ADDAPI const char* ADDCALL sass_import_get_source (Sass_Import_Entry);
ADDAPI const char* ADDCALL sass_import_get_srcmap (Sass_Import_Entry);
ADDAPI enum Sass_Import_Type ADDCALL sass_import_get_type(Sass_Import_Entry);

// Explicit functions to take ownership of these items
// The property on our struct will be reset to NULL
//ADDAPI char* ADDCALL sass_import_take_source (Sass_Import_Entry);
//ADDAPI char* ADDCALL sass_import_take_srcmap (Sass_Import_Entry);
// Getters from import error entry
ADDAPI uint32_t ADDCALL sass_import_get_error_line (Sass_Import_Entry);
ADDAPI uint32_t ADDCALL sass_import_get_error_column (Sass_Import_Entry);
ADDAPI const char* ADDCALL sass_import_get_error_message (Sass_Import_Entry);

// Deallocator for associated memory (incl. entries)
// ADDAPI void ADDCALL sass_delete_import_list (Sass_Import_List);
// Just in case we have some stray import structs
ADDAPI void ADDCALL sass_delete_import (Sass_Import_Entry);



// Creators for sass function list and function descriptors
ADDAPI Sass_Function_List ADDCALL sass_make_function_list();
ADDAPI size_t ADDCALL sass_function_list_size(Sass_Function_List list);
ADDAPI Sass_Function_Entry ADDCALL sass_function_list_shift(Sass_Function_List list);
ADDAPI void ADDCALL sass_function_list_push(Sass_Function_List list, Sass_Function_Entry);

ADDAPI Sass_Function_Entry ADDCALL sass_make_function (const char* signature, Sass_Function_Fn cb, void* cookie);
ADDAPI void ADDCALL sass_delete_function (Sass_Function_Entry entry);
ADDAPI void ADDCALL sass_delete_function_list (Sass_Function_List list);


// Setters and getters for callbacks on function lists
// ADDAPI Sass_Function_Entry ADDCALL sass_function_get_list_entry(Sass_Function_List list, uint32_t pos);
// ADDAPI void ADDCALL sass_function_set_list_entry(Sass_Function_List list, uint32_t pos, Sass_Function_Entry cb);

// Getters for custom function descriptors
ADDAPI const char* ADDCALL sass_function_get_signature (Sass_Function_Entry cb);
ADDAPI Sass_Function_Fn ADDCALL sass_function_get_function (Sass_Function_Entry cb);
ADDAPI void* ADDCALL sass_function_get_cookie (Sass_Function_Entry cb);


#ifdef __cplusplus
} // __cplusplus defined.
#endif

#endif
