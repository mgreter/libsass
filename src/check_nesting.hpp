#ifndef SASS_CHECK_NESTING_H
#define SASS_CHECK_NESTING_H

#include "ast.hpp"
#include "operation.hpp"

namespace Sass {

  typedef Environment<AST_Node_Obj> Env;

  class CheckNesting : public Operation_CRTP<Statement_Ptr, CheckNesting> {

    std::vector<Statement_Obj>  parents;
    Statement_Obj               parent;
    Definition_Obj              current_mixin_definition;

    Statement_Ptr fallback_impl(Statement_Ptr);
    Statement_Obj before(Statement_Obj);
    Statement_Obj visit_children(Statement_Ptr);

  public:
    CheckNesting();
    ~CheckNesting() { }
    void reset();

    Statement_Ptr operator()(Block_Ptr);
    Statement_Ptr operator()(Definition_Ptr);

    template <typename U>
    Statement_Ptr fallback(U x) {
        return fallback_impl(&this->before(SASS_MEMORY_CAST_PTR(Statement, x)));
    }

  private:
    void invalid_content_parent(Statement_Ptr);
    void invalid_charset_parent(Statement_Ptr);
    void invalid_extend_parent(Statement_Ptr);
    // void invalid_import_parent(Statement_Ptr);
    void invalid_mixin_definition_parent(Statement_Ptr);
    void invalid_function_parent(Statement_Ptr);

    void invalid_function_child(Statement_Ptr);
    void invalid_prop_child(Statement_Ptr);
    void invalid_prop_parent(Statement_Ptr);
    void invalid_return_parent(Statement_Ptr);

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
