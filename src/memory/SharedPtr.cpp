#include "../sass.hpp"
#include <iostream>
#include <typeinfo>

#include "SharedPtr.hpp"
#include "../ast_fwd_decl.hpp"

#ifdef DEBUG_SHARED_PTR
#include "../debugger.hpp"
#endif

namespace Sass {

   // ToDo: How to protect these? Init order?
  std::vector<void*>* memLakes = nullptr;
  size_t memPtr = std::string::npos;

  #ifdef DEBUG_SHARED_PTR
  void SharedObj::dumpMemLeaks() {
    if (!all.empty()) {
      std::cerr << "###################################\n";
      std::cerr << "# REPORTING MISSING DEALLOCATIONS #\n";
      std::cerr << "###################################\n";
      for (SharedObj* var : all) {
        if (AST_Node* ast = dynamic_cast<AST_Node*>(var)) {
          debug_ast(ast);
        } else {
          std::cerr << "LEAKED " << var << "\n";
        }
      }
      all.clear();
      deleted.clear();
      objCount = 0;
    }
  }
  size_t SharedObj::objCount = 0;
  sass::vector<SharedObj*> SharedObj::all;
  std::unordered_set<size_t> SharedObj::deleted;
  size_t SharedObj::maxRefCount = 0;
#endif

  bool SharedObj::taint = false;
}
