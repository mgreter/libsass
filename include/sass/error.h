#ifndef SASS_C_ERROR_H
#define SASS_C_ERROR_H

#include <sass/base.h>

#ifdef __cplusplus
extern "C" {
#endif

  // Error related getters (use after compiler was rendered)
  ADDAPI int ADDCALL sass_error_get_status(struct SassError* error);
  ADDAPI char* ADDCALL sass_error_get_json(struct SassError* error);
  ADDAPI const char* ADDCALL sass_error_get_what(struct SassError* error);
  ADDAPI const char* ADDCALL sass_error_get_messages(struct SassError* error);
  ADDAPI const char* ADDCALL sass_error_get_warnings(struct SassError* error);
  ADDAPI const char* ADDCALL sass_error_get_formatted(struct SassError* error);

#ifdef __cplusplus
} // __cplusplus defined.
#endif

#endif
