#ifndef SASS_SASS_FUNCTIONS_H
#define SASS_SASS_FUNCTIONS_H

#include "sass.h"
#include "environment.hpp"
#include "fn_utils.hpp"
#include "backtrace.hpp"
#include "source.hpp"

// Struct to hold custom function callback
struct SassFunctionCpp {
  Sass::sass::string signature;
  Sass_Function_Fn function;
  void* cookie;
};

struct Sass_Function_List2 : Sass::sass::vector<SassFunctionCpp*> {

};

struct Sass_Importer_List2 : Sass::sass::vector<Sass_Importer_Entry> {

};

struct Sass_Import_List2 : Sass::sass::vector<SassImportPtr> {

};

class SourceDataObj;

// External import entry
struct SassImportCpp {
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
struct SassCalleeCpp {
  const char* name;
  const char* path;
  uint32_t line;
  uint32_t column;
  enum Sass_Callee_Type type;
};

// Struct to hold importer callback
struct SassImporterCpp {
  Sass_Importer_Fn importer;
  double           priority;
  void*            cookie;
};

#endif
