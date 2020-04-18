#ifndef SASS_SASS_FUNCTIONS_H
#define SASS_SASS_FUNCTIONS_H

#include "sass.h"
#include "environment.hpp"
#include "fn_utils.hpp"
#include "backtrace.hpp"
#include "source.hpp"

// Some structures are simple c++ vectors.
// There might be a more efficient way to achieve this?
// Although compiler optimization should see this case easily!
struct SassImportListCpp : std::deque<SassImportPtr> {};
struct SassImporterListCpp : std::deque<struct SassImporterCpp*> {};
struct SassFunctionListCpp : std::deque<SassFunctionCpp*> {};

// Struct to hold custom function callback
struct SassFunctionCpp {
  Sass::sass::string signature;
  SassFunctionLambdaCpp function;
  void* cookie;
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
