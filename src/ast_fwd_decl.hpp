#ifndef SASS_AST_FWD_DECL_H
#define SASS_AST_FWD_DECL_H

#include <iostream>

/////////////////////////////////////////////
// Forward declarations for the AST visitors.
/////////////////////////////////////////////
namespace Sass {


  class Memory_Object {
  protected:
  friend class Memory_Ptr;
  friend class Memory_Manager;
    long refcounter;
    long refcount;
  public:
    Memory_Object() { refcount = 0; refcounter = 0; };
    virtual ~Memory_Object() {};
  };


  class Memory_Ptr {
  private:
    Memory_Object* node;
  public:
    // the empty constructor
    Memory_Ptr()
    : node(NULL) {};
    // the create constructor
    Memory_Ptr(Memory_Object* ptr)
    : node(ptr) {
      if (node) {
        node->refcounter += 1;
       //  std::cerr << "Create, " << node << " - increase refcount, now at " << node->refcounter << "\n";
      }
    };
    // the copy constructor
    Memory_Ptr(const Memory_Ptr& obj)
    : node(obj.node) {
      if (node) {
        node->refcounter += 1;
       //  std::cerr << "Copy, " << node << " - increase refcount, now at " << node->refcounter << "\n";
      }
    }
    ~Memory_Ptr() {
      if (this->node) {
        // this gives errors for nested?
        this->node->refcounter -= 1;
        // std::cerr << "decrease refcount, " << node << " - now at " << node->refcounter << "\n";
        if (this->node->refcounter == 0) {
          // std::cerr << "DELETE me " << node << "\n";
          // delete(node);
          // delete and remove
        }
      }
    };
  public:
    Memory_Object* obj () {
      return node;
    };
    Memory_Object* obj () const {
      return node;
    };
    Memory_Object* operator-> () {
      return node;
    };
    bool isNull () {
      return node == NULL;
    };
    bool isNull () const {
      return node == NULL;
    };
    operator bool() {
      return node != NULL;
    };
    operator bool() const {
      return node != NULL;
    };

  };

  template < typename T >
  class Memory_Node : private Memory_Ptr {
  public:
    Memory_Node()
    : Memory_Ptr(NULL) {};
    Memory_Node(T* node)
    : Memory_Ptr(node) {};
    Memory_Node(const T& node)
    : Memory_Ptr(node) {};
    ~Memory_Node() {};
  public:
    T* operator& () {
      return static_cast<T*>(this->obj());
    };
    T* operator& () const {
      return static_cast<T*>(this->obj());
    };
    T& operator* () {
      return *static_cast<T*>(this->obj());
    };
    T& operator* () const {
      return *static_cast<T*>(this->obj());
    };
    T* operator-> () {
      return static_cast<T*>(this->obj());
    };
    T* operator-> () const {
      return static_cast<T*>(this->obj());
    };
    T* ptr () {
      return static_cast<T*>(this->obj());
    };
    operator bool() {
      return this->obj() != NULL;
    };
    operator bool() const {
      return this->obj() != NULL;
    };
  };

  class AST_Node_Ref;
  typedef AST_Node_Ref AST_Node;
  typedef AST_Node* AST_Node_Ptr;
  typedef AST_Node const* AST_Node_Ptr_Const;

  class Has_Block_Ref;
  typedef Has_Block_Ref Has_Block;
  typedef Has_Block* Has_Block_Ptr;
  typedef Has_Block const* Has_Block_Ptr_Const;

  class Simple_Selector_Ref;
  typedef Simple_Selector_Ref Simple_Selector;
  typedef Simple_Selector* Simple_Selector_Ptr;
  typedef Simple_Selector const* Simple_Selector_Ptr_Const;

