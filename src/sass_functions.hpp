#ifndef SASS_SASS_FUNCTIONS_H
#define SASS_SASS_FUNCTIONS_H

#include "sass.h"
#include <sass/base.h>
#include <sass/fwdecl.h>

#include "environment.hpp"
#include "fn_utils.hpp"
#include "backtrace.hpp"
#include "source.hpp"
#include "sass_lists.hpp"

// Struct to hold custom function callback
struct SassFunction {
  sass::string signature;
  SassFunctionLambda function;
  void* cookie;
};

class SourceDataObj;

// External import entry
struct SassImport {
  enum SassImportFormat format;
  Sass::SourceDataObj srcdata;
};

// Struct to hold importer callback
struct SassImporter {
  SassImporterLambda importer;
  double           priority;
  void*            cookie;
};

#endif
