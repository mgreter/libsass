// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include <sass/base.h>
#include "sass_lists.hpp"

namespace Sass {

  extern "C" {

    /*#########################################################################*/
    /*#########################################################################*/

    struct SassImportList* ADDCALL sass_make_import_list()
    {
      return new SassImportList{};
    }

    void ADDCALL sass_delete_import_list(struct SassImportList* list)
    {
      if (list == nullptr) return;
      for (auto import : *list) {
        sass_delete_import(import);
      }
      delete list;
    }

    size_t ADDCALL sass_import_list_size(struct SassImportList* list)
    {
      return list == nullptr ? 0 : list->size();
    }

    struct SassImport* ADDCALL sass_import_list_shift(struct SassImportList* list)
    {
      if (list == nullptr) return nullptr;
      if (list->empty()) return nullptr;
      auto ptr = list->front();
      list->pop_front();
      return ptr;
    }

    void ADDCALL sass_import_list_push(struct SassImportList* list, struct SassImport* import)
    {
      if (list == nullptr) return;
      list->push_back(import);
    }

    /*#########################################################################*/
    /*#########################################################################*/

    struct SassFunctionList* ADDCALL sass_make_function_list()
    {
      return new SassFunctionList{};
    }

    void ADDCALL sass_delete_function_list(struct SassFunctionList* list)
    {
      if (list == nullptr) return;
      for (auto function : *list) {
        sass_delete_function(function);
      }
      delete list;
    }

    size_t ADDCALL sass_function_list_size(struct SassFunctionList* list)
    {
      return list == nullptr ? 0 : list->size();
    }

    SassFunction* ADDCALL sass_function_list_shift(struct SassFunctionList* list)
    {
      if (list == nullptr) return nullptr;
      if (list->empty()) return nullptr;
      auto ptr = list->front();
      list->pop_front();
      return ptr;
    }

    void ADDCALL sass_function_list_push(struct SassFunctionList* list, struct SassFunction* fn)
    {
      if (list != nullptr) {
        if (fn != nullptr) {
          list->emplace_back(fn);
        }
      }
    }

    /*#########################################################################*/
    /*#########################################################################*/

    struct SassImporterList* ADDCALL sass_make_importer_list()
    {
      return new SassImporterList{};
    }

    void ADDCALL sass_delete_importer_list(struct SassImporterList* list)
    {
      if (list == nullptr) return;
      for (auto importer : *list) {
        sass_delete_importer(importer);
      }
      delete list;
    }

    size_t ADDCALL sass_importer_list_size(struct SassImporterList* list)
    {
      return list == nullptr ? 0 : list->size();
    }

    struct SassImporter* ADDCALL sass_importer_list_shift(struct SassImporterList* list)
    {
      if (list == nullptr) return nullptr;
      if (list->empty()) return nullptr;
      auto ptr = list->front();
      list->pop_front();
      return ptr;
    }

    void ADDCALL sass_importer_list_push(struct SassImporterList* list, struct SassImporter* importer)
    {
      if (list != nullptr) {
        if (importer != nullptr) {
          list->emplace_back(importer);
        }
      }
    }

    /*#########################################################################*/
    /*#########################################################################*/

  }

}