  class PreValue_Ref;
  typedef PreValue_Ref PreValue;
  typedef PreValue* PreValue_Ptr;
  typedef PreValue const* PreValue_Ptr_Const;
  class Thunk_Ref;
  typedef Thunk_Ref Thunk;
  typedef Thunk* Thunk_Ptr;
  typedef Thunk const* Thunk_Ptr_Const;
  class Block_Ref;
  typedef Block_Ref Block;
  typedef Block* Block_Ptr;
  typedef Block const* Block_Ptr_Const;
  class Expression_Ref;
  typedef Expression_Ref Expression;
  typedef Expression* Expression_Ptr;
  typedef Expression const* Expression_Ptr_Const;
  class Statement_Ref;
  typedef Statement_Ref Statement;
  typedef Statement* Statement_Ptr;
  typedef Statement const* Statement_Ptr_Const;
  class Value_Ref;
  typedef Value_Ref Value;
  typedef Value* Value_Ptr;
  typedef Value const* Value_Ptr_Const;
  class Declaration_Ref;
  typedef Declaration_Ref Declaration;
  typedef Declaration* Declaration_Ptr;
  typedef Declaration const* Declaration_Ptr_Const;
  class Ruleset_Ref;
  typedef Ruleset_Ref Ruleset;
  typedef Ruleset* Ruleset_Ptr;
  typedef Ruleset const* Ruleset_Ptr_Const;
  class Bubble_Ref;
  typedef Bubble_Ref Bubble;
  typedef Bubble* Bubble_Ptr;
  typedef Bubble const* Bubble_Ptr_Const;
  class Trace_Ref;
  typedef Trace_Ref Trace;
  typedef Trace* Trace_Ptr;
  typedef Trace const* Trace_Ptr_Const;

  class Media_Block_Ref;
  typedef Media_Block_Ref Media_Block;
  typedef Media_Block* Media_Block_Ptr;
  typedef Media_Block const* Media_Block_Ptr_Const;
  class Supports_Block_Ref;
  typedef Supports_Block_Ref Supports_Block;
  typedef Supports_Block* Supports_Block_Ptr;
  typedef Supports_Block const* Supports_Block_Ptr_Const;
  class Directive_Ref;
  typedef Directive_Ref Directive;
  typedef Directive* Directive_Ptr;
  typedef Directive const* Directive_Ptr_Const;


  class Keyframe_Rule_Ref;
  typedef Keyframe_Rule_Ref Keyframe_Rule;
  typedef Keyframe_Rule* Keyframe_Rule_Ptr;
  typedef Keyframe_Rule const* Keyframe_Rule_Ptr_Const;
  class At_Root_Block_Ref;
  typedef At_Root_Block_Ref At_Root_Block;
  typedef At_Root_Block* At_Root_Block_Ptr;
  typedef At_Root_Block const* At_Root_Block_Ptr_Const;
  class Assignment_Ref;
  typedef Assignment_Ref Assignment;
  typedef Assignment* Assignment_Ptr;
  typedef Assignment const* Assignment_Ptr_Const;

  class Import_Ref;
  typedef Import_Ref Import;
  typedef Import* Import_Ptr;
  typedef Import const* Import_Ptr_Const;
  class Import_Stub_Ref;
  typedef Import_Stub_Ref Import_Stub;
  typedef Import_Stub* Import_Stub_Ptr;
  typedef Import_Stub const* Import_Stub_Ptr_Const;
  class Warning_Ref;
  typedef Warning_Ref Warning;
  typedef Warning* Warning_Ptr;
  typedef Warning const* Warning_Ptr_Const;

  class Error_Ref;
  typedef Error_Ref Error;
  typedef Error* Error_Ptr;
  typedef Error const* Error_Ptr_Const;
  class Debug_Ref;
  typedef Debug_Ref Debug;
  typedef Debug* Debug_Ptr;
  typedef Debug const* Debug_Ptr_Const;
  class Comment_Ref;
  typedef Comment_Ref Comment;
  typedef Comment* Comment_Ptr;
  typedef Comment const* Comment_Ptr_Const;

