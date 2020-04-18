// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"
#include "ast.hpp"
#include "sass_values.hpp"

#include "remove_placeholders.hpp"
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
#include "cssize.hpp"
#include "eval.hpp"

#include "parser_scss.hpp"
#include "parser_sass.hpp"
#include "parser_css.hpp"

#include "permutate.hpp"
#include "debugger.hpp"
#include "logger.hpp"

namespace Sass {
  using namespace Constants;
  using namespace File;
  using namespace Sass;

  inline bool cmpImporterPrio (const SassImporterPtr& i, const SassImporterPtr& j)
  { return sass_importer_get_priority(i) > sass_importer_get_priority(j); }

  static sass::string safe_input(const char* in_path)
  {
    if (in_path == nullptr || in_path[0] == '\0') return "stdin";
    return in_path;
  }

  static sass::string safe_output(const char* out_path, sass::string input_path)
  {
    if (out_path == nullptr || out_path[0] == '\0') {
      if (input_path.empty()) return "stdout";
      return input_path.substr(0, input_path.find_last_of(".")) + ".css";
    }
    return out_path;
  }


  // most functions are very simple
#define IMPLEMENT_1_ARG_FN(fn) \
struct SassValue* fn_##fn(struct SassValue* s_args, Sass_Function_Entry cb, struct Sass_Compiler* comp) \
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

  std::string crc16s(const std::string& text, struct Sass_Compiler* comp)
  {
    return "CRC16";
  }

  // most functions are very simple
#define IMPLEMENT_STR_FN(fn) \
struct SassValue* fn_##fn(struct SassValue* s_args, Sass_Function_Entry cb, struct Sass_Compiler* comp) \
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

  Context::Context(struct SassContextCpp& c_ctx)
  : SassContextCpp(),
    CWD(File::get_cwd()),
    c_options(c_ctx),
    entry_path88(""),
    head_imports(0),
    plugins(),
    emitter(c_options),
    varRoot(),
    logger(new Logger(sass_option_get_precision(&c_ctx), c_ctx.logstyle)),
    sources(),
    sheets(),
    import_stack(),
    importStack(),
    callee_stack(),
    functions(),
    extender(Extender::NORMAL, logger->callStack),
    c_compiler(NULL),

    // Initialize C-API arrays for custom functionality
    c_headers88(sass::vector<SassImporterPtr>()),
    c_importers88(sass::vector<SassImporterPtr>()),
    c_functions88(sass::vector<struct SassFunctionCpp*>()),


    input_path88(make_canonical_path(safe_input(c_options.input_path.c_str()))),
    output_path88(make_canonical_path(safe_output(c_options.output_path.c_str(), input_path88))),
    source_map_file88(make_canonical_path(c_options.source_map_file)),
    source_map_root88(make_canonical_path(c_options.source_map_root))

  {

    // Sass 3.4: The current working directory will no longer be placed onto the Sass load path by default.
    // If you need the current working directory to be available, set SASS_PATH=. in your shell's environment.
    // Or add it explicitly in your implementation, e.g. include_paths.emplace_back(CWD or '.');

    // collect more paths from different options
    collectIncludePaths(c_options.include_path);
    collectIncludePaths(c_options.include_paths);
    collectPluginPaths(c_options.plugin_path);
    collectPluginPaths(c_options.plugin_paths);

    // load plugins and register custom behaviors
    for(auto plug : plugin_paths88) plugins.load_plugins(plug);
    for(auto fn : plugins.get_headers()) c_headers88.emplace_back(fn);
    for(auto fn : plugins.get_importers()) c_importers88.emplace_back(fn);
    for(auto fn : plugins.get_functions()) c_functions88.emplace_back(fn);

    // sort the items by priority (lowest first)
    sort (c_headers88.begin(), c_headers88.end(), cmpImporterPrio);
    sort (c_importers88.begin(), c_importers88.end(), cmpImporterPrio);

    // registerExternalCallable(sass_make_function("sin($x)", fn_sin, 0));
    registerCustomFunction(sass_make_function("crc16($x)", fn_crc16s, 0));
    registerCustomFunction(sass_make_function("crc16($x)", fn_crc16s, 0));

    emitter.set_filename(abs2rel(output_path88, source_map_file88, CWD));

  }


