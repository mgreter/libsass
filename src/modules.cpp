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

  BuiltInMod::BuiltInMod(BuiltInMod&& other) noexcept :
    Module(other.idxs)
  {
    other.idxs = nullptr;
  }

  BuiltInMod& BuiltInMod::operator=(BuiltInMod&& other) noexcept
  {
    idxs = other.idxs;
    other.idxs = nullptr;
    return *this;
  }

  Env::~Env() {
    //delete idxs;
  }

}
