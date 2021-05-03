## Introduction

LibSass C-API is designed as a functional API; every access on the C-API is a
function call. This has a few drawbacks, but a lot of desirable characteristics.
The most important one is that it reduces the API surface to simple function calls.
Implementors do not need to know how internal structures look, so we are free
to adjust them as we see fit, as long as the functional API stays the same.

Under the hood, LibSass uses advanced C++ code (c++11 being the current target), so
you will need a modern compiler and also link against c++ runtime libraries. This poses
some issues in regard of portability and deployment of precompiled binary distributions.

The API has been split into a few categories which come with their own documentation:

- [Sass Basics](api-basics.md) - Further details and information about the C-API
- [Sass Compiler](api-compiler.md) - Trigger and handle the main Sass compilation
- [Sass Imports](api-import.md) - Imports to be loaded and parsed by LibSass
- [Sass Values](api-value.md) - Exchange Sass values between LibSass and implementors
- [Sass Functions](api-function.md) - Get invoked by LibSass for function statements
- [Sass Importers](api-importer.md) - Get invoked by LibSass for @import statements
- [Sass Variables](api-variable.md) - Query or update existing scope variables
- [Sass Traces](api-traces.md) - Access to traces for debug information
- [Sass Error](api-error.md) - Access to errors for debug information

### Basic usage

First you will need to include the header file!
This will automatically load all LibSass headers!

```C
#include <sass.h>
```

## Basic C Example

```C
#include <stdio.h>
#include <sass.h>

int main() {
  puts(libsass_version());
  return 0;
}
```

### Compile and link against LibSass

```bash
gcc -Wall version.c -lsass -o version && ./version
```

## More C Examples

- [Sample code for Sass Compiler](api-compiler-example.md)
- [Sample code for Sass Value](api-value-example.md)
- [Sample code for Sass Function](api-function-example.md)
- [Sample code for Sass Importer](api-importer-example.md)

## Compiling a stylesheet

LibSass parsing starts with an entry point, which can either be a
file or some text you provide directly. Relative includes are
resolved against the current working directory.

`struct SassImport* data = sass_make_content_import(text, "styles.scss");`
`struct SassImport* data = sass_make_file_import("styles.scss");`

**Building a compiler**

  sass_compiler_set_entry_point(compiler, entrypoint);
  // entry point now passed to compiler, so its reference count was increased
  // in order to not leak memory we must release our own usage (usage after is UB)
  sass_delete_import(entrypoint); // decrease ref-count

  // Execute compiler and print/write results
  sass_compiler_execute(compiler, false);

  // Get result code after all compilation steps
  int status = sass_compiler_get_status(compiler);

  // Clean-up compiler, we're done
  sass_delete_compiler(compiler);

  // exit status
  return status;

## Memory handling and life-cycles

The C-API mandates that you have one delete call for every make call. Internally
LibSass often utilizes reference counting, but you still need to call the appropriate
`sass_free` function for every object you own. APIs that return a `char*` also need
the returned memory to be freed by `sass_free_c_string`. Some APIs also allow
implementors to take over ownership of some data. Otherwise this data is attached
to the life-time of the main Compiler object. Reference counted objects will stay
alive, even after you've called `sass_delete_compiler`.

```C
// Allocate a memory block on the heap of (at least) [size].
// Make sure to release to acquired memory at some later point via
// `sass_free_memory`. You need to go through my utility function in
// case your code and my main program don't use the same memory manager.
void* sass_alloc_memory(size_t size);

// Allocate a memory block on the heap and copy [string] into it.
// Make sure to release to acquired memory at some later point via
// `sass_free_memory`. You need to go through my utility function in
// case your code and my main program don't use the same memory manager.
char* sass_copy_c_string(const char* str);

// Deallocate libsass heap memory
void sass_free_memory(void* ptr);
void sass_free_c_string(char* ptr);
```

## Miscellaneous API functions

```C
// Get compiled libsass version
const char* libsass_version(void);

// Implemented sass language version
// Hardcoded version 3.9 for time being
const char* libsass_language_version(void);
```

## Common Pitfalls

## Error Codes

The `error_code` is integer value which indicates the type of error that
occurred inside the LibSass process. Following is the list of error codes along
with the short description:

* 1: normal errors like parsing or `eval` errors
* 2: bad allocation error (memory error)
* 3: "untranslated" C++ exception (`throw std::exception`)
* 4: legacy string exceptions ( `throw const char*` or `sass::string` )
* 5: Some other unknown exception

Although for the API consumer, error codes do not offer much value except
indicating whether *any* error occurred during the compilation, it helps
debugging the LibSass internal code paths.

## Real-World Implementations

The proof is in the pudding, so we have highlighted a few implementations that
should be on par with the latest LibSass interface version. Some of them may not
have all features implemented!

1. [Perl Example](https://github.com/sass/perl-libsass/blob/master/Sass.xs)
2. [Go Example](https://godoc.org/github.com/wellington/go-libsass#example-Compiler--Stdin)
3. [Node Example](https://github.com/sass/node-sass/blob/master/src/binding.cpp)

## Plugins (experimental)

LibSass can load plugins from directories. Just define `plugin_path` on context
options to load all plugins from the directories. To implement plugins, please
consult the following example implementations.

- https://github.com/mgreter/libsass-glob
- https://github.com/mgreter/libsass-math
- https://github.com/mgreter/libsass-digest
