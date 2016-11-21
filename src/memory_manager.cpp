#include "sass.hpp"
#include "ast.hpp"
#include "memory_manager.hpp"

namespace Sass {

  Memory_Manager::Memory_Manager(size_t size)
  : nodes(std::vector<SharedObject*>())
  {
    size_t init = size;
    if (init < 8) init = 8;
    // reserve some space
    nodes.reserve(init);
  }

  Memory_Manager::~Memory_Manager()
  {
    // release memory for all controlled nodes
    // avoid calling erase for every single node
    for (size_t i = 0, S = nodes.size(); i < S; ++i) {
      deallocate(nodes[i]);
    }
    // just in case
    nodes.clear();
  }

  SharedObject* Memory_Manager::add(SharedObject* np, std::string file, size_t line)
  {
    // object has been initialized
    // it can be "deleted" from now on
    np->allocated = file;
    np->line = line;
    np->refcounter = 0;
    np->refcount = 1;
    return np;
  }

  bool Memory_Manager::has(SharedObject* np)
  {
    // check if the pointer is controlled under our pool
    return find(nodes.begin(), nodes.end(), np) != nodes.end();
  }

  SharedObject* Memory_Manager::allocate(size_t size)
  {
    // allocate requested memory
    void* heap = ::operator new(size);
    // init internal refcount status to zero
    (static_cast<SharedObject*>(heap))->refcount = 0;
    // add the memory under our management
    if (!MEM) nodes.push_back(static_cast<SharedObject*>(heap));
    // cast object to its initial type
    return static_cast<SharedObject*>(heap);
  }

  void Memory_Manager::deallocate(SharedObject* np)
  {
    // only call destructor if initialized
    // if (np->refcount) np->~SharedObject();
    // always free the memory
    // free(np);
  }

  void Memory_Manager::remove(SharedObject* np)
  {
    // remove node from pool (no longer active)
    if (!MEM) nodes.erase(find(nodes.begin(), nodes.end(), np));
    // you are now in control of the memory
  }

  void Memory_Manager::destroy(SharedObject* np)
  {
    // remove from pool
    remove(np);
    // release memory
    deallocate(np);
  }

}

