#ifndef SASS_C_BASE_H
#define SASS_C_BASE_H

#ifdef _MSC_VER
  #pragma warning(disable : 4503)
  #ifndef _SCL_SECURE_NO_WARNINGS
    #define _SCL_SECURE_NO_WARNINGS
  #endif
  #ifndef _CRT_SECURE_NO_WARNINGS
    #define _CRT_SECURE_NO_WARNINGS
  #endif
  #ifndef _CRT_NONSTDC_NO_DEPRECATE
    #define _CRT_NONSTDC_NO_DEPRECATE
  #endif
#endif

// Work around lack of `noexcept` keyword support in VS2013
#if defined(_MSC_VER) && (_MSC_VER <= 1800) && !defined(_ALLOW_KEYWORD_MACROS)
#define _ALLOW_KEYWORD_MACROS 1
#define noexcept throw( )
#endif

// Load some POD types
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __GNUC__
  #define DEPRECATED(func) func __attribute__ ((deprecated))
#elif defined(_MSC_VER)
  #define DEPRECATED(func) __declspec(deprecated) func
#else
  #pragma message("WARNING: You need to implement DEPRECATED for this compiler")
  #define DEPRECATED(func) func
#endif

#ifdef _WIN32

  /* You should define ADD_EXPORTS *only* when building the DLL. */
  #ifdef ADD_EXPORTS
    #define ADDAPI __declspec(dllexport)
    #define ADDCALL __cdecl
  #else
    #define ADDAPI
    #define ADDCALL
  #endif

#else /* _WIN32 not defined. */

  /* Define with no value on non-Windows OSes. */
  #define ADDAPI
  #define ADDCALL

#endif

#ifdef __cplusplus
extern "C" {
#endif

  // Change the virtual current working directory
  // You should probably not really use this function!
  ADDAPI void ADDCALL sass_chdir(const char* path);

  // Prints message to stderr with color for windows
  ADDAPI void ADDCALL sass_print_stdout(const char* message);
  ADDAPI void ADDCALL sass_print_stderr(const char* message);

  // Resolve a file via the given include paths in the sass option struct
  // find_file looks for the exact file name while find_include does a regular sass include
  // ADDAPI char* ADDCALL sass_find_file (const char* path, struct SassOptionsCpp* opt);
  // ADDAPI char* ADDCALL sass_find_include (const char* path, struct SassOptionsCpp* opt);

  // Resolve a file relative to last import or include paths in the sass option struct
  // find_file looks for the exact file name while find_include does a regular sass include
  // ADDAPI char* ADDCALL sass_compiler_find_file(const char* path, struct SassCompiler* compiler);
  // ADDAPI char* ADDCALL sass_compiler_find_include(const char* path, struct SassCompiler* compiler);

  // Allocate and release generic memory buffers
  ADDAPI void* ADDCALL sass_alloc_memory(size_t size);
  ADDAPI void ADDCALL sass_free_memory(void* ptr);

  // Allocate and release string (byte) buffer
  ADDAPI char* ADDCALL sass_copy_c_string(const char* str);
  ADDAPI void ADDCALL sass_free_c_string(char* ptr);

  // Some convenient string helper function
  ADDAPI char* ADDCALL sass_string_quote(const char* str, const char quote_mark);
  ADDAPI char* ADDCALL sass_string_unquote(const char* str);

  // Return implemented sass language version
  ADDAPI const char* ADDCALL libsass_version(void);

  // Return the compiled libsass language (hard-coded)
  // This is hard-coded with the library on compilation!
  ADDAPI const char* ADDCALL libsass_language_version(void);

#ifdef __cplusplus
} // __cplusplus defined.
#endif

// Include forward declarations
#include <sass/fwdecl.h>

// Include enumerations
#include <sass/enums.h>

#endif
