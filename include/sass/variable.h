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
  // Returns true if variable was set or false if it does not exist (we can't create it)
  // Note: C-API function can only access existing variables and not create new ones!
  ADDAPI bool ADDCALL sass_env_set_lexical(struct SassCompiler*, const char*, struct SassValue*);

  // Getter for global variable (only variables on the root scope are considered).
  // Note: C-API function can only access existing variables and not create new ones!
  ADDAPI struct SassValue* ADDCALL sass_env_get_global (struct SassCompiler*, const char*);

  // Setter for global variable (only variables on the root scope are considered).
  // Returns true if variable was set or false if it does not exist (we can't create it)
  // Note: C-API function can only access existing variables and not create new ones!
  ADDAPI bool ADDCALL sass_env_set_global(struct SassCompiler*, const char*, struct SassValue*);

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
} // EO extern "C".
#endif

#endif