  Context::Context()
    : SassContextCpp(),
    CWD(File::get_cwd()),
    c_options(*this),
    entry_path88(""),
    head_imports(0),
    plugins(),
    emitter(c_options),
    varRoot(),
    logger(new Logger(5, SASS_LOGGER_ASCII_MONO)),
    sources(),
    sheets(),
    import_stack(),
    importStack(),
    callee_stack(),
    functions(),
    extender(Extender::NORMAL, logger->callStack),
    c_compiler(NULL),

    // Initialize C-API arrays for custom functionality
    c_headers88(sass::vector<SassImporterPtr>()),
    c_importers88(sass::vector<SassImporterPtr>()),
    c_functions88(sass::vector<struct SassFunctionCpp*>()),


    input_path88(make_canonical_path(safe_input(c_options.input_path.c_str()))),
    output_path88(make_canonical_path(safe_output(c_options.output_path.c_str(), input_path88))),
    source_map_file88(make_canonical_path(c_options.source_map_file)),
    source_map_root88(make_canonical_path(c_options.source_map_root))

  {

    // Sass 3.4: The current working directory will no longer be placed onto the Sass load path by default.
    // If you need the current working directory to be available, set SASS_PATH=. in your shell's environment.
    // Or add it explicitly in your implementation, e.g. include_paths.emplace_back(CWD or '.');

    // collect more paths from different options
    collectIncludePaths(c_options.include_path);
    collectIncludePaths(c_options.include_paths);
    collectPluginPaths(c_options.plugin_path);
    collectPluginPaths(c_options.plugin_paths);

    // load plugins and register custom behaviors
    for (auto plug : plugin_paths88) plugins.load_plugins(plug);
    for (auto fn : plugins.get_headers()) c_headers88.emplace_back(fn);
    for (auto fn : plugins.get_importers()) c_importers88.emplace_back(fn);
    for (auto fn : plugins.get_functions()) c_functions88.emplace_back(fn);

    // sort the items by priority (lowest first)
    sort(c_headers88.begin(), c_headers88.end(), cmpImporterPrio);
    sort(c_importers88.begin(), c_importers88.end(), cmpImporterPrio);

    // registerExternalCallable(sass_make_function("sin($x)", fn_sin, 0));
    registerCustomFunction(sass_make_function("crc16($x)", fn_crc16s, 0));
    registerCustomFunction(sass_make_function("crc16($x)", fn_crc16s, 0));

    emitter.set_filename(abs2rel(output_path88, source_map_file88, CWD));

  }

  // Object destructor
  Context::~Context()
  {
    // release logger
    delete logger;
  }

  /*#########################################################################*/
  // Interface for built in functions
  /*#########################################################################*/

  // Register built-in function with only one parameter list.
  void Context::registerBuiltInFunction(const sass::string& name,
    const sass::string& signature, SassFnSig cb)
  {
    auto source = SASS_MEMORY_NEW(SourceString,
      "sass://signature", "(" + signature + ")");
    auto args = ArgumentDeclaration::parse(*this, source);
    auto callable = SASS_MEMORY_NEW(BuiltInCallable, name, args, cb);
    functions.insert(std::make_pair(name, callable));
    varRoot.createFunction(name);
    fnCache.push_back(callable);
  }
  // EO registerBuiltInFunction

  // Register built-in functions that can take different
  // functional arguments (best suited will be chosen).
  void Context::registerBuiltInOverloadFns(const sass::string& name,
    const std::vector<std::pair<sass::string, SassFnSig>>& overloads)
  {
    SassFnPairs pairs;
    for (auto overload : overloads) {
      SourceDataObj source = SASS_MEMORY_NEW(SourceString,
        "sass://signature", "(" + overload.first + ")");
      auto args = ArgumentDeclaration::parse(*this, source);
      pairs.emplace_back(std::make_pair(args, overload.second));
    }
    auto callable = SASS_MEMORY_NEW(BuiltInCallables, name, pairs);
    functions.insert(std::make_pair(name, callable));
    varRoot.createFunction(name);
    fnCache.push_back(callable);
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
  ExternalCallable* Context::makeExternalCallable(struct SassFunctionCpp* function)
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
  void Context::registerCustomFunction(struct SassFunctionCpp* function)
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
    if (functions.count(callable->name()) == 0) {
      functions.insert(std::make_pair(callable->name(), callable));
      varRoot.createFunction(callable->name());
      fnCache.push_back(callable);
    }
  }
  // EO registerCustomFunction

