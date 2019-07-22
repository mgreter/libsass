// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include "listize.hpp"
#include "operators.hpp"
#include "fn_utils.hpp"
#include "fn_lists.hpp"
#include "debugger.hpp"

namespace Sass {

  namespace Functions {

    /////////////////
    // LIST FUNCTIONS
    /////////////////

    Signature keywords_sig = "keywords($args)";
    BUILT_IN(keywords)
    {
      List_Obj arglist = SASS_MEMORY_COPY(ARG("$args", List, "an argument list")); // copy
      Map_Obj result = SASS_MEMORY_NEW(Map, pstate, 1);
      if (!arglist->is_arglist()) {
        error("$args: " + arglist->to_string() + " is not an argument list.", pstate, traces);
      }
      for (size_t i = arglist->size(), L = arglist->length(); i < L; ++i) {
        Expression_Obj obj = arglist->at(i);
        Argument_Obj arg = (Argument*) obj.ptr(); // XXX
        std::string name = std::string(arg->name());
        Util::ascii_normalize_underscore(name);
        name = name.erase(0, 1); // sanitize name (remove dollar sign)
        *result << std::make_pair(SASS_MEMORY_NEW(String_Quoted,
                 pstate, name),
                 arg->value());
      }
      return result.detach();
    }

    Signature length_sig = "length($list)";
    BUILT_IN(length)
    {
      Expression* v = ARG("$list", Expression, "an expression");
      if (v->concrete_type() == Expression::MAP) {
        Map* map = Cast<Map>(env["$list"]);
        return SASS_MEMORY_NEW(Number, pstate, (double)(map ? map->length() : 1));
      }

      if (SassList * list = Cast<SassList>(env["$list"])) {
        return SASS_MEMORY_NEW(Number,
          pstate,
          (double)(list ? list->length() : 1));
      }

      if (List * list = Cast<List>(env["$list"])) {
        return SASS_MEMORY_NEW(Number,
          pstate,
          (double)(list ? list->size() : 1));
      }

      List* list = Cast<List>(env["$list"]);
      return SASS_MEMORY_NEW(Number,
        pstate,
        (double)(list ? list->size() : 1));
    }

    Signature nth_sig = "nth($list, $n)";
    BUILT_IN(nth)
    {
      double nr = ARGVAL("$n");
      Map* m = Cast<Map>(env["$list"]);
      List_Obj l = Cast<List>(env["$list"]);
      SassListObj sl = Cast<SassList>(env["$list"]);
      if (nr == 0) error("$n: List index may not be 0.", pstate, traces);
      // if the argument isn't a list, then wrap it in a singleton list
      if (!m && !l && !sl) {
        sl = SASS_MEMORY_NEW(SassList, pstate);
        sl->append(ARG("$list", Expression, "an expression"));
      }
      size_t len = m ? m->length() : sl ? sl->length() : l->length();
      bool empty = m ? m->empty() : sl ? sl->empty() : l->empty();
      if (empty) error("argument `$list` of `" + std::string(sig) + "` must not be empty", pstate, traces);
      double index = std::floor(nr < 0 ? len + nr : nr - 1);
      if (index < 0 || index > len - 1) {
        if (std::floor(nr) == 0) error("$n: List index may not be 0.", pstate, traces);
        std::stringstream strm;
        strm << "$n: Invalid index ";
        strm << std::floor(nr);
        strm << " for a list with ";
        strm << len << " elements.";
        error(strm.str(), pstate, traces);
      }

      if (m) {
        SassListObj l = SASS_MEMORY_NEW(SassList, pstate);
        l->append(m->keys()[static_cast<unsigned int>(index)]);
        l->append(m->at(m->keys()[static_cast<unsigned int>(index)]));
        return l.detach();
      }
      else if (sl) {
        Value_Obj rv = sl->get(static_cast<int>(index));
        rv = SASS_MEMORY_COPY(rv);
        return rv.detach();
      }
      else {
        Value_Obj rv = l->value_at_index(static_cast<int>(index));
        rv = SASS_MEMORY_COPY(rv);
        return rv.detach();
      }
    }

