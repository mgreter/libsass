#ifndef SASS_C_TRACES_H
#define SASS_C_TRACES_H

#include <sass/base.h>

struct SassTrace;
struct SassTraces;
struct SassSrcSpan;
struct SassSource;

#ifdef __cplusplus
extern "C" {
#endif

  ADDAPI size_t ADDCALL sass_traces_get_size(struct SassTraces* traces);
  // ADDAPI struct SassTrace* ADDCALL sass_traces_get_last(struct SassTraces* traces);
  ADDAPI struct SassTrace* ADDCALL sass_traces_get_trace(struct SassTraces* traces, size_t i);

  ADDAPI size_t ADDCALL sass_traces_get_size(struct SassTraces* traces);
  ADDAPI const char* ADDCALL sass_trace_get_name(struct SassTrace* trace);
  ADDAPI bool ADDCALL sass_trace_get_was_fncall(struct SassTrace* trace);
  ADDAPI struct SassSrcSpan* ADDCALL sass_trace_get_srcspan(struct SassTrace* trace);

  ADDAPI size_t ADDCALL sass_srcspan_get_src_ln(struct SassSrcSpan* pstate);
  ADDAPI size_t ADDCALL sass_srcspan_get_src_col(struct SassSrcSpan* pstate);
  ADDAPI size_t ADDCALL sass_srcspan_get_span_ln(struct SassSrcSpan* pstate);
  ADDAPI size_t ADDCALL sass_srcspan_get_span_col(struct SassSrcSpan* pstate);
  ADDAPI size_t ADDCALL sass_srcspan_get_src_line(struct SassSrcSpan* pstate);
  ADDAPI size_t ADDCALL sass_srcspan_get_src_column(struct SassSrcSpan* pstate);

  ADDAPI struct SassSource* ADDCALL sass_srcspan_get_source(struct SassSrcSpan* pstate);

  ADDAPI const char* ADDCALL sass_source_get_abs_path(struct SassSource* source);
  ADDAPI const char* ADDCALL sass_source_get_imp_path(struct SassSource* source);
  ADDAPI const char* ADDCALL sass_source_get_content(struct SassSource* source);
  ADDAPI const char* ADDCALL sass_source_get_srcmap(struct SassSource* source);


#ifdef __cplusplus
} // __cplusplus defined.
#endif

#endif
