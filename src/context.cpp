// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"
#include "ast.hpp"
#include "sass_values.hpp"

#include "remove_placeholders.hpp"
#include "source_map.hpp"
#include "sass_functions.hpp"
#include "string_utils.hpp"
#include "fn_selectors.hpp"
#include "fn_strings.hpp"
#include "fn_numbers.hpp"
#include "fn_colors.hpp"
#include "fn_meta.hpp"
#include "fn_lists.hpp"
#include "fn_maps.hpp"
#include "context.hpp"
#include "eval.hpp"

#include "parser_scss.hpp"
#include "parser_sass.hpp"
#include "parser_css.hpp"

#include "permutate.hpp"
#include "debugger.hpp"
#include "plugins.hpp"
#include "logger.hpp"

#include "json.hpp"

namespace Sass {
  using namespace Constants;
  using namespace File;
  using namespace Sass;

  sass::string CWD(File::get_cwd());

  inline bool cmpImporterPrio (struct SassImporter* i, struct SassImporter* j)
  { return sass_importer_get_priority(i) > sass_importer_get_priority(j); }

  // static sass::string safe_input(const char* in_path)
  // {
  //   if (in_path == nullptr || in_path[0] == '\0') return "stdin";
  //   return in_path;
  // }

  // static sass::string safe_output(const char* out_path, sass::string input_path)
  // {
  //   if (out_path == nullptr || out_path[0] == '\0') {
  //     if (input_path.empty()) return "stdout";
  //     return input_path.substr(0, input_path.find_last_of(".")) + ".css";
  //   }
  //   return out_path;
  // }


