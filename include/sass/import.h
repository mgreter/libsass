#ifndef SASS_C_IMPORT_H
#define SASS_C_IMPORT_H

#include <sass/base.h>

#ifdef __cplusplus
extern "C" {
#endif

  // Create an import entry by reading from `stdin`
  ADDAPI struct SassImport* ADDCALL sass_make_stdin_import();

  // Crate an import entry for the passed data content
  ADDAPI struct SassImport* ADDCALL sass_make_data_import(char* content);

  // Crate an import entry for the passed input path
  ADDAPI struct SassImport* ADDCALL sass_make_file_import(struct SassCompiler* compiler, const char* imp_path);

  // Set specific import format for the given import
  ADDAPI void ADDCALL sass_import_set_format(struct SassImport* import, enum SassImportFormat format);

#ifdef __cplusplus
} // __cplusplus defined.
#endif

#endif
