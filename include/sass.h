#ifndef SASS_H
#define SASS_H

// #define DEBUG 1

// Note: we can't forward declare with inheritance
// https://stackoverflow.com/a/10145303/1550314

// include API headers
#include <sass/base.h>
#include <sass/version.h>
#include <sass/values.h>
#include <sass/functions.h>
#include <sass/context.h>

typedef SassImportPtr Sass_Import_Entry;
typedef SassImporterPtr Sass_Importer_Entry;
typedef SassFunctionPtr Sass_Function_Entry;

typedef SassImportListPtr Sass_Import_List;
typedef SassImporterListPtr Sass_Importer_List;
typedef SassFunctionListPtr Sass_Function_List;

struct SassOptionsCpp;
struct SassCompilerCpp;

#define Sass_Options SassOptionsCpp
#define Sass_Compiler SassCompilerCpp

#endif
