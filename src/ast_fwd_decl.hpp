#ifndef SASS_AST_FWD_DECL_H
#define SASS_AST_FWD_DECL_H

/////////////////////////////////////////////
// Forward declarations for the AST visitors.
/////////////////////////////////////////////
namespace Sass {

  class AST_Node;
  typedef AST_Node* AST_Node_Ptr;

  class Has_Block;
  typedef Has_Block* Has_Block_Ptr;

  class Simple_Selector;
  typedef Simple_Selector* Simple_Selector_Ptr;

  class PreValue;
  typedef PreValue* PreValue_Ptr;

  class Thunk;
  typedef Thunk* Thunk_Ptr;

  class Expression;
  typedef Expression* Expression_Ptr;

  class Statement;
  typedef Statement* Statement_Ptr;

  class Value;
  typedef Value* Value_Ptr;

  class Declaration;
  typedef Declaration* Declaration_Ptr;

  class Block;
  typedef Block* Block_Ptr;

  class Ruleset;
  typedef Ruleset* Ruleset_Ptr;

  class Bubble;
  typedef Bubble* Bubble_Ptr;

  class Trace;
  typedef Trace* Trace_Ptr;

  class Media_Block;
  typedef Media_Block* Media_Block_Ptr;

  class Supports_Block;
  typedef Supports_Block* Supports_Block_Ptr;

  class Directive;
  typedef Directive* Directive_Ptr;

  class Keyframe_Rule;
  typedef Keyframe_Rule* Keyframe_Rule_Ptr;

  class At_Root_Block;
  typedef At_Root_Block* At_Root_Block_Ptr;

  class Assignment;
  typedef Assignment* Assignment_Ptr;

  class Import;
  typedef Import* Import_Ptr;

  class Import_Stub;
  typedef Import_Stub* Import_Stub_Ptr;

  class Warning;
  typedef Warning* Warning_Ptr;

  class Error;
  typedef Error* Error_Ptr;

  class Debug;
  typedef Debug* Debug_Ptr;

  class Comment;
  typedef Comment* Comment_Ptr;

  class If;
  typedef If* If_Ptr;

  class For;
  typedef For* For_Ptr;

  class Each;
  typedef Each* Each_Ptr;

  class While;
  typedef While* While_Ptr;

  class Return;
  typedef Return* Return_Ptr;

  class Content;
  typedef Content* Content_Ptr;

  class Extension;
  typedef Extension* Extension_Ptr;

  class Definition;
  typedef Definition* Definition_Ptr;

  class Mixin_Call;
  typedef Mixin_Call* Mixin_Call_Ptr;

  class List;
  typedef List* List_Ptr;

  class Map;
  typedef Map* Map_Ptr;

  class Binary_Expression;
  typedef Binary_Expression* Binary_Expression_Ptr;

  class Unary_Expression;
  typedef Unary_Expression* Unary_Expression_Ptr;

  class Function_Call;
  typedef Function_Call* Function_Call_Ptr;

  class Function_Call_Schema;
  typedef Function_Call_Schema* Function_Call_Schema_Ptr;

  class Custom_Warning;
  typedef Custom_Warning* Custom_Warning_Ptr;

  class Custom_Error;
  typedef Custom_Error* Custom_Error_Ptr;

  class Variable;
  typedef Variable* Variable_Ptr;

  class Textual;
  typedef Textual* Textual_Ptr;

  class Number;
  typedef Number* Number_Ptr;

  class Color;
  typedef Color* Color_Ptr;

  class Boolean;
  typedef Boolean* Boolean_Ptr;

  class Null;
  typedef Null* Null_Ptr;

  class String;
  typedef String* String_Ptr;

  class String_Schema;
  typedef String_Schema* String_Schema_Ptr;

  class String_Constant;
  typedef String_Constant* String_Constant_Ptr;

  class String_Quoted;
  typedef String_Quoted* String_Quoted_Ptr;

  class Media_Query;
  typedef Media_Query* Media_Query_Ptr;

  class Media_Query_Expression;
  typedef Media_Query_Expression* Media_Query_Expression_Ptr;

  class Supports_Condition;
  typedef Supports_Condition* Supports_Condition_Ptr;

  class Supports_Operator;
  typedef Supports_Operator* Supports_Operator_Ptr;

  class Supports_Negation;
  typedef Supports_Negation* Supports_Negation_Ptr;

  class Supports_Declaration;
  typedef Supports_Declaration* Supports_Declaration_Ptr;

  class Supports_Interpolation;
  typedef Supports_Interpolation* Supports_Interpolation_Ptr;

  class At_Root_Query;
  typedef At_Root_Query* At_Root_Query_Ptr;

  class Parent_Selector;
  typedef Parent_Selector* Parent_Selector_Ptr;

  class Parameter;
  typedef Parameter* Parameter_Ptr;

  class Parameters;
  typedef Parameters* Parameters_Ptr;

  class Argument;
  typedef Argument* Argument_Ptr;

  class Arguments;
  typedef Arguments* Arguments_Ptr;

  class Selector;
  typedef Selector* Selector_Ptr;

  class Selector_Schema;
  typedef Selector_Schema* Selector_Schema_Ptr;

  class Placeholder_Selector;
  typedef Placeholder_Selector* Placeholder_Selector_Ptr;

  class Type_Selector;
  typedef Type_Selector* Type_Selector_Ptr;

  class Class_Selector;
  typedef Class_Selector* Class_Selector_Ptr;

  class Id_Selector;
  typedef Id_Selector* Id_Selector_Ptr;

  class Attribute_Selector;
  typedef Attribute_Selector* Attribute_Selector_Ptr;

  class Pseudo_Selector;
  typedef Pseudo_Selector* Pseudo_Selector_Ptr;

  class Wrapped_Selector;
  typedef Wrapped_Selector* Wrapped_Selector_Ptr;

  class Compound_Selector;
  typedef Compound_Selector* Compound_Selector_Ptr;

  class Complex_Selector;
  typedef Complex_Selector* Complex_Selector_Ptr;

  class Selector_List;
  typedef Selector_List* Selector_List_Ptr;

  class Context;

}

#endif
