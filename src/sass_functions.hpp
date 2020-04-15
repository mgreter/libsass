#ifndef SASS_SASS_FUNCTIONS_H
#define SASS_SASS_FUNCTIONS_H

#include "sass.h"
#include "environment.hpp"
#include "fn_utils.hpp"
#include "backtrace.hpp"
#include "source.hpp"

// Struct to hold custom function callback
struct Sass_Function {
  Sass::sass::string signature;
  Sass_Function_Fn function;
  void* cookie;
};

struct Sass_Function_List2 : Sass::sass::vector<Sass_Function*> {

};

class SourceDataObj;

// External import entry
struct Sass_Import {
  //char* imp_path; // path as found in the import statement
  // char *abs_path; // path after importer has resolved it
  // char* source;
  //char* srcmap;
  // error handling
  Sass::sass::string error;
  uint32_t line;
  uint32_t column;
  // the import type
  // Sass_Import_Type type;
  Sass::SourceDataObj srcdata;
};

// External call entry
struct Sass_Callee {
  const char* name;
  const char* path;
  uint32_t line;
  uint32_t column;
  enum Sass_Callee_Type type;
};

// Struct to hold importer callback
struct Sass_Importer {
  Sass_Importer_Fn importer;
  double           priority;
  void*            cookie;
};

#endif
