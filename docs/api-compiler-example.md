## Example for `data_context`

```C:data.c
#include <stdio.h>
#include <sass.h>

int main(int argc, const char* argv[])
{

  // LibSass will take control of data you pass in
  // Therefore we need to make a copy of static data
  char* text = sass_copy_c_string("a{b:c;}");
  // Normally you'll load data into a buffer from i.e. the disk.
  // Use `sass_alloc_memory` to get a buffer to pass to LibSass
  // then fill it with data you load from disk or somewhere else.

  // create the data context and get all related structs
  struct SassCompiler* compiler = sass_make_compiler();
  struct SassImport* data = sass_make_content_import(text, "styles");
  sass_compiler_set_entry_point(compiler, data);
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

}
```

### Compile data.c

```bash
gcc -c data.c -o data.o
gcc -o sample data.o -lsass
echo "foo { margin: 21px * 2; }" > foo.scss
./sample foo.scss => "foo { margin: 42px }"
```

## Example for `file_context`

```C:file.c
#include <stdio.h>
#include <sass.h>

int main(int argc, const char* argv[])
{

  // LibSass will take control of data you pass in
  // Therefore we need to make a copy of static data
  char* text = sass_copy_c_string("a{b:c;}");
  // Normally you'll load data into a buffer from i.e. the disk.
  // Use `sass_alloc_memory` to get a buffer to pass to LibSass
  // then fill it with data you load from disk or somewhere else.

  // create the data context and get all related structs
  struct SassCompiler* compiler = sass_make_compiler();
  struct SassImport* data = sass_make_file_import("foo.scss");
  sass_compiler_set_entry_point(compiler, data);
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

}
```

### Compile file.c

```bash
gcc -c file.c -o file.o
gcc -o sample file.o -lsass
echo "foo { margin: 21px * 2; }" > foo.scss
./sample foo.scss => "foo { margin: 42px }"
```

