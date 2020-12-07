/*****************************************************************************/
/* Part of LibSass, released under the MIT license (See LICENSE.txt).        */
/*****************************************************************************/
#include "capi_error.hpp"

#include "json.hpp"

// Create error formatted as serialized json.
// You must free the data returned from here.
// You must do so by using `sass_free_c_string`
char* SassError::getJson(bool include_sources) const
{

  // Create json root object
  JsonNode* json = json_mkobject();

  // Attach all stack traces
  if (traces.size() > 0) {
    JsonNode* json_traces = json_mkarray();
    for (const Sass::StackTrace& trace : traces) {
      JsonNode* json_trace = json_mkobject();
      const Sass::SourceSpan& pstate(trace.pstate);
      json_append_member(json_trace, "file", json_mkstring(pstate.getAbsPath()));
      json_append_member(json_trace, "line", json_mknumber((double)(pstate.getLine())));
      json_append_member(json_trace, "column", json_mknumber((double)(pstate.getColumn())));
      if (include_sources) json_append_member(json_trace, "source", json_mkstring(pstate.getContent()));
      json_append_element(json_traces, json_trace);
    }
    json_append_member(json, "traces", json_traces);
  }

  // Attach the generic error reporting items
  json_append_member(json, "status", json_mknumber(status));
  json_append_member(json, "error", json_mkstring(what.c_str()));
  json_append_member(json, "formatted", json_mkstring(formatted.c_str()));

  char* serialized = nullptr;
  // Stringification may fail for strange reason
  try { serialized = json_stringify(json, "  "); }
  // If it fails at least return a valid json with special status 9999
  catch (...) { serialized = sass_copy_c_string("{\"status\":9999}"); }

  // Delete json tree
  json_delete(json);

  // Return new memory
  return serialized;

}
// EO SassError::getJson

#ifdef __cplusplus
extern "C" {
#endif

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  using namespace Sass;

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Error related getters (use after compiler was rendered)
  int ADDCALL sass_error_get_status(const struct SassError* error) { return error->status; }

  // Getter for plain error message (use after compiler was rendered).
  const char* ADDCALL sass_error_get_string(const struct SassError* error) { return error->what.c_str(); }

  // Getter for error status as json object (Useful to pass to downstream).
  char* ADDCALL sass_error_get_json(const struct SassError* error) { return error->getJson(true); }

  // Getter for formatted error message. According to logger style this
  // may be in unicode and may contain ANSI escape codes for colors.
  const char* ADDCALL sass_error_get_formatted(const struct SassError* error) { return error->formatted.c_str(); }

  // Getter for line position where error occurred (starts from 1).
  size_t ADDCALL sass_error_get_line(const struct SassError* error)
  {
    if (error->traces.empty()) return 0;
    return error->traces.back().pstate.getLine();
  }

  // Getter for column position where error occurred (starts from 1).
  size_t ADDCALL sass_error_get_column(const struct SassError* error)
  {
    if (error->traces.empty()) return 0;
    return error->traces.back().pstate.getColumn();
  }

  // Getter for source content referenced in line and column.
  const char* ADDCALL sass_error_get_content(const struct SassError* error)
  {
    if (error->traces.empty()) return 0;
    return error->traces.back().pstate.getContent();
  }

  // Getter for path where the error occurred.
  const char* ADDCALL sass_error_get_path(const struct SassError* error)
  {
    if (error->traces.empty()) return nullptr;
    return error->traces.back().pstate.getAbsPath();
  }

  // Getter for number of traces attached to error object.
  size_t ADDCALL sass_error_count_traces(const struct SassError* error)
  {
    return error->traces.size();
  }

  // Getter for last trace (or nullptr if none are available).
  const struct SassTrace* ADDCALL sass_error_last_trace(const struct SassError* error)
  {
    if (error->traces.empty()) return nullptr;
    return error->traces.back().wrap();
  }

  // Getter for nth trace (or nullptr if `n` is invalid).
  const struct SassTrace* ADDCALL sass_error_get_trace(const struct SassError* error, size_t i)
  {
    if (error->traces.size() < i) return nullptr;
    return error->traces.at(i).wrap();
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
} // __cplusplus defined.
#endif
