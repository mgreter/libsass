/*****************************************************************************/
/* Part of LibSass, released under the MIT license (See LICENSE.txt).        */
/*****************************************************************************/
#ifndef SASS_CAPI_VALUES_HPP
#define SASS_CAPI_VALUES_HPP

// sass.hpp must go before all system headers
// to get the __EXTENSIONS__ fix on Solaris.
#include "capi_sass.hpp"

  // Getter for callee entry
  // const char* ADDCALL sass_callee_get_name(struct SassCallee* entry) { return entry->name; }
  // const char* ADDCALL sass_callee_get_path(struct SassCallee* entry) { return entry->path; }
  // uint32_t ADDCALL sass_callee_get_line(struct SassCallee* entry) { return entry->line; }
  // uint32_t ADDCALL sass_callee_get_column(struct SassCallee* entry) { return entry->column; }
  // enum Sass_Callee_Type ADDCALL sass_callee_get_type(struct SassCallee* entry) { return entry->type; }


  // Explicit functions to take ownership of the memory
  // Resets our own property since we do not know if it is still alive
  // char* ADDCALL sass_import_take_source(struct SassImport* entry) { char* ptr = entry->source; entry->source = 0; return ptr; }
  // char* ADDCALL sass_import_take_srcmap(struct SassImport* entry) { char* ptr = entry->srcmap; entry->srcmap = 0; return ptr; }

#endif
