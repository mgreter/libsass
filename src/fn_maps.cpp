#include "operators.hpp"
#include "fn_utils.hpp"
#include "fn_maps.hpp"

namespace Sass {

  namespace Functions {

    namespace Maps {

      BUILT_IN_FN(get)
      {
        Map* map = arguments[0]->assertMap("map");
        Value* key = arguments[1]->assertValue("key");
        if (Value* value = map->at(key)) { return value; }
        else { return SASS_MEMORY_NEW(Null, pstate); }
      }

      BUILT_IN_FN(merge)
      {
        Map* map1 = arguments[0]->assertMap("map1");
        Map* map2 = arguments[1]->assertMap("map2");
        Map* map = SASS_MEMORY_NEW(Map, pstate);
        for (auto key : map1->keys()) { map->insert(key, map1->at(key)); }
        for (auto key : map2->keys()) { map->insert(key, map2->at(key)); }
        // std::copy(map1->begin(), map1->end(),
        //   std::back_inserter(map->elements()));
        // std::copy(map2->begin(), map2->end(),
        //   std::back_inserter(map->elements()));
        return map;
      }

      // Because the signature below has an explicit `$key` argument, it doesn't
      // allow zero keys to be passed. We want to allow that case, so we add an
      // explicit overload for it.
      BUILT_IN_FN(remove_one)
      {
        return arguments[0]->assertMap("map");
      }

      BUILT_IN_FN(remove_many)
      {
        SassMap* map = arguments[0]->assertMap("map");
        SassMapObj copy = SASS_MEMORY_COPY(map);
        copy->erase(arguments[1]);
        auto values = arguments[2]->asVector();
        for (Value* key : values) {
          copy->erase(key);
        }
        return copy.detach();
      }

      BUILT_IN_FN(keys)
      {
        return SASS_MEMORY_NEW(SassList, pstate,
          arguments[0]->assertMap("map")->keys(),
          SASS_COMMA);
      }

      BUILT_IN_FN(values)
      {
        return SASS_MEMORY_NEW(SassList, pstate,
          arguments[0]->assertMap("map")->values(),
          SASS_COMMA);
      }

      BUILT_IN_FN(hasKey)
      {
        Map* map = arguments[0]->assertMap("map");
        Value* key = arguments[1]->assertValue("key");
        return SASS_MEMORY_NEW(Boolean, pstate, map->has(key));
      }

    }

    /////////////////
    // MAP FUNCTIONS
    /////////////////

    Signature map_get_sig = "map-get($map, $key)";
    BUILT_IN(map_get)
    {
      // leaks for "map-get((), foo)" if not Obj
      // investigate why this is (unexpected)
      Map_Obj m = ARGM("$map", Map);
      Expression_Obj v = ARG("$key", Expression, "an expression");
      try {
        Value_Obj val = m->at(v);
        if (!val) return SASS_MEMORY_NEW(Null, pstate);
        return val.detach();
      } catch (const std::out_of_range&) {
        return SASS_MEMORY_NEW(Null, pstate);
      }
      catch (...) { throw; }
    }

    Signature map_has_key_sig = "map-has-key($map, $key)";
    BUILT_IN(map_has_key)
    {
      Map_Obj m = ARGM("$map", Map);
      Expression_Obj v = ARG("$key", Expression, "an expression");
      return SASS_MEMORY_NEW(Boolean, pstate, m->has(v));
    }

    Signature map_keys_sig = "map-keys($map)";
    BUILT_IN(map_keys)
    {
      Map_Obj m = ARGM("$map", Map);
      SassList* result = SASS_MEMORY_NEW(SassList, pstate, {}, SASS_COMMA);
      for ( auto key : m->keys()) {
        result->append(key);
      }
      return result;
    }

    Signature map_values_sig = "map-values($map)";
    BUILT_IN(map_values)
    {
      Map_Obj m = ARGM("$map", Map);
      SassList* result = SASS_MEMORY_NEW(SassList, pstate, {}, SASS_COMMA);
      for ( auto key : m->keys()) {
        result->append(m->at(key));
      }
      return result;
    }

    Signature map_merge_sig = "map-merge($map1, $map2)";
    BUILT_IN(map_merge)
    {
      Map_Obj m1 = ARGM("$map1", Map);
      Map_Obj m2 = ARGM("$map2", Map);

      size_t len = m1->length() + m2->length();
      Map* result = SASS_MEMORY_NEW(Map, pstate, len);
      // concat not implemented for maps
      *result += m1;
      *result += m2;
      return result;
    }

    Signature map_remove_sig = "map-remove($map, $keys...)";
    BUILT_IN(map_remove)
    {
      bool remove;
      Map_Obj m = ARGM("$map", Map);
      List_Obj arglist = ARGLIST("$keys");
      Map* result = SASS_MEMORY_NEW(Map, pstate, 1);
      for (auto key : m->keys()) {
        remove = false;
        for (size_t j = 0, K = arglist->length(); j < K && !remove; ++j) {
          remove = Operators::eq(key, arglist->value_at_index(j));
        }
        if (!remove) *result << std::make_pair(key, m->at(key));
      }
      return result;
    }

  }

}
