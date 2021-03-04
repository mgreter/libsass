## LibSass C-API for variables

This API allows custom functions to access and modify existing variables.
It is not possible to create new variables with this API, as variables are
optimized in a way that declaration are hard baked once parsing is done.
Therefore we can't add new variables during runtime. This API allows to
query and set already existing variables. To define new variables you must
use an API that is executed during the parsing phase (see plugins).

### Basic Usage

```C
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

// Getter for lexical variable (lexical to scope where function is called).
// Note: C-API function can only access existing variables and not create new ones!
struct SassValue* sass_env_get_lexical (struct SassCompiler*, const char*);

// Setter for lexical variable (lexical to scope where function is called).
// Note: C-API function can only access existing variables and not create new ones!
void sass_env_set_lexical(struct SassCompiler*, const char*, struct SassValue*);

// Getter for local variable (local only to scope where function is called).
// Note: C-API function can only access existing variables and not create new ones!
struct SassValue* sass_env_get_global (struct SassCompiler*, const char*);

// Setter for local variable (local only to scope where function is called).
// Note: C-API function can only access existing variables and not create new ones!
void sass_env_set_global(struct SassCompiler*, const char*, struct SassValue*);

/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
```
