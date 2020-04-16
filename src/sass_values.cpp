// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include <cstdlib>
#include <cstring>
#include "util.hpp"
#include "eval.hpp"
#include "operators.hpp"
#include "sass/values.h"
#include "sass_values.hpp"

extern "C" {
  using namespace Sass;

  Map* getMap(struct SassValue* value) { return reinterpret_cast<Map*>(value); }
  Value* getValue(struct SassValue* value) { return reinterpret_cast<Value*>(value); }
  Number* getNumber(struct SassValue* value) { return reinterpret_cast<Number*>(value); }
  SassString* getString(struct SassValue* value) { return reinterpret_cast<SassString*>(value); }
  Boolean* getBoolean(struct SassValue* value) { return reinterpret_cast<Boolean*>(value); }
  Color_RGBA* getColor(struct SassValue* value) { return reinterpret_cast<Color_RGBA*>(value); }
  SassList* getList(struct SassValue* value) { return reinterpret_cast<SassList*>(value); }
  Custom_Error* getError(struct SassValue* value) { return reinterpret_cast<Custom_Error*>(value); }
  Custom_Warning* getWarning(struct SassValue* value) { return reinterpret_cast<Custom_Warning*>(value); }

  struct SassValue* newSassValue(Value* value) { value->refcount += 1; return reinterpret_cast<struct SassValue*>(value); }


  // this works!??
  struct SassyNumber;

  // Return the sass tag for a generic sass value
  enum Sass_Tag ADDCALL sass_value_get_tag(struct SassValue* v) { return getValue(v)->getTag(); }

  // Check value for specified type
  bool ADDCALL sass_value_is_null(struct SassValue* val) { return getValue(val)->isNull(); }
  bool ADDCALL sass_value_is_number(struct SassValue* val) { return getValue(val)->isNumber(); }
  bool ADDCALL sass_value_is_string(struct SassValue* val) { return getValue(val)->isString(); }
  bool ADDCALL sass_value_is_boolean(struct SassValue* val) { return getValue(val)->isBoolean(); }
  bool ADDCALL sass_value_is_color(struct SassValue* val) { return getValue(val)->isColor(); }
  bool ADDCALL sass_value_is_list(struct SassValue* val) { return getValue(val)->isList(); }
  bool ADDCALL sass_value_is_map(struct SassValue* val) { return getValue(val)->isMap(); }
  bool ADDCALL sass_value_is_error(struct SassValue* val) { return getValue(val)->isError(); }
  bool ADDCALL sass_value_is_warning(struct SassValue* val) { return getValue(val)->isWarning(); }

  // Getters and setters for Sass_Number
  double ADDCALL sass_number_get_value(struct SassValue* number) { return getNumber(number)->value(); }
  void ADDCALL sass_number_set_value(struct SassValue* number, double value) { getNumber(number)->value(value); }
  const char* ADDCALL sass_number_get_unit(struct SassValue* number) { return getNumber(number)->unit().c_str(); }
  void ADDCALL sass_number_set_unit(struct SassValue* number, const char* unit) { getNumber(number)->unit(unit); }
  void ADDCALL sass_number_normalize(struct SassValue* number) { getNumber(number)->normalize(); }
  void ADDCALL sass_number_reduce(struct SassValue* number) { getNumber(number)->reduce(); }


  // Getters and setters for Sass_String
  const char* ADDCALL sass_string_get_value(struct SassValue* string) { return getString(string)->value().c_str(); }
  void ADDCALL sass_string_set_value(struct SassValue* string, char* value) { getString(string)->value(value); }
  bool ADDCALL sass_string_is_quoted(struct SassValue* string) { return getString(string)->hasQuotes(); }
  void ADDCALL sass_string_set_quoted(struct SassValue* string, bool quoted) { getString(string)->hasQuotes(quoted); }

  // Getters and setters for Sass_Boolean
  bool ADDCALL sass_boolean_get_value(struct SassValue* boolean) { return getBoolean(boolean)->value(); }
  void ADDCALL sass_boolean_set_value(struct SassValue* boolean, bool value) { getBoolean(boolean)->value(value); }

  // Getters and setters for Sass_Color
  double ADDCALL sass_color_get_r(struct SassValue* color) { return getColor(color)->r(); }
  void ADDCALL sass_color_set_r(struct SassValue* color, double r) { getColor(color)->r(r); }
  double ADDCALL sass_color_get_g(struct SassValue* color) { return getColor(color)->g(); }
  void ADDCALL sass_color_set_g(struct SassValue* color, double g) { getColor(color)->g(g); }
  double ADDCALL sass_color_get_b(struct SassValue* color) { return getColor(color)->b(); }
  void ADDCALL sass_color_set_b(struct SassValue* color, double b) { getColor(color)->b(b); }
  double ADDCALL sass_color_get_a(struct SassValue* color) { return getColor(color)->a(); }
  void ADDCALL sass_color_set_a(struct SassValue* color, double a) { getColor(color)->a(a); }

  struct SassValue* ADDCALL sass_map_get(struct SassValue* map, struct SassValue* key)
  {
    return getMap(map)->at(getValue(key))->toSassValue();
  }

  struct SassMapIterator {
    Hashed<ValueObj, ValueObj>::ordered_map_type::iterator pos;
    Hashed<ValueObj, ValueObj>::ordered_map_type::iterator end;
  };

  struct SassMapIterator* ADDCALL sass_map_make_iterator(struct SassValue* map)
  {
    return new SassMapIterator{ getMap(map)->begin(), getMap(map)->end() };
  }

  void ADDCALL sass_map_delete_iterator(struct SassMapIterator* it)
  {
    delete it;
  }

  struct SassValue* ADDCALL sass_map_iterator_get_key(struct SassMapIterator* it)
  {
    return it->pos->first->toSassValue();
  }

  struct SassValue* ADDCALL sass_map_iterator_get_value(struct SassMapIterator* it)
  {
    return it->pos->second->toSassValue();
  }


  bool ADDCALL sass_map_iterator_exhausted(struct SassMapIterator* it)
  {
    return it->pos == it->end;
  }

  void ADDCALL sass_map_iterator_next(struct SassMapIterator* it)
  {
    it->pos += 1;
  }


  void ADDCALL sass_map_set(struct SassValue* map, struct SassValue* key, struct SassValue* value)
  {
    getMap(map)->insert(getValue(key), getValue(value));
  }

  // Getters and setters for Sass_List
  size_t ADDCALL sass_list_get_length(struct SassValue* v) { return getList(v)->length(); }

  enum Sass_Separator ADDCALL sass_list_get_separator(struct SassValue* v) { return getList(v)->separator(); }
  void ADDCALL sass_list_set_separator(struct SassValue* v, enum Sass_Separator separator) { getList(v)->separator(separator); }
  bool ADDCALL sass_list_get_is_bracketed(struct SassValue* v) { return getList(v)->hasBrackets(); }
  void ADDCALL sass_list_set_is_bracketed(struct SassValue* v, bool is_bracketed) { getList(v)->hasBrackets(is_bracketed); }
  // Getters and setters for Sass_List values
  struct SassValue* ADDCALL sass_list_get_value(struct SassValue* v, size_t i) { return getList(v)->at(i)->toSassValue(); }
  void ADDCALL sass_list_set_value(struct SassValue* v, size_t i, struct SassValue* value) { getList(v)->at(i) = getValue(value); }

  // Getters and setters for Sass_Error
  const char* ADDCALL sass_error_get_message(struct SassValue* v) { return getError(v)->message().c_str(); };
  void ADDCALL sass_error_set_message(struct SassValue* v, const char* msg) { getError(v)->message(msg); };

  // Getters and setters for Sass_Warning
  const char* ADDCALL sass_warning_get_message(struct SassValue* v) { return getWarning(v)->message().c_str(); };
  void ADDCALL sass_warning_set_message(struct SassValue* v, const char* msg) { getWarning(v)->message(msg); };

  // Creator functions for all value types

  struct SassValue* ADDCALL sass_make_boolean(bool state)
  {
    return newSassValue(SASS_MEMORY_NEW(
      Boolean, SourceSpan::tmp("[BOOLEAN]"), state));
  }

  struct SassValue* ADDCALL sass_make_number(double val, const char* unit)
  {
    return newSassValue(SASS_MEMORY_NEW(
      Number, SourceSpan::tmp("[NUMBER]"), val, unit ? unit : ""));
  }

  struct SassValue* ADDCALL sass_make_color(double r, double g, double b, double a)
  {
    return newSassValue(SASS_MEMORY_NEW(
      Color_RGBA, SourceSpan::tmp("[COLOR]"), r, g, b, a));
  }

  struct SassValue* ADDCALL sass_make_string(const char* val)
  {
    return newSassValue(SASS_MEMORY_NEW(
      SassString, SourceSpan::tmp("[STRING]"), val, false));
  }

  struct SassValue* ADDCALL sass_make_qstring(const char* val)
  {
    return newSassValue(SASS_MEMORY_NEW(
      SassString, SourceSpan::tmp("[STRING]"), val, true));
  }

  struct SassValue* ADDCALL sass_list_at(struct SassValue* list, size_t i)
  {
    return getList(list)->at(i)->toSassValue();
  }

  void ADDCALL sass_list_push(struct SassValue* list, struct SassValue* value)
  {
    getList(list)->append(getValue(value));
  }

  struct SassValue* ADDCALL sass_make_list(enum Sass_Separator sep, bool is_bracketed)
  {
    return newSassValue(SASS_MEMORY_NEW(
      SassList, SourceSpan::tmp("[LIST]"), {}, sep, is_bracketed));
  }

  struct SassValue* ADDCALL sass_make_map()
  {
    return newSassValue(SASS_MEMORY_NEW(
      Map, SourceSpan::tmp("[MAP]")));
  }

  struct SassValue* ADDCALL sass_make_null(void)
  {
    return newSassValue(SASS_MEMORY_NEW(
      Null, SourceSpan::tmp("[NULL]")));
  }

  struct SassValue* ADDCALL sass_make_error(const char* msg)
  {
    return newSassValue(SASS_MEMORY_NEW(
      Custom_Error, SourceSpan::tmp("[ERROR]"), msg));
  }

  struct SassValue* ADDCALL sass_make_warning(const char* msg)
  {
    return newSassValue(SASS_MEMORY_NEW(
      Custom_Warning, SourceSpan::tmp("[WARNING]"), msg));
  }


  // will free all associated sass values
  void ADDCALL sass_delete_value(struct SassValue* val)
  {
    Value* value = getValue(val);
    if (value) {
      value->refcount -= 1;
      if (value->refcount == 0) {
        delete value;
      }
    }
  }

  // Make a deep cloned copy of the given sass value
  struct SassValue* ADDCALL sass_clone_value (struct SassValue* val)
  {
    return newSassValue(getValue(val)->clone());
  }

  struct SassValue* ADDCALL sass_value_stringify (struct SassValue* v, bool compressed, int precision)
  {
    ValueObj val = sass_value_to_ast_node(v);
    Sass_Inspect_Options options(compressed ? COMPRESSED : NESTED, precision);
    sass::string str(val->to_string(options));
    return sass_make_qstring(str.c_str());
  }

  struct SassValue* ADDCALL sass_value_op (enum Sass_OP op, struct SassValue* a, struct SassValue* b)
  {

    Sass::ValueObj rv;

    try {

      ValueObj lhs = sass_value_to_ast_node(a);
      ValueObj rhs = sass_value_to_ast_node(b);
      struct Sass_Inspect_Options options(NESTED, 5);

      // see if it's a relational expression
      switch(op) {
        case Sass_OP::EQ:  return sass_make_boolean(Operators::eq(lhs, rhs));
        case Sass_OP::NEQ: return sass_make_boolean(Operators::neq(lhs, rhs));
        case Sass_OP::GT:  return sass_make_boolean(Operators::gt(lhs, rhs));
        case Sass_OP::GTE: return sass_make_boolean(Operators::gte(lhs, rhs));
        case Sass_OP::LT:  return sass_make_boolean(Operators::lt(lhs, rhs));
        case Sass_OP::LTE: return sass_make_boolean(Operators::lte(lhs, rhs));
        case Sass_OP::AND: return ast_node_to_sass_value(lhs->isTruthy() ? rhs : lhs);
        case Sass_OP::OR:  return ast_node_to_sass_value(lhs->isTruthy() ? lhs : rhs);
        default: break;
      }

      if (sass_value_is_number(a) && sass_value_is_number(b)) {
        const Number* l_n = lhs->isNumber();
        const Number* r_n = rhs->isNumber();
        rv = Operators::op_numbers(op, *l_n, *r_n, l_n->pstate());
      }
      else if (sass_value_is_number(a) && sass_value_is_color(a)) {
        const Number* l_n = lhs->isNumber();
        // Direct HSLA operations are not supported
        // All color maths will be deprecated anyway
        Color_RGBA_Obj r_c = rhs->isColor()->toRGBA();
        rv = Operators::op_number_color(op, *l_n, *r_c, options, l_n->pstate());
      }
      else if (sass_value_is_color(a) && sass_value_is_number(b)) {
        // Direct HSLA operations are not supported
        // All color maths will be deprecated anyway
        Color_RGBA_Obj l_c = lhs->isColor()->toRGBA();
        const Number* r_n = rhs->isNumber();
        rv = Operators::op_color_number(op, *l_c, *r_n, options, l_c->pstate());
      }
      else if (sass_value_is_color(a) && sass_value_is_color(b)) {
        // Direct HSLA operations are not supported
        // All color maths will be deprecated anyway
        Color_RGBA_Obj l_c = lhs->isColor()->toRGBA();
        Color_RGBA_Obj r_c = rhs->isColor()->toRGBA();
        rv = Operators::op_colors(op, *l_c, *r_c, l_c->pstate());
      }
      else /* convert other stuff to string and apply operation */ {
        rv = Operators::op_strings(op, *lhs, *rhs, options, lhs->pstate());
      }

      // ToDo: maybe we should return null value?
      if (!rv) return sass_make_error("invalid return value");

      // convert result back to AST node
      return ast_node_to_sass_value(rv.ptr());
    }

    // simply pass the error message back to the caller for now
    // catch (Exception::InvalidSass& e) { return sass_make_error(e.what()); }
    catch (std::bad_alloc&) { return sass_make_error("memory exhausted"); }
    catch (std::exception& e) { return sass_make_error(e.what()); }
    catch (sass::string& e) { return sass_make_error(e.c_str()); }
    catch (const char* e) { return sass_make_error(e); }
    catch (...) { return sass_make_error("unknown"); }
  }

}