    Signature set_nth_sig = "set-nth($list, $n, $value)";
    BUILT_IN(set_nth)
    {
      Map_Obj m = Cast<Map>(env["$list"]);
      List_Obj l = Cast<List>(env["$list"]);
      SassList_Obj sl = Cast<SassList>(env["$list"]);
      Number_Obj n = ARGNUM("$n");
      Expression_Obj v = ARG("$value", Expression, "an expression");
      if (!l && !sl) {
        sl = SASS_MEMORY_NEW(SassList, pstate);
        sl->append(ARG("$list", Expression, "an expression"));
      }
      if (m) {
        sl = m->to_list(pstate);
      }
      if (l && !sl) {
        sl = list_to_sass_list(l);
      }
      if (sl->empty()) error("argument `$list` of `" + std::string(sig) + "` must not be empty", pstate, traces);
      double index = std::floor(n->value() < 0 ? sl->length() + n->value() : n->value() - 1);
      if (index < 0 || index > sl->length() - 1) {
        if (std::floor(n->value()) == 0) error("$n: List index may not be 0.", pstate, traces);
        std::stringstream strm;
        strm << "$n: Invalid index ";
        strm << std::floor(n->value());
        strm << " for a list with ";
        strm << sl->length() << " elements.";
        error(strm.str(), pstate, traces);
      }
      SassList* result = SASS_MEMORY_NEW(SassList, pstate, sl->separator());
      result->hasBrackets(sl->hasBrackets());
      for (size_t i = 0, L = sl->length(); i < L; ++i) {
        if (i == index) result->append(v);
        else result->append(sl->get(i));
      }
      return result;
    }

    Signature index_sig = "index($list, $value)";
    BUILT_IN(index)
    {
      Map_Obj m = Cast<Map>(env["$list"]);
      List_Obj l = Cast<List>(env["$list"]);
      SassList_Obj sl = Cast<SassList>(env["$list"]);

      Expression_Obj v = ARG("$value", Expression, "an expression");
      if (!l && !sl) {
        sl = SASS_MEMORY_NEW(SassList, pstate);
        sl->append(ARG("$list", Expression, "an expression"));
      }
      if (m) {
        sl = m->to_list(pstate);
      }
      if (!sl && l) {
        sl = list_to_sass_list(l);
      }
      for (size_t i = 0, L = sl->length(); i < L; ++i) {
        if (Operators::eq(sl->get(i), v)) {
          return SASS_MEMORY_NEW(Number, pstate, (double)(i + 1));
        }
      }
      return SASS_MEMORY_NEW(Null, pstate);
    }