  /*#########################################################################*/
  // Helpers for `sass_prepare_context`
  // Obsolete when c_ctx and cpp_ctx are merged.
  /*#########################################################################*/

  void Context::addCustomHeaders(sass::vector<struct SassImporterCpp*>& headers)
  {
    for (auto header : headers) {
      if (header == nullptr) continue;
      c_headers88.emplace_back(header);
    }
    // need to sort the array afterwards (no big deal)
    sort(c_importers88.begin(), c_importers88.end(), cmpImporterPrio);
  }

  void Context::addCustomImporters(sass::vector<struct SassImporterCpp*>& importers)
  {
    for (auto importer : importers) {
      if (importer == nullptr) continue;
      c_importers88.emplace_back(importer);
    }
    // need to sort the array afterwards (no big deal)
    sort(c_importers88.begin(), c_importers88.end(), cmpImporterPrio);
  }

  void Context::addCustomFunctions(sass::vector<struct SassFunctionCpp*>& functions)
  {
    for (auto function : functions) {
      if (function == nullptr) continue;
      c_functions88.emplace_back(function);
    }
  }

  /*#########################################################################*/
  // Helpers for search path handling
  /*#########################################################################*/

  // Split path-separated string and add them to include paths.
  // On windows the path separator is `;`, most others are `:`.
  void Context::collectIncludePaths(const sass::string& paths)
  {
    if (paths.empty()) return;
    sass::vector<sass::string> split =
      StringUtils::split(paths, PATH_SEP, true);
    for (sass::string& path : split) {
      if (*path.rbegin() != '/') path += '/';
      include_paths88.emplace_back(path);
    }
  }
  // EO collectIncludePaths

  // Split path-separated string and add them to plugin paths.
  // On windows the path separator is `;`, most others are `:`.
  void Context::collectPluginPaths(const sass::string& paths)
  {
    if (paths.empty()) return;
    sass::vector<sass::string> split =
      StringUtils::split(paths, PATH_SEP, true);
    for (sass::string& path : split) {
      if (*path.rbegin() != '/') path += '/';
      plugin_paths88.emplace_back(path);
    }
  }
  // EO collectPluginPaths

  // Call collect for every item inside the vector.
  void Context::collectIncludePaths(const sass::vector<sass::string>& paths)
  {
    for (const sass::string& path : paths) {
      collectIncludePaths(path.c_str());
    }
  }
  // EO collectIncludePaths

