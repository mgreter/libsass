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
  struct SassImportListCpp* ADDCALL sass_make_import_list()
  {
    return new SassImportListCpp{};
  }

  void ADDCALL sass_delete_import_list(struct SassImportListCpp* list)
  {
    delete list;
  }
  size_t ADDCALL sass_import_list_size(struct SassImportListCpp* list)
  {
    return list == nullptr ? 0 : list->size();
  }

  struct SassImportCpp* ADDCALL sass_import_list_shift(struct SassImportListCpp* list)
  {
    if (list == nullptr) return nullptr;
    if (list->empty()) return nullptr;
    auto ptr = list->front();
    list->pop_front();
    return ptr;
  }

  void ADDCALL sass_import_list_push(struct SassImportListCpp* list, struct SassImportCpp* import)
  {
    if (list != nullptr) {
      list->push_back(import);
    }
  }


  struct SassFunctionListCpp* ADDCALL sass_make_function_list()
  {
    return new SassFunctionListCpp{};
  }

  size_t ADDCALL sass_function_list_size(struct SassFunctionListCpp* list)
  {
    return list == nullptr ? 0 : list->size();
  }

  SassFunctionCpp* ADDCALL sass_function_list_shift(struct SassFunctionListCpp* list)
  {
    if (list == nullptr) return nullptr;
    if (list->empty()) return nullptr;
    auto ptr = list->front();
    list->pop_front();
    return ptr;
  }

  size_t ADDCALL sass_importer_list_size(struct SassImporterListCpp* list)
  {
    return list == nullptr ? 0 : list->size();
  }

  struct SassImporterCpp* ADDCALL sass_importer_list_shift(struct SassImporterListCpp* list)
  {
    if (list == nullptr) return nullptr;
    if (list->empty()) return nullptr;
    auto ptr = list->front();
    list->pop_front();
    return ptr;
  }

  void ADDCALL sass_importer_list_push(struct SassImporterListCpp* list, struct SassImporterCpp* importer)
  {
    if (list != nullptr) {
      if (importer != nullptr) {
        list->emplace_back(importer);
      }
    }
  }


  void ADDCALL sass_function_list_push(struct SassFunctionListCpp* list, struct SassFunctionCpp* fn)
  {
    if (list != nullptr) {
      if (fn != nullptr) {
        list->emplace_back(fn);
      }
    }
  }

  struct SassFunctionCpp* ADDCALL sass_make_function(const char* signature, SassFunctionLambdaCpp function, void* cookie)
  {
    struct SassFunctionCpp* cb = new SassFunctionCpp{};
    if (cb == 0) return 0;
    cb->signature = signature;
    cb->function = function;
    cb->cookie = cookie;
    return cb;
  }

  void ADDCALL sass_delete_function(struct SassFunctionCpp* entry)
  {
    delete entry;
  }

  // Deallocator for the allocated memory
  void ADDCALL sass_delete_function_list(struct SassFunctionListCpp* list)
  {
    if (list == nullptr) return;
    for (auto function : *list) {
      sass_delete_function(function);
    }
    delete list;
  }

  // Setters and getters for callbacks on function lists
  // struct SassFunctionCpp* ADDCALL sass_function_get_list_entry(struct SassFunctionListCpp* list, uint32_t pos) { return list[pos]; }
  // void sass_function_set_list_entry(struct SassFunctionListCpp* list, uint32_t pos, struct SassFunctionCpp* cb) { list[pos] = cb; }

  const char* ADDCALL sass_function_get_signature(struct SassFunctionCpp* cb) { return cb->signature.empty() ? 0 : cb->signature.c_str(); }
  SassFunctionLambdaCpp ADDCALL sass_function_get_function(struct SassFunctionCpp* cb) { return cb->function; }
  void* ADDCALL sass_function_get_cookie(struct SassFunctionCpp* cb) { return cb->cookie; }

  struct SassImporterCpp* ADDCALL sass_make_importer(SassImporterLambdaCpp fn, double priority, void* cookie)
  {
    struct SassImporterCpp* importer = new SassImporterCpp{};
    if (importer == 0) return 0;
    importer->importer = fn;
    importer->priority = priority;
    importer->cookie = cookie;
    return importer;
  }

  SassImporterLambdaCpp ADDCALL sass_importer_get_function(struct SassImporterCpp* cb) { return cb->importer; }
  double ADDCALL sass_importer_get_priority (struct SassImporterCpp* cb) { return cb->priority; }
  void* ADDCALL sass_importer_get_cookie(struct SassImporterCpp* cb) { return cb->cookie; }

  // Just in case we have some stray import structs
  void ADDCALL sass_delete_importer (struct SassImporterCpp* importer)
  {
    delete importer;
  }

  // Creator for sass custom importer function list
  struct SassImporterListCpp* ADDCALL sass_make_importer_list()
  {
    return new SassImporterListCpp{};
  }

  // Deallocator for the allocated memory
  void ADDCALL sass_delete_importer_list(struct SassImporterListCpp* list)
  {
    if (list == nullptr) return;
    for (auto importer : *list) {
      sass_delete_importer(importer);
    }
    delete list;
  }

  // Creator for a single import entry returned by the custom importer inside the list
  // We take ownership of the memory for source and srcmap (freed when context is destroyed)
  struct SassImportCpp* ADDCALL sass_make_import(const char* imp_path, const char* abs_path, char* source, char* srcmap, enum Sass_Import_Type type)
  {
    struct SassImportCpp* v = new SassImportCpp();
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

  // Upgrade a normal import entry to throw an error (original path can be re-used by error reporting)
  struct SassImportCpp* ADDCALL sass_import_set_error(struct SassImportCpp* import, const char* error, uint32_t line, uint32_t col)
  {
    if (import == 0) return 0;
    import->error = error ? error : "";
    import->line = line ? line : -1;
    import->column = col ? col : -1;
    return import;
  }

  // Setters and getters for entries on the import list
  // void ADDCALL sass_import_set_list_entry(struct SassImportListCpp* list, uint32_t idx, struct SassImportCpp* entry) { list[idx] = entry; }
  // struct SassImportCpp* ADDCALL sass_import_get_list_entry(struct SassImportListCpp* list, uint32_t idx) { return list[idx]; }

  // Just in case we have some stray import structs
  void ADDCALL sass_delete_import(struct SassImportCpp* import)
  {
    delete import;
  }

  // Getter for callee entry
  const char* ADDCALL sass_callee_get_name(struct SassCalleeCpp* entry) { return entry->name; }
  const char* ADDCALL sass_callee_get_path(struct SassCalleeCpp* entry) { return entry->path; }
  uint32_t ADDCALL sass_callee_get_line(struct SassCalleeCpp* entry) { return entry->line; }
  uint32_t ADDCALL sass_callee_get_column(struct SassCalleeCpp* entry) { return entry->column; }
  enum Sass_Callee_Type ADDCALL sass_callee_get_type(struct SassCalleeCpp* entry) { return entry->type; }
  
  // Getters and Setters for environments (lexical, local and global)
  struct SassValue* ADDCALL sass_env_get_lexical (struct SassCompiler* compiler, const char* name) {
    return 0; // compiler->cpp_ctx->varRoot.getLexicalVariable(Sass::EnvKey(name))->toSassValue();
  }

  void ADDCALL sass_env_set_lexical (struct SassCompiler* compiler, const char* name, struct SassValue* val) {
    // compiler->cpp_ctx->varRoot.setLexicalVariable(Sass::EnvKey(name), reinterpret_cast<Value*>(val));
  }

  struct SassValue* ADDCALL sass_env_get_local (struct SassCompiler* compiler, const char* name) {
    return 0; // compiler->cpp_ctx->varRoot.getLocalVariable(Sass::EnvKey(name))->toSassValue();
  }

  void ADDCALL sass_env_set_local (struct SassCompiler* compiler, const char* name, struct SassValue* val) {
    // compiler->cpp_ctx->varRoot.setLocalVariable(Sass::EnvKey(name), reinterpret_cast<Value*>(val));
  }

  struct SassValue* ADDCALL sass_env_get_global (struct SassCompiler* compiler, const char* name) {
    return 0; // compiler->cpp_ctx->varRoot.getGlobalVariable(Sass::EnvKey(name))->toSassValue();
  }

  void ADDCALL sass_env_set_global (struct SassCompiler* compiler, const char* name, struct SassValue* val) {
    // compiler->cpp_ctx->varRoot.setGlobalVariable(Sass::EnvKey(name), reinterpret_cast<Value*>(val));
  }

  // Getter for import entry
  const char* ADDCALL sass_import_get_imp_path(struct SassImportCpp* entry) { return entry->srcdata->getImpPath(); }
  const char* ADDCALL sass_import_get_abs_path(struct SassImportCpp* entry) { return entry->srcdata->getAbsPath(); }
  const char* ADDCALL sass_import_get_source(struct SassImportCpp* entry) { return entry->srcdata->content(); }
  const char* ADDCALL sass_import_get_srcmap(struct SassImportCpp* entry) { return entry->srcdata->srcmaps(); }
  enum Sass_Import_Type ADDCALL sass_import_get_type(struct SassImportCpp* entry) { return entry->srcdata->getType();  }

  // Getter for import error entry
  uint32_t ADDCALL sass_import_get_error_line(struct SassImportCpp* entry) { return entry->line; }
  uint32_t ADDCALL sass_import_get_error_column(struct SassImportCpp* entry) { return entry->column; }
  const char* ADDCALL sass_import_get_error_message(struct SassImportCpp* entry) { return entry->error.empty() ? 0 : entry->error.c_str(); }

  // Explicit functions to take ownership of the memory
  // Resets our own property since we do not know if it is still alive
  // char* ADDCALL sass_import_take_source(struct SassImportCpp* entry) { char* ptr = entry->source; entry->source = 0; return ptr; }
  // char* ADDCALL sass_import_take_srcmap(struct SassImportCpp* entry) { char* ptr = entry->srcmap; entry->srcmap = 0; return ptr; }

}
