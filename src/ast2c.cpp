// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include "ast.hpp"
#include "sass_values.hpp"

namespace Sass {

  union Sass_Value* Boolean::toSassValue() const {
    return sass_make_boolean(value());
  }

  union Sass_Value* Number::toSassValue() const {
    return sass_make_number(value(), unit().c_str());
  }

  union Sass_Value* Custom_Warning::toSassValue() const {
    return sass_make_warning(message().c_str());
  }

  union Sass_Value* Custom_Error::toSassValue() const {
    return sass_make_error(message().c_str());
  }

  union Sass_Value* Color_RGBA::toSassValue() const {
    return sass_make_color(r(), g(), b(), a());
  }

  union Sass_Value* Color_HSLA::toSassValue() const {
    Color_RGBA_Obj rgba = copyAsRGBA();
    return rgba->toSassValue();
  }

  union Sass_Value* SassString::toSassValue() const {
    if (hasQuotes()) {
      return sass_make_qstring(value().c_str());
    }
    else {
      return sass_make_string(value().c_str());
    }
  }

  union Sass_Value* SassList::toSassValue() const {
    union Sass_Value* v = sass_make_list(length(), separator(), hasBrackets());
    for (size_t i = 0, L = length(); i < L; ++i) {
      sass_list_set_value(v, i, get(i)->toSassValue());
    }
    return v;
  }

  union Sass_Value* Map::toSassValue() const {
    union Sass_Value* v = sass_make_map(size());
    int i = 0;
    for (auto kv : elements()) {
      sass_map_set_key(v, i, kv.first->toSassValue());
      sass_map_set_value(v, i, kv.second->toSassValue());
      i++;
    }
    return v;
  }

  union Sass_Value* Null::toSassValue() const {
    return sass_make_null();
  }

  union Sass_Value* Parent_Reference::toSassValue() const {
    return sass_make_null();
  }

  union Sass_Value* SassFunction::toSassValue() const {
    return sass_make_null();
  }

};
