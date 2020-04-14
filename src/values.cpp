// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include "sass.h"
#include "values.hpp"
#include "ast.hpp"

#include <stdint.h>

namespace Sass {

  // convert value from C++ side to C-API
  union Sass_Value* ast_node_to_sass_value (const Value* val)
  {
    if (val == nullptr) return nullptr;
    if (const Number* number = val->isNumber())
    {   
      return sass_make_number(number->value(), number->unit().c_str());
    }
    else if (const Color_RGBA* rgba = val->isColorRGBA()) {
      return sass_make_color(rgba->r(), rgba->g(), rgba->b(), rgba->a());
    }
    else if (const Color_HSLA* hsla = val->isColorHSLA()) {
      Color_RGBA_Obj col = hsla->copyAsRGBA();
      return sass_make_color(col->r(), col->g(), col->b(), col->a());
    }
    else if (const SassList* l = val->isList())
    {
      union Sass_Value* list = sass_make_list(l->length(), l->separator(), l->hasBrackets());
      for (size_t i = 0, L = l->length(); i < L; ++i) {
        ValueObj obj = l->at(i);
        auto val = ast_node_to_sass_value(obj);
        sass_list_set_value(list, i, val);
      }
      return list;
    }
    else if (const SassMap* m = val->isMap())
    {
      union Sass_Value* map = sass_make_map(m->size());
      size_t i = 0; for (auto kv : m->elements()) {
        sass_map_set_key(map, i, ast_node_to_sass_value(kv.first));
        sass_map_set_value(map, i, ast_node_to_sass_value(kv.second));
        ++ i;
      }
      return map;
    }
    else if (val->isNull())
    {
      return sass_make_null();
    }
    else if (const SassBoolean* res = val->isBoolean())
    {
      return sass_make_boolean(res->value());
    }
    else if (const SassString* cstr = val->isString())
    {
      return cstr->hasQuotes()
        ? sass_make_qstring(cstr->value().c_str())
        : sass_make_string(cstr->value().c_str());
    }
    return sass_make_error("unknown sass value type");
  }

  // convert value from C-API to C++ side
  Value* sass_value_to_ast_node (const union Sass_Value* val)
  {
    switch (sass_value_get_tag(val)) {
      case SASS_NUMBER:
        return SASS_MEMORY_NEW(Number,
                               SourceSpan::tmp("[C-VALUE]"),
                               sass_number_get_value(val),
                               sass_number_get_unit(val));
      case SASS_BOOLEAN:
        return SASS_MEMORY_NEW(Boolean,
                               SourceSpan::tmp("[C-VALUE]"),
                               sass_boolean_get_value(val));
      case SASS_COLOR:
        // ToDo: allow to also use HSLA colors!!
        return SASS_MEMORY_NEW(Color_RGBA,
                               SourceSpan::tmp("[C-VALUE]"),
                               sass_color_get_r(val),
                               sass_color_get_g(val),
                               sass_color_get_b(val),
                               sass_color_get_a(val));
      case SASS_STRING:
        if (sass_string_is_quoted(val)) {
          return SASS_MEMORY_NEW(SassString,
                                 SourceSpan::tmp("[C-VALUE]"),
                                 sass_string_get_value(val),
                                 true);
        }
        return SASS_MEMORY_NEW(SassString,
                                 SourceSpan::tmp("[C-VALUE]"),
                                 sass_string_get_value(val),
                                 false);
      case SASS_LIST: {
        SassList* l = SASS_MEMORY_NEW(SassList,
                                  SourceSpan::tmp("[C-VALUE]"),
          sass::vector<ValueObj>(), // sass_list_get_length(val),
                                  sass_list_get_separator(val));
        for (size_t i = 0, L = sass_list_get_length(val); i < L; ++i) {
          l->append(sass_value_to_ast_node(sass_list_get_value(val, i)));
        }
        l->hasBrackets(sass_list_get_is_bracketed(val));
        return l;
      }
      case SASS_MAP: {
        Map* m = SASS_MEMORY_NEW(Map, SourceSpan::tmp("[C-VALUE]"));
        for (size_t i = 0, L = sass_map_get_length(val); i < L; ++i) {
          m->insert(
            sass_value_to_ast_node(sass_map_get_key(val, i)),
            sass_value_to_ast_node(sass_map_get_value(val, i)));
        }
        return m;
      }
      case SASS_NULL:
        return SASS_MEMORY_NEW(Null, SourceSpan::tmp("[C-VALUE]"));
      case SASS_ERROR:
        return SASS_MEMORY_NEW(Custom_Error,
                               SourceSpan::tmp("[C-VALUE]"),
                               sass_error_get_message(val));
      case SASS_WARNING:
        return SASS_MEMORY_NEW(Custom_Warning,
                               SourceSpan::tmp("[C-VALUE]"),
                               sass_warning_get_message(val));
      default: break;
    }
    return 0;
  }

}
