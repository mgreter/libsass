#include "operators.hpp"
#include "fn_utils.hpp"
#include "fn_maps.hpp"
#include "ast.hpp"
#include "debugger.hpp"
#include "settings.hpp"

namespace Sass {

  namespace Functions {

    namespace Maps {

      BUILT_IN_FN(get)
      {
        SassMapObj map = arguments[0]->assertMap(*ctx.logger, Sass::Strings::map);
        Value* key = arguments[1]->assertValue(*ctx.logger, Sass::Strings::key);
        if (Value* value = map->at(key)) { return value; }
        else { return SASS_MEMORY_NEW(Null, pstate); }
      }

      BUILT_IN_FN(merge)
      {
        SassMapObj map1 = arguments[0]->assertMap(*ctx.logger, Sass::Strings::map1);
        SassMapObj map2 = arguments[1]->assertMap(*ctx.logger, Sass::Strings::map2);
        // We assign to ourself, so we can optimize this
        // This can shave off a few percent of run-time
        if (selfAssign && map1->refcount < AssignableRefCount + 1) {
          for (auto kv : map2->elements()) { map1->set(kv); }
          return map1.detach();
        }
        auto values(map1->elements());
        for (auto kv : map2->elements()) {
          values[kv.first] = kv.second; }
        return SASS_MEMORY_NEW(Map, pstate, std::move(values));
      }

      // Because the signature below has an explicit `$key` argument, it doesn't
      // allow zero keys to be passed. We want to allow that case, so we add an
      // explicit overload for it.
      BUILT_IN_FN(remove_one)
      {
        return arguments[0]->assertMap(*ctx.logger, Sass::Strings::map);
      }

      BUILT_IN_FN(remove_many)
      {
        SassMapObj map = arguments[0]->assertMap(*ctx.logger, Sass::Strings::map);
        if (selfAssign && map->refcount < AssignableRefCount + 1) {

          map->erase(arguments[1]);
          for (Value* key : arguments[2]->iterator()) {
            map->erase(key);
          }
          return map.detach();
        }

        SassMapObj copy = SASS_MEMORY_COPY(map);
        copy->erase(arguments[1]);
        for (Value* key : arguments[2]->iterator()) {
          copy->erase(key);
        }
        return copy.detach();
      }

      BUILT_IN_FN(keys)
      {
        SassMapObj map = arguments[0]->assertMap(*ctx.logger, Sass::Strings::map);
        return SASS_MEMORY_NEW(SassList,
          pstate, map->keys(), SASS_COMMA);
      }

      BUILT_IN_FN(values)
      {
        SassMapObj map = arguments[0]->assertMap(*ctx.logger, Sass::Strings::map);
        return SASS_MEMORY_NEW(SassList,
          pstate, map->values(), SASS_COMMA);
      }

      BUILT_IN_FN(hasKey)
      {
        SassMapObj map = arguments[0]->assertMap(*ctx.logger, Sass::Strings::map);
        Value* key = arguments[1]->assertValue(*ctx.logger, Sass::Strings::key);
        return SASS_MEMORY_NEW(Boolean, pstate, map->has(key));
      }

    }

  }

}
