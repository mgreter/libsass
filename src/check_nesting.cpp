#include "sass.hpp"
#include <vector>

#include "check_nesting.hpp"

namespace Sass {

  CheckNesting::CheckNesting()
  : parents(std::vector<Statement_Ptr>()),
    parent(0),
    current_mixin_definition(0)
  { }

  Statement_Ptr CheckNesting::before(Statement_Ptr s) {
      if (this->should_visit(s)) return s;
      return 0;
  }

  Statement_Ptr CheckNesting::visit_children(Statement_Ptr parent) {

    Statement_Ptr old_parent = this->parent;

    if (dynamic_cast<At_Root_Block_Ptr>(parent)) {
      std::vector<Statement_Ptr> old_parents = this->parents;
      std::vector<Statement_Ptr> new_parents;

      for (size_t i = 0, L = this->parents.size(); i < L; i++) {
        Statement_Ptr p = this->parents.at(i);
        if (!dynamic_cast<At_Root_Block_Ptr>(parent)->exclude_node(p)) {
          new_parents.push_back(p);
        }
      }
      this->parents = new_parents;

      for (size_t i = this->parents.size(); i > 0; i--) {
        Statement_Ptr p = 0;
        Statement_Ptr gp = 0;
        if (i > 0) p = this->parents.at(i - 1);
        if (i > 1) gp = this->parents.at(i - 2);

        if (!this->is_transparent_parent(p, gp)) {
          this->parent = p;
          break;
        }
      }

      At_Root_Block_Ptr ar = dynamic_cast<At_Root_Block_Ptr>(parent);
      Statement_Ptr ret = this->visit_children(ar->block());

      this->parent = old_parent;
      this->parents = old_parents;

      return ret;
    }


    if (!this->is_transparent_parent(parent, old_parent)) {
      this->parent = parent;
    }

    this->parents.push_back(parent);

    Block_Obj b = SASS_MEMORY_CAST(Block, *parent);

    if (!b) {
      if (Has_Block_Obj bb = SASS_MEMORY_CAST(Has_Block, *parent)) {
        b = bb->block();
      }
    }

    if (b) {
      for (auto n : b->elements()) {
        n->perform(this);
      }
    }

    this->parent = old_parent;
    this->parents.pop_back();

    return &b;
  }


  Statement_Ptr CheckNesting::operator()(Block_Ptr b)
  {
    return this->visit_children(b);
  }

  Statement_Ptr CheckNesting::operator()(Definition_Ptr n)
  {
    if (!is_mixin(n)) return n;

    Definition_Ptr old_mixin_definition = this->current_mixin_definition;
    this->current_mixin_definition = n;

    visit_children(n);

    this->current_mixin_definition = old_mixin_definition;

    return n;
  }

  Statement_Ptr CheckNesting::fallback_impl(Statement_Ptr s)
  {
    if (dynamic_cast<Block_Ptr>(s) || dynamic_cast<Has_Block_Ptr>(s)) {
      return visit_children(s);
    }
    return s;
  }

  bool CheckNesting::should_visit(Statement_Ptr node)
  {
    if (!this->parent) return true;

    if (dynamic_cast<Content_Ptr>(node))
    { this->invalid_content_parent(this->parent); }

    if (is_charset(node))
    { this->invalid_charset_parent(this->parent); }

    if (dynamic_cast<Extension_Ptr>(node))
    { this->invalid_extend_parent(this->parent); }

    // if (dynamic_cast<Import_Ptr>(node))
    // { this->invalid_import_parent(this->parent); }

    if (this->is_mixin(node))
    { this->invalid_mixin_definition_parent(this->parent); }

    if (this->is_function(node))
    { this->invalid_function_parent(this->parent); }

    if (this->is_function(this->parent))
    { this->invalid_function_child(node); }

    if (dynamic_cast<Declaration_Ptr>(node))
    { this->invalid_prop_parent(this->parent); }

    if (
      dynamic_cast<Declaration_Ptr>(this->parent)
    ) { this->invalid_prop_child(node); }

    if (dynamic_cast<Return_Ptr>(node))
    { this->invalid_return_parent(this->parent); }

    return true;
  }

