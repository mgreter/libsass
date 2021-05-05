/*****************************************************************************/
/* Part of LibSass, released under the MIT license (See LICENSE.txt).        */
/*****************************************************************************/
#include "capi_getopt.hpp"

#include <cstring>
#include <iomanip>
#include <algorithm>
#include "exceptions.hpp"
#include "string_utils.hpp"
#include "capi_compiler.hpp"

using namespace Sass;

struct SassOptionEnum
{
  const char* string;
  int enumid;
};

struct SassOption {
  const char shrt = '\0';
  const char* name;
  const char* desc;
  const bool boolean = false;
  const char* argument = nullptr;
  const bool optional = false;
  const struct SassOptionEnum* enums;
  void (*cb) (struct SassGetOpt* getopt, union SassOptionValue value);
  SassOption(
    const char shrt = '\0',
    const char* name = nullptr,
    const char* desc = nullptr,
    const bool boolean = false,
    const char* argument = nullptr,
    const bool optional = false,
    const struct SassOptionEnum* enums = nullptr,
    void (*cb) (struct SassGetOpt* getopt, union SassOptionValue value) = nullptr
  ) :
    shrt(shrt),
    name(name),
    desc(desc),
    boolean(boolean),
    argument(argument),
    optional(optional),
    enums(enums),
    cb(cb)
  {}
};

struct SassArgument {
  bool optional = false;
  const char* name;
  void (*cb) (struct SassGetOpt* getopt, const char* arg);
  SassArgument(
    bool optional = false,
    const char* name = nullptr,
    void (*cb) (struct SassGetOpt* getopt, const char* arg) = nullptr
  ) :
    optional(optional),
    name(name),
    cb(cb)
  {}
};

struct SassGetOpt {
  Compiler& compiler;
  sass::string wasAssignment;
  bool lastArgWasShort = false;
  bool needsArgumentWasShort = false;
  const struct SassOption* lastArg = nullptr;
  const struct SassOption* needsArgument = nullptr;
  sass::vector<sass::string> args = {};
  sass::vector<struct SassOption> options;
  sass::vector<struct SassArgument> arguments;
  SassGetOpt(Compiler& compiler) :
    compiler(compiler)
  {}
};

const struct SassOptionEnum style_options[] = {
  { "compressed", SASS_STYLE_COMPRESSED },
  { "compact", SASS_STYLE_COMPACT },
  { "expanded", SASS_STYLE_EXPANDED },
  { "nested", SASS_STYLE_NESTED },
  nullptr
};

const struct SassOptionEnum format_options[] = {
  { "auto", SASS_IMPORT_AUTO },
  { "css", SASS_IMPORT_CSS },
  { "sass", SASS_IMPORT_SASS },
  { "scss", SASS_IMPORT_SCSS },
  nullptr
};

const struct SassOptionEnum srcmap_options[] = {
  { "none", SASS_SRCMAP_NONE },
  { "create", SASS_SRCMAP_CREATE },
  { "link", SASS_SRCMAP_EMBED_LINK },
  { "embed", SASS_SRCMAP_EMBED_JSON },
  nullptr
};

void getopt_set_input_format(struct SassGetOpt* getopt, union SassOptionValue value) { getopt->compiler.input_syntax = value.syntax; }
void getopt_set_output_style(struct SassGetOpt* getopt, union SassOptionValue value) { getopt->compiler.output_style = value.style; }
void getopt_add_include_path(struct SassGetOpt* getopt, union SassOptionValue value) { getopt->compiler.addIncludePaths(value.string); }
void getopt_load_plugins(struct SassGetOpt* getopt, union SassOptionValue value) { getopt->compiler.loadPlugins(value.string); }
void getopt_set_srcmap_mode(struct SassGetOpt* getopt, union SassOptionValue value) { getopt->compiler.mapopt.mode = value.mode; }
void getopt_set_srcmap_file_urls(struct SassGetOpt* getopt, union SassOptionValue value) { getopt->compiler.mapopt.file_urls = value.boolean; }
void getopt_set_srcmap_contents(struct SassGetOpt* getopt, union SassOptionValue value) { getopt->compiler.mapopt.embed_contents = value.boolean; }
void getopt_set_srcmap_root(struct SassGetOpt* getopt, union SassOptionValue value) { getopt->compiler.mapopt.root = value.boolean; }
void getopt_set_srcmap_path(struct SassGetOpt* getopt, union SassOptionValue value) { getopt->compiler.mapopt.path = value.boolean; }
void getopt_set_unicode(struct SassGetOpt* getopt, union SassOptionValue value) { getopt->compiler.support_unicode = value.boolean; }
void getopt_set_colors(struct SassGetOpt* getopt, union SassOptionValue value) { getopt->compiler.support_colors = value.boolean; }

