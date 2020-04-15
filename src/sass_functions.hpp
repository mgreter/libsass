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
  SassFunctionLambdaCpp function;
  void* cookie;
};

struct SassFunctionListCpp : Sass::sass::vector<SassFunctionCpp*> {

};

struct SassImporterListCpp : Sass::sass::vector<SassImporterPtr> {

};

struct SassImportListCpp : Sass::sass::vector<SassImportPtr> {

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
  SassImporterLambdaCpp importer;
  double           priority;
  void*            cookie;
};

#endif
