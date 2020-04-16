#ifndef SASS_C_FWDECL_H
#define SASS_C_FWDECL_H

#ifdef __cplusplus
extern "C" {
#endif

// Forward declare structs
struct SassCalleeCpp;
struct SassImportCpp;
struct SassOptionsCpp;
struct SassImporterCpp;
struct SassFunctionCpp;
struct SassCompilerCpp;
struct SassOptionsCpp; // base struct
struct SassContextCpp; // : SassOptionsCpp
struct SassFileContextCpp; // : SassContextCpp
struct SassDataContextCpp; // : SassContextCpp

// Typedef helpers for pointer returns
typedef struct SassCalleeCpp* (SassCalleePtr);
typedef struct SassImportCpp* (SassImportPtr);
typedef struct SassImporterCpp* (SassImporterPtr);
typedef struct SassFunctionCpp* (SassFunctionPtr);
typedef struct SassImportListCpp* (SassImportListPtr);
typedef struct SassImporterListCpp* (SassImporterListPtr);
typedef struct SassFunctionListCpp* (SassFunctionListPtr);
typedef struct SassCompilerCpp* (SassCompilerPtr);
typedef struct SassOptionsCpp* (SassOptionsPtr);
typedef struct SassContextCpp* (SassContextPtr);
typedef struct SassFileContextCpp* (SassFileContextPtr);
typedef struct SassDataContextCpp* (SassDataContextPtr);

// Typedef defining importer/function callback signature and return type
typedef SassImportListPtr(*SassImporterLambdaCpp)(const char* url, SassImporterPtr cb, SassCompilerPtr compiler);
typedef struct SassValue* (*SassFunctionLambdaCpp)(struct SassValue*, SassFunctionPtr cb, SassCompilerPtr compiler);

#ifdef __cplusplus
} // __cplusplus defined.
#endif

#endif