void getopt_error(struct SassGetOpt* getopt, const char* what)
{
  if (getopt) {
    handle_error(getopt->compiler, 9, what, nullptr);
  }
}

void getopt_set_precision(struct SassGetOpt* getopt, union SassOptionValue value)
{
  // The GetOpt API does not yet know integers
  try {
    // Try to convert the precision string to int
    getopt->compiler.setPrecision(std::stoi(value.string));
  }
  catch (std::exception&) {
    sass::sstream strm;
    strm << "option '--precision' is not a valid integer";
    sass::string msg(strm.str());
    getopt_error(getopt, msg.c_str());
    return; // return after error
  }
}

void cli_sass_compiler_set_line_numbers(struct SassGetOpt* getopt, union SassOptionValue value) {
  std::cerr << "cli_sass_compiler_set_line_numbers " << value.boolean << "\n";
}


void getopt_print_help(struct SassGetOpt* getopt, std::ostream& stream);

void cli_sass_compiler_version(struct SassGetOpt* getopt, union SassOptionValue value) {
  getopt_print_help(getopt, std::cerr);
  exit(0);
}
void cli_sass_compiler_help(struct SassGetOpt* getopt, union SassOptionValue value) {
  getopt_print_help(getopt, std::cerr);
  exit(0);
}

void cli_sass_compiler_input_file_arg(struct SassGetOpt* getopt, const char* path)
{
  struct SassImport* entry = strncmp(path, "--", 2) == 0
    ? sass_make_stdin_import("stream://stdin")
    : sass_make_file_import(path);
  sass_compiler_set_entry_point(getopt->compiler.wrap(), entry);
}

void cli_sass_compiler_output_file_arg(struct SassGetOpt* getopt, const char* path)
{
  sass_compiler_set_output_path(getopt->compiler.wrap(), path);
}


sass::string format_option(struct SassGetOpt* getopt, struct SassOption& option)
{
  sass::sstream line;
  if (option.shrt) {
    line << getopt->compiler.getColor(Terminal::green);
    line << "-" << option.shrt;
    line << getopt->compiler.getColor(Terminal::reset);
    if (option.name) line << ", ";
  }
  else {
    line << "    ";
  }
  if (option.name) {
    line << getopt->compiler.getColor(Terminal::green);
    line << "--";
    line << getopt->compiler.getColor(Terminal::reset);
    if (option.boolean) {
      line << getopt->compiler.getColor(Terminal::blue);
      line << "[no-]";
      line << getopt->compiler.getColor(Terminal::reset);
    }
    line << getopt->compiler.getColor(Terminal::green);
    line << option.name;
    line << getopt->compiler.getColor(Terminal::reset);
  }
  if (option.argument) {
    if (option.optional) line << "[";
    line << "=";
    line << getopt->compiler.getColor(Terminal::cyan);
    line << option.argument;
    line << getopt->compiler.getColor(Terminal::reset);
    if (option.optional) line << "]";
  }
  return line.str();
}

void getopt_print_help(struct SassGetOpt* getopt, std::ostream& stream)
{
  size_t longest = 20;
  // Determine longest option to align them all
  for (struct SassOption& option : getopt->options) {
    sass::string fmt(format_option(getopt, option));
    size_t len = Terminal::count_printable(fmt.c_str()) + 2;
    if (len > longest) longest = len;
  }
  // Print out each option line by line
  for (struct SassOption& option : getopt->options) {
    sass::string fmt(format_option(getopt, option));
    size_t len = Terminal::count_printable(fmt.c_str());
    stream << "  " << fmt;
    stream.width(longest - len + 2);
    stream << " " << option.desc;
    stream << "\n";
    if (option.enums) {
      auto enums = option.enums;
      sass::vector<sass::string> names;
      while (enums && enums->string) {
        names.push_back(enums->string);
        enums += 1;
      }
      stream << std::setw(longest + 4) << "";
      if (option.argument) {
        stream << getopt->compiler.getColor(Terminal::cyan);
        stream << option.argument;
        stream << getopt->compiler.getColor(Terminal::reset);
        stream << " must be ";
      }
      stream << toSentence(names, "or",
        getopt->compiler.getColor(Terminal::yellow),
        getopt->compiler.getColor(Terminal::reset), '\'');
      stream << "\n";
    }
  }
}

sass::vector<const struct SassOption*> find_short_options(struct SassGetOpt* getopt, const char arg)
{
  sass::vector<const struct SassOption*> matches;
  for (struct SassOption& option : getopt->options) {
    if (option.shrt == arg) {
      matches.push_back(&option);
    }
  }
  return matches;
}