  class If_Ref;
  typedef If_Ref If;
  typedef If* If_Ptr;
  typedef If const* If_Ptr_Const;
  class For_Ref;
  typedef For_Ref For;
  typedef For* For_Ptr;
  typedef For const* For_Ptr_Const;
  class Each_Ref;
  typedef Each_Ref Each;
  typedef Each* Each_Ptr;
  typedef Each const* Each_Ptr_Const;
  class While_Ref;
  typedef While_Ref While;
  typedef While* While_Ptr;
  typedef While const* While_Ptr_Const;
  class Return_Ref;
  typedef Return_Ref Return;
  typedef Return* Return_Ptr;
  typedef Return const* Return_Ptr_Const;
  class Content_Ref;
  typedef Content_Ref Content;
  typedef Content* Content_Ptr;
  typedef Content const* Content_Ptr_Const;
  class Extension_Ref;
  typedef Extension_Ref Extension;
  typedef Extension* Extension_Ptr;
  typedef Extension const* Extension_Ptr_Const;
  class Definition_Ref;
  typedef Definition_Ref Definition;
  typedef Definition* Definition_Ptr;
  typedef Definition const* Definition_Ptr_Const;

  class List_Ref;
  typedef List_Ref List;
  typedef List* List_Ptr;
  typedef List const* List_Ptr_Const;
  class List2_Ref;
  typedef List2_Ref List2;
  typedef List2* List2_Ptr;
  typedef List2 const* List2_Ptr_Const;
  class Map_Ref;
  typedef Map_Ref Map;
  typedef Map* Map_Ptr;
  typedef Map const* Map_Ptr_Const;

  class Mixin_Call_Ref;
  typedef Mixin_Call_Ref Mixin_Call;
  typedef Mixin_Call* Mixin_Call_Ptr;
  typedef Mixin_Call const* Mixin_Call_Ptr_Const;
  class Binary_Expression_Ref;
  typedef Binary_Expression_Ref Binary_Expression;
  typedef Binary_Expression* Binary_Expression_Ptr;
  typedef Binary_Expression const* Binary_Expression_Ptr_Const;
  class Unary_Expression_Ref;
  typedef Unary_Expression_Ref Unary_Expression;
  typedef Unary_Expression* Unary_Expression_Ptr;
  typedef Unary_Expression const* Unary_Expression_Ptr_Const;
  class Function_Call_Ref;
  typedef Function_Call_Ref Function_Call;
  typedef Function_Call* Function_Call_Ptr;
  typedef Function_Call const* Function_Call_Ptr_Const;
  class Function_Call_Schema_Ref;
  typedef Function_Call_Schema_Ref Function_Call_Schema;
  typedef Function_Call_Schema* Function_Call_Schema_Ptr;
  typedef Function_Call_Schema const* Function_Call_Schema_Ptr_Const;
  class Custom_Warning_Ref;
  typedef Custom_Warning_Ref Custom_Warning;
  typedef Custom_Warning* Custom_Warning_Ptr;
  typedef Custom_Warning const* Custom_Warning_Ptr_Const;
  class Custom_Error_Ref;
  typedef Custom_Error_Ref Custom_Error;
  typedef Custom_Error* Custom_Error_Ptr;
  typedef Custom_Error const* Custom_Error_Ptr_Const;

  class Variable_Ref;
  typedef Variable_Ref Variable;
  typedef Variable* Variable_Ptr;
  typedef Variable const* Variable_Ptr_Const;
  class Textual_Ref;
  typedef Textual_Ref Textual;
  typedef Textual* Textual_Ptr;
  typedef Textual const* Textual_Ptr_Const;
  class Number_Ref;
  typedef Number_Ref Number;
  typedef Number* Number_Ptr;
  typedef Number const* Number_Ptr_Const;
  class Color_Ref;
  typedef Color_Ref Color;
  typedef Color* Color_Ptr;
  typedef Color const* Color_Ptr_Const;
  class Boolean_Ref;
  typedef Boolean_Ref Boolean;
  typedef Boolean* Boolean_Ptr;
  typedef Boolean const* Boolean_Ptr_Const;
  class String_Ref;
  typedef String_Ref String;
  typedef String* String_Ptr;
  typedef String const* String_Ptr_Const;

  class String_Schema_Ref;
  typedef String_Schema_Ref String_Schema;
  typedef String_Schema* String_Schema_Ptr;
  typedef String_Schema const* String_Schema_Ptr_Const;
  class String_Constant_Ref;
  typedef String_Constant_Ref String_Constant;
  typedef String_Constant* String_Constant_Ptr;
  typedef String_Constant const* String_Constant_Ptr_Const;
  class String_Quoted_Ref;
  typedef String_Quoted_Ref String_Quoted;
  typedef String_Quoted* String_Quoted_Ptr;
  typedef String_Quoted const* String_Quoted_Ptr_Const;

