#ifndef SASS_CHECK_NESTING_H
#define SASS_CHECK_NESTING_H

#include "ast.hpp"
#include "operation.hpp"

namespace Sass {

  typedef Environment<AST_Node_Obj> Env;

  class CheckNesting : public Operation_CRTP<Statement_Obj, CheckNesting> {

    std::vector<Statement_Obj>  parents;
    Statement_Obj               parent;
    Definition_Obj              current_mixin_definition;

    Statement_Obj fallback_impl(Statement_Obj);
    Statement_Obj before(Statement_Obj);
    Statement_Obj visit_children(Statement_Obj);

  public:
    CheckNesting();
    ~CheckNesting() { }

    Statement_Obj operator()(Block_Obj);
    Statement_Obj operator()(Definition_Obj);

    template <typename U>
    Statement_Obj fallback(U x) {
        return fallback_impl(&this->before(SASS_MEMORY_CAST_PTR(Statement, x)));
    }

  private:
    void invalid_content_parent(Statement_Obj);
    void invalid_charset_parent(Statement_Obj);
    void invalid_extend_parent(Statement_Obj);
    // void invalid_import_parent(Statement_Obj);
    void invalid_mixin_definition_parent(Statement_Obj);
    void invalid_function_parent(Statement_Obj);

    void invalid_function_child(Statement_Obj);
    void invalid_prop_child(Statement_Obj);
    void invalid_prop_parent(Statement_Obj);
    void invalid_return_parent(Statement_Obj);

    bool is_transparent_parent(Statement_Obj, Statement_Obj);

    bool should_visit(Statement_Obj);

    bool is_charset(Statement_Obj);
    bool is_mixin(Statement_Obj);
    bool is_function(Statement_Obj);
    bool is_root_node(Statement_Obj);
    bool is_at_root_node(Statement_Obj);
    bool is_directive_node(Statement_Obj);
  };

}

#endif
