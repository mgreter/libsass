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

typedef struct SassImportCpp* Sass_Import_Entry;
typedef struct SassImporterCpp* Sass_Importer_Entry;
typedef struct SassFunctionCpp* Sass_Function_Entry;
// 
typedef struct SassImportListCpp* Sass_Import_List;
typedef struct SassImporterListCpp* Sass_Importer_List;
typedef struct SassFunctionListCpp* Sass_Function_List;
// 
// struct SassOptionsCpp;
// // struct SassCompilerCpp;

#define Sass_Options SassOptionsCpp
#define Sass_Compiler SassCompiler

#endif