  class Media_Query_Ref;
  typedef Media_Query_Ref Media_Query;
  typedef Media_Query* Media_Query_Ptr;
  typedef Media_Query const* Media_Query_Ptr_Const;
  class Media_Query_Expression_Ref;
  typedef Media_Query_Expression_Ref Media_Query_Expression;
  typedef Media_Query_Expression* Media_Query_Expression_Ptr;
  typedef Media_Query_Expression const* Media_Query_Expression_Ptr_Const;
  class Supports_Condition_Ref;
  typedef Supports_Condition_Ref Supports_Condition;
  typedef Supports_Condition* Supports_Condition_Ptr;
  typedef Supports_Condition const* Supports_Condition_Ptr_Const;
  class Supports_Operator_Ref;
  typedef Supports_Operator_Ref Supports_Operator;
  typedef Supports_Operator* Supports_Operator_Ptr;
  typedef Supports_Operator const* Supports_Operator_Ptr_Const;
  class Supports_Negation_Ref;
  typedef Supports_Negation_Ref Supports_Negation;
  typedef Supports_Negation* Supports_Negation_Ptr;
  typedef Supports_Negation const* Supports_Negation_Ptr_Const;
  class Supports_Declaration_Ref;
  typedef Supports_Declaration_Ref Supports_Declaration;
  typedef Supports_Declaration* Supports_Declaration_Ptr;
  typedef Supports_Declaration const* Supports_Declaration_Ptr_Const;
  class Supports_Interpolation_Ref;
  typedef Supports_Interpolation_Ref Supports_Interpolation;
  typedef Supports_Interpolation* Supports_Interpolation_Ptr;
  typedef Supports_Interpolation const* Supports_Interpolation_Ptr_Const;


  class Null_Ref;
  typedef Null_Ref Null;
  typedef Null* Null_Ptr;
  typedef Null const* Null_Ptr_Const;

  class At_Root_Query_Ref;
  typedef At_Root_Query_Ref At_Root_Query;
  typedef At_Root_Query* At_Root_Query_Ptr;
  typedef At_Root_Query const* At_Root_Query_Ptr_Const;
  class Parent_Selector_Ref;
  typedef Parent_Selector_Ref Parent_Selector;
  typedef Parent_Selector* Parent_Selector_Ptr;
  typedef Parent_Selector const* Parent_Selector_Ptr_Const;
  class Parameter_Ref;
  typedef Parameter_Ref Parameter;
  typedef Parameter* Parameter_Ptr;
  typedef Parameter const* Parameter_Ptr_Const;
  class Parameters_Ref;
  typedef Parameters_Ref Parameters;
  typedef Parameters* Parameters_Ptr;
  typedef Parameters const* Parameters_Ptr_Const;
  class Argument_Ref;
  typedef Argument_Ref Argument;
  typedef Argument* Argument_Ptr;
  typedef Argument const* Argument_Ptr_Const;
  class Arguments_Ref;
  typedef Arguments_Ref Arguments;
  typedef Arguments* Arguments_Ptr;
  typedef Arguments const* Arguments_Ptr_Const;
  class Selector_Ref;
  typedef Selector_Ref Selector;
  typedef Selector* Selector_Ptr;
  typedef Selector const* Selector_Ptr_Const;