    Signature join_sig = "join($list1, $list2, $separator: auto, $bracketed: auto)";
    BUILT_IN(join)
    {
      Map_Obj m1 = Cast<Map>(env["$list1"]);
      Map_Obj m2 = Cast<Map>(env["$list2"]);
      List_Obj l1 = Cast<List>(env["$list1"]);
      List_Obj l2 = Cast<List>(env["$list2"]);
      SassList_Obj sl1 = Cast<SassList>(env["$list1"]);
      SassList_Obj sl2 = Cast<SassList>(env["$list2"]);
      Sass_Separator l1sep = SASS_UNDEF;
      Sass_Separator l2sep = SASS_UNDEF;
      String_Constant_Obj sep = ARGSTRC("$separator");
      enum Sass_Separator sep_val = SASS_UNDEF; // (l1 ? l1->separator() : SASS_SPACE);
      Value* bracketed = ARG("$bracketed", Value, "a value");
      bool is_bracketed = (l1 ? l1->is_bracketed() : sl1 ? sl1->hasBrackets() : false);

      if (!l1 && !sl1) {
        sl1 = SASS_MEMORY_NEW(SassList, pstate);
        sl1->append(ARG("$list1", Expression, "an expression"));
        sl1->separator(SASS_UNDEF);
        // sep_val = (l2 ? l2->separator() : SASS_UNDEF);
        is_bracketed = (l2 ? l2->is_bracketed() : sl2 ? sl2->hasBrackets() : false);
      }
      if (!l2 && !sl2) {
        sl2 = SASS_MEMORY_NEW(SassList, pstate);
        sl2->append(ARG("$list2", Expression, "an expression"));
        sl2->separator(SASS_UNDEF);
      }

      if (l1 && !sl1) {
        sl1 = list_to_sass_list(l1);
      }

      if (l2 && !sl2) {
        sl2 = list_to_sass_list(l2);
      }

      if (m1) {
        sl1 = m1->to_list(pstate);
      }
      if (m2) {
        sl2 = m2->to_list(pstate);
      }

      // size_t len = sl1->length() + sl2->length();
      std::string sep_str = unquote(sep->value());
      if (sep_str == "space") sep_val = SASS_SPACE;
      else if (sep_str == "comma") sep_val = SASS_COMMA;
      else if (sep_str != "auto") error("$separator: Must be \"space\", \"comma\", or \"auto\".", pstate, traces);

      if (sl1) { l1sep = sl1->separator(); }
      if (sl2) { l2sep = sl2->separator(); }

      if (sep->value() == "auto") {
        // std::cerr << "is auto\n";
        // debug_ast(l1);
        if (l1sep != SASS_UNDEF) {
          // std::cerr << "from l1 " << debug_vec(l1) << "\n";
          sep_val = l1sep;
        }
        else if (l2sep != SASS_UNDEF) {
          // std::cerr << "from l2 " << debug_vec(l2) << "\n";
          sep_val = l2sep;
        }
        else {
          sep_val = SASS_SPACE;
        }
      }
      else if (sep->value() == "space") {
        sep_val = SASS_SPACE;
      }
      else if (sep->value() == "comma") {
        sep_val = SASS_COMMA;
      }


      String_Constant_Obj bracketed_as_str = Cast<String_Constant>(bracketed);
      bool bracketed_is_auto = bracketed_as_str && unquote(bracketed_as_str->value()) == "auto";
      if (!bracketed_is_auto) {
        is_bracketed = !bracketed->is_false();
      }
      SassList_Obj result = SASS_MEMORY_NEW(SassList, pstate, sep_val);
      result->hasBrackets(is_bracketed);
      for (auto item1 : sl1->elements()) {
        if (Argument * arg = Cast<Argument>(item1)) {
          result->append(arg->value());
        }
        else {
          result->append(item1);
        }
      }
      for (auto item2 : sl2->elements()) {
        if (Argument * arg = Cast<Argument>(item2)) {
          result->append(arg->value());
        }
        else {
          result->append(item2);
        }
      }
      // result->concat(sl1);
      // result->concat(sl2);
      return result.detach();
    }

    Signature append_sig = "append($list, $val, $separator: auto)";
    BUILT_IN(append)
    {
      Map_Obj m = Cast<Map>(env["$list"]);
      List_Obj l = Cast<List>(env["$list"]);
      SassList_Obj sl = Cast<SassList>(env["$list"]);
      Expression_Obj v = ARG("$val", Expression, "an expression");
      String_Constant_Obj sep = ARGSTRC("$separator");
      if (!l && !sl) {
        sl = SASS_MEMORY_NEW(SassList, pstate);
        sl->append(ARG("$list", Expression, "an expression"));
      }
      if (m) {
        sl = m->to_list(pstate);
      }
      if (sl) {
        SassListObj result = SASS_MEMORY_COPY(sl);
        std::string sep_str(unquote(sep->value()));
        if (sep_str != "auto") { // check default first
          if (sep_str == "space") result->separator(SASS_SPACE);
          else if (sep_str == "comma") result->separator(SASS_COMMA);
          else error("$separator: Must be \"space\", \"comma\", or \"auto\".", pstate, traces);
        }
        result->append(v);
        if (!result->empty() && result->separator() == SASS_UNDEF) {
          result->separator(SASS_SPACE);
        }
        return result.detach();

      }
      ListObj result = SASS_MEMORY_COPY(l);
      std::string sep_str(unquote(sep->value()));
      if (sep_str != "auto") { // check default first
        if (sep_str == "space") result->separator(SASS_SPACE);
        else if (sep_str == "comma") result->separator(SASS_COMMA);
        else error("$separator: Must be \"space\", \"comma\", or \"auto\".", pstate, traces);
      }
      if (l->is_arglist()) {
        result->append(SASS_MEMORY_NEW(Argument,
                                       v->pstate(),
                                       v,
                                       "",
                                       false,
                                       false));

      } else {
        result->append(v);
      }
      if (!result->empty() && result->separator() == SASS_UNDEF) {
        result->separator(SASS_SPACE);
      }
      return result.detach();
    }

