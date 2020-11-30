#ifndef SASS_COMPILER_H
#define SASS_COMPILER_H

// sass.hpp must go before all system headers
// to get the __EXTENSIONS__ fix on Solaris.
#include "capi_sass.hpp"

#include "logger.hpp"
#include "context.hpp"
#include "ast_css.hpp"
#include "stylesheet.hpp"
#include "capi_error.hpp"
#include "capi_context.hpp"

struct SassImport;

namespace Sass {

  // The split between Context and Compiler is technically not really needed.
  // But it helps a little to organize the different aspects of the compilation.
  // The Context mainly stores all the read-only values (e.g. settings).
  class Compiler final : public Context, public Logger {

  public:

    EnvRefs* getCurrentFrame() const {
      if (varRoot.stack.empty()) return nullptr;
      return varRoot.stack.back();
    }

    EnvRefs* getCurrentModule() const {
      if (varRoot.stack.empty()) return nullptr;
      auto current = varRoot.stack.back();
      while (current->pscope) {
        if (current->module) break;
        current = current->pscope;
      }
      return current;
    }

    BuiltInMod& createModule(const sass::string& name) {
      auto it = modules.find(name);
      if (it != modules.end()) {
        return *it->second;
      }
      BuiltInMod* module = new BuiltInMod(varRoot);
      modules.insert({ name, module });
      return *module;
    }

    BuiltInMod* getModule(const sass::string& name) {
      auto it = modules.find(name);
      if (it == modules.end()) return nullptr;
      return it->second;
    }


    bool implicitWithConfig = false;

    // Stack of environment frames. New frames are appended
    // when parser encounters a new environment scoping.
    sass::vector<EnvRefs*> varStack3312;

    // The root environment where parsed root variables
    // and (custom) functions plus mixins are registered.
    EnvRoot varRoot; // Must be after varStack!

    // Get the value object for the variable by [name] on runtime.
    // If [global] flag is given, we 
    ValueObj findVariable(const EnvKey& name, bool global = false) {
      return varRoot.findVariable(name, global);
    }

    EnvIdx setVariable(const EnvKey& name, bool guarded, bool global) {
      return varRoot.setVariable(name, guarded, global);
    }

    // Functions only for evaluation phase (C-API functions and eval itself)
    CallableObj* findFunction(const EnvKey& name) { return varRoot.findFunction(name); }

    // The current state the compiler is in.
    enum SassCompilerState state;

    // Option struct for source map options
    struct SassSrcMapOptions srcmap_options;

    // Where we want to store the output.
    // Source-map path is deducted from it.
    // Defaults to `stream://stdout`.
    sass::string output_path;

    // main entry point for compilation
    ImportObj entry_point;

    // Parsed ast-tree
    RootObj sheet;

    // Evaluated ast-tree
    CssRootObj compiled;

    // The rendered css content.
    sass::string content;

    // Rendered warnings and debugs. They can be emitted at any stage.
    // Therefore we make a copy into our string after each stage from
    // the actual logger instance (created by context). This is needed
    // in order to return a `c_str` on the API from the output-stream.
    sass::string warnings;

    // The rendered output footer. This includes the
    // rendered css comment footer for the source-map.
    // Append after output for the full output document.
    char* footer;

    // The rendered source map. This is what an implementor
    // would normally write out to the `output.css.map` file
    char* srcmap;

    // Runtime error
    SassError error;

    // Constructor
    Compiler();

    // Parse ast tree
    void parse();

    // Compile parsed tree
    void compile();

    // Render compiled tree
    OutputBuffer renderCss();

    // Get path of compilation entry point
    // Returns the resolved/absolute path
    sass::string getInputPath() const;

    // Get the output path for this compilation
    // Can be explicit or deducted from input path
    sass::string getOutputPath() const;

    // ToDO, maybe belongs to compiler?
    CssRootObj compileRoot(bool plainCss);

    char* renderSrcMapJson(struct SassSrcMapOptions options,
      const SourceMap& source_map);

    char* renderSrcMapLink(struct SassSrcMapOptions options,
      const SourceMap& source_map);

    char* renderEmbeddedSrcMap(struct SassSrcMapOptions options,
      const SourceMap& source_map);