  class Selector_Schema_Ref;
  typedef Selector_Schema_Ref Selector_Schema;
  typedef Selector_Schema* Selector_Schema_Ptr;
  typedef Selector_Schema const* Selector_Schema_Ptr_Const;
  class Placeholder_Selector_Ref;
  typedef Placeholder_Selector_Ref Placeholder_Selector;
  typedef Placeholder_Selector* Placeholder_Selector_Ptr;
  typedef Placeholder_Selector const* Placeholder_Selector_Ptr_Const;
  class Element_Selector_Ref;
  typedef Element_Selector_Ref Element_Selector;
  typedef Element_Selector* Element_Selector_Ptr;
  typedef Element_Selector const* Element_Selector_Ptr_Const;
  class Class_Selector_Ref;
  typedef Class_Selector_Ref Class_Selector;
  typedef Class_Selector* Class_Selector_Ptr;
  typedef Class_Selector const* Class_Selector_Ptr_Const;
  class Id_Selector_Ref;
  typedef Id_Selector_Ref Id_Selector;
  typedef Id_Selector* Id_Selector_Ptr;
  typedef Id_Selector const* Id_Selector_Ptr_Const;
  class Attribute_Selector_Ref;
  typedef Attribute_Selector_Ref Attribute_Selector;
  typedef Attribute_Selector* Attribute_Selector_Ptr;
  typedef Attribute_Selector const* Attribute_Selector_Ptr_Const;

  class Pseudo_Selector_Ref;
  typedef Pseudo_Selector_Ref Pseudo_Selector;
  typedef Pseudo_Selector* Pseudo_Selector_Ptr;
  typedef Pseudo_Selector const * Pseudo_Selector_Ptr_Const;
  class Wrapped_Selector_Ref;
  typedef Wrapped_Selector_Ref Wrapped_Selector;
  typedef Wrapped_Selector* Wrapped_Selector_Ptr;
  typedef Wrapped_Selector const * Wrapped_Selector_Ptr_Const;
  class SimpleSequence_Selector_Ref;
  typedef SimpleSequence_Selector_Ref SimpleSequence_Selector;
  typedef SimpleSequence_Selector* SimpleSequence_Selector_Ptr;
  typedef SimpleSequence_Selector const * SimpleSequence_Selector_Ptr_Const;
  class Sequence_Selector_Ref;
  typedef Sequence_Selector_Ref Sequence_Selector;
  typedef Sequence_Selector* Sequence_Selector_Ptr;
  typedef Sequence_Selector const * Sequence_Selector_Ptr_Const;
  class CommaSequence_Selector_Ref;
  typedef CommaSequence_Selector_Ref CommaSequence_Selector;
  typedef CommaSequence_Selector* CommaSequence_Selector_Ptr;
  typedef CommaSequence_Selector const * CommaSequence_Selector_Ptr_Const;


  // common classes
  class Context;
  class Expand;
  class Eval;


