#ifndef SASS_PLUGINS_H
#define SASS_PLUGINS_H

#include <string>
#include <vector>
#include "unicode.hpp"
#include "sass/functions.h"

#ifdef _WIN32

  #define LOAD_LIB(var, path) HMODULE var = LoadLibraryW(Unicode::utf8to16(path).c_str())
  #define LOAD_LIB_WCHR(var, path_wide_str) HMODULE var = LoadLibraryW(path_wide_str.c_str())
  #define LOAD_LIB_FN(type, var, name) type var = (type) GetProcAddress(plugin, name)
  #define CLOSE_LIB(var) FreeLibrary(var)

  #ifndef dlerror
  #define dlerror() 0
  #endif

#else

  #define LOAD_LIB(var, path) void* var = dlopen(path.c_str(), RTLD_LAZY)
  #define LOAD_LIB_FN(type, var, name) type var = (type) dlsym(plugin, name)
  #define CLOSE_LIB(var) dlclose(var)

#endif

namespace Sass {


  class Plugins {

    public: // c-tor
      Plugins();
      ~Plugins();

    public: // methods
      // load one specific plugin
      bool load_plugin(const sass::string& path);
      // load all plugins from a directory
      size_t load_plugins(const sass::string& path);

    public: // public accessors
      const sass::vector<struct SassImporterCpp*> get_headers(void) { return headers; }
      const sass::vector<struct SassImporterCpp*> get_importers(void) { return importers; }
      const sass::vector<struct SassFunctionCpp*> get_functions(void) { return functions; }

    private: // private vars
      sass::vector<struct SassImporterCpp*> headers;
      sass::vector<struct SassImporterCpp*> importers;
      sass::vector<struct SassFunctionCpp*> functions;

  };

}

#endif
