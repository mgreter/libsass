#include "sass.hpp"
#include "bind.hpp"
#include "ast.hpp"
#include "backtrace.hpp"
#include "debugger.hpp"
#include "context.hpp"
#include "expand.hpp"
#include "eval.hpp"
#include <map>
#include <iostream>
#include <sstream>

namespace Sass {

  void bind(std::string type, std::string name, Parameters_Obj ps, Arguments_Obj as, Env* env, Eval* eval, Backtraces& traces)
  {
    std::string callee(type + " " + name);

    size_t asL = 0, psL = 0;
    if (as) asL = as->length();
    if (ps) psL = ps->length();

    NormalizedMap<Parameter_Obj> param_map;
    List_Obj varargs = SASS_MEMORY_NEW(List, as->pstate());
    varargs->is_arglist(true); // enable keyword size handling

    for (size_t i = 0, L = asL; i < L; ++i) {
      if (auto str = Cast<String_Quoted>((*as)[i]->value())) {
        // force optional quotes (only if needed)
        if (str->quote_mark()) {
          str->quote_mark('*');
        }
      }
    }

    // Set up a map to ensure named arguments refer to actual parameters. Also
    // eval each default value left-to-right, wrt env, populating env as we go.
    for (size_t i = 0, L = psL; i < L; ++i) {
      Parameter_Obj  p = ps->at(i);
      param_map[p->name()] = p;
      // if (p->default_value()) {
      //   env->local_frame()[p->name()] = p->default_value()->perform(eval->with(env));
      // }
    }

    // plug in all args; if we have leftover params, deal with it later
    size_t ip = 0, LP = psL;
    size_t ia = 0, LA = asL;
    while (ia < LA) {
      Argument_Obj a = as->at(ia);
      // std::cerr << "first run\n";
      // debug_ast(a);
      if (ip >= LP) {
        // skip empty rest arguments
        if (a->is_rest_argument()) {
          if (List_Obj l = Cast<List>(a->value())) {
            if (l->length() == 0) {
              ++ia; continue;
            }
          }
          if (SassList_Obj l = Cast<SassList>(a->value())) {
            if (l->length() == 0) {
              ++ia; continue;
            }
          }
        }
        std::stringstream msg;
        msg << "Only " << LP << " ";
        msg << (LP == 1 ? "argument" : "arguments");
        msg << " allowed, but " << LA << " ";
        msg << (LA == 1 ? "was" : "were");
        msg << " passed.";
        return error(msg.str(), as->pstate(), traces);
      }
      Parameter_Obj p = ps->at(ip);

      // If the current parameter is the rest parameter, process and break the loop
      if (p->is_rest_parameter()) {
        // The next argument by coincidence provides a rest argument
        if (a->is_rest_argument()) {

          // We should always get a list for rest arguments
          if (SassList_Obj rest = Cast<SassList>(a->value())) {
            // std::cerr << "Got a SassList\n";
            // create a new list object for wrapped items
            List* arglist = SASS_MEMORY_NEW(List,
              p->pstate(),
              0,
              rest->separator(),
              true);
            // wrap each item from list as an argument
            for (Expression_Obj item : rest->elements()) {
              if (Argument_Obj arg = Cast<Argument>(item)) {
                arglist->append(SASS_MEMORY_COPY(arg)); // copy
              }
              else {
                arglist->append(SASS_MEMORY_NEW(Argument,
                  item->pstate(),
                  item,
                  "",
                  false,
                  false));
              }
            }
            // assign new arglist to environment
            // std::cerr << "Assign " << p->name() << "\n";
            env->local_frame()[p->name()] = arglist;
          }
          // We should always get a list for rest arguments
          else if (List_Obj rest = Cast<List>(a->value())) {
            // create a new list object for wrapped items
            List* arglist = SASS_MEMORY_NEW(List,
              p->pstate(),
              0,
              rest->separator(),
              true);
            // wrap each item from list as an argument
            for (Expression_Obj item : rest->elements()) {
              if (Argument_Obj arg = Cast<Argument>(item)) {
                arglist->append(SASS_MEMORY_COPY(arg)); // copy
              }
              else {
                arglist->append(SASS_MEMORY_NEW(Argument,
                  item->pstate(),
                  item,
                  "",
                  false,
                  false));
              }
            }
            // assign new arglist to environment
            env->local_frame()[p->name()] = arglist;
          }
          // invalid state
          else {
            throw std::runtime_error("invalid state");
          }
        } else if (a->is_keyword_argument()) {

          // expand keyword arguments into their parameters
          List* arglist = SASS_MEMORY_NEW(List, p->pstate(), 0, SASS_COMMA, true);
          // std::cerr << "Assign2 " << p->name() << "\n";
          env->local_frame()[p->name()] = arglist;
          Map_Obj argmap = Cast<Map>(a->value());
          for (auto key : argmap->keys()) {
            if (String_Constant_Obj str = Cast<String_Constant>(key)) {
              std::string param = unquote(str->value());
              arglist->append(SASS_MEMORY_NEW(Argument,
                key->pstate(),
                argmap->at(key),
                "$" + param,
                false,
                false));
            }
            else if (StringLiteralObj str = Cast<StringLiteral>(key)) {
              std::string param = unquote(str->text());
              arglist->append(SASS_MEMORY_NEW(Argument,
                                              key->pstate(),
                                              argmap->at(key),
                                              "$" + param,
                                              false,
                                              false));
            } else {
              traces.push_back(Backtrace(key->pstate()));
              throw Exception::InvalidVarKwdType(key->pstate(), traces, key->inspect(), a);
            }
          }

        } else {

          // create a new list object for wrapped items
          List_Obj arglist = SASS_MEMORY_NEW(List,
                                          p->pstate(),
                                          0,
                                          SASS_COMMA,
                                          true);
          // consume the next args
          while (ia < LA) {
            // get and post inc
            a = (*as)[ia++];
            // maybe we have another list as argument
            List_Obj ls = Cast<List>(a->value());
            SassListObj lsl = Cast<SassList>(a->value());
            // skip any list completely if empty
            if (ls && ls->empty() && a->is_rest_argument()) continue;
            if (lsl && lsl->empty() && a->is_rest_argument()) continue;

            Expression_Obj value = a->value();
            if (Argument_Obj arg = Cast<Argument>(value)) {
              arglist->append(arg);
            }
            // check if we have rest argument
            else if (a->is_rest_argument()) {
              // preserve the list separator from rest args
              if (SassListObj rest = Cast<SassList>(a->value())) {
                arglist->separator(rest->separator());

                for (size_t i = 0, L = rest->length(); i < L; ++i) {
                  ValueObj obj = rest->get(i);
                  arglist->append(SASS_MEMORY_NEW(Argument,
                    obj->pstate(),
                    obj,
                    "",
                    false,
                    false));
                }
              }
              else if (List_Obj rest = Cast<List>(a->value())) {
                arglist->separator(rest->separator());

                for (size_t i = 0, L = rest->length(); i < L; ++i) {
                  Expression_Obj obj = rest->value_at_index(i);
                  arglist->append(SASS_MEMORY_NEW(Argument,
                    obj->pstate(),
                    obj,
                    "",
                    false,
                    false));
                }
              }
              // no more arguments
              break;
            }
            // wrap all other value types into Argument
            else {
              arglist->append(SASS_MEMORY_NEW(Argument,
                                            a->pstate(),
                                            a->value(),
                                            a->name(),
                                            false,
                                            false));
            }
          }
          // assign new arglist to environment
          // std::cerr << "Assign3 " << p->name() << "\n";
          env->local_frame()[p->name()] = arglist;
        }
        // consumed parameter
        ++ip;
        // no more paramaters
        break;
      }

      // If the current argument is the rest argument, extract a value for processing
      else if (a->is_rest_argument()) {
        // normal param and rest arg
        List_Obj arglist = Cast<List>(a->value());
        SassListObj arglist2 = Cast<SassList>(a->value());
        // debug_ast(arglist, "== ");
        if (!arglist && !arglist2) {
          if (Expression_Obj arg = Cast<Expression>(a->value())) {
            arglist2 = SASS_MEMORY_NEW(SassList, a->pstate(), {});
            arglist2->append(arg);
          }
        }

        // debug_ast(a->value());

        if (arglist2) {
          // std::cerr << "go a sass list\n";
          if (!arglist2 || !arglist2->length()) {
            break;
          }
          else {
            if (arglist2->length() > LP - ip && !ps->has_rest_parameter()) {
              size_t arg_count = (arglist2->length() + LA - 1);
              std::stringstream msg;
              msg << callee << " takes " << LP;
              msg << (LP == 1 ? " argument" : " arguments");
              msg << " but " << arg_count;
              msg << (arg_count == 1 ? " was passed" : " were passed.");
              deprecated_bind(msg.str(), as->pstate());

              while (arglist2->length() > LP - ip) {
                arglist2->elements().erase(arglist2->elements().end() - 1);
              }
            }
          }
          // otherwise move one of the rest args into the param, converting to argument if necessary
          Expression_Obj obj = arglist2->at(0);
          if (!(a = Cast<Argument>(obj))) {
            Expression* a_to_convert = obj;
            a = SASS_MEMORY_NEW(Argument,
              a_to_convert->pstate(),
              a_to_convert,
              "",
              false,
              false);
          }
          arglist2->elements().erase(arglist2->elements().begin());
          if (!arglist2->length() || (ip + 1 == LP)) {
            ++ia;
          }

        }
        else {
          // std::cerr << "got regular list\n";
          // empty rest arg - treat all args as default values
          if (!arglist || !arglist->length()) {
            break;
          }
          else {
            if (arglist->length() > LP - ip && !ps->has_rest_parameter()) {
              size_t arg_count = (arglist->length() + LA - 1);
              std::stringstream msg;
              msg << callee << " takes " << LP;
              msg << (LP == 1 ? " argument" : " arguments");
              msg << " but " << arg_count;
              msg << (arg_count == 1 ? " was passed" : " were passed.");
              deprecated_bind(msg.str(), as->pstate());

              while (arglist->length() > LP - ip) {
                arglist->elements().erase(arglist->elements().end() - 1);
              }
            }
          }
          // otherwise move one of the rest args into the param, converting to argument if necessary
          Expression_Obj obj = arglist->at(0);
          if (!(a = Cast<Argument>(obj))) {
            Expression* a_to_convert = obj;
            a = SASS_MEMORY_NEW(Argument,
              a_to_convert->pstate(),
              a_to_convert,
              "",
              false,
              false);
          }
          arglist->elements().erase(arglist->elements().begin());
          if (!arglist->length() || (!arglist->is_arglist() && ip + 1 == LP)) {
            ++ia;
          }

        }

      } else if (a->is_keyword_argument()) {
        Map_Obj argmap = Cast<Map>(a->value());

        for (auto key : argmap->keys()) {
          String_Constant* val = Cast<String_Constant>(key);
          if (val == NULL) {
            traces.push_back(Backtrace(key->pstate()));
            throw Exception::InvalidVarKwdType(key->pstate(), traces, key->inspect(), a);
          }
          std::string param = "$" + unquote(val->value());

          if (!param_map.count(param)) {
            std::stringstream msg;
            msg << "No argument2 named " << param << ".";
            error(msg.str(), a->pstate(), traces);
          }
          // std::cerr << "Assign4 " << p->name() << "\n";
          env->local_frame()[param] = argmap->at(key);
        }
        ++ia;
        continue;
      } else {
        ++ia;
      }

      if (a->name().empty()) {
        if (env->has_local(p->name())) {
          std::stringstream msg;
          msg << "parameter " << p->name()
          << " provided more than once in call to " << callee;
          error(msg.str(), a->pstate(), traces);
        }
        // ordinal arg -- bind it to the next param
        // std::cerr << "Assign5 " << p->name() << "\n";
        env->local_frame()[p->name()] = a->value();
        ++ip;
      }
      else {
        // named arg -- bind it to the appropriately named param
        if (!param_map.count(a->name())) {
          if (ps->has_rest_parameter()) {
            varargs->append(a);
          } else {
            std::stringstream msg;
            msg << "No argument3 named " << a->name() << ".";
            error(msg.str(), a->pstate(), traces);
          }
        }
        if (param_map[a->name()]) {
          if (param_map[a->name()]->is_rest_parameter()) {
            std::stringstream msg;
            msg << "argument " << a->name() << " of " << callee
                << "cannot be used as named argument";
            error(msg.str(), a->pstate(), traces);
          }
        }
        if (env->has_local(a->name())) {
          std::stringstream msg;
          msg << "parameter " << p->name()
              << "provided more than once in call to " << callee;
          error(msg.str(), a->pstate(), traces);
        }
        // std::cerr << "Assign6 " << p->name() << "\n";
        env->local_frame()[a->name()] = a->value();
      }
    }
    // EO while ia

    // If we make it here, we're out of args but may have leftover params.
    // That's only okay if they have default values, or were already bound by
    // named arguments, or if it's a single rest-param.
    for (size_t i = ip; i < LP; ++i) {
      Parameter_Obj leftover = ps->at(i);
      // cerr << "env for default params:" << endl;
      // env->print();
      // cerr << "********" << endl;
      if (!env->has_local(leftover->name())) {
        if (leftover->is_rest_parameter()) {
          // std::cerr << "Assign7 " << leftover->name() << "\n";
          env->local_frame()[leftover->name()] = varargs;
        }
        else if (leftover->default_value()) {
          Expression* dv = leftover->default_value()->perform(eval);
          // std::cerr << "Assign8 " << leftover->name() << "\n";
          env->local_frame()[leftover->name()] = dv;
        }
        else {
          // param is unbound and has no default value -- error
          throw Exception::MissingArgument(as->pstate(), traces, name, leftover->name(), type);
        }
      }
    }

    return;
  }


}
