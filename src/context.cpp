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

  inline bool sort_importers (const Sass_Importer_Entry& i, const Sass_Importer_Entry& j)
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

  Context::Context(struct Sass_Context& c_ctx)
  : CWD(File::get_cwd()),
    c_options(c_ctx),
    entry_path(""),
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
    c_headers               (sass::vector<Sass_Importer_Entry>()),
    c_importers             (sass::vector<Sass_Importer_Entry>()),
    c_functions             (sass::vector<Sass_Function_Entry>()),

    // Get some common options with and few default
    indent                  (safe_str(c_options.indent, "  ")),
    linefeed                (safe_str(c_options.linefeed, "\n")),

    input_path              (make_canonical_path(safe_input(c_options.input_path))),
    output_path             (make_canonical_path(safe_output(c_options.output_path, input_path))),
    source_map_file         (make_canonical_path(safe_str(c_options.source_map_file, ""))),
    source_map_root         (make_canonical_path(safe_str(c_options.source_map_root, "")))

  {

    // Sass 3.4: The current working directory will no longer be placed onto the Sass load path by default.
    // If you need the current working directory to be available, set SASS_PATH=. in your shell's environment.
    // Or add it explicitly in your implementation, e.g. include_paths.emplace_back(CWD or '.');

    // collect more paths from different options
    collect_include_paths(c_options.include_path);
    collect_include_paths(c_options.include_paths);
    collect_plugin_paths(c_options.plugin_path);
    collect_plugin_paths(c_options.plugin_paths);

    // load plugins and register custom behaviors
    for(auto plug : plugin_paths) plugins.load_plugins(plug);
    for(auto fn : plugins.get_headers()) c_headers.emplace_back(fn);
    for(auto fn : plugins.get_importers()) c_importers.emplace_back(fn);
    for(auto fn : plugins.get_functions()) c_functions.emplace_back(fn);

    // sort the items by priority (lowest first)
    sort (c_headers.begin(), c_headers.end(), sort_importers);
    sort (c_importers.begin(), c_importers.end(), sort_importers);

    emitter.set_filename(abs2rel(output_path, source_map_file, CWD));

  }

  void Context::add_c_function(Sass_Function_Entry function)
  {
    c_functions.emplace_back(function);
  }
  void Context::add_c_header(Sass_Importer_Entry header)
  {
    c_headers.emplace_back(header);
    // need to sort the array afterwards (no big deal)
    sort (c_headers.begin(), c_headers.end(), sort_importers);
  }
  void Context::add_c_importer(Sass_Importer_Entry importer)
  {
    c_importers.emplace_back(importer);
    // need to sort the array afterwards (no big deal)
    sort (c_importers.begin(), c_importers.end(), sort_importers);
  }

  Context::~Context()
  {
    // free all strings we kept alive during compiler execution
    // for (size_t n = 0; n < strings.size(); ++n) free(strings[n]);
    // everything that gets put into sources will be freed by us
    // this shouldn't have anything in it anyway!?
    for (size_t m = 0; m < import_stack.size(); ++m) {
      // sass_import_take_source(import_stack[m]);
      // sass_import_take_srcmap(import_stack[m]);
      sass_delete_import(import_stack[m]);
    }
    // clear inner structures (vectors) and input source
    import_stack.clear();
    importStack.clear();
    sources.clear();
    sheets.clear();
    functions.clear();
    // builtins.clear();
    // externals.clear();
    // release logger
    delete logger;
  }

  Data_Context::~Data_Context()
  {
  }

  File_Context::~File_Context()
  {
  }

  void Context::collect_include_paths(const char* paths_str)
  {
    if (paths_str == nullptr) return;
    sass::vector<sass::string> paths =
      StringUtils::split(paths_str, PATH_SEP, true);
    for (sass::string path : paths) {
      if (*path.rbegin() != '/') path += '/';
      include_paths.emplace_back(path);
    }
  }

  void Context::collect_include_paths(string_list* paths_array)
  {
    while (paths_array)
    {
      collect_include_paths(paths_array->string);
      paths_array = paths_array->next;
    }
  }

  void Context::collect_plugin_paths(const char* paths_str)
  {
    if (paths_str == nullptr) return;
    sass::vector<sass::string> paths =
      StringUtils::split(paths_str, PATH_SEP, true);
    for (sass::string path : paths) {
      if (*path.rbegin() != '/') path += '/';
      plugin_paths.emplace_back(path);
    }
  }

  void Context::collect_plugin_paths(string_list* paths_array)
  {
    while (paths_array)
    {
      collect_plugin_paths(paths_array->string);
      paths_array = paths_array->next;
    }
  }

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

  void Context::register_import(const Include& inc, Sass_Import_Entry& import)
  {

    // get index for this resource
    SourceDataObj source = import->srcdata;
    size_t idx = sources.size();
    source->setSrcId(idx);

    // Append to the resources
    sources.emplace_back(source);

    // add a relative link to the working directory
    included_files.emplace_back(inc.abs_path);

    // tell emitter about new resource
    emitter.add_source_index(idx);


    // add a relative link to the source map output file
    srcmap_links.emplace_back(abs2rel(inc.abs_path, source_map_file, CWD));

    if (StringUtils::endsWithIgnoreCase(inc.abs_path, ".css", 4)) {
      source->setType(SASS_IMPORT_CSS);
    }
    else if (StringUtils::endsWithIgnoreCase(inc.abs_path, ".sass", 5)) {
      source->setType(SASS_IMPORT_SASS);
    }
    else {
      source->setType(SASS_IMPORT_SCSS);
    }

    // add the entry to the stack
    import_stack.emplace_back(import);
    importStack.emplace_back(sources.back());

    // get pointer to the loaded content

  // This should already be File(Re)Sources
    // const char* contents = sources[idx]->begin();

    // check existing import stack for possible recursion
    for (size_t i = 0; i < import_stack.size() - 2; ++i) {
      Sass_Import_Entry parent = import_stack[i];
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
        // error(stack, prstate ? *prstate : pstate, import_stack);
      }
    }

    Block_Obj root;

    bool isPlainCss = false;

    // callStackFrame frame(*logger,
    //   BackTrace("[import]", "@import"));

    // auto source = SASS_MEMORY_NEW(SourceFile,
    //   inc, contents, idx);

    if (import->srcdata->getType() == SASS_IMPORT_CSS)
    {
      CssParser parser(*this, source);
      // take control of these buffers
      // sass_import_take_source(import);
      // sass_import_take_srcmap(import);
      // then parse the root block
      root = parser.parse7();
      // mark as pure css
      isPlainCss = true;
    }
    else if (import->srcdata->getType() == SASS_IMPORT_SASS)
    {
      SassParser parser(*this, source);
      // do not yet dispose these buffers
      // sass_import_take_source(import);
      // sass_import_take_srcmap(import);
      // then parse the root block
      root = parser.parse7();
    }
    else {
      // create a parser instance from the given c_str buffer
      ScssParser parser(*this, source);
      // do not yet dispose these buffers
      // sass_import_take_source(import);
      // sass_import_take_srcmap(import);
      // then parse the root block
      root = parser.parse7();
    }

    StyleSheet stylesheet(source, root);
    stylesheet.plainCss = isPlainCss;
    stylesheet.syntax = import->srcdata->getType();

    // delete memory of current stack frame
    sass_delete_import(import);
    // remove current stack frame
    import_stack.pop_back();
    importStack.pop_back();

    // create key/value pair for ast node
    std::pair<const sass::string, StyleSheet>
      ast_pair(inc.abs_path, stylesheet);
    // register resulting resource
    sheets.insert(ast_pair);

  }

  // register include with resolved path and its content
  // memory of the resources will be freed by us on exit
  void Context::register_resource(const Include& inc, char* contents, char* srcmap)
  {

    // get pointer to the loaded content
    Sass_Import_Entry import = sass_make_import(
      inc.imp_path.c_str(),
      inc.abs_path.c_str(),
      contents,
      srcmap
    );

    register_import(inc, import);

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
      bool use_cache = c_importers.size() == 0;
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

  // call custom importers on the given (unquoted) load_path and eventually parse the resulting style_sheet
  bool Context::call_loader2(const sass::string& load_path, const char* ctx_path, SourceSpan& pstate, ImportRule* rule, sass::vector<Sass_Importer_Entry> importers, bool only_one)
  {
    // unique counter
    size_t count = 0;
    // need one correct import
    bool has_import = false;
    // process all custom importers (or custom headers)
    for (Sass_Importer_Entry& importer_ent : importers) {
      // int priority = sass_importer_get_priority(importer);
      Sass_Importer_Fn fn = sass_importer_get_function(importer_ent);
      // skip importer if it returns NULL
      if (Sass_Import_List includes =
        fn(load_path.c_str(), importer_ent, c_compiler)
        ) {
        // get c pointer copy to iterate over
        Sass_Import_List it_includes = includes;
        while (*it_includes) {
          ++count;
          // create unique path to use as key
          sass::string uniq_path = load_path;
          if (!only_one && count) {
            sass::sstream path_strm;
            path_strm << uniq_path << ":" << count;
            uniq_path = path_strm.str();
          }
          // create the importer struct
          Importer importer(uniq_path, ctx_path);
          // query data from the current include
          Sass_Import_Entry include_ent = *it_includes;
          char* source = sass_copy_c_string(sass_import_get_source(include_ent));
          char* srcmap = sass_copy_c_string(sass_import_get_srcmap(include_ent));
          size_t line = sass_import_get_error_line(include_ent);
          size_t column = sass_import_get_error_column(include_ent);
          const char* abs_path = sass_import_get_abs_path(include_ent);
          // handle error message passed back from custom importer
          // it may (or may not) override the line and column info
          if (const char* err_message = sass_import_get_error_message(include_ent)) {
            if (source || srcmap) register_resource({ importer, uniq_path, include_ent->srcdata->getType() }, source, srcmap, pstate);
            if (line == sass::string::npos && column == sass::string::npos) error(err_message, pstate, *logger);
            // else error(err_message, SourceSpan(ctx_path, source, Position(line, column)), *logger);
            else {
              std::cerr << "re implement this\n";
              exit(1);
            }
          }
          // content for import was set
          else if (source) {
            // resolved abs_path should be set by custom importer
            // use the created uniq_path as fall-back (maybe enforce)
            sass::string path_key(abs_path ? abs_path : uniq_path);
            // create the importer struct
            Include include(importer, path_key, include_ent->srcdata->getType());
            // attach information to AST node

            auto dyn = SASS_MEMORY_NEW(DynamicImport, pstate, load_path);
            rule->append(SASS_MEMORY_NEW(IncludeImport, dyn, include));

            // imp->incs().emplace_back(include);

            // register the resource buffers
            register_resource(include, source, srcmap, pstate);
          }
          // only a path was returned
          // try to load it like normal
          else if (abs_path) {
            // checks some urls to preserve
            // `http://`, `https://` and `//`
            // or dispatches to `import_file`
            // which will check for a `.css` extension
            // or resolves the file on the file-system
            // added and resolved via `add_file`
            // finally stores everything on `imp`

            SourceSpan pstate(rule->pstate());
            sass::string imp_path(unquote(load_path));
            sass::string protocol("file");
            const Importer importer(imp_path, ctx_path);

            Include include(load_import(importer, pstate));
            auto dyn = SASS_MEMORY_NEW(DynamicImport, pstate, imp_path);
            rule->append(SASS_MEMORY_NEW(IncludeImport, dyn, include));

            if (include.abs_path.empty()) {
              error("Can't find stylesheet to import.",
                rule->pstate(), *logger);
            }

          }
          // move to next
          ++it_includes;
        }
        // deallocate the returned memory
        sass_delete_import_list(includes);
        // set success flag
        has_import = true;
        // break out of loop
        if (only_one) break;
      }
    }
    // return result
    return has_import;
  }
  void register_built_in_functions(Context&);
  void register_c_function2(Context&, Sass_Function_Entry);

  char* Context::render(Block_Obj root)
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
      else if (source_map_file != "") {
        emitted.buffer += linefeed;
        emitted.buffer += format_source_mapping_url(source_map_file);
      }
    }
    // create a copy of the resulting buffer string
    // this must be freed or taken over by implementor
    return sass_copy_c_string(emitted.buffer.c_str());
  }

  void Context::apply_custom_headers2(sass::vector<StatementObj>& statements, SourceSpan pstate)
  {
    // create a custom import to resolve headers
    ImportRuleObj rule = SASS_MEMORY_NEW(ImportRule, pstate);
    // dispatch headers which will add custom functions
    // custom headers are added to the import instance
    call_headers2(entry_path, pstate.getAbsPath(), pstate, rule);
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
    if (input_path.empty()) return {};

    // create absolute path from input filename
    // ToDo: this should be resolved via custom importers
    sass::string abs_path(rel2abs(input_path, CWD, CWD));

    // try to load the entry file
    char* contents = slurp_file(abs_path, CWD);

    // alternatively also look inside each include path folder
    // I think this differs from ruby sass (IMO too late to remove)
    for (size_t i = 0, S = include_paths.size(); contents == 0 && i < S; ++i) {
      // build absolute path for this include path entry
      abs_path = rel2abs(input_path, include_paths[i], CWD);
      // try to load the resulting path
      contents = slurp_file(abs_path, CWD);
    }

    // abort early if no content could be loaded (various reasons)
    if (!contents) throw std::runtime_error("File to read not found or unreadable: " + std::string(input_path.c_str()));

    // store entry path
    entry_path = abs_path;

    // create entry only for import stack
    Sass_Import_Entry import = sass_make_import(
      input_path.c_str(),
      entry_path.c_str(),
      contents,
      0
    );
    // add the entry to the stack
    import_stack.emplace_back(import);

    // Prepare environment
    prepareEnvironment();

    // create the source entry for file entry
    register_resource({{ input_path, "." }, abs_path, type }, contents, 0);

    importStack.emplace_back(sources.back());


    // create root ast tree node
    return compile();

  }

  Block_Obj Data_Context::parse(Sass_Import_Type type)
  {

    // check if source string is given
    if (!source_c_str) return {};

    // convert indented sass syntax
    // c_options.is_indented_syntax_src

    // remember entry path (defaults to stdin for string)
    entry_path = input_path.empty() ? "stdin" : input_path;

    // ToDo: this may be resolved via custom importers
    sass::string abs_path(rel2abs(entry_path, ".", CWD));

    // create entry only for the import stack
    Sass_Import_Entry import = sass_make_import(
      entry_path.c_str(),
      abs_path.c_str(),
      source_c_str,
      srcmap_c_str
    );
    // add the entry to the stack
    import_stack.emplace_back(import);
    // importStack2.emplace_back(source);

    // Prepare environment
    prepareEnvironment();

    // register a synthetic resource (path does not really exist, skip in includes)
    register_resource({{ input_path, "." }, input_path, type }, source_c_str, srcmap_c_str);

    importStack.emplace_back(sources.back());

    // create root ast tree node
    return compile();
  }

  void Context::prepareEnvironment()
  {

    // register built-in functions on env
    register_built_in_functions(*this);

    varStack.push_back(&varRoot);

    // register custom functions (defined via C-API)
    for (size_t i = 0, S = c_functions.size(); i < S; ++i)
    {
      ScopedStackFrame<EnvFrame> scoped(varStack, &varRoot);
      register_c_function2(*this, c_functions[i]);
    }


  }

  // parse root block from includes
  Block_Obj Context::compile()
  {

    // abort if there is no data
    if (sources.size() == 0) return {};
    // get root block from the first style sheet
    StyleSheet sheet = sheets.at(entry_path);
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
    sass::string url = abs2rel(file, output_path, CWD);
    return "/*# sourceMappingURL=" + url + " */";
  }

  char* Context::render_srcmap()
  {
    if (source_map_file == "") return 0;
    sass::string map = emitter.render_srcmap(*this);
    return sass_copy_c_string(map.c_str());
  }

  char* Context::render_stderr()
  {
    sass::string err = logger->errors.str();
    return sass_copy_c_string(err.c_str());
  }

  // for data context we want to start after "stdin"
  // we probably always want to skip the header includes?
  sass::vector<sass::string> Context::get_included_files(bool skip, size_t headers)
  {
    // create a copy of the vector for manipulations
    sass::vector<sass::string> includes = included_files;
    if (includes.size() == 0) return includes;
    if (skip) { includes.erase(includes.begin(), includes.begin() + 1 + headers); }
    else { includes.erase(includes.begin() + 1, includes.begin() + 1 + headers); }
    includes.erase(std::unique(includes.begin(), includes.end()), includes.end());
    std::sort(includes.begin() + (skip ? 0 : 1), includes.end());
    return includes;
  }


  void register_built_in_function(Context& ctx, sass::string name, const sass::string& signature, SassFnSig cb)
  {
    auto source = SASS_MEMORY_NEW(SourceString, true,
      "sass://signature", "(" + signature + ")", -1);
    ArgumentDeclaration* args = ArgumentDeclaration::parse(ctx, source);
    BuiltInCallable* callable = SASS_MEMORY_NEW(BuiltInCallable, name, args, cb);
    ctx.functions.insert(std::make_pair(name, callable));
    ctx.varRoot.createFunction(name);
    ctx.fnCache.push_back(callable);
  }

  void register_built_in_overloads(Context& ctx, sass::string name,
    std::vector<std::pair<sass::string, SassFnSig>> overloads)
  {
    SassFnPairs pairs;
    for (auto overload : overloads) {
      SourceDataObj source = SASS_MEMORY_NEW(SourceString, true,
        "sass://signature", "(" + overload.first + ")", -1);
      ArgumentDeclaration* args = ArgumentDeclaration::parse(ctx, source);
      pairs.emplace_back(std::make_pair(args, overload.second));
    }
    auto callable = SASS_MEMORY_NEW(BuiltInCallables, name, pairs);
    ctx.functions.insert(std::make_pair(name, callable));
    ctx.varRoot.createFunction(name);
    ctx.fnCache.push_back(callable);
  }

  union Sass_Value* customSassFn(
    const union Sass_Value* s_args,
    void* cookie
  ) {
    return sass_clone_value(s_args);
  }

  union Sass_Value* call_fn_foo(const union Sass_Value* v, Sass_Function_Entry cb, struct Sass_Compiler* compiler)
  {
    // we actually abuse the void* to store an "int"
    return sass_clone_value(v);
  }

  void register_built_in_functions(Context& ctx)
  {
    using namespace Functions;

    // EnvFrame local(&ctx.varRoot, true);
    // ScopedStackFrame<EnvFrame>
    //   scoped(ctx.varStack.root, &local);

    // List Functions
    register_built_in_function(ctx, "length", "$list", Functions::Lists::length);
    register_built_in_function(ctx, "nth", "$list, $n", Functions::Lists::nth);
    register_built_in_function(ctx, "set-nth", "$list, $n, $value", Functions::Lists::setNth);
    register_built_in_function(ctx, "join", "$list1, $list2, $separator: auto, $bracketed: auto", Functions::Lists::join);
    register_built_in_function(ctx, "append", "$list, $val, $separator: auto", Functions::Lists::append);
    register_built_in_function(ctx, "zip", "$lists...", Functions::Lists::zip);
    register_built_in_function(ctx, "index", "$list, $value", Functions::Lists::index);
    register_built_in_function(ctx, "list-separator", "$list", Functions::Lists::separator);
    register_built_in_function(ctx, "is-bracketed", "$list", Functions::Lists::isBracketed);

    // Map Functions
    register_built_in_function(ctx, "map-get", "$map, $key", Functions::Maps::get);
    register_built_in_function(ctx, "map-merge", "$map1, $map2", Functions::Maps::merge);
    register_built_in_overloads(ctx, "map-remove", {
        std::make_pair("$map", Functions::Maps::remove_one),
        std::make_pair("$map, $key, $keys...", Functions::Maps::remove_many)
      });

    // register_built_in_function(ctx, "remove", "$list", Functions::Maps::remove); // overloaded
    register_built_in_function(ctx, "map-keys", "$map", Functions::Maps::keys);
    register_built_in_function(ctx, "map-values", "$map", Functions::Maps::values);
    register_built_in_function(ctx, "map-has-key", "$map, $key", Functions::Maps::hasKey);

    // Math Functions
    register_built_in_function(ctx, "round", "$number", Functions::Math::round);
    register_built_in_function(ctx, "ceil", "$number", Functions::Math::ceil);
    register_built_in_function(ctx, "floor", "$number", Functions::Math::floor);
    register_built_in_function(ctx, "abs", "$number", Functions::Math::abs);
    register_built_in_function(ctx, "max", "$numbers...", Functions::Math::max);
    register_built_in_function(ctx, "min", "$numbers...", Functions::Math::min);
    register_built_in_function(ctx, "random", "$limit: null", Functions::Math::random);
    register_built_in_function(ctx, "unit", "$number", Functions::Math::unit);
    register_built_in_function(ctx, "percentage", "$number", Functions::Math::percentage);
    register_built_in_function(ctx, "unitless", "$number", Functions::Math::isUnitless);
    register_built_in_function(ctx, "comparable", "$number1, $number2", Functions::Math::compatible);

    // String functions
    register_built_in_function(ctx, "unquote", "$string", Functions::Strings::unquote);
    register_built_in_function(ctx, "quote", "$string", Functions::Strings::quote);
    register_built_in_function(ctx, "to-upper-case", "$string", Functions::Strings::toUpperCase);
    register_built_in_function(ctx, "to-lower-case", "$string", Functions::Strings::toLowerCase);
    register_built_in_function(ctx, "str-length", "$string", Functions::Strings::length);
    register_built_in_function(ctx, "str-insert", "$string, $insert, $index", Functions::Strings::insert);
    register_built_in_function(ctx, "str-index", "$string, $substring", Functions::Strings::index);
    register_built_in_function(ctx, "str-slice", "$string, $start-at, $end-at: -1", Functions::Strings::slice);
    register_built_in_function(ctx, "unique-id", "", Functions::Strings::uniqueId);

    // Color functions
    register_built_in_overloads(ctx, "rgb", {
        std::make_pair("$red, $green, $blue, $alpha", Functions::Colors::rgb_4),
        std::make_pair("$red, $green, $blue", Functions::Colors::rgb_3),
        std::make_pair("$color, $alpha", Functions::Colors::rgb_2),
        std::make_pair("$channels", Functions::Colors::rgb_1),
      });
    register_built_in_overloads(ctx, "rgba", {
        std::make_pair("$red, $green, $blue, $alpha", Functions::Colors::rgba_4),
        std::make_pair("$red, $green, $blue", Functions::Colors::rgba_3),
        std::make_pair("$color, $alpha", Functions::Colors::rgba_2),
        std::make_pair("$channels", Functions::Colors::rgba_1),
      });
    register_built_in_overloads(ctx, "hsl", {
        std::make_pair("$hue, $saturation, $lightness, $alpha", Functions::Colors::hsl_4),
        std::make_pair("$hue, $saturation, $lightness", Functions::Colors::hsl_3),
        std::make_pair("$color, $alpha", Functions::Colors::hsl_2),
        std::make_pair("$channels", Functions::Colors::hsl_1),
      });
    register_built_in_overloads(ctx, "hsla", {
        std::make_pair("$hue, $saturation, $lightness, $alpha", Functions::Colors::hsla_4),
        std::make_pair("$hue, $saturation, $lightness", Functions::Colors::hsla_3),
        std::make_pair("$color, $alpha", Functions::Colors::hsla_2),
        std::make_pair("$channels", Functions::Colors::hsla_1),
      });

    register_built_in_function(ctx, "red", "$color", Functions::Colors::red);
    register_built_in_function(ctx, "green", "$color", Functions::Colors::green);
    register_built_in_function(ctx, "blue", "$color", Functions::Colors::blue);
    register_built_in_function(ctx, "hue", "$color", Functions::Colors::hue);
    register_built_in_function(ctx, "lightness", "$color", Functions::Colors::lightness);
    register_built_in_function(ctx, "saturation", "$color", Functions::Colors::saturation);
    register_built_in_function(ctx, "invert", "$color, $weight: 100%", Functions::Colors::invert);
    register_built_in_function(ctx, "grayscale", "$color", Functions::Colors::grayscale);
    register_built_in_function(ctx, "complement", "$color", Functions::Colors::complement);
    register_built_in_function(ctx, "lighten", "$color, $amount", Functions::Colors::lighten);
    register_built_in_function(ctx, "darken", "$color, $amount", Functions::Colors::darken);
    register_built_in_function(ctx, "desaturate", "$color, $amount", Functions::Colors::desaturate);
    register_built_in_overloads(ctx, "saturate", {
        std::make_pair("$amount", Functions::Colors::saturate_1),
        std::make_pair("$color, $amount", Functions::Colors::saturate_2),
      });
    
    register_built_in_function(ctx, "adjust-hue", "$color, $degrees", Functions::Colors::adjustHue);
    register_built_in_function(ctx, "adjust-color", "$color, $kwargs...", Functions::Colors::adjust);
    register_built_in_function(ctx, "change-color", "$color, $kwargs...", Functions::Colors::change);
    register_built_in_function(ctx, "scale-color", "$color, $kwargs...", Functions::Colors::scale);
    register_built_in_function(ctx, "mix", "$color1, $color2, $weight: 50%", Functions::Colors::mix);

    // Opacity functions
    register_built_in_function(ctx, "opacify", "$color, $amount", Functions::Colors::opacify);
    register_built_in_function(ctx, "fade-in", "$color, $amount", Functions::Colors::opacify);
    register_built_in_function(ctx, "fade-out", "$color, $amount", Functions::Colors::transparentize);
    register_built_in_function(ctx, "transparentize", "$color, $amount", Functions::Colors::transparentize);
    register_built_in_function(ctx, "ie-hex-str", "$color", Functions::Colors::ieHexStr);
    register_built_in_overloads(ctx, "alpha", {
        std::make_pair("$color", Functions::Colors::alpha_one),
        std::make_pair("$args...", Functions::Colors::alpha_any),
      });
    register_built_in_function(ctx, "opacity", "$color", Functions::Colors::opacity);

    // Selector functions
    register_built_in_function(ctx, "selector-nest", "$selectors...", Functions::Selectors::nest);
    register_built_in_function(ctx, "selector-append", "$selectors...", Functions::Selectors::append);
    register_built_in_function(ctx, "selector-extend", "$selector, $extendee, $extender", Functions::Selectors::extend);
    register_built_in_function(ctx, "selector-replace", "$selector, $original, $replacement", Functions::Selectors::replace);
    register_built_in_function(ctx, "selector-unify", "$selector1, $selector2", Functions::Selectors::unify);
    register_built_in_function(ctx, "is-superselector", "$super, $sub", Functions::Selectors::isSuper);
    register_built_in_function(ctx, "simple-selectors", "$selector", Functions::Selectors::simple);
    register_built_in_function(ctx, "selector-parse", "$selector", Functions::Selectors::parse);

    // Meta functions
    register_built_in_function(ctx, "feature-exists", "$feature", Functions::Meta::featureExists);
    register_built_in_function(ctx, "type-of", "$value", Functions::Meta::typeOf);
    register_built_in_function(ctx, "inspect", "$value", Functions::Meta::inspect);
    register_built_in_function(ctx, "keywords", "$args", Functions::Meta::keywords);

    // ToDo: dart-sass keeps them on the local environment scope, see below:
    // These functions are defined in the context of the evaluator because
    // they need access to the [_environment] or other local state.
    register_built_in_function(ctx, "global-variable-exists", "$name, $module: null", Functions::Meta::globalVariableExists);
    register_built_in_function(ctx, "variable-exists", "$name", Functions::Meta::variableExists);
    register_built_in_function(ctx, "function-exists", "$name, $module: null", Functions::Meta::functionExists);
    register_built_in_function(ctx, "mixin-exists", "$name, $module: null", Functions::Meta::mixinExists);
    register_built_in_function(ctx, "content-exists", "", Functions::Meta::contentExists);
    // register_built_in_function(ctx, "module-variables", "$module", Functions::Meta::moduleVariables);
    // register_built_in_function(ctx, "module-functions", "$module", Functions::Meta::moduleFunctions);
    register_built_in_function(ctx, "get-function", "$name, $css: false, $module: null", Functions::Meta::getFunction);
    register_built_in_function(ctx, "call", "$function, $args...", Functions::Meta::call);

  }

  void register_c_function2(Context& ctx, Sass_Function_Entry descr)
  {
    EnvFrame local(&ctx.varRoot, true);
    ScopedStackFrame<EnvFrame> scoped(ctx.varStack, &local);
    ExternalCallable* callable = make_c_function2(descr, ctx);
    callable->idxs(local.getIdxs());
    ctx.functions.insert(std::make_pair(callable->name(), callable));
    ctx.varRoot.createFunction(callable->name());
    ctx.fnCache.push_back(callable);
  }

}
