/*****************************************************************************/
/* Part of LibSass, released under the MIT license (See LICENSE.txt).        */
/*****************************************************************************/
#include "capi_variable.hpp"

#include "compiler.hpp"

using namespace Sass;

extern "C" {

  // Getters and Setters for environments (lexical and global)
  struct SassValue* ADDCALL sass_env_get_lexical (struct SassCompiler* compiler, const char* name) {
    EnvRef ref = Compiler::unwrap(compiler).varRoot.findVarIdx(EnvKey{ name }, "", false);
    if (ref.isValid()) return Value::wrap(Compiler::unwrap(compiler).varRoot.getVariable(ref));
    return nullptr;
  }

  void ADDCALL sass_env_set_lexical (struct SassCompiler* compiler, const char* name, struct SassValue* val) {
    EnvRef ref = Compiler::unwrap(compiler).varRoot.findVarIdx(EnvKey{ name }, "", false);
    if (ref.isValid()) Compiler::unwrap(compiler).varRoot.setVariable(ref, &Value::unwrap(val), false);
  }

  struct SassValue* ADDCALL sass_env_get_global (struct SassCompiler* compiler, const char* name) {
    EnvRef ref = Compiler::unwrap(compiler).varRoot.findVarIdx(EnvKey{ name }, "", true);
    if (ref.isValid()) return Value::wrap(Compiler::unwrap(compiler).varRoot.getVariable(ref));
    return nullptr;
  }

  void ADDCALL sass_env_set_global (struct SassCompiler* compiler, const char* name, struct SassValue* val) {
    EnvRef ref = Compiler::unwrap(compiler).varRoot.findVarIdx(EnvKey{ name }, "", true);
    if (ref.isValid()) Compiler::unwrap(compiler).varRoot.setVariable(ref, &Value::unwrap(val), false);
  }

}