    Signature zip_sig = "zip($lists...)";
    BUILT_IN(zip)
    {
      List_Obj arglist = SASS_MEMORY_COPY(ARGLIST("$lists"));
      size_t shortest = 0;
      for (size_t i = 0, L = arglist->length(); i < L; ++i) {
        List_Obj sl = Cast<List>(arglist->value_at_index(i));
        SassList_Obj ith = Cast<SassList>(arglist->value_at_index(i));
        if (sl) {
          ith = SASS_MEMORY_NEW(SassList, pstate, sl->separator());
          ith->hasBrackets(sl->is_bracketed());
          for (auto item : sl->elements()) {
            ith->append(item);
          }
        }
        Map_Obj mith = Cast<Map>(arglist->value_at_index(i));
        if (!ith) {
          if (mith) {
            ith = mith->to_list(pstate);
            // ith = list_to_sass_list(sll);
          } else {
            ith = SASS_MEMORY_NEW(SassList, pstate);
            ith->append(arglist->value_at_index(i));
          }
          if (arglist->is_arglist()) {
            Argument_Obj arg = (Argument*)(arglist->at(i).ptr()); // XXX
            arg->value(ith);
          } else {
            (*arglist)[i] = ith;
          }
        }
        shortest = (i ? std::min(shortest, ith->length()) : ith->length());
      }
      SassList* zippers = SASS_MEMORY_NEW(SassList, pstate, SASS_COMMA);
      size_t L = arglist->length();
      for (size_t i = 0; i < shortest; ++i) {
        SassList* zipper = SASS_MEMORY_NEW(SassList, pstate);
        for (size_t j = 0; j < L; ++j) {
          if (Cast<SassList>(arglist->value_at_index(j))) {
            zipper->append(Cast<SassList>(arglist->value_at_index(j))->at(i));
          }
          else {
            zipper->append(Cast<List>(arglist->value_at_index(j))->at(i));
          }
        }
        zippers->append(zipper);
      }
      return zippers;
    }

    Signature list_separator_sig = "list-separator($list)";
    BUILT_IN(list_separator)
    {
      MapObj m = Cast<Map>(env["$list"]);
      ListObj l = Cast<List>(env["$list"]);
      SassListObj sl = Cast<SassList>(env["$list"]);
      if (m) {
        if (m->empty()) { return SASS_MEMORY_NEW(StringLiteral, pstate, "space"); }
        else { return SASS_MEMORY_NEW(StringLiteral, pstate, "comma"); }
      }
      std::string sep = "space";
      if (sl) {
        if (sl->separator() == SASS_COMMA) sep = "comma";
        else if (sl->separator() == SASS_SPACE) sep = "space";
      }
      else if (l) {
        if (l->separator() == SASS_COMMA) sep = "comma";
        else if (l->separator() == SASS_SPACE) sep = "space";
      }

      return SASS_MEMORY_NEW(StringLiteral,
                               pstate,
                               sep);
    }

    Signature is_bracketed_sig = "is-bracketed($list)";
    BUILT_IN(is_bracketed)
    {
      Value_Obj value = ARG("$list", Value, "a value");
      if (SassListObj sl = Cast<SassList>(value)) {
        return SASS_MEMORY_NEW(Boolean, pstate, sl && sl->hasBrackets());
      }
      List_Obj list = Cast<List>(value);
      return SASS_MEMORY_NEW(Boolean, pstate, list && list->is_bracketed());
    }

  }

}
