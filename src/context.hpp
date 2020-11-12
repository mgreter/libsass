#ifndef SASS_CONTEXT_HPP
#define SASS_CONTEXT_HPP

// sass.hpp must go before all system headers
// to get the __EXTENSIONS__ fix on Solaris.
#include "capi_sass.hpp"

#define BUFFERSIZE 255
#include "b64/encode.h"

#include <map>
#include "file.hpp"
#include "stylesheet.hpp"
#include "ast_callables.hpp"

// #include "output.hpp"
// #include "sass_context.hpp"
// #include "stylesheet.hpp"
// #include "environment_stack.hpp"

namespace Sass {

  class Module
  {
  public:
    VarRefs* idxs;
    bool isActive = false;
    void addFunction(const sass::string& name, uint32_t offset);
    void addVariable(const sass::string& name, uint32_t offset);
    void addMixin(const sass::string& name, uint32_t offset);
    Module();
  };

  // Compiler is stateful, while Context is more low-level
  class Context : public SassOutputOptionsCpp {

  private:

    // Checking if a file exists can be quite extensive
    // Keep an internal map to avoid repeated system calls
    std::unordered_map<sass::string, bool> fileExistsCache;

    // Include paths are local to context since we need to know
    // it for lookups during parsing. You may reset this for
    // another compilation when reusing the context.
    sass::vector<sass::string> includePaths;

  public:

    EnvKeyMap<Module> modules;

    Module& createModule(const sass::string& name) {
      return modules[name];
    }

    Module* getModule(const sass::string& name) {
      auto it = modules.find(name);
      if (it == modules.end()) return nullptr;
      return &it->second;
    }

  protected:

    // Functions in order of appearance
    // Same order needed for function stack
    std::vector<CallableObj> fnList;

    // Lookup functions by function name
    // Due to EnvKayMap case insensitive.
    EnvKeyMap<CallableObj> fnLookup;


  public:

    // sass::vector<ConfiguredVariable>* withConfig = nullptr;

    EnvKeyMap<ConfiguredVariable> withConfig;

    // Sheets are filled after resources are parsed
    // This could be shared, should go to engine!?
    // ToDo: should be case insensitive on windows?
    std::map<const sass::string, StyleSheetObj> sheets;

    // Only used to cache `loadImport` calls
    std::map<const sass::string, ImportObj> sources;

    // Additional C-API stuff for interaction
    sass::vector<struct SassImporter*> cHeaders;
    sass::vector<struct SassImporter*> cImporters;
    sass::vector<struct SassFunction*> cFunctions;

  public:

    // The import stack during evaluation phase
    sass::vector<ImportObj> import_stack;

    // List of all sources that have been included
    sass::vector<SourceDataObj> included_sources;

    /////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////

    // Constructor
    Context();

    /////////////////////////////////////////////////////////////////////////
    // Helpers for `sass_prepare_context`
    /////////////////////////////////////////////////////////////////////////

    void addCustomHeader(struct SassImporter* header);
    void addCustomImporter(struct SassImporter* importer);
    void addCustomFunction(struct SassFunction* function);

    /////////////////////////////////////////////////////////////////////////
    // Some simple delegations to variable root for runtime queries
    /////////////////////////////////////////////////////////////////////////

    /////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////

    // Load plugins from path, which can be path separated
    void loadPlugins(const sass::string& paths);

    // Add additional include paths, which can be path separated
    void addIncludePaths(const sass::string& paths);

    // Load import from the file-system and create source object
    // Results will be stored at `sources[source->getAbsPath()]`
    Import* loadImport(const ResolvedImport& import);

    // Implementation for `sass_compiler_find_file`
    // Looks for the file in regard to the current
    // import and then looks in all include folders.
    sass::string findFile(const sass::string& path);

    // Implementation for `resolveDynamicImport`
    // Look for all possible filename variants (e.g. partials)
    // Returns all results (e.g. for ambiguous but valid imports)
    sass::vector<ResolvedImport> findIncludes(const ImportRequest& import);

  };
  // EO Context

}

#endif
