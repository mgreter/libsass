#include "sass.hpp"
#include <iostream>
#include <typeinfo>

#include "ast_fwd_decl.hpp"
#include "debugger.hpp"

namespace Sass {

  Memory_Ptr::~Memory_Ptr() {
    if (this->node) {
      // this gives errors for nested?
      this->node->refcounter -= 1;
      if (this->node->refcounter == 0) {
        AST_Node_Ptr ptr = SASS_MEMORY_CAST(AST_Node, *this->node);
        debug_ast(ptr, "DELETE: ");
        if (MEM)  delete(node);
      }
    }
  }
/*
     {
      if (this->node) {
        // this gives errors for nested?
        this->node->refcounter -= 1;
        // if (DBG)  std::cerr << "decrease refcount, " << node << " - now at " << node->refcounter << "\n";
        if (this->node->refcounter == 0) {
         debug_ast(node, "DELETE: ");
         if (DBG)  std::cerr << "DELETE me " << node << "\n";
         if (MEM)  delete(node);
          // delete and remove
        }
      }
    }
*/
}