sass::vector<const struct SassOption*> find_long_options(struct SassGetOpt* getopt, const sass::string& arg)
{
  sass::vector<const struct SassOption*> matches;
  for (const struct SassOption& option : getopt->options) {
    if (StringUtils::startsWithIgnoreCase(option.name, arg)) {
      if (arg == option.name) return { &option };
      matches.push_back(&option);
    }
    if (option.boolean) {
      if (StringUtils::startsWithIgnoreCase(arg, "no-", 3)) {
        sass::string name(arg.begin() + 3, arg.end());
        if (StringUtils::startsWithIgnoreCase(option.name, name)) {
          if (arg == option.name) return { &option };
          matches.push_back(&option);
        }
      }
    }
  }
  return matches;
}

sass::vector<const struct SassOptionEnum*> find_options_enum(
  const struct SassOptionEnum* enums, const sass::string& arg)
{
  sass::vector<const struct SassOptionEnum*> matches;
  while (enums && enums->string) {
    if (StringUtils::startsWithIgnoreCase(enums->string, arg)) {
      matches.push_back(enums);
    }
    enums += 1;
  }
  return matches;
}

// Check for too many or not enough arguments
// Skip this check if nothing is expected at all
// This will simply store the arguments in `args`
void getopt_check_and_consume_arguments(struct SassGetOpt* getopt)
{
  if (getopt->compiler.state) return;
  if (getopt->arguments.empty()) return;
  size_t want_size = getopt->arguments.size();
  size_t requires = getopt->arguments.size();
  for (const auto& arg : getopt->arguments) {
    if (arg.optional) requires -= 1;
  }
  size_t have_size = getopt->args.size();
  for (size_t i = 0; i < have_size; i += 1) {
    if (want_size <= i) {
      sass::sstream strm;
      sass::string& value(getopt->args[i]);
      StringUtils::makeReplace(value, "'", "\\'");
      strm << "extra argument '" << value;
      sass::string msg(strm.str());
      getopt_error(getopt, msg.c_str());
      return; // return after error
    }
    else {
      // Call back to registered handler
      getopt->arguments[i].cb(getopt, getopt->args[i].c_str());
    }
  }
  for (size_t i = have_size; i < requires; i += 1) {
    sass::sstream strm;
    strm << "missing required argument '"
      << getopt->arguments[i].name << "'";
    sass::string msg(strm.str());
    getopt_error(getopt, msg.c_str());
    return; // return after error
  }

}

// Check for pending required option
void getopt_check_required_option(struct SassGetOpt* getopt)
{
  if (getopt->compiler.state) return;
  // Expected argument, error
  if (getopt->needsArgument) {
    sass::sstream strm;
    if (getopt->needsArgumentWasShort) {
      strm << "option '-" <<
        getopt->needsArgument->shrt <<
        "' requires an argument'";
    }
    else {
      strm << "option '--" <<
        getopt->needsArgument->name <<
        "' requires an argument'";
    }
    sass::string msg(strm.str());
    getopt_error(getopt, msg.c_str());
    return; // return after error
  }
}

