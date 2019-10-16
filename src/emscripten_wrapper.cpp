#include <cstdlib>
#include <cstring>
#include "iostream"
#include "file.hpp"
#include "sass/context.h"
#include "emscripten_wrapper.hpp"
#include <emscripten.h>

void sass_compile_emscripten(
  char *input
) {

  std::cerr << "Compiling [" << input << "]\n";

  // const char* input = "bolt-bench/bolt-bench.scss";

  // create the file context and get all related structs
  struct Sass_File_Context* file_ctx = sass_make_file_context(input);
  struct Sass_Context* ctx = sass_file_context_get_context(file_ctx);
  struct Sass_Options* ctx_opt = sass_context_get_options(ctx);

  sass_option_push_include_path(ctx_opt, "bolt-bench");

  // context is set up, call the compile step now
  int status = sass_compile_file_context(file_ctx);

  // print the result or the error to the stdout
  if (status == 0) puts("Successfully compiled\n");
  else puts(sass_context_get_error_message(ctx));

  // release allocated memory
  sass_delete_file_context(file_ctx);

}

Sass_Import_List sass_importer_emscripten(
  const char* cur_path,
  Sass_Importer_Entry cb,
  struct Sass_Compiler* comp
) {
  struct Sass_Import* previous = sass_compiler_get_last_import(comp);
  const char* prev_base = sass_import_get_abs_path(previous);
  // struct Sass_Context* ctx = sass_compiler_get_context(comp);
  // struct Sass_Options* opts = sass_context_get_options(ctx);

  // flag denoting if JS importer callback has completed
  bool done = false;

  // kick off the (potentially) asynchronous JS importer callback
  // NOTE: there can only be one importer running concurrently
  EM_ASM_INT({
    Importer.find(
      pointerToString($0),
      pointerToString($1)
    );
  }, cur_path, prev_base);


  // check if the JS importer callback has already finished,
  // otherwise abuse emscripten_sleep() to interrupt the
  // otherwise synchronous execution of the C function for
  // 20ms to give the (potentially) asynchronous JS importer
  // callback a chance to breathe.
  // see https://github.com/kripken/emscripten/wiki/Emterpreter#emterpreter-async-run-synchronous-code
  while (true) {
    done = (bool)EM_ASM_INT({
      return Number(Importer.finished());
    }, 0);

    if (done) {
      break;
    }

    emscripten_sleep(20);
  }

  // the JS importer callback could have reported an unrecoverable error
  char *error = (char *)EM_ASM_INT({
    return Number(Importer.error());
  }, 0);

  if (error) {
    Sass_Import_List list = sass_make_import_list(1);
    list[0] = sass_make_import_entry(cur_path, 0, 0);
    sass_import_set_error(list[0], strdup(error), 0, 0);
    return list;
  }

  // obtain path and or content provided by the JS importer callback
  char *path = (char *)EM_ASM_INT({
    return Number(Importer.path());
  }, 0);
  char *content = (char *)EM_ASM_INT({
    return Number(Importer.content());
  }, 0);

  if (content || path) {
    Sass_Import_List list = sass_make_import_list(1);
    // TODO: figure out if strdup() is really required
    list[0] = sass_make_import_entry(strdup(path ? path : cur_path), content ? strdup(content) : 0, 0);
    return list;
  }

  // JS importer callback did not find anything, nor did it report an error,
  // have the next importer (likely libsass default file loader) determine
  // how to proceed
  return NULL;
}
