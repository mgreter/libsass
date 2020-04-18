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

  class Context : public SassOptionsCpp {
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
      const sass::vector<SassImporterPtr>& importers, bool singletone = true);

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

    /*#########################################################################*/
    // Split path-separated string and add them to include paths.
    // On windows the path separator is `;`, most others are `:`.
    /*#########################################################################*/
    void collectIncludePaths(const sass::string& paths);

    /*#########################################################################*/
    // Split path-separated string and add them to plugin paths.
    // On windows the path separator is `;`, most others are `:`.
    /*#########################################################################*/
    void collectPluginPaths(const sass::string& paths);

    /*#########################################################################*/
    // Call collect for every item inside the vector.
    /*#########################################################################*/
    void collectIncludePaths(const sass::vector<sass::string>& paths);
    void collectPluginPaths(const sass::vector<sass::string>& paths);


  protected:

    BlockObj parseImport(SassImportPtr import);

    void prepareEnvironment();

  public:

    // The attached options passed from C-API
    struct SassOptionsCpp& c_options;

    struct SassImportCpp* entry; // XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

    size_t head_imports;
    Plugins plugins;
    Output emitter; // XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

    // Global available functions
    std::vector<CallableObj> fnCache;

    // Stacks of all parsed functions 
    sass::vector<EnvFrame*> varStack;

    // Checking if a file exists can be quite extensive
    // Keep an internal map to avoid multiple check calls
    std::unordered_map<sass::string, bool> fileExistsCache;

    // Runtime variables
    EnvRoot varRoot;

    // Current content block
    UserDefinedCallable* content88;

    // The logger is created on context instantiation.
    // It assigns a specific logger according to options.
    Logger* logger; // XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

    // resources under our control
    sass::vector<SourceDataObj> sources;

    // Sheets are filled after resources are parsed
    std::map<const sass::string, StyleSheet> sheets;

    sass::vector<SassImportPtr> import_stack;
    sass::vector<SourceDataObj> importStack;

    sass::vector<SassCalleeCpp> callee_stack;

    EnvKeyMap<CallableObj> functions;
    // EnvKeyMap<BuiltInCallableObj> built-ins;
    // EnvKeyMap<ExternalCallableObj> externals;

    Extender extender;

    struct SassCompiler* c_compiler;

    // Absolute paths to all includes we have seen so far.
    // Consumers are encouraged to clear this vector after they
    // have copied/moved the items after parsing the entry point.
    // They should ideally be known for every stylesheet.
    sass::vector<sass::string> included_files88;

    // relative includes for sourcemap
    sass::vector<sass::string> srcmap_links88;

    sass::vector<sass::string> plugin_paths88; // relative paths to load plugins
    sass::vector<sass::string> include_paths88; // lookup paths for includes

    sass::vector<SassImporterPtr> c_headers88;
    sass::vector<SassImporterPtr> c_importers88;
    sass::vector<struct SassFunctionCpp*> c_functions88;

    // const sass::string indent88; // String to be used for indentation
    // const sass::string linefeed88; // String to be used for line feeds
    sass::string input_path88; // for relative paths in src-map
    sass::string output_path88; // for relative paths to the output
    sass::string source_map_file88; // path to source map file (enables feature)
    sass::string source_map_root88; // path for sourceRoot property (pass-through)

    void apply_custom_headers2(sass::vector<StatementObj>& root, SourceSpan pstate);

    virtual ~Context();
    Context();


    BlockObj parse2(struct SassImportCpp* entry) {

      return parseImport(entry);

      // Now we must decide what to do!
      // std::cerr << "Untangle me 22\n";
      // return compileImport(import);
    }

    virtual BlockObj compile(BlockObj root, bool plainCss);
    // ToDo: return const string& ?
    // virtual sass::string render(Block_Obj root);
    // virtual sass::string render_srcmap();
    virtual sass::string render_stderr();

    BlockObj register_import(SassImportPtr& entry);
    void register_resource(const Include&, char* contents, char* srcmap);
    void register_resource(const Include&, char* contents, char* srcmap, SourceSpan&);

    // search for valid imports (e.g. partials) on the file-system
    // returns multiple valid result for ambiguous import path
    sass::vector<Include> find_includes(const Importer& import);


    // Add a new import to the context (called from `import_url`)
    Include load_import(const Importer&, SourceSpan pstate);

    Sass_Output_Style output_style() { return c_options.output_style; };
    sass::vector<sass::string> get_included_files(bool skip = false, size_t headers = 0);

  private:
    sass::string format_embedded_source_map();
    sass::string format_source_mapping_url(const sass::string& out_path);

  };

}

#endif
