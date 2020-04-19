#ifndef SASS_CONTEXT_H
#define SASS_CONTEXT_H

// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"
#include "ast.hpp"


#define BUFFERSIZE 255
#include "b64/encode.h"

#include "memory.hpp"
#include "sass_functions.hpp"
#include "sass_context.hpp"
#include "stylesheet.hpp"
#include "environment_stack.hpp"
#include "plugins.hpp"
#include "logger.hpp"
#include "output.hpp"
#include "file.hpp"


namespace Sass {

  class Context : public SassOutputOptionsCpp {
  public:

    /*#########################################################################*/
    // @param imp_path The relative or custom path for be imported
    // @param pstate SourceSpan where import occurred (parent context)
    // @param rule The backing ImportRule that is added to the document
    /*#########################################################################*/
    bool callCustomHeaders(const sass::string& imp_path, SourceSpan& pstate, ImportRule* rule)
    {
      return callCustomLoader(imp_path, pstate, rule, c_headers88, false);
    };

    /*#########################################################################*/
    // @param imp_path The relative or custom path for be imported
    // @param pstate SourceSpan where import occurred (parent context)
    // @param rule The backing ImportRule that is added to the document
    /*#########################################################################*/
    bool callCustomImporters(const sass::string& imp_path, SourceSpan& pstate, ImportRule* rule)
    {
      return callCustomLoader(imp_path, pstate, rule, c_importers88, true);
    };

  private:

    /*#########################################################################*/
    // @param imp_path The relative or custom path for be imported
    // @param pstate SourceSpan where import occurred (parent context)
    // @param rule The backing ImportRule that is added to the document
    // @param importers Array of custom importers/headers to go through
    // @param singleton Whether to use all importers or only first successful
    /*#########################################################################*/
    bool callCustomLoader(const sass::string& imp_path, SourceSpan& pstate, ImportRule* rule,
      const sass::vector<struct SassImporterCpp*>& importers, bool singletone = true);

  public:

    /*#########################################################################*/
    // Register built-in function with only one parameter list.
    /*#########################################################################*/
    void registerBuiltInFunction(const sass::string& name,
      const sass::string& signature, SassFnSig cb);

    /*#########################################################################*/
    // Register built-in functions that can take different
    // functional arguments (best suited will be chosen).
    /*#########################################################################*/
    void registerBuiltInOverloadFns(const sass::string& name,
      const std::vector<std::pair<sass::string, SassFnSig>>& overloads);

    /*#########################################################################*/
    // Helpers for `sass_prepare_context`
    /*#########################################################################*/
    void addCustomHeaders(sass::vector<struct SassImporterCpp*>& headers);
    void addCustomImporters(sass::vector<struct SassImporterCpp*>& importers);
    void addCustomFunctions(sass::vector<struct SassFunctionCpp*>& functions);

  private:

    /*#########################################################################*/
    // Create a new external callable from the sass function. Parses
    // function signature into function name and argument declaration.
    /*#########################################################################*/
    ExternalCallable* makeExternalCallable(struct SassFunctionCpp* function);

    /*#########################################################################*/
    // Register an external custom sass function on the global scope.
    // Main entry point for custom functions passed through the C-API.
    /*#########################################################################*/
    void registerCustomFunction(struct SassFunctionCpp* function);


    /*#########################################################################*/
    // Called once to register all built-in functions.
    // This will invoke parsing for parameter lists.
    /*#########################################################################*/
    void loadBuiltInFunctions();

  public:

