
#ifndef _EMSCRIPTEN_WRAPPER_H
#define _EMSCRIPTEN_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "sass.h"

void sass_compile_emscripten(
  const char *input
);

struct Sass_Import** sass_importer_emscripten(
  const char* cur_path, Sass_Importer_Entry cb,
  struct Sass_Compiler* comp
);

#ifdef __cplusplus
}
#endif


#endif