  void CheckNesting::invalid_content_parent(Statement_Ptr parent)
  {
    if (!this->current_mixin_definition) {
      throw Exception::InvalidSass(
        parent->pstate(),
        "@content may only be used within a mixin."
      );
    }
  }

  void CheckNesting::invalid_charset_parent(Statement_Ptr parent)
  {
    if (!(
        is_root_node(parent)
    )) {
      throw Exception::InvalidSass(
        parent->pstate(),
        "@charset may only be used at the root of a document."
      );
    }
  }

  void CheckNesting::invalid_extend_parent(Statement_Ptr parent)
  {
    if (!(
        dynamic_cast<Ruleset_Ptr>(parent) ||
        dynamic_cast<Mixin_Call_Ptr>(parent) ||
        is_mixin(parent)
    )) {
      throw Exception::InvalidSass(
        parent->pstate(),
        "Extend directives may only be used within rules."
      );
    }
  }

  // void CheckNesting::invalid_import_parent(Statement_Ptr parent)
  // {
  //   for (auto pp : this->parents) {
  //     if (
  //         dynamic_cast<Each_Ptr>(pp) ||
  //         dynamic_cast<For_Ptr>(pp) ||
  //         dynamic_cast<If_Ptr>(pp) ||
  //         dynamic_cast<While_Ptr>(pp) ||
  //         dynamic_cast<Trace_Ptr>(pp) ||
  //         dynamic_cast<Mixin_Call_Ptr>(pp) ||
  //         is_mixin(pp)
  //     ) {
  //       throw Exception::InvalidSass(
  //         parent->pstate(),
  //         "Import directives may not be defined within control directives or other mixins."
  //       );
  //     }
  //   }

  //   if (this->is_root_node(parent)) {
  //     return;
  //   }

  //   if (false/*n.css_import?*/) {
  //     throw Exception::InvalidSass(
  //       parent->pstate(),
  //       "CSS import directives may only be used at the root of a document."
  //     );
  //   }
  // }

  void CheckNesting::invalid_mixin_definition_parent(Statement_Ptr parent)
  {
    for (auto pp : this->parents) {
      if (
          dynamic_cast<Each_Ptr>(pp) ||
          dynamic_cast<For_Ptr>(pp) ||
          dynamic_cast<If_Ptr>(pp) ||
          dynamic_cast<While_Ptr>(pp) ||
          dynamic_cast<Trace_Ptr>(pp) ||
          dynamic_cast<Mixin_Call_Ptr>(pp) ||
          is_mixin(pp)
      ) {
        throw Exception::InvalidSass(
          parent->pstate(),
          "Mixins may not be defined within control directives or other mixins."
        );
      }
    }
  }

  void CheckNesting::invalid_function_parent(Statement_Ptr parent)
  {
    for (auto pp : this->parents) {
      if (
          dynamic_cast<Each_Ptr>(pp) ||
          dynamic_cast<For_Ptr>(pp) ||
          dynamic_cast<If_Ptr>(pp) ||
          dynamic_cast<While_Ptr>(pp) ||
          dynamic_cast<Trace_Ptr>(pp) ||
          dynamic_cast<Mixin_Call_Ptr>(pp) ||
          is_mixin(pp)
      ) {
        throw Exception::InvalidSass(
          parent->pstate(),
          "Functions may not be defined within control directives or other mixins."
        );
      }
    }
  }

  void CheckNesting::invalid_function_child(Statement_Ptr child)
  {
    if (!(
        dynamic_cast<Each_Ptr>(child) ||
        dynamic_cast<For_Ptr>(child) ||
        dynamic_cast<If_Ptr>(child) ||
        dynamic_cast<While_Ptr>(child) ||
        dynamic_cast<Trace_Ptr>(child) ||
        dynamic_cast<Comment_Ptr>(child) ||
        dynamic_cast<Debug_Ptr>(child) ||
        dynamic_cast<Return_Ptr>(child) ||
        dynamic_cast<Variable_Ptr>(child) ||
        dynamic_cast<Warning_Ptr>(child) ||
        dynamic_cast<Error_Ptr>(child)
    )) {
      throw Exception::InvalidSass(
        child->pstate(),
        "Functions can only contain variable declarations and control directives."
      );
    }
  }

