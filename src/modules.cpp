/*****************************************************************************/
/* Part of LibSass, released under the MIT license (See LICENSE.txt).        */
/*****************************************************************************/
#include "modules.hpp"

#include "stylesheet.hpp"

namespace Sass {

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Module::Module(VarRefs* idxs) :
    Env(idxs)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  BuiltInMod::BuiltInMod(EnvRoot& root) :
    Module(new VarRefs(
      root,
      nullptr,
      0xFFFFFFFF,
      0xFFFFFFFF,
      0xFFFFFFFF,
      false, // permeable
      false, // isImport
      true)) // isModule
  {
    isBuiltIn = true;
    isLoaded = true;
    isCompiled = true;
  }

  BuiltInMod::~BuiltInMod()
  {
    delete idxs;
  }

  void BuiltInMod::addFunction(const EnvKey& name, uint32_t offset)
  {
    idxs->fnIdxs[name] = offset;
  }

  void BuiltInMod::addVariable(const EnvKey& name, uint32_t offset)
  {
    idxs->varIdxs[name] = offset;
  }

  void BuiltInMod::addMixin(const EnvKey& name, uint32_t offset)
  {
    idxs->mixIdxs[name] = offset;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

}
