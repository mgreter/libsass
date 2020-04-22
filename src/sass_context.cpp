// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"
#include "ast.hpp"

#include "sass_functions.hpp"
#include "sass_context.hpp"
#include "source_span.hpp"
#include "backtrace.hpp"
#include "compiler.hpp"
#include "terminal.hpp"
#include "source.hpp"
#include <iomanip>

#ifdef __cplusplus
namespace Sass {
extern "C" {
#endif

  void ADDCALL sass_import_set_format(struct SassImport* import, enum SassImportFormat format)
  {
    import->format = format;
  }


  struct SassImport* ADDCALL sass_make_stdin_import()
  {
    std::istreambuf_iterator<char> begin(std::cin), end;
    sass::string text(begin, end);
    return sass_make_import(
      "stdin", "stdin",
      sass_copy_string(text), 0,
      SASS_IMPORT_AUTO
    );
  }

  size_t ADDCALL sass_traces_get_size(struct SassTraces* traces)
  {
    return reinterpret_cast<StackTraces*>(traces)->size();
  }

  const char* ADDCALL sass_trace_get_name(struct SassTrace* trace)
  {
    return reinterpret_cast<struct Traced*>(trace)->getName().c_str();
  }

  bool ADDCALL sass_trace_get_was_fncall(struct SassTrace* trace)
  {
    return reinterpret_cast<struct Traced*>(trace)->isFn();
  }

  const struct SassSrcSpan* ADDCALL sass_trace_get_srcspan(struct SassTrace* trace)
  {
    return reinterpret_cast<const struct SassSrcSpan*>
      (&reinterpret_cast<struct Traced*>(trace)->getPstate());
  }

  struct SassTrace* ADDCALL sass_traces_get_trace(struct SassTraces* traces, size_t i)
  {
    return reinterpret_cast<struct SassTrace*>
      (&(struct Traced&)reinterpret_cast<StackTraces*>(traces)->at(i));
  }



  size_t ADDCALL sass_srcspan_get_src_ln(struct SassSrcSpan* pstate)
  {
    return reinterpret_cast<SourceSpan*>(pstate)->position.line;
  }
  size_t ADDCALL sass_srcspan_get_src_col(struct SassSrcSpan* pstate)
  {
    return reinterpret_cast<SourceSpan*>(pstate)->position.column;
  }
  size_t ADDCALL sass_srcspan_get_src_line(struct SassSrcSpan* pstate)
  {
    return reinterpret_cast<SourceSpan*>(pstate)->getLine();
  }
  size_t ADDCALL sass_srcspan_get_src_column(struct SassSrcSpan* pstate)
  {
    return reinterpret_cast<SourceSpan*>(pstate)->getColumn();
  }
  size_t ADDCALL sass_srcspan_get_span_ln(struct SassSrcSpan* pstate)
  {
    return reinterpret_cast<SourceSpan*>(pstate)->span.line;
  }
  size_t ADDCALL sass_srcspan_get_span_col(struct SassSrcSpan* pstate)
  {
    return reinterpret_cast<SourceSpan*>(pstate)->span.column;
  }

  struct SassSource* ADDCALL sass_srcspan_get_source(struct SassSrcSpan* pstate)
  {
    return reinterpret_cast<struct SassSource*>
      (reinterpret_cast<SourceSpan*>(pstate)->getSource());
  }

  const char* ADDCALL sass_source_get_abs_path(struct SassSource* source)
  {
    return reinterpret_cast<SourceData*>(source)->getAbsPath();
  }

  const char* ADDCALL sass_source_get_imp_path(struct SassSource* source)
  {
    return reinterpret_cast<SourceData*>(source)->getImpPath();
  }

  const char* ADDCALL sass_source_get_content(struct SassSource* source)
  {
    return reinterpret_cast<SourceData*>(source)->content();
  }

  const char* ADDCALL sass_source_get_srcmap(struct SassSource* source)
  {
    return reinterpret_cast<SourceData*>(source)->srcmaps();
  }

  struct SassTraces* ADDCALL sass_error_get_traces(struct SassError* error)
  {
    return reinterpret_cast<struct SassTraces*>(&error->traces);
  }



#ifdef __cplusplus
} // __cplusplus defined.
} // EP namespace Sass
#endif
