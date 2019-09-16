#include "operators.hpp"
#include "fn_utils.hpp"
#include "fn_maps.hpp"
#include "ast.hpp"
#include "debugger.hpp"

namespace Sass {

  namespace Functions {

    namespace Maps {

      BUILT_IN_FN(get)
      {
        SassMapObj map = arguments[0]->assertMap(*ctx.logger, "map");
        Value* key = arguments[1]->assertValue(*ctx.logger, "key");
        if (Value* value = map->at(key)) { return value; }
        else { return SASS_MEMORY_NEW(Null, pstate); }
      }

      BUILT_IN_FN(merge)
      {
        // debug_ast(arguments[0]);
        SassMapObj map1 = arguments[0]->assertMap(*ctx.logger, "map1");
        SassMapObj map2 = arguments[1]->assertMap(*ctx.logger, "map2");
        // We assign to ourself, so we can optimize this
        if (ctx.assigningTo.isValid()) {
          ExpressionObj& value = ctx.varRoot.getVariable(ctx.assigningTo);
          if (value.ptr() == map1.ptr()) {
            for (auto kv : map2->elements()) { map1->set(kv); }
            return map1.detach();
          }
        }
        SassMapObj map = SASS_MEMORY_NEW(Map, pstate, map1);
        // map->reserve(map->size() + map2->size());
        for (auto kv : map2->elements()) { map->set(kv); }
        return map.detach();
      }

      // Because the signature below has an explicit `$key` argument, it doesn't
      // allow zero keys to be passed. We want to allow that case, so we add an
      // explicit overload for it.
      BUILT_IN_FN(remove_one)
      {
        return arguments[0]->assertMap(*ctx.logger, "map");
      }

      BUILT_IN_FN(remove_many)
      {
        SassMapObj map = arguments[0]->assertMap(*ctx.logger, "map");
        // if (ctx.assigningTo.isValid()) {
        //   ExpressionObj& value = ctx.varRoot.getVariable(ctx.assigningTo);
        //   if (value.ptr() && value.ptr() == map.ptr()) {
        //     auto& values = arguments[2]->asVector();
        //     map->erase(arguments[1]);
        //     for (auto key : values) {
        //       map->erase(key);
        //     }
        //     return map.detach();
        //   }
        // }

        SassMapObj copy = SASS_MEMORY_COPY(map);
        auto& values = arguments[2]->asVector();
        copy->erase(arguments[1]);
        for (Value* key : values) {
          copy->erase(key);
        }
        return copy.detach();
      }

      BUILT_IN_FN(keys)
      {
        SassMapObj map = arguments[0]->assertMap(*ctx.logger, "map");
        return SASS_MEMORY_NEW(SassList,
          pstate, map->keys(), SASS_COMMA);
      }

      BUILT_IN_FN(values)
      {
        SassMapObj map = arguments[0]->assertMap(*ctx.logger, "map");
        return SASS_MEMORY_NEW(SassList,
          pstate, map->values(), SASS_COMMA);
      }

      BUILT_IN_FN(hasKey)
      {
        SassMapObj map = arguments[0]->assertMap(*ctx.logger, "map");
        Value* key = arguments[1]->assertValue(*ctx.logger, "key");
        return SASS_MEMORY_NEW(Boolean, pstate, map->has(key));
      }

    }

  }

}
