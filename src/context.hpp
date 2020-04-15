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

namespace Sass {

  class Context {

  public:

    /*#########################################################################*/
    // @param imp_path The relative or custom path for be imported
    // @param pstate SourceSpan where import occurred (parent context)
    // @param rule The backing ImportRule that is added to the document
    /*#########################################################################*/
    bool callCustomHeaders(const sass::string& imp_path, SourceSpan& pstate, ImportRule* rule)
    {
      return callCustomLoader(imp_path, pstate, rule, c_headers, false);
    };

    /*#########################################################################*/
    // @param imp_path The relative or custom path for be imported
    // @param pstate SourceSpan where import occurred (parent context)
    // @param rule The backing ImportRule that is added to the document
    /*#########################################################################*/
    bool callCustomImporters(const sass::string& imp_path, SourceSpan& pstate, ImportRule* rule)
    {
      return callCustomLoader(imp_path, pstate, rule, c_importers, true);
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
      const sass::vector<Sass_Importer_Entry>& importers, bool singletone = true);

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

  private:

    /*#########################################################################*/
    // Called once to register all built-in functions.
    // This will invoke parsing for parameter lists.
    /*#########################################################################*/
    void loadBuiltInFunctions();

    /*#########################################################################*/
    // Split path-separated string and add them to plugin paths.
    // On windows the path separator is `;`, most others are `:`.
    /*#########################################################################*/
    void collectPluginPaths(const char* paths_str);

    /*#########################################################################*/
    // Split path-separated string and add them to plugin paths.
    // On windows the path separator is `;`, most others are `:`.
    /*#########################################################################*/
    void collectIncludePaths(const char* paths_str);

    void collect_plugin_paths(string_list* paths_array);
    void collect_include_paths(string_list* paths_array);


  protected:

    void prepareEnvironment();

  public:

    // Keep a copy of the current working directory.
    // We must not change it during runtime, but there
    // might be a chance our embedder does so after we
    // got initialized. We will not pick up this change!
    // In our defense, it is considered rude to do so!
    const sass::string CWD;

    // The attached options passed from C-API
    struct Sass_Options& c_options;

    sass::string entry_path;

    size_t head_imports;
    Plugins plugins;
    Output emitter;

    // Stacks of all parsed functions 
    sass::vector<EnvFrame*> varStack;

    // Checking if a file exists can be quite extensive
    // Keep an internal map to avoid multiple check calls
    std::unordered_map<sass::string, bool> fileExistsCache;

    // Runtime variables
    EnvRoot varRoot;

    // Current content block
    UserDefinedCallable* content;

    // The logger is created on context instantiation.
    // It assigns a specific logger according to options.
    Logger* logger;

    // resources under our control
    sass::vector<SourceDataObj> sources;

    // Sheets are filled after resources are parsed
    std::map<const sass::string, StyleSheet> sheets;

    sass::vector<Sass_Import_Entry> import_stack;
    sass::vector<SourceDataObj> importStack;

    sass::vector<Sass_Callee> callee_stack;

    EnvKeyMap<CallableObj> functions;
    // EnvKeyMap<BuiltInCallableObj> built-ins;
    // EnvKeyMap<ExternalCallableObj> externals;

    Extender extender;

    struct Sass_Compiler* c_compiler;

    // absolute paths to includes
    sass::vector<sass::string> included_files;
    // relative includes for sourcemap
    sass::vector<sass::string> srcmap_links;
    // vectors above have same size

    sass::vector<sass::string> plugin_paths; // relative paths to load plugins
    sass::vector<sass::string> include_paths; // lookup paths for includes

    void apply_custom_headers2(sass::vector<StatementObj>& root, SourceSpan pstate);

    sass::vector<Sass_Importer_Entry> c_headers;
    sass::vector<Sass_Importer_Entry> c_importers;
    sass::vector<Sass_Function_Entry> c_functions;

    void add_c_header(Sass_Importer_Entry header);
    void add_c_importer(Sass_Importer_Entry importer);
    void add_c_function(Sass_Function_Entry function);

    const sass::string indent; // String to be used for indentation
    const sass::string linefeed; // String to be used for line feeds
    const sass::string input_path; // for relative paths in src-map
    const sass::string output_path; // for relative paths to the output
    const sass::string source_map_file; // path to source map file (enables feature)
    const sass::string source_map_root; // path for sourceRoot property (pass-through)

    virtual ~Context();
    Context(struct Sass_Context&);
    virtual Block_Obj parse(Sass_Import_Type type) = 0;
    virtual Block_Obj compile();
    virtual char* render(Block_Obj root);
    virtual char* render_srcmap();
    virtual char* render_stderr();

    void register_import(Sass_Import_Entry& entry);
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

  public:
    const sass::string& cwd() { return CWD; };
  };

  class File_Context : public Context {
  public:
    File_Context(struct Sass_File_Context& ctx)
    : Context(ctx)
    { }
    virtual ~File_Context();
    virtual Block_Obj parse(Sass_Import_Type type);
  };

  class Data_Context : public Context {
  public:
    char* source_c_str;
    char* srcmap_c_str;
    Data_Context(struct Sass_Data_Context& ctx)
    : Context(ctx)
    {
      source_c_str       = ctx.source_string;
      srcmap_c_str       = ctx.srcmap_string;
      ctx.source_string = 0; // passed away
      ctx.srcmap_string = 0; // passed away
    }
    virtual ~Data_Context();
    virtual Block_Obj parse(Sass_Import_Type type);
  };

}

#endif
