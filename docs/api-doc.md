## Introduction

LibSass C-API is designed as a functional API; every access on the C-API is a
function call. This has a few drawbacks, but a lot of desirable characteristics.
The most important one is that it reduces the API surface to simple function calls.
Any implementor does not need to know how internal structures look, so we are free
to adjust them as we see fit, as long as the functional API stays the same.

Under the hood, LibSass uses advanced C++ code (c++11 being the current target), so
you will need a modern compiler and also link against c++ runtime libraries. This poses
some issues in regard of portability and deployment of precompiled binary distributions.

The API has been split into a few categories which come with their own documentation:

- [Sass Basics](api-basics.md) - Further details and information about the C-API
- [Sass Compiler](api-compiler.md) - Trigger and handle the main Sass compilation
- [Sass Value](api-value.md) - Exchange values and its format with LibSass
- [Sass Function](api-function.md) - Get invoked by LibSass for function statments
- [Sass Importer](api-importer.md) - Get invoked by LibSass for @import statments
- [Sass Variable](api-variable.md) - Query or update existing scope variables
- [Sass Traces](api-traces.md) - Access to traces for debug information

### Basic usage

First you will need to include the header file!
This will automatically load all other headers too!

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

```bash
gcc -Wall version.c -lsass -o version && ./version
```

## More C Examples

- [Sample code for Sass Context](api-context-example.md)
- [Sample code for Sass Value](api-value-example.md)
- [Sample code for Sass Function](api-function-example.md)
- [Sample code for Sass Importer](api-importer-example.md)

## Compiling your code

LibSass parsing starts with some entry point, which can either be a
file or some code you provide directly. Further relative includes are
resolved against CWD. In order to tell LibSass the entry point, there
are two main ways, either defining the content directly or let LibSass
load a file.

`struct SassImport* data = sass_make_content_import(text, "styles.scss");`
`struct SassImport* data = sass_make_file_import("styles.scss");`

**Building a compiler**

  sass_compiler_set_entry_point(compiler, entrypoint);
  // everything you make you must delete
  sass_delete_import(data); // passed away

  // Execute all three phases
  sass_compiler_parse(compiler);
  sass_compiler_compile(compiler);
  sass_compiler_render(compiler);

  // Print any warning to console
  if (sass_compiler_get_warn_string(compiler)) {
    sass_print_stderr(sass_compiler_get_warn_string(compiler));
  }

  // Print error message if we have an error
  if (sass_compiler_get_status(compiler) != 0) {
    const struct SassError* error = sass_compiler_get_error(compiler);
    sass_print_stderr(sass_error_get_string(error));
  }

  // Get result code after all compilation steps
  int status = sass_compiler_get_status(compiler);

  // Write to output if no errors occurred
  if (status == 0) {

    // Paths where to write stuff to (might be `stream://stdout`)
    const char* outfile = sass_compiler_get_output_path(compiler);
    const char* mapfile = sass_compiler_get_srcmap_path(compiler);
    // Get the parts to be added to the output file (or stdout)
    const char* content = sass_compiler_get_output_string(compiler);
    const char* footer = sass_compiler_get_footer_string(compiler);
    const char* srcmap = sass_compiler_get_srcmap_string(compiler);

    // Output all results
    if (content) puts(content);
    if (footer) puts(footer);

  }

  // exit status
  return status;

## Sass Context Internals

Everything is stored in structs:

```C
struct Sass_Options;
struct Sass_Context : Sass_Options;
struct Sass_File_context : Sass_Context;
struct Sass_Data_context : Sass_Context;
```

This mirrors very well how `libsass` uses these structures.

- `Sass_Options` holds everything you feed in before the compilation. It also hosts
`input_path` and `output_path` options, because they are used to generate/calculate
relative links in source-maps. The `input_path` is shared with `Sass_File_Context`.
- `Sass_Context` holds all the data returned by the compilation step.
- `Sass_File_Context` is a specific implementation that requires no additional fields
- `Sass_Data_Context` is a specific implementation that adds the `input_source` field

Structs can be down-casted to access `context` or `options`!

## Memory handling and life-cycles

We keep memory around for as long as the main [context](api-context.md) object
is not destroyed (`sass_delete_context`). LibSass will create copies of most
inputs/options beside the main sass code. You need to allocate and fill that
buffer before passing it to LibSass. You may also overtake memory management
from libsass for certain return values (i.e. `sass_context_take_output_string`).
Make sure to free it via `sass_free_memory`.

```C
// to allocate buffer to be filled
void* sass_alloc_memory(size_t size);
// to allocate a buffer from existing string
char* sass_copy_c_string(const char* str);
// to free overtaken memory when done
void sass_free_memory(void* ptr);
```

## Miscellaneous API functions

```C
// Some convenient string helper function
char* sass_string_unquote (const char* str);
char* sass_string_quote (const char* str, const char quote_mark);

// Get compiled libsass version
const char* libsass_version(void);

// Implemented sass language version
// Hardcoded version 3.4 for time being
const char* libsass_language_version(void);
```

## Common Pitfalls

**input_path**

The `input_path` is part of `Sass_Options`, but it also is the main option for
`Sass_File_Context`. It is also used to generate relative file links in source-
maps. Therefore it is pretty useful to pass this information if you have a
`Sass_Data_Context` and know the original path.

**output_path**

Be aware that `libsass` does not write the output file itself. This option
merely exists to give `libsass` the proper information to generate links in
source-maps. The file has to be written to the disk by the
binding/implementation. If the `output_path` is omitted, `libsass` tries to
extrapolate one from the `input_path` by replacing (or adding) the file ending
with `.css`.

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

## ABI forward compatibility

We use a functional API to make dynamic linking more robust and future
compatible. The API is not yet 100% stable, so we do not yet guarantee
[ABI](https://gcc.gnu.org/onlinedocs/libstdc++/manual/abi.html) forward
compatibility.

## Plugins (experimental)

LibSass can load plugins from directories. Just define `plugin_path` on context
options to load all plugins from the directories. To implement plugins, please
consult the following example implementations.

- https://github.com/mgreter/libsass-glob
- https://github.com/mgreter/libsass-math
- https://github.com/mgreter/libsass-digest

## Internal Structs

- [Sass Context Internals](api-context-internal.md)
- [Sass Value Internals](api-value-internal.md)
- [Sass Function Internals](api-function-internal.md)
- [Sass Importer Internals](api-importer-internal.md)