void getopt_parse(struct SassGetOpt* getopt, const char* value)
{
  if (value == nullptr) return;
  if (getopt->compiler.state) return;
  sass::string arg(value);
  StringUtils::makeTrimmed(arg);
  // if (arg.empty()) return;
  union SassOptionValue result {};
  if (arg != "-" && arg != "--" && arg[0] == '-' && getopt->wasAssignment.empty()) {
    getopt_check_required_option(getopt);
    sass::vector<const struct SassOption*> opts;

    // Check if we have some assignment
    size_t assign = arg.find_first_of('=');
    if (assign != std::string::npos) {
      sass::string key(arg.begin(), arg.begin() + assign);
      sass::string val(arg.begin() + assign + 1, arg.end());
      getopt_parse(getopt, key.c_str());
      getopt->wasAssignment = key;
      getopt_parse(getopt, val.c_str());
      getopt->wasAssignment.clear();
      return;
    }

    // Long argument
    if (arg[1] == '-') {
      arg.erase(0, 2);
      opts = find_long_options(getopt, arg);
    }
    // Short argument
    else {
      arg.erase(0, 1);
      // Split multiple short args
      if (arg.size() > 1) {
        for (size_t i = 0; i < arg.size(); i += 1) {
          sass::string split("-"); split += arg[i];
          sass_getopt_parse(getopt, split.c_str());
        }
        return;
      }
      opts = find_short_options(getopt, arg[0]);
      // Consume further until has arg
    }
    if (opts.size() == 0) {
      sass::sstream strm;
      strm << "unrecognized option '--" << arg << "'";
      sass::string msg(strm.str());
      getopt_error(getopt, msg.c_str());
      return; // return after error
    }
    if (opts.size() > 1) {
      sass::sstream strm;
      if (value[0] == '-' && value[1] == '-') {
        strm << "option '--" << arg << "' is ambiguous; possibilities: ";
        for (auto opt : opts) strm << "'--" << opt->name << "'" << std::setw(4);
      }
      else {
        // Should never happen if you configured your options right
        strm << "option '-" << arg << "' is ambiguous (internal error)";
      }
      sass::string msg(strm.str());
      getopt_error(getopt, msg.c_str());
      return; // return after error
    }
    getopt->lastArg = opts[0];
    getopt->needsArgument = opts[0]->argument ? opts[0] : nullptr;
    getopt->needsArgumentWasShort = value[0] == '-' && value[1] != '-';

    // Check boolean options right away
    if (opts[0]->boolean) {
      // Get boolean result (invert if argument has "no-" prefix)
      result.boolean = !StringUtils::startsWithIgnoreCase(arg, "no-", 3);
    }
    if (!getopt->needsArgument) {
      opts[0]->cb(getopt, result);
    }
  }
  else if (getopt->needsArgument) {
    if (getopt->needsArgument->enums) {
      auto matches = find_options_enum(
        getopt->needsArgument->enums, arg);
      if (matches.size() == 0) {
        sass::sstream strm;
        strm << "enum '" << arg << "' is not valid for option '";
        if (getopt->needsArgumentWasShort) {
          strm << "-" << getopt->needsArgument->shrt;
        }
        else {
          strm << "--" << getopt->needsArgument->name;
        }
        strm << "' (valid enums are ";
        auto enums = getopt->needsArgument->enums;
        sass::vector<sass::string> names;
        while (enums && enums->string) {
          names.push_back(enums->string);
          enums += 1;
        }
        strm << Sass::toSentence(names, "or",
          getopt->compiler.getColor(Terminal::yellow),
          getopt->compiler.getColor(Terminal::reset), '\'') << ")";
        sass::string msg(strm.str());
        getopt_error(getopt, msg.c_str());
        return; // return after error
      }
      else if (matches.size() > 1) {

        sass::sstream strm;
        strm << "enum '" << arg << "' for option '";
        if (getopt->needsArgumentWasShort) {
          strm << "-" << getopt->needsArgument->shrt;
        }
        else {
          strm << "--" << getopt->needsArgument->name;
        }
        strm << "' is ambiguous (possibilities are ";
        sass::vector<sass::string> names;
        for (auto match : matches) {
          names.push_back(match->string);
        }
        strm << toSentence(names, "or",
          getopt->compiler.getColor(Terminal::yellow),
          getopt->compiler.getColor(Terminal::reset), '\'') << ")";
        sass::string msg(strm.str());
        getopt_error(getopt, msg.c_str());
        return; // return after error
      }
      result.integer = matches[0]->enumid;
    }
    else {
      result.string = arg.c_str();
    }
    getopt->needsArgument->cb(getopt, result);
    getopt->needsArgumentWasShort = false;
    getopt->needsArgument = nullptr;
  }
  else if (!getopt->wasAssignment.empty()) {
    sass::sstream strm;
    strm << "option '";
    if (getopt->lastArgWasShort) {
      strm << "-" << getopt->lastArg->shrt;
    }
    else {
      strm << "--" << getopt->lastArg->name;
    }
    strm << "' doesn't allow an argument";
    sass::string msg(strm.str());
    getopt_error(getopt, msg.c_str());
    return; // return after error
  }
  else {
    // This is a regular argument
    getopt->args.push_back(arg);
  }

}

/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