  // most functions are very simple
#define IMPLEMENT_1_ARG_FN(fn) \
struct SassValue* fn_##fn(struct SassValue* s_args, Sass_Function_Entry cb, struct SassCompiler* comp) \
{ \
  if (!sass_value_is_list(s_args)) { \
    return sass_make_error("Invalid arguments for " #fn); \
  } \
  if (sass_list_get_length(s_args) != 1) { \
    return sass_make_error("Exactly one arguments expected for " #fn); \
  } \
  struct SassValue* inp = sass_list_get_value(s_args, 0); \
  if (!sass_value_is_number(inp)) { \
    return sass_make_error("You must pass a number into " #fn); \
  } \
  double inp_nr = sass_number_get_value(inp); \
  const char* inp_unit = sass_number_get_unit(inp); \
  return sass_make_number(fn(inp_nr), inp_unit); \
}

  std::string crc16s(const std::string& text, struct SassCompiler* comp)
  {
    return "CRC16";
  }

  // most functions are very simple
#define IMPLEMENT_STR_FN(fn) \
struct SassValue* fn_##fn(struct SassValue* s_args, struct SassFunction* cb, struct SassCompiler* comp) \
{ \
  if (!sass_value_is_list(s_args)) { \
    return sass_make_error("Invalid arguments for " #fn); \
  } \
  if (sass_list_get_length(s_args) != 1) { \
    return sass_make_error("Exactly one arguments expected for " #fn); \
  } \
  struct SassValue* inp = sass_list_get_value(s_args, 0); \
  if (!sass_value_is_string(inp)) { \
    return sass_make_error("You must pass a string into " #fn); \
  } \
  const char* inp_str = sass_string_get_value(inp); \
  std::string rv = fn(inp_str, comp); \
  return sass_make_string(rv.c_str()); \
} \

  IMPLEMENT_STR_FN(crc16s)

  // IMPLEMENT_1_ARG_FN(sin)



// we are called by libsass to dispatch to registered functions
struct SassValue* call_sass_function(struct SassValue* s_args, struct SassFunction* cb, struct SassCompiler* compiler)
  {

    puts("WHOA");
    struct SassTrace* trace = sass_compiler_last_trace(compiler);
    puts(sass_trace_get_name(trace));
    puts("WHOA");

    if (struct SassValue* qwe = sass_compiler_get_lexical_variable(compiler, "$qwe")) {
      struct SassValue* str = sass_value_stringify(qwe, false, 5);
      std::cerr << "IS : " << sass_string_get_value(str) << "\n";
    }

    return sass_clone_value(s_args);
  }

  Context::Context()
    : SassOutputOptionsCpp(),
    // emitter(c_options),
    logger123(new Logger(5, SASS_LOGGER_AUTO))

  {
    // Sass::CWD = File::get_cwd();

    // Sass 3.4: The current working directory will no longer be placed onto the Sass load path by default.
    // If you need the current working directory to be available, set SASS_PATH=. in your shell's environment.
    // Or add it explicitly in your implementation, e.g. include_paths.emplace_back(CWD or '.');

    // Explode separated include paths
    if (!include_paths.empty()) {
      sass::vector<sass::string> exploded;
      for (auto& paths : include_paths) {
        sass::vector<sass::string> splitted =
          StringUtils::split(paths, PATH_SEP, true);
        for (sass::string& path : splitted) {
          if (*path.rbegin() != '/') path += '/';
          exploded.emplace_back(path);
        }
      }
      include_paths = std::move(exploded);
    }


    // registerExternalCallable(sass_make_function("sin($x)", fn_sin, 0));
    registerCustomFunction(sass_make_function("crcew($x)", call_sass_function, 0));
    // registerCustomFunction(sass_make_function("crc16($x)", fn_crc16s, 0));

    // emitter.set_filename(abs2rel(output_path88, source_map_file88, CWD));

  }

  // Load plugins from path, which can be path separated
  void Context::addIncludePaths(const sass::string& paths)
  {
    // Check if we have anything to do
    if (paths.empty()) return;
    // Load plugins from all paths
    sass::vector<sass::string> split =
      StringUtils::split(paths, PATH_SEP, true);
    for (sass::string& path : split) {
      if (*path.rbegin() != '/') path += '/';
      include_paths.emplace_back(path);
    }
  }

  // Load plugins from path, which can be path separated
  void Context::loadPlugins(const sass::string& paths)
  {
    Plugins plugins;
    // Check if we have anything to do
    if (paths.empty()) return;
    // Load plugins from all paths
    sass::vector<sass::string> split =
      StringUtils::split(paths, PATH_SEP, true);
    for (sass::string& path : split) {
      if (*path.rbegin() != '/') path += '/';
      plugins.load_plugins(path);
    }
    // Register all items for next compilation request
    for (auto fn : plugins.get_headers()) c_headers88.emplace_back(fn);
    for (auto fn : plugins.get_importers()) c_importers88.emplace_back(fn);
    for (auto fn : plugins.get_functions()) c_functions88.emplace_back(fn);
    // Sort the resulting array by priorities
    sort(c_headers88.begin(), c_headers88.end(), cmpImporterPrio);
    sort(c_importers88.begin(), c_importers88.end(), cmpImporterPrio);
  }

  // Object destructor
  Context::~Context()
  {
    // release logger
    // delete logger;
  }

  /*#########################################################################*/
  // Interface for built in functions
  /*#########################################################################*/

  // Register built-in function with only one parameter list.
  void Context::registerBuiltInFunction(const sass::string& name,
    const sass::string& signature, SassFnSig cb)
  {
    EnvRoot root;
    ScopedStackFrame<EnvFrame>
      scoped(varStack, &root);
    // context.varStack.push_back(&root);
    auto source = SASS_MEMORY_NEW(SourceString,
      "sass://signature", "(" + signature + ")");
    auto args = ArgumentDeclaration::parse(*this, source);
    auto callable = SASS_MEMORY_NEW(BuiltInCallable, name, args, cb);
    fnLookup.insert(std::make_pair(name, callable));
    varRoot.createFunction(name);
    fnList.push_back(callable);
  }
  // EO registerBuiltInFunction

  // Register built-in functions that can take different
  // functional arguments (best suited will be chosen).
  void Context::registerBuiltInOverloadFns(const sass::string& name,
    const std::vector<std::pair<sass::string, SassFnSig>>& overloads)
  {
    EnvRoot root;
    SassFnPairs pairs;
    ScopedStackFrame<EnvFrame>
      scoped(varStack, &root);
    for (auto overload : overloads) {
      SourceDataObj source = SASS_MEMORY_NEW(SourceString,
        "sass://signature", "(" + overload.first + ")");
      auto args = ArgumentDeclaration::parse(*this, source);
      pairs.emplace_back(std::make_pair(args, overload.second));
    }
    auto callable = SASS_MEMORY_NEW(BuiltInCallables, name, pairs);
    fnLookup.insert(std::make_pair(name, callable));
    varRoot.createFunction(name);
    fnList.push_back(callable);
  }
  // EO registerBuiltInOverloadFns

  // Called once to register all built-in functions.
  // This will invoke parsing for parameter lists.
  void Context::loadBuiltInFunctions()
  {
    Functions::Meta::registerFunctions(*this);
    Functions::Math::registerFunctions(*this);
    Functions::Maps::registerFunctions(*this);
    Functions::Lists::registerFunctions(*this);
    Functions::Colors::registerFunctions(*this);
    Functions::Strings::registerFunctions(*this);
    Functions::Selectors::registerFunctions(*this);
  }
  // EO loadBuiltInFunctions

  /*#########################################################################*/
  // Interface for external custom functions
  /*#########################################################################*/

  // Create a new external callable from the sass function. Parses
  // function signature into function name and argument declaration.
  ExternalCallable* Context::makeExternalCallable(struct SassFunction* function)
  {
    // Create temporary source object for signature
    SourceStringObj source = SASS_MEMORY_NEW(SourceString,
      "sass://signature", sass_function_get_signature(function));
    // Create a new scss parser instance
    ScssParser parser(*this, source);
    // LibSass specials functions start with an `@`
    bool hasAt = parser.scanner.scanChar(Character::$at);
    sass::string name((hasAt ? "@" : "") + parser.identifier());
    // Return new external callable object
    return SASS_MEMORY_NEW(ExternalCallable, name,
      parser.parseArgumentDeclaration2(), function);
  }
  // EO makeExternalCallable

  // Register an external custom sass function on the global scope.
  // Main entry point for custom functions passed through the C-API.
  void Context::registerCustomFunction(struct SassFunction* function)
  {
    // Create the local environment
    EnvFrame local(&varRoot, true);
    // Put it on the stack where variables are created
    ScopedStackFrame<EnvFrame> scoped(varStack, &local);
    // Create a new external callable from the sass function
    ExternalCallable* callable = makeExternalCallable(function);
    // Get newly declared environment items
    callable->idxs(local.getIdxs());
    // Currently external functions are treated globally
    if (fnLookup.count(callable->name37()) == 0) {
      fnLookup.insert(std::make_pair(callable->name37(), callable));
      varRoot.createFunction(callable->name37());
      fnList.push_back(callable);
    }
  }
  // EO registerCustomFunction

  /*#########################################################################*/
  // Helpers for `sass_prepare_context`
  // Obsolete when c_ctx and cpp_ctx are merged.
  /*#########################################################################*/

  void Context::addCustomHeaders(sass::vector<struct SassImporter*>& headers)
  {
    for (auto header : headers) {
      if (header == nullptr) continue;
      c_headers88.emplace_back(header);
    }
    // need to sort the array afterwards (no big deal)
    sort(c_importers88.begin(), c_importers88.end(), cmpImporterPrio);
  }

  void Context::addCustomHeader(struct SassImporter* header)
  {
    if (header == nullptr) return;
    c_headers88.emplace_back(header);
    // need to sort the array afterwards (no big deal)
    sort(c_importers88.begin(), c_importers88.end(), cmpImporterPrio);
  }

  void Context::addCustomImporters(sass::vector<struct SassImporter*>& importers)
  {
    for (auto importer : importers) {
      if (importer == nullptr) continue;
      c_importers88.emplace_back(importer);
    }
    // need to sort the array afterwards (no big deal)
    sort(c_importers88.begin(), c_importers88.end(), cmpImporterPrio);
  }

  void Context::addCustomImporter(struct SassImporter* importer)
  {
    if (importer == nullptr) return;
    c_importers88.emplace_back(importer);
    // need to sort the array afterwards (no big deal)
    sort(c_importers88.begin(), c_importers88.end(), cmpImporterPrio);
  }

  void Context::addCustomFunctions(sass::vector<struct SassFunction*>& functions)
  {
    for (auto function : functions) {
      if (function == nullptr) continue;
      c_functions88.emplace_back(function);
    }
  }

  void Context::addCustomFunction(struct SassFunction* function)
  {
    if (function == nullptr) return;
    c_functions88.emplace_back(function);
  }

  /*#########################################################################*/
  /*#########################################################################*/

  // resolve the imp_path in base_path or include_paths
  // looks for alternatives and returns a list from one directory
  sass::vector<Include> Context::find_includes(const Importer& import)
  {
    // make sure we resolve against an absolute path
    sass::string base_path(rel2abs(import.base_path, ".", CWD));
    // first try to resolve the load path relative to the base path
    sass::vector<Include> vec(resolve_includes(base_path, import.imp_path, CWD, fileExistsCache));
    // then search in every include path (but only if nothing found yet)
    for (size_t i = 0, S = include_paths.size(); vec.size() == 0 && i < S; ++i)
    {
      // call resolve_includes and individual base path and append all results
      sass::vector<Include> resolved(resolve_includes(include_paths[i], import.imp_path, CWD, fileExistsCache));
      if (resolved.size()) vec.insert(vec.end(), resolved.begin(), resolved.end());
    }
    // return vector
    return vec;
  }

  RootObj Context::register_import(struct SassImport* import)
  {

    SourceData* source = import->srcdata;
    const sass::string& abs_path(source->getAbsPath());
    size_t idx = included_sources.size();
    source->setSrcIdx(idx);

    // Append to the resources
    // sources97.emplace_back(source);

    // ToDo
    included_sources.emplace_back(source);

    // add the entry to the stack
    import_stack.emplace_back(import);

    // check existing import stack for possible recursion
    for (size_t i = 0; i < import_stack.size() - 2; ++i) {
      struct SassImport* parent = import_stack[i];
      if (std::strcmp(parent->srcdata->getAbsPath(), import->srcdata->getAbsPath()) == 0) {
        // make path relative to the current directory
        sass::string stack("An @import loop has been found:");
        for (size_t n = 1; n < i + 2; ++n) {
          stack += "\n    " + sass::string(File::abs2rel(import_stack[n]->srcdata->getAbsPath(), CWD, CWD)) +
            " imports " + sass::string(File::abs2rel(import_stack[n + 1]->srcdata->getAbsPath(), CWD, CWD));
        }
        // implement error throw directly until we
        // decided how to handle full stack traces
        throw Exception::InvalidSyntax(*logger123, stack);
      }
    }

    RootObj root;
    bool isPlainCss = false;

    // callStackFrame frame(*logger,
    //   BackTrace("[import]", "@import"));

    // Auto detect input file format
    if (import->format == SASS_IMPORT_AUTO) {
      using namespace StringUtils;
      if (endsWithIgnoreCase(abs_path, ".css", 4)) {
        import->format = SASS_IMPORT_CSS;
      }
      else if (endsWithIgnoreCase(abs_path, ".sass", 5)) {
        import->format = SASS_IMPORT_SASS;
      }
      else if (endsWithIgnoreCase(abs_path, ".scss", 5)) {
        import->format = SASS_IMPORT_SCSS;
      }
      else {
        // Throw a basic error to report issue
        throw "Unrecognized file format for "
          + File::base_name(abs_path) + ".\n";
      }
    }

    // Use specific parser instance accordingly
    if (import->format == SASS_IMPORT_CSS)
    {
      CssParser parser(*this, source);
      root = parser.parse();
      isPlainCss = true;
    }
    else if (import->format == SASS_IMPORT_SASS)
    {
      SassParser parser(*this, source);
      root = parser.parse();
      isPlainCss = false;
    }
    else {
      ScssParser parser(*this, source);
      root = parser.parse();
      isPlainCss = false;
    }

    StyleSheet stylesheet(source, root);
    stylesheet.syntax = import->format;
    stylesheet.plainCss = isPlainCss;

    // remove current stack frame
    import_stack.pop_back();

    // Create key/value pair for loaded stylesheet
    // Note that we have a small chance of collisions!
    // Same path might be parsed with different syntax?
    std::pair<const sass::string, StyleSheet>
      ast_pair(abs_path, stylesheet);

    // register the result
    sheets.insert(ast_pair);

    return root;

  }

  // register include with resolved path and its content
  // memory of the resources will be freed by us on exit
  void Context::register_resource(const Include& inc, char* contents, char* srcmap)
  {

    // get pointer to the loaded content
    struct SassImport* import = sass_make_import(
      inc.imp_path.c_str(),
      inc.abs_path.c_str(),
      contents, srcmap,
      inc.syntax
    );

    RootObj sheet = register_import(import);

    // delete memory of current stack frame
    sass_delete_import(import);

  }

  // register include with resolved path and its content
  // memory of the resources will be freed by us on exit
  void Context::register_resource(const Include& inc, char* contents, char* srcmap, SourceSpan& prstate)
  {
    // Add a call stack frame
    callStackFrame frame(*logger123,
      BackTrace(prstate, Strings::importRule));
    register_resource(inc, contents, srcmap);
  }

  // Add a new import to the context (called from `import_url`)
  Include Context::load_import(const Importer& imp, SourceSpan pstate)
  {

    // search for valid imports (e.g. partials) on the file-system
    // returns multiple valid result for ambiguous import path
    const sass::vector<Include> resolved(find_includes(imp));

    // error nicely on ambiguous imp_path
    if (resolved.size() > 1) {
      sass::sstream msg_stream;
      msg_stream << "It's not clear which file to import. Found:\n";
      for (size_t i = 0, L = resolved.size(); i < L; ++i)
      { msg_stream << "  " << resolved[i].imp_path << "\n"; }
      error(msg_stream.str(), pstate, *logger123);
    }

    // process the resolved entry
    else if (resolved.size() == 1) {
      bool use_cache = c_importers88.size() == 0;
      // use cache for the resource loading
      if (use_cache && sheets.count(resolved[0].abs_path)) return resolved[0];
      // try to read the content of the resolved file entry
      // the memory buffer returned must be freed by us!
      if (char* contents = slurp_file(resolved[0].abs_path, CWD)) {

        // register the newly resolved file resource
        register_resource(resolved[0], contents, 0, pstate);
        // return resolved entry
        return resolved[0];
      }
    }

    // nothing found
    return Include{ imp, "", SASS_IMPORT_AUTO };

  }

  /*#########################################################################*/
  // @param imp_path The relative or custom path for be imported
  // @param pstate SourceSpan where import occurred (parent context)
  // @param rule The backing ImportRule that is added to the document
  // @param importers Array of custom importers/headers to go through
  // @param singleton Whether to use all importers or only first successful
  /*#########################################################################*/
  bool Context::callCustomLoader(const sass::string& imp_path, SourceSpan& pstate,
    ImportRule* rule, const sass::vector<struct SassImporter*>& importers, bool singleton)
  {
    // unique counter
    size_t count = 0;
    // need one correct import
    bool has_import = false;

    const char* ctx_path = pstate.getAbsPath();

    // Process custom importers and headers.
    // They must be presorted by priorities.
    for (struct SassImporter* importer : importers) {
      // Get the external importer function
      SassImporterLambda fn = sass_importer_get_callback(importer);
      // Call the external function, then check what it returned
      struct SassImportList* includes = fn(imp_path.c_str(), importer);
      // External provider want to handle this
      if (includes != nullptr) {
        // Get the list of possible includes
        // A list with zero items does nothing
        while (sass_import_list_size(includes) > 0) {
          // Increment counter
          ++count;

          auto import = sass_import_list_shift(includes);
          // Create a unique path to use as key
          sass::string uniq_path = imp_path;
          // Append counter to the path
          // Note: only for headers!
          if (!singleton && count) {
            sass::sstream path_strm;
            path_strm << uniq_path << ":" << count;
            uniq_path = path_strm.str();
          }
          // create the importer struct
          Importer importer(uniq_path, ctx_path);
          // query data from the current include
          SourceDataObj source = import->srcdata;
          // const char* content = sass_import_get_source(import);
          // const char* srcmap = sass_import_get_srcmap(import);
          const char* abs_path = sass_import_get_abs_path(import);

          // Handle error message passed back from custom importer
          // It may (or may not) override the line and column info
          if (const char* err_message = sass_import_get_error_message(import)) {
            size_t line = sass_import_get_error_line(import);
            size_t column = sass_import_get_error_column(import);
            // sass_delete_import(import); // will error afterwards
            if (line == sass::string::npos) error(err_message, pstate, *logger123);
            else if (column == sass::string::npos) error(err_message, pstate, *logger123);
            else error(err_message, { source, Offset::init(line, column) }, *logger123);
          }
          // Content for import was set.
          // No need to load it ourself.
          else if (sass_import_get_source(import)) {
            // Resolved abs_path should be set by custom importer
            // Use the created uniq_path as fall-back (enforce?)
            sass::string path_key(abs_path ? abs_path : uniq_path);
            // Create import statement in the document
            Include include(importer, path_key, import->format);
            auto statement = SASS_MEMORY_NEW(DynamicImport, pstate, path_key);
            rule->append(SASS_MEMORY_NEW(IncludeImport, statement, include));
            // Parse to stylesheet
            RootObj sheet = register_import(import);
          }
          // Only a path was returned
          // Try to load it like normal
          else if (abs_path) {
            // Create a new internal file importer
            const Importer importer(abs_path, ctx_path);
            // Create import statement in the document
            Include include(load_import(importer, pstate));
            auto statement = SASS_MEMORY_NEW(DynamicImport, pstate, imp_path);
            rule->append(SASS_MEMORY_NEW(IncludeImport, statement, include));
            // Check if loading was successful
            if (include.abs_path.empty()) {
              sass_delete_import(import);
              error("Can't find stylesheet to import.",
                rule->pstate(), *logger123);
            }
          }
          sass_delete_import(import);
        }
        // Deallocate the returned memory
        sass_delete_import_list(includes);
        // Set success flag
        has_import = true;
        // Break out of loop
        if (singleton) break;
      }
    }
    // Return result
    return has_import;
  }
  // EO callCustomLoader

  void Context::apply_custom_headers2(sass::vector<StatementObj>& statements, SourceSpan pstate)
  {
    // std::cerr << "Where is apply custom gone\n";
    /*
    // create a custom import to resolve headers
    ImportRuleObj rule = SASS_MEMORY_NEW(ImportRule, pstate);
    // dispatch headers which will add custom functions
    // custom headers are added to the import instance
    callCustomHeaders(entry_path88, pstate, rule);
    // increase head count to skip later
    head_imports += sources.size() - 1;
    // add the statement if we have urls
    // if (!imp->urls().empty()) statements.emplace_back(imp);
    // process all other resources (add Import_Stub nodes)
    for (ImportBase* inc : rule->elements()) {
      statements.emplace_back(inc);
    }
    */
  }

  RootObj Context::parseImport(struct SassImport* import)
  {

    if (import == nullptr) throw std::runtime_error("No entry point given");

    // add the entry to the stack
    import_stack.emplace_back(import);

    loadBuiltInFunctions();

    ScopedStackFrame<EnvFrame> scoped(varStack, &varRoot);

    // register custom functions (defined via C-API)
    for (size_t i = 0, S = c_functions88.size(); i < S; ++i)
    {
      registerCustomFunction(c_functions88[i]);
    }


    // load and register import
    RootObj root = register_import(import);

    // create root ast tree node
    return root;

  }

  sass::string Context::render_stderr()
  {
    return logger123->errors.str();
  }




}