  void CheckNesting::invalid_prop_child(Statement_Ptr child)
  {
    if (!(
        dynamic_cast<Each_Ptr>(child) ||
        dynamic_cast<For_Ptr>(child) ||
        dynamic_cast<If_Ptr>(child) ||
        dynamic_cast<While_Ptr>(child) ||
        dynamic_cast<Trace_Ptr>(child) ||
        dynamic_cast<Comment_Ptr>(child) ||
        dynamic_cast<Declaration_Ptr>(child) ||
        dynamic_cast<Mixin_Call_Ptr>(child)
    )) {
      throw Exception::InvalidSass(
        child->pstate(),
        "Illegal nesting: Only properties may be nested beneath properties."
      );
    }
  }

  void CheckNesting::invalid_prop_parent(Statement_Ptr parent)
  {
    if (!(
        is_mixin(parent) ||
        is_directive_node(parent) ||
        dynamic_cast<Ruleset_Ptr>(parent) ||
        dynamic_cast<Keyframe_Rule_Ptr>(parent) ||
        dynamic_cast<Declaration_Ptr>(parent) ||
        dynamic_cast<Mixin_Call_Ptr>(parent)
    )) {
      throw Exception::InvalidSass(
        parent->pstate(),
        "Properties are only allowed within rules, directives, mixin includes, or other properties."
      );
    }
  }

  void CheckNesting::invalid_return_parent(Statement_Ptr parent)
  {
    if (!this->is_function(parent)) {
      throw Exception::InvalidSass(
        parent->pstate(),
        "@return may only be used within a function."
      );
    }
  }

  bool CheckNesting::is_transparent_parent(Statement_Ptr parent, Statement_Ptr grandparent)
  {
    bool parent_bubbles = parent && parent->bubbles();

    bool valid_bubble_node = parent_bubbles &&
                             !is_root_node(grandparent) &&
                             !is_at_root_node(grandparent);

    return dynamic_cast<Import_Ptr>(parent) ||
           dynamic_cast<Each_Ptr>(parent) ||
           dynamic_cast<For_Ptr>(parent) ||
           dynamic_cast<If_Ptr>(parent) ||
           dynamic_cast<While_Ptr>(parent) ||
           dynamic_cast<Trace_Ptr>(parent) ||
           valid_bubble_node;
  }

  bool CheckNesting::is_charset(Statement_Ptr n)
  {
    Directive_Ptr d = dynamic_cast<Directive_Ptr>(n);
    return d && d->keyword() == "charset";
  }

  bool CheckNesting::is_mixin(Statement_Ptr n)
  {
    Definition_Ptr def = dynamic_cast<Definition_Ptr>(n);
    return def && def->type() == Definition::MIXIN;
  }

  bool CheckNesting::is_function(Statement_Ptr n)
  {
    Definition_Ptr def = dynamic_cast<Definition_Ptr>(n);
    return def && def->type() == Definition::FUNCTION;
  }

  bool CheckNesting::is_root_node(Statement_Ptr n)
  {
    if (dynamic_cast<Ruleset_Ptr>(n)) return false;

    Block_Ptr b = dynamic_cast<Block_Ptr>(n);
    return b && b->is_root();
  }

  bool CheckNesting::is_at_root_node(Statement_Ptr n)
  {
    return dynamic_cast<At_Root_Block_Ptr>(n) != NULL;
  }

  bool CheckNesting::is_directive_node(Statement_Ptr n)
  {
    return dynamic_cast<Directive_Ptr>(n) ||
           dynamic_cast<Import_Ptr>(n) ||
           dynamic_cast<Media_Block_Ptr>(n) ||
           dynamic_cast<Supports_Block_Ptr>(n);
  }
}
