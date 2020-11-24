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

  Env::~Env() {
    //delete idxs;
  }

}