    /*#########################################################################*/
    // Some simple delegations to variable root for runtime queries
    /*#########################################################################*/
    CallableObj getLexicalMixin(const EnvKey& name) { return varRoot.getLexicalMixin(name); }
    CallableObj getLexicalFunction(const EnvKey& name) { return varRoot.getLexicalFunction(name); }
    ValueObj getLocalVariable(const EnvKey& name) { return varRoot.getLocalVariable(name); }
    ValueObj getGlobalVariable(const EnvKey& name) { return varRoot.getGlobalVariable(name); }
    ValueObj getLexicalVariable(const EnvKey& name) { return varRoot.getLexicalVariable(name); }
    void setLocalVariable(const EnvKey& name, ValueObj val) { return varRoot.setLocalVariable(name, val); }
    void setGlobalVariable(const EnvKey& name, ValueObj val) { return varRoot.setGlobalVariable(name, val); }
    void setLexicalVariable(const EnvKey& name, ValueObj val) { return varRoot.setLexicalVariable(name, val); }

  protected:

    BlockObj parseImport(struct SassImportCpp* import);

    void prepareEnvironment();

  private:

    // Global available functions
    std::vector<CallableObj> fnCache;

    // Checking if a file exists can be quite extensive
    // Keep an internal map to avoid multiple check calls
    std::unordered_map<sass::string, bool> fileExistsCache;

  protected:

    // Runtime variables
    friend class Eval;
    EnvRoot varRoot;

    // Stacks of all parsed functions
    friend class StylesheetParser;
    sass::vector<EnvFrame*> varStack;

  public:

    // Include paths are local to context since we need to know
    // it for lookups during parsing. You may reset this for
    // another compilation when reusing the context.
    strings include_paths;

    // The logger is created on context instantiation.
    // It assigns a specific logger according to options.
    Logger* logger123; // XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

    // resources under our control
    sass::vector<SourceDataObj> sources;

    // Sheets are filled after resources are parsed
    std::map<const sass::string, StyleSheet> sheets;

    sass::vector<struct SassImportCpp*> import_stack;
    sass::vector<SourceDataObj> importStack;

    // Only used for is in mixin clause!?
    sass::vector<SassCalleeCpp> callee_stack;

    EnvKeyMap<CallableObj> functions;

    // Absolute paths to all includes we have seen so far.
    // Consumers are encouraged to clear this vector after they
    // have copied/moved the items after parsing the entry point.
    // They should ideally be known for every stylesheet.
    sass::vector<SourceDataObj> included_sources;

    // mainly used in context::find_includes
    // which is in turn called by Context::load_import
    sass::vector<sass::string> include_paths88; // lookup paths for includes

    sass::vector<struct SassImporterCpp*> c_headers88;
    sass::vector<struct SassImporterCpp*> c_importers88;
    sass::vector<struct SassFunctionCpp*> c_functions88;


    void apply_custom_headers2(sass::vector<StatementObj>& root, SourceSpan pstate);

    virtual ~Context();
    Context(enum Sass_Logger_Style logstyle);

    void loadPlugins(const sass::string& path);


    BlockObj parse2(struct SassImportCpp* entry) {

      return parseImport(entry);

      // Now we must decide what to do!
      // std::cerr << "Untangle me 22\n";
      // return compileImport(import);
    }

    // ToDO, maybe belongs to compiler?
    virtual BlockObj compile(BlockObj root, bool plainCss, struct SassCompiler& compiler);

    // ToDo: return const string& ?
    // virtual sass::string render(Block_Obj root);
    // virtual sass::string render_srcmap();
    virtual sass::string render_stderr();

    BlockObj register_import(struct SassImportCpp* entry);
    void register_resource(const Include&, char* contents, char* srcmap);
    void register_resource(const Include&, char* contents, char* srcmap, SourceSpan&);

    // search for valid imports (e.g. partials) on the file-system
    // returns multiple valid result for ambiguous import path
    sass::vector<Include> find_includes(const Importer& import);


    // Add a new import to the context (called from `import_url`)
    Include load_import(const Importer&, SourceSpan pstate);

    sass::vector<sass::string> get_included_files(bool skip = false, size_t headers = 0);

  private:
    // sass::string format_embedded_source_map();
    // sass::string format_source_mapping_url(const sass::string& out_path);

  };

}

#endif
