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
struct SassImportList : std::deque<struct SassImport*> {};
struct SassImporterList : std::deque<struct SassImporter*> {};
struct SassFunctionList : std::deque<struct SassFunction*> {};

// Struct to hold custom function callback
struct SassFunction {
  sass::string signature;
  SassFunctionLambda function;
  void* cookie;
};

class SourceDataObj;

// External import entry
struct SassImport {
  //char* imp_path; // path as found in the import statement
  // char *abs_path; // path after importer has resolved it
  // char* source;
  //char* srcmap;
  // error handling
  sass::string error;
  uint32_t line;
  uint32_t column;
  enum SassImportFormat format;
  // the import type
  // Sass_Import_Type type;
  Sass::SourceDataObj srcdata;
};

// External call entry
struct SassCallee {
  const char* name;
  const char* path;
  uint32_t line;
  uint32_t column;
  enum Sass_Callee_Type type;
};

// Struct to hold importer callback
struct SassImporter {
  SassImporterLambda importer;
  double           priority;
  void*            cookie;
};

#endif
