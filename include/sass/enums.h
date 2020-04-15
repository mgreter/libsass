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
  // only used internaly
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

#ifdef __cplusplus
} // __cplusplus defined.
#endif

#endif
