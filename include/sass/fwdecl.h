/*****************************************************************************/
/* Part of LibSass, released under the MIT license (See LICENSE.txt).        */
/*****************************************************************************/
#ifndef SASS_FWDECL_H
#define SASS_FWDECL_H

#include <sass/base.h>

#ifdef __cplusplus
extern "C" {
#endif

  // Forward declare anonymous structs
  // C never sees any implementation
  struct SassTrace;
  struct SassError;
  struct SassCompiler;
  struct SassFunction;
  struct SassSource;
  struct SassSrcSpan;
  struct SassImport;
  struct SassImporter;
  struct SassImportList;
  struct SassMapIterator;

#ifdef __cplusplus
} // __cplusplus defined.
#endif

#endif
