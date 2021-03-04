
## LibSass custom importer C-API

The custom importer C-API allows implementors to decide how `@import` and similar
rules (e.g. `@use`) are handled with given sass code. Custom importers will we called
once we try to resolve an import rule. Without any custom importer LibSass will try
to resolve the import by looking into all include directories. Custom importers will
be invoked before this happens, if any are registered. LibSass passes the current
path that should be imported and the resolved path to the parent import, in order
for the custom importer to resolve relative imports to the parent.

The custom importer must return a pointer to a `SassImportList`, with which it can
represent three different states. It can return a `nullptr`, which will let LibSass
execute the next custom importer. It can return an empty `SassImportList`, which will
mean that the import is consumed, but nothing is really imported. Last it can return
one or more entries in the `SassImportList`. That list consists of `SassImport` entries,
which can either be just rewritten paths, or also already have fully loaded content.

This feature is still considered experimental as we don't exactly follow the official
Sass implementation here.

## C-API for `SassImportList`

You have to return a list of imports, since some importers may want to import multiple
files from one import statement (ie. a glob/star importer). The memory you pass with source and
srcmap is taken over by LibSass and freed automatically when the import is done. You are also
allowed to return `0` or `nullptr` instead of a list, which will tell LibSass to handle the
import by itself (as if no custom importer was in use).

The C-API for `SassImportList` is designed like a FiFo queue (first in, first out). The C-API
only allows to push or shift items from the list.

```C
Sass_Import_Entry* rv = sass_make_import_list(1);
rv[0] = sass_make_import(rel, abs, source, srcmap);
```

Every import will then be included in LibSass. You are allowed to only return a file path
without any loaded source. This way you can ie. implement rewrite rules for import paths
and leave the loading part for LibSass.

Please note that LibSass doesn't use the srcmap parameter yet. It has been added to not
deprecate the C-API once support has been implemented. It will be used to re-map the
actual sourcemap with the provided ones.

### Basic Usage

```C
#include <sass/functions.h>
```

## Sass Importer API

```C
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

// Type definitions for importer functions
typedef struct SassImportList* (*SassImporterLambda)(
  const char* url, struct SassImporter* cb, struct SassCompiler* compiler);

/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

// Create custom importer (with arbitrary data pointer called `cookie`)
// The pointer is often used to store the callback into the actual binding.
ADDAPI struct SassImporter* ADDCALL sass_make_importer(SassImporterLambda lambda, double priority, void* cookie);

// Deallocate the importer and release memory
ADDAPI void ADDCALL sass_delete_importer(struct SassImporter* cb);

/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

// Getter for importer lambda function (the one being actually invoked)
ADDAPI SassImporterLambda ADDCALL sass_importer_get_lambda(struct SassImporter* cb);

// Getter for importer priority (lowest priority is invoked first)
ADDAPI double ADDCALL sass_importer_get_priority(struct SassImporter* cb);

// Getter for arbitrary cookie (used by implementers to store stuff)
ADDAPI void* ADDCALL sass_importer_get_cookie(struct SassImporter* cb);

/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
```

### More links

- [Sass Importer Example](api-importer-example.md)
- [Sass Importer Internal](api-importer-internal.md)

