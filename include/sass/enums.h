#ifndef SASS_C_ENUMS_H
#define SASS_C_ENUMS_H

#ifdef __cplusplus
extern "C" {
#endif

// Different render styles
enum Sass_Output_Style {
  SASS_STYLE_NESTED,
  SASS_STYLE_EXPANDED,
  SASS_STYLE_COMPACT,
  SASS_STYLE_COMPRESSED,
  // only used internally
  SASS_STYLE_INSPECT,
  SASS_STYLE_TO_CSS
};

// Type of parser to use
enum Sass_Import_Type {
  SASS_IMPORT_AUTO,
  SASS_IMPORT_SCSS,
  SASS_IMPORT_SASS,
  SASS_IMPORT_CSS,
};

// Type of function calls
enum Sass_Callee_Type {
  SASS_CALLEE_MIXIN,
  SASS_CALLEE_FUNCTION,
  SASS_CALLEE_C_FUNCTION,
};

#define SASS_LOGGER_MONO 1
#define SASS_LOGGER_COLOR 2
#define SASS_LOGGER_ASCII 4
#define SASS_LOGGER_UNICODE 8

// Logging style
enum Sass_Logger_Style {
  SASS_LOGGER_AUTO = 0,
  SASS_LOGGER_ASCII_MONO = SASS_LOGGER_ASCII | SASS_LOGGER_MONO,
  SASS_LOGGER_ASCII_COLOR = SASS_LOGGER_ASCII | SASS_LOGGER_COLOR,
  SASS_LOGGER_UNICODE_MONO = SASS_LOGGER_UNICODE | SASS_LOGGER_MONO,
  SASS_LOGGER_UNICODE_COLOR = SASS_LOGGER_UNICODE | SASS_LOGGER_COLOR,
};

#ifdef __cplusplus
} // __cplusplus defined.
#endif

#endif
