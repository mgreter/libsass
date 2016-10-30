#include "sass.hpp"
#include <iostream>
#include <typeinfo>

#include "ast_fwd_decl.hpp"
#include "debugger.hpp"

namespace Sass {

  void Memory_Ptr::decRefCount(std::string event) {
    if (this->node) {
      this->node->refcounter -= 1;
      if (this->node->dbg)  std::cerr << "- " << node << " X " << node->refcounter << " (" << this << ") " << event << "\n";
      if (this->node->refcounter == 0) {
        AST_Node_Ptr ptr = SASS_MEMORY_CAST(AST_Node, *this->node);
        std::cerr << "DELETE NODE " << node << "\n";
        debug_ast(ptr, "DELETE: ");
        if (MEM)  delete(node);
      }
    }
  }

  void Memory_Ptr::incRefCount(std::string event) {
    if (this->node) {
      this->node->refcounter += 1;
      if (this->node->dbg)  std::cerr << "+ " << node << " X " << node->refcounter << " (" << this << ") " << event << "\n";
    }
  }

  Memory_Ptr::~Memory_Ptr() {
    decRefCount("dtor");
  }


  // the create constructor
  Memory_Ptr::Memory_Ptr(Memory_Object* ptr)
  : node(ptr) {
    incRefCount("ctor");
  }
  // copy assignment operator
  Memory_Ptr& Memory_Ptr::operator=(const Memory_Ptr& rhs) {
    void* cur_ptr = (void*) node;
    void* rhs_ptr = (void*) rhs.node;
    if (cur_ptr == rhs_ptr) {
      return *this;
    }
    decRefCount("COPY");
    node = rhs.node;
    incRefCount("COPY");
    return *this;
  }
  // move assignment operator
  Memory_Ptr& Memory_Ptr::operator=(Memory_Ptr&& rhs) {
    void* cur_ptr = (void*) node;
    void* rhs_ptr = (void*) rhs.node;
    if (cur_ptr == rhs_ptr) {
      return *this;
    }
    decRefCount("MOVE");
    node = rhs.node;
    incRefCount("MOVE");
    return *this;
  }


  // the copy constructor
  Memory_Ptr::Memory_Ptr(const Memory_Ptr& obj)
  : node(obj.node) {
    incRefCount("copy");
  }


/*
     {
      if (this->node) {
        // this gives errors for nested?
        this->node->refcounter -= 1;
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