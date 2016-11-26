#ifndef SASS_AST_FACTORY_H
#define SASS_AST_FACTORY_H

#include <vector>

#include "ast.hpp"

namespace Sass {

  class AST_Factory {
    std::vector<AST_Node_Ptr> nodes;
  public:
    // statements
    Block_Ptr new_Block(std::string p, size_t l, size_t s = 0, bool r = false);
    Ruleset_Ptr new_Ruleset(std::string p, size_t l, Selector_Ptr s, Block_Ptr b);
    Supports_Query* new_Supports_Query(std::string p, size_t l, Supports_Query* f, Block_Ptr b);
    Media_Query_Ptr new_Media_Query(std::string p, size_t l, List_Ptr q, Block_Ptr b);
    At_Root_Block_Ptr new_At_Root_Block(std::string p, size_t l, Selector_Ptr sel, Block_Ptr b);
    Directive_Ptr new_At_Rule(std::string p, size_t l, std::string kwd, Selector_Ptr sel, Block_Ptr b);
    Keyframe_Rule_Ptr new_Keyframe_Rule(std::string p, size_t l, Block_Ptr b);
    Declaration_Ptr new_Declaration(std::string p, size_t l, String_Ptr prop, List_Ptr vals, Block_Ptr b);
    Assignment_Ptr new_Assignment(std::string p, size_t l, std::string var, Expression_Ptr val, bool guarded = false);
    Import<Function_Call_Ptr>* new_CSS_Import(std::string p, size_t l, Function_Call_Ptr loc);
    Import<String_Ptr>* new_SASS_Import(std::string p, size_t l, String_Ptr loc);
    Custom_Warning_Ptr new_Custom_Warning(std::string msg, size_t l, std::string msg);
    Custom_Error_Ptr new_Custom_Error(std::string p, size_t l, std::string msg);
    Warning_Ptr new_Warning(std::string p, size_t l, Expression_Ptr msg);
    Error_Ptr new_Error(std::string p, size_t l, Expression_Ptr msg);
    Debug_Ptr new_Debug(std::string p, size_t l, Expression_Ptr val);
    Comment_Ptr new_Comment(std::string p, size_t l, String_Ptr txt);
    If_Ptr new_If(std::string p, size_t l, Expression_Ptr pred, Block_Ptr con, Block_Ptr alt = 0);
    For_Ptr new_For(std::string p, size_t l, std::string var, Expression_Ptr lo, Expression_Ptr hi, Block_Ptr b, bool inc);
    Each_Ptr new_Each(std::string p, size_t l, std::vector<std::string> vars, Expression_Ptr lst, Block_Ptr b);
    While_Ptr new_While(std::string p, size_t l, Expression_Ptr pred, Block_Ptr b);
    Extension_Ptr new_Extension(std::string p, size_t l, Selector_Ptr s);
    Definition<MIXIN>* new_Mixin_Definition(std::string p, size_t l, std::string n, Parameters_Ptr params, Block_Ptr b);
    Definition<FUNCTION>* new_Function_Definition(std::string p, size_t l, std::string n, Parameters_Ptr params, Block_Ptr b);
    Mixin_Call_Ptr new_Mixin_Call(std::string p, size_t l, std::string n, Arguments_Ptr args, Block_Ptr b = 0);
    // expressions
    List_Ptr new_List(std::string p, size_t l, size_t size = 0, enum Sass_Separator sep = List::space, bool argl = false);
    Map_Ptr new_Map(std::string p, size_t l, size_t size = 0);
    Binary_Expression<AND>* new_And(std::string p, size_t l, Expression_Ptr lhs, Expression_Ptr rhs);
    Binary_Expression<OR>* new_Or(std::string p, size_t l, Expression_Ptr lhs, Expression_Ptr rhs);
    Binary_Expression<EQ>* new_Eq(std::string p, size_t l, Expression_Ptr lhs, Expression_Ptr rhs);
    Binary_Expression<NEQ>* new_Neq(std::string p, size_t l, Expression_Ptr lhs, Expression_Ptr rhs);
    Binary_Expression<GT>* new_Gt(std::string p, size_t l, Expression_Ptr lhs, Expression_Ptr rhs);
    Binary_Expression<GTE>* new_Gte(std::string p, size_t l, Expression_Ptr lhs, Expression_Ptr rhs);
    Binary_Expression<LT>* new_Lt(std::string p, size_t l, Expression_Ptr lhs, Expression_Ptr rhs);
    Binary_Expression<LTE>* new_Lte(std::string p, size_t l, Expression_Ptr lhs, Expression_Ptr rhs);
    Binary_Expression<ADD>* new_Add(std::string p, size_t l, Expression_Ptr lhs, Expression_Ptr rhs);
    Binary_Expression<SUB>* new_Sub(std::string p, size_t l, Expression_Ptr lhs, Expression_Ptr rhs);
    Binary_Expression<MUL>* new_Mul(std::string p, size_t l, Expression_Ptr lhs, Expression_Ptr rhs);
    Binary_Expression<DIV>* new_Div(std::string p, size_t l, Expression_Ptr lhs, Expression_Ptr rhs);
    Negation* new_Negation(std::string p, size_t l, Expression_Ptr o);
    Function_Call_Ptr new_Function_Call(std::string p, size_t l, String_Ptr n, Arguments_Ptr args);
    Variable_Ptr new_Variable(std::string p, size_t l, std::string n);
    Textual<NUMBER>* new_Textual_Number(std::string p, size_t l, std::string val);
    Textual<PERCENTAGE>* new_Textual_Percentage(std::string p, size_t l, std::string val);
    Textual<DIMENSION>* new_Textual_Dimension(std::string p, size_t l, std::string val);
    Textual<HEX>* new_Textual_Hex(std::string p, size_t l, std::string val);
    Number_Ptr new_Number(std::string p, size_t l, double val);
    Percentage* new_Percentage(std::string p, size_t l, double val);
    Dimension* new_Dimension(std::string p, size_t l, double val, std::string unit);
    Color_Ptr new_Color(std::string p, size_t l, double r, double g, double b, double a = 1, std::string disp = "");
    Boolean_Ptr new_Boolean(std::string p, size_t l, bool val);
    String_Schema_Ptr new_String_Schema(std::string p, size_t l, size_t size = 0);
    String_Constant_Ptr new_String_Constant(std::string p, size_t l, std::string val);
    String_Constant_Ptr new_String_Constant(std::string p, size_t l, const char* beg);
    String_Constant_Ptr new_String_Constant(std::string p, size_t l, const char* beg, const char* end);
    Supports_Condition_Ptr new_Supports_Condition(std::string p, size_t l, String_Ptr f, Expression_Ptr v);
    Media_Expression* new_Media_Expression(std::string p, size_t l, String_Ptr f, Expression_Ptr v);
    Parent_Selector_Ptr new_Parent_Selector(std::string p, size_t l, Selector_Ptr s);
    // parameters and arguments
    Parameter_Ptr new_Parameter(std::string p, size_t l, std::string n, Expression_Ptr def = 0, bool rest = false);
    Parameters_Ptr new_Parameters(std::string p, size_t l);
    Argument_Ptr new_Argument(std::string p, size_t l, Expression_Ptr val, std::string n = "", bool rest = false);
    Arguments_Ptr new_Arguments(std::string p, size_t l);
    // selectors
    Selector_Schema_Ptr new_Selector_Schema(std::string p, size_t l, String_Ptr c);
    Attribute_Selector_Ptr new_Attribute_Selector(std::string p, size_t l, std::string n, std::string m, String_Ptr v);
    Simple_Selector* new_Simple_Selector(std::string p, size_t l, std::string c);
    Reference_Selector* new_Reference_Selector(std::string p, size_t l);
    Placeholder_Selector_Ptr new_Placeholder_Selector(std::string p, size_t l, std::string n);
    Pseudo_Selector_Ptr new_Pseudo_Selector(std::string p, size_t l, std::string n, Expression_Ptr expr = 0);
    Wrapped_Selector_Ptr new_Wrapped_Selector(std::string p, size_t l, std::string n, Simple_Base* sel);
    Compound_Selector_Ptr new_Compound_Selector(std::string p, size_t l, size_t s = 0);
    Complex_Selector_Ptr new_Complex_Selector(std::string p, size_t l, Complex_Selector::Combinator c, Complex_Selector_Ptr ctx, Compound_Selector_Ptr sel);
    Selector_List_Ptr new_Selector_List(std::string p, size_t l, size_t s = 0);
  };
}

#endif