  // declare classes that are instances of memory nodes
  using AST_Node_Obj = Memory_Node<AST_Node_Ref>;
  using Statement_Obj = Memory_Node<Statement_Ref>;
  using Block_Obj = Memory_Node<Block_Ref>;
  using Ruleset_Obj = Memory_Node<Ruleset_Ref>;
  using Bubble_Obj = Memory_Node<Bubble_Ref>;
  using Trace_Obj = Memory_Node<Trace_Ref>;
  using Media_Block_Obj = Memory_Node<Media_Block_Ref>;
  using Supports_Block_Obj = Memory_Node<Supports_Block_Ref>;
  using Directive_Obj = Memory_Node<Directive_Ref>;
  using Keyframe_Rule_Obj = Memory_Node<Keyframe_Rule_Ref>;
  using At_Root_Block_Obj = Memory_Node<At_Root_Block_Ref>;
  using Declaration_Obj = Memory_Node<Declaration_Ref>;
  using Assignment_Obj = Memory_Node<Assignment_Ref>;
  using Import_Obj = Memory_Node<Import_Ref>;
  using Import_Stub_Obj = Memory_Node<Import_Stub_Ref>;
  using Warning_Obj = Memory_Node<Warning_Ref>;
  using Error_Obj = Memory_Node<Error_Ref>;
  using Debug_Obj = Memory_Node<Debug_Ref>;
  using Comment_Obj = Memory_Node<Comment_Ref>;
  using PreValue_Obj = Memory_Node<PreValue_Ref>;
  using Has_Block_Obj = Memory_Node<Has_Block_Ref>;
  using Thunk_Obj = Memory_Node<Thunk_Ref>;
  using If_Obj = Memory_Node<If_Ref>;
  using For_Obj = Memory_Node<For_Ref>;
  using Each_Obj = Memory_Node<Each_Ref>;
  using While_Obj = Memory_Node<While_Ref>;
  using Return_Obj = Memory_Node<Return_Ref>;
  using Content_Obj = Memory_Node<Content_Ref>;
  using Extension_Obj = Memory_Node<Extension_Ref>;
  using Definition_Obj = Memory_Node<Definition_Ref>;
  using Mixin_Call_Obj = Memory_Node<Mixin_Call_Ref>;
  using Value_Obj = Memory_Node<Value_Ref>;
  using Expression_Obj = Memory_Node<Expression_Ref>;
  using List_Obj = Memory_Node<List_Ref>;
  using List2_Obj = Memory_Node<List2_Ref>;
  using Map_Obj = Memory_Node<Map_Ref>;
  using Binary_Expression_Obj = Memory_Node<Binary_Expression_Ref>;
  using Unary_Expression_Obj = Memory_Node<Unary_Expression_Ref>;
  using Function_Call_Obj = Memory_Node<Function_Call_Ref>;
  using Function_Call_Schema_Obj = Memory_Node<Function_Call_Schema_Ref>;
  using Custom_Warning_Obj = Memory_Node<Custom_Warning_Ref>;
  using Custom_Error_Obj = Memory_Node<Custom_Error_Ref>;
  using Variable_Obj = Memory_Node<Variable_Ref>;
  using Textual_Obj = Memory_Node<Textual_Ref>;
  using Number_Obj = Memory_Node<Number_Ref>;
  using Color_Obj = Memory_Node<Color_Ref>;
  using Boolean_Obj = Memory_Node<Boolean_Ref>;
  using String_Schema_Obj = Memory_Node<String_Schema_Ref>;
  using String_Obj = Memory_Node<String_Ref>;
  using String_Constant_Obj = Memory_Node<String_Constant_Ref>;
  using String_Quoted_Obj = Memory_Node<String_Quoted_Ref>;
  using Media_Query_Obj = Memory_Node<Media_Query_Ref>;
  using Media_Query_Expression_Obj = Memory_Node<Media_Query_Expression_Ref>;
  using Supports_Condition_Obj = Memory_Node<Supports_Condition_Ref>;
  using Supports_Operator_Obj = Memory_Node<Supports_Operator_Ref>;
  using Supports_Negation_Obj = Memory_Node<Supports_Negation_Ref>;
  using Supports_Declaration_Obj = Memory_Node<Supports_Declaration_Ref>;
  using Supports_Interpolation_Obj = Memory_Node<Supports_Interpolation_Ref>;
  using At_Root_Query_Obj = Memory_Node<At_Root_Query_Ref>;
  using Null_Obj = Memory_Node<Null_Ref>;
  using Parent_Selector_Obj = Memory_Node<Parent_Selector_Ref>;
  using Parameter_Obj = Memory_Node<Parameter_Ref>;
  using Parameters_Obj = Memory_Node<Parameters_Ref>;
  using Argument_Obj = Memory_Node<Argument_Ref>;
  using Arguments_Obj = Memory_Node<Arguments_Ref>;
  using Selector_Obj = Memory_Node<Selector_Ref>;
  using Selector_Schema_Obj = Memory_Node<Selector_Schema_Ref>;
  using Simple_Selector_Obj = Memory_Node<Simple_Selector_Ref>;
  using Placeholder_Selector_Obj = Memory_Node<Placeholder_Selector_Ref>;
  using Element_Selector_Obj = Memory_Node<Element_Selector_Ref>;
  using Class_Selector_Obj = Memory_Node<Class_Selector_Ref>;
  using Id_Selector_Obj = Memory_Node<Id_Selector_Ref>;
  using Attribute_Selector_Obj = Memory_Node<Attribute_Selector_Ref>;
  using Pseudo_Selector_Obj = Memory_Node<Pseudo_Selector_Ref>;
  using Wrapped_Selector_Obj = Memory_Node<Wrapped_Selector_Ref>;
  using SimpleSequence_Selector_Obj = Memory_Node<SimpleSequence_Selector_Ref>;
  using Sequence_Selector_Obj = Memory_Node<Sequence_Selector_Ref>;
  using CommaSequence_Selector_Obj = Memory_Node<CommaSequence_Selector_Ref>;

}

#endif