extern "C" {

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Create a new GetOpt parser to help with parsing config from users
  // Optimized to act like GNU GetOpt Long to consume `argv` items
  // But can also be used to parse any other list of config strings
  struct SassGetOpt* ADDCALL sass_make_getopt(struct SassCompiler* compiler)
  {
    return new struct SassGetOpt(Compiler::unwrap(compiler));
  }
  // EO sass_make_getopt

  void ADDCALL sass_getopt_parse(struct SassGetOpt* getopt, const char* value)
  {
    getopt_parse(getopt, value);
  }

  // Return string with the full help message describing all commands
  // This is formatted in a similar fashion as GNU tools using getopt
  char* ADDCALL sass_getopt_get_help(struct SassGetOpt* getopt)
  {
    sass::sstream usage;
    getopt_print_help(getopt, usage);
    return sass_copy_string(usage.str());
  }
  // EO sass_getopt_get_help

  // Delete and finalize the GetOpt parser. Make sure to call
  // this before you want to start the actual compilation phase.
  void ADDCALL sass_delete_getopt(struct SassGetOpt* getopt)
  {
    // Check for pending required option
    getopt_check_required_option(getopt);
    // Check for too many or not enough arguments
    // Skip this check if nothing is expected at all
    // This will simply store the arguments in `args`
    getopt_check_and_consume_arguments(getopt);
    // Release memory
    delete getopt;
  }
  // EO sass_delete_getopt

  // Register additional option that can be parsed
  void ADDCALL sass_getopt_register_option(struct SassGetOpt* getopt,
    // Short and long parameter names
    const char short_name,
    const char* long_name,
    // Description used in help/usage message
    const char* description,
    // Whether to act like a boolean
    const bool boolean,
    // Name of required argument
    const char* argument,
    // Make argument optional
    const bool optional,
    // Arguments must be one of this enum
    const struct SassOptionEnum* enums,
    // Callback function, where we pass back the given option value
    void (*cb) (struct SassGetOpt* getopt, union SassOptionValue value))
  {
    getopt->options.emplace_back(SassOption{ short_name, long_name,
      description, boolean, argument, optional, enums, cb });
  }
  // EO sass_getopt_register_option

  // Register additional argument that can be parsed
  void ADDCALL sass_getopt_register_argument(struct SassGetOpt* getopt,
    // Whether this argument is optional
    bool optional,
    // Name used in messages
    const char* name,
    // Callback function, where we pass back the given argument value
    void (*cb) (struct SassGetOpt* getopt, const char* value))
  {
    getopt->arguments.emplace_back(SassArgument{ optional, name, cb });
  }
  // EO sass_getopt_register_argument

  // Utility function to tell LibSass to register its default options
  void ADDCALL sass_getopt_populate_options(struct SassGetOpt* getopt)
  {
    sass_getopt_register_option(getopt, 't', "style", "Set output style (nested, expanded, compact or compressed).", false, "STYLE", false, style_options, getopt_set_output_style);
    sass_getopt_register_option(getopt, 'f', "format", "Set explicit input syntax (scss, sass, css or auto).", false, "SYNTAX", true, format_options, getopt_set_input_format);
    sass_getopt_register_option(getopt, 'l', "line-comments", "Emit comments showing original line numbers.", true, nullptr, false, nullptr, cli_sass_compiler_set_line_numbers);
    sass_getopt_register_option(getopt, 'I', "include-path", "Add include path to look for imports.", false, "PATH", false, nullptr, getopt_add_include_path);
    sass_getopt_register_option(getopt, 'P', "plugin-path", "Add plugin path to auto load plugins.", false, "PATH", false, nullptr, getopt_load_plugins);
    sass_getopt_register_option(getopt, 'm', "sourcemap", "Set how to create and emit source mappings.", false, "TYPE", true, srcmap_options, getopt_set_srcmap_mode);
    sass_getopt_register_option(getopt, '\0', "sourcemap-file-urls", "Emit absolute file:// urls in includes array.", true, nullptr, true, nullptr, getopt_set_srcmap_file_urls);
    sass_getopt_register_option(getopt, 'C', "sourcemap-contents", "Embed contents of imported files in source map.", true, nullptr, true, nullptr, getopt_set_srcmap_contents);
    sass_getopt_register_option(getopt, 'M', "sourcemap-path", "Set path where source map file is saved.", false, "PATH", false, nullptr, getopt_set_srcmap_path);
    sass_getopt_register_option(getopt, 'P', "precision", "Set the precision for numbers.", false, "{0-12}", false, nullptr, getopt_set_precision);
    sass_getopt_register_option(getopt, '\0', "unicode", "Enable/Disable terminal unicode support.", true, nullptr, false, nullptr, getopt_set_unicode);
    sass_getopt_register_option(getopt, '\0', "colors", "Enable/Disable terminal ANSI color support.", true, nullptr, false, nullptr, getopt_set_colors);
  }
  // EO sass_getopt_populate_options

  // Utility function to tell LibSass to register its default arguments
  void ADDCALL sass_getopt_populate_arguments(struct SassGetOpt* getopt)
  {
    sass_getopt_register_argument(getopt, false, "INPUT_FILE|--", cli_sass_compiler_input_file_arg);
    sass_getopt_register_argument(getopt, true, "OUTPUT_FILE|--", cli_sass_compiler_output_file_arg);
  }
  // EO sass_getopt_populate_arguments

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

}
