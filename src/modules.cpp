#include "modules.hpp"

#include "stylesheet.hpp"

namespace Sass {

  Module::Module(VarRefs* idxs) :
    Env(idxs)
  {
  }

  BuiltInMod::~BuiltInMod() {
    delete idxs;
  }

  BuiltInMod::BuiltInMod(BuiltInMod&& other) noexcept:
    Module(other.idxs)
  {
    other.idxs = nullptr;
  }

  Env::~Env() {
    //delete idxs;
  }

}
