/*****************************************************************************/
/* Part of LibSass, released under the MIT license (See LICENSE.txt).        */
/*****************************************************************************/
#ifndef SASS_VARIABLE_H
#define SASS_VARIABLE_H

#include <sass/base.h>

#ifdef __cplusplus
extern "C" {
#endif

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Getter for lexical variable (lexical to scope where function is called).
  // Note: C-API function can only access existing variables and not create new ones!
  ADDAPI struct SassValue* ADDCALL sass_env_get_lexical (struct SassCompiler*, const char*);

  // Setter for lexical variable (lexical to scope where function is called).
  // Note: C-API function can only access existing variables and not create new ones!
  ADDAPI void ADDCALL sass_env_set_lexical(struct SassCompiler*, const char*, struct SassValue*);

  // Getter for local variable (local only to scope where function is called).
  // Note: C-API function can only access existing variables and not create new ones!
  ADDAPI struct SassValue* ADDCALL sass_env_get_global (struct SassCompiler*, const char*);

  // Setter for local variable (local only to scope where function is called).
  // Note: C-API function can only access existing variables and not create new ones!
  ADDAPI void ADDCALL sass_env_set_global(struct SassCompiler*, const char*, struct SassValue*);

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
} // EO extern "C".
#endif

#endif
