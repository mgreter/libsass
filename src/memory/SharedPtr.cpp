#include "../sass.hpp"
#include <iostream>
#include <typeinfo>

#include "SharedPtr.hpp"
#include "../ast_fwd_decl.hpp"
#include "../debugger.hpp"

namespace Sass {

  #ifdef DEBUG_SHARED_PTR
  void SharedObj::debugEnd() {
    if (!all.empty()) {
      std::cerr << "###################################\n";
      std::cerr << "# REPORTING MISSING DEALLOCATIONS #\n";
      std::cerr << "###################################\n";
      for (auto var : all) {
        std::cerr << "MISS " << var << "\n";
        if (AST_Node_Ptr ast = dynamic_cast<AST_Node_Ptr>(var)) {
          debug_ast(ast);
        }
      }
    }
  }
  std::vector<SharedObj*> SharedObj::all;
  #endif

#ifdef DEBUG_SHARED_PTR
  bool SharedObj::taint = false;
#endif

  SharedObj::SharedObj() : detached(false)
#ifdef DEBUG_SHARED_PTR
  , dbg(false)
#endif
{
      // refcount = 0;
      refcounter = 0;
    };

    SharedObj::~SharedObj() {
    };



  inline void SharedPtr::decRefCount() {
    if (node) {
      -- node->refcounter;
      if (node->refcounter == 0) {
        if (!node->detached) {
          delete(node);
        }
      }
    }
  }

  inline void SharedPtr::incRefCount() {
    if (node) {
      ++ node->refcounter;
      node->detached = false;
    }
  }

  SharedPtr::~SharedPtr() {
    decRefCount();
  }


  // the create constructor
  SharedPtr::SharedPtr(SharedObj* ptr)
  : node(ptr) {
    incRefCount();
  }
  // copy assignment operator
  SharedPtr& SharedPtr::operator=(const SharedPtr& rhs) {
    if (node == rhs.node) {
      return *this;
    }
    decRefCount();
    node = rhs.node;
    incRefCount();
    return *this;
  }
  // move assignment operator
  SharedPtr& SharedPtr::operator=(SharedPtr&& rhs) {
    if (node == rhs.node) {
      return *this;
    }
    decRefCount();
    node = rhs.node;
    incRefCount();
    return *this;
  }


  // the copy constructor
  SharedPtr::SharedPtr(const SharedPtr& obj)
  : node(obj.node) {
    incRefCount();
  }

/*
  // the move constructor
  SharedPtr::SharedPtr(SharedPtr&& obj)
  : node(obj.node) {
    std::cerr << "MOVE CTOR\n";
    // incRefCount("copy");
  }
*/

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