    /////////////////////////////////////////////////////////////////////////
    // Register built-in function with only one parameter list.
    /////////////////////////////////////////////////////////////////////////
    uint32_t createBuiltInFunction(const EnvKey& name,
      const sass::string& signature, SassFnSig cb);
    uint32_t registerBuiltInFunction(const EnvKey& name,
      const sass::string& signature, SassFnSig cb);

    void exposeFunction(const EnvKey& name, uint32_t idx);

    uint32_t createBuiltInVariable(const EnvKey& name, Value* value);
    uint32_t createBuiltInMixin(const EnvKey& name, const sass::string& signature, SassFnSig cb);

    /////////////////////////////////////////////////////////////////////////
    // Register built-in functions that can take different
    // functional arguments (best suited will be chosen).
    /////////////////////////////////////////////////////////////////////////
    uint32_t createBuiltInOverloadFns(const EnvKey& name,
      const sass::vector<std::pair<const sass::string, SassFnSig>>& overloads);
    uint32_t registerBuiltInOverloadFns(const EnvKey& name,
      const sass::vector<std::pair<const sass::string, SassFnSig>>& overloads);

    /////////////////////////////////////////////////////////////////////////
    // @param imp_path The relative or custom path for be imported
    // @param pstate SourceSpan where import occurred (parent context)
    // @param rule The backing ImportRule that is added to the document
    /////////////////////////////////////////////////////////////////////////
    bool callCustomHeaders(const sass::string& imp_path, SourceSpan& pstate, ImportRule* rule)
    {
      return callCustomLoader(imp_path, pstate, rule, cHeaders, false);
    };

    /////////////////////////////////////////////////////////////////////////
    // @param imp_path The relative or custom path for be imported
    // @param pstate SourceSpan where import occurred (parent context)
    // @param rule The backing ImportRule that is added to the document
    /////////////////////////////////////////////////////////////////////////
    bool callCustomImporters(const sass::string& imp_path, SourceSpan& pstate, ImportRule* rule)
    {
      return callCustomLoader(imp_path, pstate, rule, cImporters, true);
    };

    // Parse the import (updates syntax flag if AUTO was set)
    // Results will be stored at `sheets[source->getAbsPath()]`
    Root* registerImport(ImportObj import);

    // Called by parserStylesheet on the very first parse call
    void applyCustomHeaders(StatementVector& root, SourceSpan pstate);

  protected:

    /////////////////////////////////////////////////////////////////////////
    // Called once to register all built-in functions.
    // This will invoke parsing for parameter lists.
    /////////////////////////////////////////////////////////////////////////
    void loadBuiltInFunctions();

    Root* parseRoot(ImportObj import);

  private:

    /////////////////////////////////////////////////////////////////////////
    // @param imp_path The relative or custom path for be imported
    // @param pstate SourceSpan where import occurred (parent context)
    // @param rule The backing ImportRule that is added to the document
    // @param importers Array of custom importers/headers to go through
    // @param singleton Whether to use all importers or only first successful
    /////////////////////////////////////////////////////////////////////////
    bool callCustomLoader(const sass::string& imp_path, SourceSpan& pstate, ImportRule* rule,
      const sass::vector<struct SassImporter*>& importers, bool singletone = true);

    /////////////////////////////////////////////////////////////////////////
    // Create a new external callable from the sass function. Parses
    // function signature into function name and argument declaration.
    // The function you pass in will be taken over and freed by us!
    /////////////////////////////////////////////////////////////////////////
    ExternalCallable* makeExternalCallable(struct SassFunction* function);

    /////////////////////////////////////////////////////////////////////////
    // Register an external custom sass function on the global scope.
    // Main entry point for custom functions passed through the C-API.
    // The function you pass in will be taken over and freed by us!
    /////////////////////////////////////////////////////////////////////////
    void registerCustomFunction(struct SassFunction* function);

    // Invoke parser according to import format
    RootObj parseSource(ImportObj source);

  public:

    CAPI_WRAPPER(Compiler, SassCompiler);

  };

  class ImportStackFrame
  {
  private:

    Compiler& compiler;

  public:

    ImportStackFrame(
      Compiler& compiler,
      Import* import);

    ~ImportStackFrame();

  };


}


#endif
