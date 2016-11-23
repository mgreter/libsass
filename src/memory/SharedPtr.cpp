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

  bool SharedObj::taint = false;

  SharedObj::SharedObj() : detached(false)
#ifdef DEBUG_SHARED_PTR
  , dbg(false)
#endif
{
      // refcount = 0;
      refcounter = 0;
  #ifdef DEBUG_SHARED_PTR
    if (DBG && taint) std::cerr << "Create " << this << "\n";
      if (taint) all.push_back(this);
      std::vector<void*> parents;
  #endif
    };

    SharedObj::~SharedObj() {
  #ifdef DEBUG_SHARED_PTR
      if (dbg) std::cerr << "DEstruCT " << this << "\n";
      if(!all.empty()) { // check needed for MSVC (no clue why?)
        all.erase(std::remove(all.begin(), all.end(), this), all.end());
      }
  #endif
    };



  void SharedPtr::decRefCount() {
    if (node) {
      -- node->refcounter;
  #ifdef DEBUG_SHARED_PTR
      if (node->dbg)  std::cerr << "- " << node << " X " << node->refcounter << " (" << this << ") " << event << "\n";
  #endif
      if (node->refcounter == 0) {
  #ifdef DEBUG_SHARED_PTR
        AST_Node_Ptr ptr = SASS_MEMORY_CAST_PTR(AST_Node, node);
        if (node->dbg) std::cerr << "DELETE NODE " << node << "\n";
        if (DBG) std::cerr << "DELETE NODE " << node << "\n";
        if (DBG) debug_ast(ptr, "DELETE: ");
  #endif
#ifdef MEM
		if (!node->detached) {
			delete(node);
		}
#endif
      }
    }
  }

  void SharedPtr::incRefCount() {
    if (node) {
      ++ node->refcounter;
      node->detached = false;
  #ifdef DEBUG_SHARED_PTR
      if (node->dbg) {
        std::cerr << "+ " << node << " X " << node->refcounter << " (" << this << ") " << event << "\n";
      }
  #endif
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
    void* cur_ptr = (void*) node;
    void* rhs_ptr = (void*) rhs.node;
    if (cur_ptr == rhs_ptr) {
      return *this;
    }
    decRefCount();
    node = rhs.node;
    incRefCount();
    return *this;
  }
  // move assignment operator
  SharedPtr& SharedPtr::operator=(SharedPtr&& rhs) {
    void* cur_ptr = (void*) node;
    void* rhs_ptr = (void*) rhs.node;
    if (cur_ptr == rhs_ptr) {
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