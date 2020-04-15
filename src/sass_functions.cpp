// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include <cstring>
#include "util.hpp"
#include "context.hpp"
#include "values.hpp"
#include "sass/functions.h"
#include "sass_functions.hpp"

extern "C" {
  using namespace Sass;



  // Creator for sass custom importer return argument list
  SassImportListPtr ADDCALL sass_make_import_list()
  {
    return new Sass_Import_List2{};
  }

  void ADDCALL sass_delete_import_list(SassImportListPtr list)
  {
    delete list;
  }
  size_t ADDCALL sass_import_list_size(SassImportListPtr list)
  {
    return list == nullptr ? 0 : list->size();
  }

  SassImportPtr ADDCALL sass_import_list_shift(SassImportListPtr list)
  {
    if (list == nullptr) return nullptr;
    if (list->empty()) return nullptr;
    auto ptr = list->front();
    list->erase(list->begin());
    return ptr;
  }

  void ADDCALL sass_import_list_push(SassImportListPtr list, SassImportPtr import)
  {
    if (list != nullptr) {
      list->push_back(import);
    }
  }


  Sass_Function_List ADDCALL sass_make_function_list()
  {
    return new Sass_Function_List2{};
  }

  size_t ADDCALL sass_function_list_size(Sass_Function_List list)
  {
    return list == nullptr ? 0 : list->size();
  }

  SassFunctionCpp* ADDCALL sass_function_list_shift(Sass_Function_List list)
  {
    if (list == nullptr) return nullptr;
    if (list->empty()) return nullptr;
    auto ptr = list->front();
    list->erase(list->begin());
    return ptr;
  }

  size_t ADDCALL sass_importer_list_size(SassImporterListPtr list)
  {
    return list == nullptr ? 0 : list->size();
  }

  SassImporterPtr ADDCALL sass_importer_list_shift(SassImporterListPtr list)
  {
    if (list == nullptr) return nullptr;
    if (list->empty()) return nullptr;
    auto ptr = list->front();
    list->erase(list->begin());
    return ptr;
  }

  void ADDCALL sass_importer_list_push(SassImporterListPtr list, SassImporterPtr importer)
  {
    if (list != nullptr) {
      list->push_back(importer);
    }
  }


  void ADDCALL sass_function_list_push(Sass_Function_List list, Sass_Function_Entry fn)
  {
    if (list != nullptr) {
      list->push_back(fn);
    }
  }

  Sass_Function_Entry ADDCALL sass_make_function(const char* signature, Sass_Function_Fn function, void* cookie)
  {
    Sass_Function_Entry cb = new SassFunctionCpp{};
    if (cb == 0) return 0;
    cb->signature = signature;
    cb->function = function;
    cb->cookie = cookie;
    return cb;
  }

  void ADDCALL sass_delete_function(Sass_Function_Entry entry)
  {
    delete entry;
  }

  // Deallocator for the allocated memory
  void ADDCALL sass_delete_function_list(Sass_Function_List list)
  {
    if (list == nullptr) return;
    for (auto function : *list) {
      sass_delete_function(function);
    }
    delete list;
  }

  // Setters and getters for callbacks on function lists
  // Sass_Function_Entry ADDCALL sass_function_get_list_entry(Sass_Function_List list, uint32_t pos) { return list[pos]; }
  // void sass_function_set_list_entry(Sass_Function_List list, uint32_t pos, Sass_Function_Entry cb) { list[pos] = cb; }

  const char* ADDCALL sass_function_get_signature(Sass_Function_Entry cb) { return cb->signature.empty() ? 0 : cb->signature.c_str(); }
  Sass_Function_Fn ADDCALL sass_function_get_function(Sass_Function_Entry cb) { return cb->function; }
  void* ADDCALL sass_function_get_cookie(Sass_Function_Entry cb) { return cb->cookie; }

  SassImporterPtr ADDCALL sass_make_importer(SassImporterFnCpp fn, double priority, void* cookie)
  {
    SassImporterPtr importer = new SassImporterCpp{};
    if (importer == 0) return 0;
    importer->importer = fn;
    importer->priority = priority;
    importer->cookie = cookie;
    return importer;
  }

  SassImporterFnCpp ADDCALL sass_importer_get_function(SassImporterPtr cb) { return cb->importer; }
  double ADDCALL sass_importer_get_priority (SassImporterPtr cb) { return cb->priority; }
  void* ADDCALL sass_importer_get_cookie(SassImporterPtr cb) { return cb->cookie; }

  // Just in case we have some stray import structs
  void ADDCALL sass_delete_importer (SassImporterPtr importer)
  {
    delete importer;
  }

  // Creator for sass custom importer function list
  SassImporterListPtr ADDCALL sass_make_importer_list()
  {
    return new Sass_Importer_List2{};
  }

  // Deallocator for the allocated memory
  void ADDCALL sass_delete_importer_list(SassImporterListPtr list)
  {
    if (list == nullptr) return;
    for (auto importer : *list) {
      sass_delete_importer(importer);
    }
    delete list;
  }

  // SassImporterPtr ADDCALL sass_importer_get_list_entry(SassImporterListPtr list, uint32_t idx) { return list[idx]; }
  // void ADDCALL sass_importer_set_list_entry(SassImporterListPtr list, uint32_t idx, SassImporterPtr cb) { list[idx] = cb; }

  // Creator for a single import entry returned by the custom importer inside the list
  // We take ownership of the memory for source and srcmap (freed when context is destroyed)
  SassImportPtr ADDCALL sass_make_import(const char* imp_path, const char* abs_path, char* source, char* srcmap)
  {
    SassImportPtr v = new SassImportCpp();
    if (v == 0) return 0;
    v->line = -1;
    v->column = -1;
    // Stuff was shifted to internal C++ API
    v->srcdata = SASS_MEMORY_NEW(SourceFile,
      imp_path, abs_path,
      source ? source : 0,
      srcmap ? srcmap : 0,
      SASS_IMPORT_AUTO, -1);
    return v;
  }

  // Creator for a single import entry returned by the custom importer inside the list
  // We take ownership of the memory for source and srcmap (freed when context is destroyed)
  SassImportPtr ADDCALL sass_make_import2(const char* imp_path, const char* abs_path, char* source, char* srcmap, enum Sass_Import_Type type)
  {
    SassImportPtr v = new SassImportCpp();
    if (v == 0) return 0;
    v->line = -1;
    v->column = -1;
    // Stuff was shifted to internal C++ API
    v->srcdata = SASS_MEMORY_NEW(SourceFile,
      imp_path, abs_path,
      source ? source : 0,
      srcmap ? srcmap : 0,
      type, -1);
    return v;
  }

  // Older style, but somehow still valid - keep around or deprecate?
  SassImportPtr ADDCALL sass_make_import_entry(const char* path, char* source, char* srcmap)
  {
    return sass_make_import(path, path, source, srcmap);
  }

  // Upgrade a normal import entry to throw an error (original path can be re-used by error reporting)
  SassImportPtr ADDCALL sass_import_set_error(SassImportPtr import, const char* error, uint32_t line, uint32_t col)
  {
    if (import == 0) return 0;
    import->error = error ? error : "";
    import->line = line ? line : -1;
    import->column = col ? col : -1;
    return import;
  }

  // Setters and getters for entries on the import list
  // void ADDCALL sass_import_set_list_entry(SassImportListPtr list, uint32_t idx, SassImportPtr entry) { list[idx] = entry; }
  // SassImportPtr ADDCALL sass_import_get_list_entry(SassImportListPtr list, uint32_t idx) { return list[idx]; }

  // Just in case we have some stray import structs
  void ADDCALL sass_delete_import(SassImportPtr import)
  {
    delete import;
  }

  // Getter for callee entry
  const char* ADDCALL sass_callee_get_name(SassCalleePtr entry) { return entry->name; }
  const char* ADDCALL sass_callee_get_path(SassCalleePtr entry) { return entry->path; }
  uint32_t ADDCALL sass_callee_get_line(SassCalleePtr entry) { return entry->line; }
  uint32_t ADDCALL sass_callee_get_column(SassCalleePtr entry) { return entry->column; }
  enum Sass_Callee_Type ADDCALL sass_callee_get_type(SassCalleePtr entry) { return entry->type; }
  
  // Getters and Setters for environments (lexical, local and global)
  union Sass_Value* ADDCALL sass_env_get_lexical (struct SassCompilerCpp* compiler, const char* name) {
    return ast_node_to_sass_value(compiler->cpp_ctx->varRoot.getLexicalVariable(Sass::EnvKey(name)));
  }

  void ADDCALL sass_env_set_lexical (struct SassCompilerCpp* compiler, const char* name, union Sass_Value* val) {
    compiler->cpp_ctx->varRoot.setLexicalVariable(Sass::EnvKey(name), sass_value_to_ast_node(val));
  }

  union Sass_Value* ADDCALL sass_env_get_local (struct SassCompilerCpp* compiler, const char* name) {
    return ast_node_to_sass_value(compiler->cpp_ctx->varRoot.getLocalVariable(Sass::EnvKey(name)));
  }

  void ADDCALL sass_env_set_local (struct SassCompilerCpp* compiler, const char* name, union Sass_Value* val) {
    compiler->cpp_ctx->varRoot.setLocalVariable(Sass::EnvKey(name), sass_value_to_ast_node(val));
  }

  union Sass_Value* ADDCALL sass_env_get_global (struct SassCompilerCpp* compiler, const char* name) {
    return ast_node_to_sass_value(compiler->cpp_ctx->varRoot.getGlobalVariable(Sass::EnvKey(name)));
  }

  void ADDCALL sass_env_set_global (struct SassCompilerCpp* compiler, const char* name, union Sass_Value* val) {
    compiler->cpp_ctx->varRoot.setGlobalVariable(Sass::EnvKey(name), sass_value_to_ast_node(val));
  }

  // Getter for import entry
  const char* ADDCALL sass_import_get_imp_path(SassImportPtr entry) { return entry->srcdata->getImpPath(); }
  const char* ADDCALL sass_import_get_abs_path(SassImportPtr entry) { return entry->srcdata->getAbsPath(); }
  const char* ADDCALL sass_import_get_source(SassImportPtr entry) { return entry->srcdata->content(); }
  const char* ADDCALL sass_import_get_srcmap(SassImportPtr entry) { return entry->srcdata->srcmaps(); }
  enum Sass_Import_Type ADDCALL sass_import_get_type(SassImportPtr entry) { return entry->srcdata->getType();  }

  // Getter for import error entry
  uint32_t ADDCALL sass_import_get_error_line(SassImportPtr entry) { return entry->line; }
  uint32_t ADDCALL sass_import_get_error_column(SassImportPtr entry) { return entry->column; }
  const char* ADDCALL sass_import_get_error_message(SassImportPtr entry) { return entry->error.empty() ? 0 : entry->error.c_str(); }

  // Explicit functions to take ownership of the memory
  // Resets our own property since we do not know if it is still alive
  // char* ADDCALL sass_import_take_source(SassImportPtr entry) { char* ptr = entry->source; entry->source = 0; return ptr; }
  // char* ADDCALL sass_import_take_srcmap(SassImportPtr entry) { char* ptr = entry->srcmap; entry->srcmap = 0; return ptr; }

}
