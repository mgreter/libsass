#ifndef SASS_C_FWDECL_H
#define SASS_C_FWDECL_H

#ifdef __cplusplus
extern "C" {
#endif

// Forward declare structs
struct SassError;
struct SassCallee;
struct SassCompiler;
struct SassFunction;

struct SassImport;
struct SassImporter;

struct SassImportList;
struct SassImporterList;
struct SassFunctionList;

// Typedef defining importer/function callback signature and return type
typedef struct SassImportList* (*SassImporterLambda)(const char* url, struct SassImporter* cb);
typedef struct SassValue* (*SassFunctionLambda)(struct SassValue*, struct SassFunction* cb, struct SassCompiler* compiler);


#ifdef __cplusplus
} // __cplusplus defined.
#endif

#endif