  // Call collect for every item inside the vector.
  void Context::collectPluginPaths(const sass::vector<sass::string>& paths)
  {
    for (const sass::string& path : paths) {
      collectPluginPaths(path.c_str());
    }
  }
  // EO collectPluginPaths

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
    for (size_t i = 0, S = include_paths88.size(); vec.size() == 0 && i < S; ++i)
    {
      // call resolve_includes and individual base path and append all results
      sass::vector<Include> resolved(resolve_includes(include_paths88[i], import.imp_path, CWD, fileExistsCache));
      if (resolved.size()) vec.insert(vec.end(), resolved.begin(), resolved.end());
    }
    // return vector
    return vec;
  }

  void Context::register_import(SassImportPtr& import)
  {

    SourceData* source = import->srcdata;
    const sass::string& abs_path(source->getAbsPath());
    size_t idx = sources.size();
    source->setSrcIdx(idx);

    // Append to the resources
    sources.emplace_back(source);

    // tell emitter about new resource
    emitter.add_source_index(idx);

    // add a relative link to the working directory
    included_files88.emplace_back(abs_path);

    // add a relative link to the source map output file
    srcmap_links88.emplace_back(abs2rel(abs_path, source_map_file88, CWD));

    // add the entry to the stack
    import_stack.emplace_back(import);
    importStack.emplace_back(sources.back());

    // check existing import stack for possible recursion
    for (size_t i = 0; i < import_stack.size() - 2; ++i) {
      SassImportPtr parent = import_stack[i];
      if (std::strcmp(parent->srcdata->getAbsPath(), import->srcdata->getAbsPath()) == 0) {
        // make path relative to the current directory
        sass::string stack("An @import loop has been found:");
        for (size_t n = 1; n < i + 2; ++n) {
          stack += "\n    " + sass::string(File::abs2rel(import_stack[n]->srcdata->getAbsPath(), CWD, CWD)) +
            " imports " + sass::string(File::abs2rel(import_stack[n + 1]->srcdata->getAbsPath(), CWD, CWD));
        }
        // implement error throw directly until we
        // decided how to handle full stack traces
        throw Exception::InvalidSyntax(*logger, stack);
      }
    }

    Block_Obj root;
    bool isPlainCss = false;

    // callStackFrame frame(*logger,
    //   BackTrace("[import]", "@import"));

    if (source->getType() == SASS_IMPORT_CSS)
    {
      CssParser parser(*this, source);
      root = parser.parse();
      isPlainCss = true;
    }
    else if (source->getType() == SASS_IMPORT_SASS)
    {
      SassParser parser(*this, source);
      root = parser.parse();
    }
    else {
      // create a parser instance from the given c_str buffer
      ScssParser parser(*this, source);
      root = parser.parse();
    }

    StyleSheet stylesheet(source, root);
    stylesheet.syntax = source->getType();
    stylesheet.plainCss = isPlainCss;

    // remove current stack frame
    import_stack.pop_back();
    importStack.pop_back();

    // create key/value pair for ast node
    std::pair<const sass::string, StyleSheet>
      ast_pair(abs_path, stylesheet);

    // register the result
    sheets.insert(ast_pair);

  }

  // register include with resolved path and its content
  // memory of the resources will be freed by us on exit
  void Context::register_resource(const Include& inc, char* contents, char* srcmap)
  {

    // get pointer to the loaded content
    SassImportPtr import = sass_make_import(
      inc.imp_path.c_str(),
      inc.abs_path.c_str(),
      contents, srcmap,
      inc.type
    );

    register_import(import);

    // delete memory of current stack frame
    sass_delete_import(import);

  }

  // register include with resolved path and its content
  // memory of the resources will be freed by us on exit
  void Context::register_resource(const Include& inc, char* contents, char* srcmap, SourceSpan& prstate)
  {
    // Add a call stack frame
    callStackFrame frame(*logger,
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
      error(msg_stream.str(), pstate, *logger);
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
    return { imp, "", SASS_IMPORT_AUTO };

  }

  /*#########################################################################*/
  // @param imp_path The relative or custom path for be imported
  // @param pstate SourceSpan where import occurred (parent context)
  // @param rule The backing ImportRule that is added to the document
  // @param importers Array of custom importers/headers to go through
  // @param singleton Whether to use all importers or only first successful
  /*#########################################################################*/
  bool Context::callCustomLoader(const sass::string& imp_path, SourceSpan& pstate,
    ImportRule* rule, const sass::vector<SassImporterPtr>& importers, bool singleton)
  {
    // unique counter
    size_t count = 0;
    // need one correct import
    bool has_import = false;

    const char* ctx_path = pstate.getAbsPath();

    // Process custom importers and headers.
    // They must be presorted by priorities.
    for (SassImporterPtr importer : importers) {
      // Get the external importer function
      SassImporterLambdaCpp fn = sass_importer_get_function(importer);
      // Call the external function, then check what it returned
      SassImportListPtr includes = fn(imp_path.c_str(), importer, c_compiler);
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
            if (line == sass::string::npos) error(err_message, pstate, *logger);
            else if (column == sass::string::npos) error(err_message, pstate, *logger);
            else error(err_message, { source, Offset::init(line, column) }, *logger);
          }
          // Content for import was set.
          // No need to load it ourself.
          else if (sass_import_get_source(import)) {
            // Resolved abs_path should be set by custom importer
            // Use the created uniq_path as fall-back (enforce?)
            sass::string path_key(abs_path ? abs_path : uniq_path);
            // Create import statement in the document
            Include include(importer, path_key, source->getType());
            auto statement = SASS_MEMORY_NEW(DynamicImport, pstate, path_key);
            rule->append(SASS_MEMORY_NEW(IncludeImport, statement, include));
            // Parse to stylesheet
            register_import(import);
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
                rule->pstate(), *logger);
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

  sass::string Context::render(Block_Obj root)
  {
    // check for valid block
    if (!root) return 0;
    // start the render process
    root->perform(&emitter);
    // finish emitter stream
    emitter.finalize();
    // get the resulting buffer from stream
    OutputBuffer& emitted = emitter.get_buffer();
    // should we append a source map url?
    if (!c_options.omit_source_map_url) {
      // generate an embedded source map
      if (c_options.source_map_embed) {
        emitted.buffer += linefeed;
        emitted.buffer += format_embedded_source_map();
      }
      // or just link the generated one
      else if (!source_map_file88.empty()) {
        emitted.buffer += linefeed;
        emitted.buffer += format_source_mapping_url(source_map_file88);
      }
    }
    // create a copy of the resulting buffer string
    // this must be freed or taken over by implementor
    return emitted.buffer;
  }

  void Context::apply_custom_headers2(sass::vector<StatementObj>& statements, SourceSpan pstate)
  {
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
  }

  Block_Obj File_Context::parse(Sass_Import_Type type)
  {

    // check if entry file is given
    if (input_path88.empty()) return {};

    // create absolute path from input filename
    // ToDo: this should be resolved via custom importers
    sass::string abs_path(rel2abs(input_path88, CWD, CWD));

    // try to load the entry file
    char* contents = slurp_file(abs_path, CWD);

    // alternatively also look inside each include path folder
    // I think this differs from ruby sass (IMO too late to remove)
    for (size_t i = 0, S = include_paths88.size(); contents == 0 && i < S; ++i) {
      // build absolute path for this include path entry
      abs_path = rel2abs(input_path88, include_paths88[i], CWD);
      // try to load the resulting path
      contents = slurp_file(abs_path, CWD);
    }

    // abort early if no content could be loaded (various reasons)
    if (!contents) throw std::runtime_error("File to read not found or unreadable: " + std::string(input_path88.c_str()));

    // store entry path
    entry_path88 = abs_path;

    return compileImport(sass_make_import(
      input_path88.c_str(),
      abs_path.c_str(),
      contents,
      0, type
    ));

  }

  SassImportPtr ADDCALL sass_make_file_import(const char* input_path88)
  {
    // check if entry file is given
    if (input_path88 == nullptr) return {};

    // create absolute path from input filename
    // ToDo: this should be resolved via custom importers
    sass::string abs_path(rel2abs(input_path88, ".", "."));

    // try to load the entry file
    char* contents = slurp_file(abs_path, ".");
/*
    // alternatively also look inside each include path folder
    // I think this differs from ruby sass (IMO too late to remove)
    for (size_t i = 0, S = include_paths88.size(); contents == 0 && i < S; ++i) {
      // build absolute path for this include path entry
      abs_path = rel2abs(input_path88, include_paths88[i], CWD);
      // try to load the resulting path
      contents = slurp_file(abs_path, CWD);
    }
    */
    // abort early if no content could be loaded (various reasons)
    if (!contents) throw std::runtime_error("File to read not found or unreadable: " + std::string(input_path88));

    // store entry path
    // entry_path88 = abs_path;

    return sass_make_import(
      input_path88,
      abs_path.c_str(),
      contents,
      0, SASS_IMPORT_AUTO
    );

  }

  SassImportPtr ADDCALL sass_make_data_import(char* source_string)
  {
    return nullptr;
  }

  Block_Obj Data_Context::parse(Sass_Import_Type type)
  {

    // check if source string is given
    if (source_c_str.empty()) return {};

    // remember entry path (defaults to stdin for string)
    entry_path88 = input_path88.empty() ? "stdin" : input_path88;

    // ToDo: this may be resolved via custom importers
    sass::string abs_path(rel2abs(entry_path88, ".", CWD));

    return compileImport(sass_make_import(
      input_path88.c_str(),
      input_path88.c_str(),
      sass_copy_string(source_c_str),
      sass_copy_string(srcmap_c_str),
      type
    ));

  }

  Block_Obj Context::compileImport(SassImportPtr import)
  {
    // add the entry to the stack
    import_stack.emplace_back(import);
    // importStack2.emplace_back(source);

    // Prepare environment
    prepareEnvironment();

    // load and register import
    register_import(import);

    importStack.emplace_back(sources.back());

    // create root ast tree node
    return compile();

  }

  void Context::prepareEnvironment()
  {

    loadBuiltInFunctions();

    varStack.push_back(&varRoot);

    // register custom functions (defined via C-API)
    for (size_t i = 0, S = c_functions88.size(); i < S; ++i)
    {
      ScopedStackFrame<EnvFrame> scoped(varStack, &varRoot);
      registerCustomFunction(c_functions88[i]);
    }


  }

  // parse root block from includes
  Block_Obj Context::compile()
  {

    // abort if there is no data
    if (sources.size() == 0) return {};
    // get root block from the first style sheet
    StyleSheet sheet = sheets.at(entry_path88);
    Block_Obj root = sheet.root;
    // abort on invalid root
    if (root.isNull()) return {};

    //    debug_ast(root);
    
    Eval eval(*this);
    eval.plainCss = sheet.plainCss;
    EnvScope scoped(varRoot, varRoot.getIdxs());
    for (size_t i = 0; i < fnCache.size(); i++) {
      varRoot.functions[i] = fnCache[i];
    }
    root = eval.visitRootBlock99(root); // 50%
//    debug_ast(root);

    Extension unsatisfied;
    // check that all extends were used
    if (extender.checkForUnsatisfiedExtends(unsatisfied)) {
      throw Exception::UnsatisfiedExtend(*logger, unsatisfied);
    }

    // This can use up to 10% runtime
    Cssize cssize(*this);
    root = cssize(root); // 5%

    // clean up by removing empty placeholders
    // ToDo: maybe we can do this somewhere else?
    Remove_Placeholders remove_placeholders;
    root->perform(&remove_placeholders); // 3%

    // return processed tree
    return root;
  }
  // EO compile

  sass::string Context::format_embedded_source_map()
  {
    sass::string map = emitter.render_srcmap(*this);
    std::istringstream is( map.c_str() );
    std::ostringstream buffer;
    base64::encoder E;
    E.encode(is, buffer);
    sass::string url = "data:application/json;base64,";
    url += buffer.str().c_str();
    url.erase(url.size() - 1);
    return "/*# sourceMappingURL=" + url + " */";
  }

  sass::string Context::format_source_mapping_url(const sass::string& file)
  {
    sass::string url = abs2rel(file, output_path88, CWD);
    return "/*# sourceMappingURL=" + url + " */";
  }

  sass::string Context::render_srcmap()
  {
    if (source_map_file88.empty()) return "";
    return emitter.render_srcmap(*this);
  }

  sass::string Context::render_stderr()
  {
    return logger->errors.str();
  }

  // for data context we want to start after "stdin"
  // we probably always want to skip the header includes?
  sass::vector<sass::string> Context::get_included_files(bool skip, size_t headers)
  {
    // create a copy of the vector for manipulations
    sass::vector<sass::string> includes = included_files88;
    if (includes.size() == 0) return includes;
    if (skip) { includes.erase(includes.begin(), includes.begin() + 1 + headers); }
    else { includes.erase(includes.begin() + 1, includes.begin() + 1 + headers); }
    includes.erase(std::unique(includes.begin(), includes.end()), includes.end());
    std::sort(includes.begin() + (skip ? 0 : 1), includes.end());
    return includes;
  }




}


struct SassContextReal* ADDCALL sass_make_context()
{
  return reinterpret_cast<SassContextReal*>(new Sass::Context());
}

void ADDCALL sass_context_set_precision(struct SassContextReal* context, int precision)
{
  reinterpret_cast<Sass::Context*>(context)->precision = precision;
}
void ADDCALL sass_context_set_output_style(struct SassContextReal* context, enum Sass_Output_Style output_style)
{
  reinterpret_cast<Sass::Context*>(context)->c_options.output_style = output_style;
}

int ADDCALL sass_context_get_precision(struct SassContextReal* context)
{
  return reinterpret_cast<Sass::Context*>(context)->precision;
}
enum Sass_Output_Style ADDCALL sass_context_get_output_style(struct SassContextReal* context)
{
  return reinterpret_cast<Sass::Context*>(context)->c_options.output_style;
}

bool ADDCALL sass_context_get_source_comments(struct SassContextReal* context)
{
  return reinterpret_cast<Sass::Context*>(context)->source_comments;
}

bool ADDCALL sass_context_get_source_map_embed(struct SassContextReal* context)
{
  return reinterpret_cast<Sass::Context*>(context)->source_map_embed;
}
bool ADDCALL sass_context_get_source_map_contents(struct SassContextReal* context)
{
  return reinterpret_cast<Sass::Context*>(context)->source_map_contents;
}

bool ADDCALL sass_context_get_source_map_file_urls(struct SassContextReal* context)
{
  return reinterpret_cast<Sass::Context*>(context)->source_comments;
}

bool ADDCALL sass_context_get_omit_source_map_url(struct SassContextReal* context)
{
  return reinterpret_cast<Sass::Context*>(context)->omit_source_map_url;
}

void ADDCALL sass_context_set_source_comments(struct SassContextReal* context, bool source_comments)
{
  reinterpret_cast<Sass::Context*>(context)->source_comments = source_comments;
}
void ADDCALL sass_context_set_source_map_embed(struct SassContextReal* context, bool source_map_embed)
{
  reinterpret_cast<Sass::Context*>(context)->source_map_embed = source_map_embed;
}
void ADDCALL sass_context_set_source_map_contents(struct SassContextReal* context, bool source_map_contents)
{
  reinterpret_cast<Sass::Context*>(context)->source_map_contents = source_map_contents;
}
void ADDCALL sass_context_set_source_map_file_urls(struct SassContextReal* context, bool source_map_file_urls)
{
  reinterpret_cast<Sass::Context*>(context)->source_map_file_urls = source_map_file_urls;
}
void ADDCALL sass_context_set_omit_source_map_url(struct SassContextReal* context, bool omit_source_map_url)
{
  reinterpret_cast<Sass::Context*>(context)->omit_source_map_url = omit_source_map_url;
}

const char* ADDCALL sass_context_get_input_path(struct SassContextReal* context)
{
  return reinterpret_cast<Sass::Context*>(context)->input_path.c_str();
}

const char* ADDCALL sass_context_get_output_path(struct SassContextReal* context)
{
  return reinterpret_cast<Sass::Context*>(context)->output_path.c_str();
}

const char* ADDCALL sass_context_get_source_map_file(struct SassContextReal* context)
{
  return reinterpret_cast<Sass::Context*>(context)->source_map_file.c_str();
}
const char* ADDCALL sass_context_get_source_map_root(struct SassContextReal* context)
{
  return reinterpret_cast<Sass::Context*>(context)->source_map_root.c_str();
}


void ADDCALL sass_context_set_input_path(struct SassContextReal* context, const char* input_path)
{
  reinterpret_cast<Sass::Context*>(context)->input_path = input_path;
}
void ADDCALL sass_context_set_output_path(struct SassContextReal* context, const char* output_path)
{
  reinterpret_cast<Sass::Context*>(context)->output_path = output_path;
}

void ADDCALL sass_context_set_source_map_file(struct SassContextReal* context, const char* source_map_file)
{
  reinterpret_cast<Sass::Context*>(context)->source_map_file = source_map_file;
}
void ADDCALL sass_context_set_source_map_root(struct SassContextReal* context, const char* source_map_root)
{
  reinterpret_cast<Sass::Context*>(context)->source_map_root = source_map_root;
}
