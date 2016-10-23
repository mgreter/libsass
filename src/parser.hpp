#ifndef SASS_PARSER_H
#define SASS_PARSER_H

#include <string>
#include <vector>

#include "ast.hpp"
#include "position.hpp"
#include "context.hpp"
#include "position.hpp"
#include "prelexer.hpp"

struct Lookahead {
  const char* found;
  const char* error;
  const char* position;
  bool parsable;
  bool has_interpolants;
};

namespace Sass {

  class Parser : public ParserState {
  public:

    enum Scope { Root, Mixin, Function, Media, Control, Properties, Rules };

    Context& ctx;
    std::vector<Block_Obj> block_stack;
    std::vector<Scope> stack;
    Media_Block_Ptr last_media_block;
    const char* source;
    const char* position;
    const char* end;
    Position before_token;
    Position after_token;
    ParserState pstate;
    int indentation;


    Token lexed;
    bool in_at_root;

    Parser(Context& ctx, const ParserState& pstate)
    : ParserState(pstate), ctx(ctx), block_stack(), stack(0), last_media_block(0),
      source(0), position(0), end(0), before_token(pstate), after_token(pstate), pstate(pstate), indentation(0)
    { in_at_root = false; stack.push_back(Scope::Root); }

    // static Parser from_string(const std::string& src, Context& ctx, ParserState pstate = ParserState("[STRING]"));
    static Parser from_c_str(const char* src, Context& ctx, ParserState pstate = ParserState("[CSTRING]"), const char* source = 0);
    static Parser from_c_str(const char* beg, const char* end, Context& ctx, ParserState pstate = ParserState("[CSTRING]"), const char* source = 0);
    static Parser from_token(Token t, Context& ctx, ParserState pstate = ParserState("[TOKEN]"), const char* source = 0);
    // special static parsers to convert strings into certain selectors
    static CommaSequence_Selector_Ptr parse_selector(const char* src, Context& ctx, ParserState pstate = ParserState("[SELECTOR]"), const char* source = 0);

#ifdef __clang__

    // lex and peak uses the template parameter to branch on the action, which
    // triggers clangs tautological comparison on the single-comparison
    // branches. This is not a bug, just a merging of behaviour into
    // one function

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtautological-compare"

#endif


    // skip current token and next whitespace
    // moves ParserState right before next token
    void advanceToNextToken();

    bool peek_newline(const char* start = 0);

    // skip over spaces, tabs and line comments
    template <Prelexer::prelexer mx>
    const char* sneak(const char* start = 0)
    {
      using namespace Prelexer;

      // maybe use optional start position from arguments?
      const char* it_position = start ? start : position;

      // skip white-space?
      if (mx == spaces ||
          mx == no_spaces ||
          mx == css_comments ||
          mx == css_whitespace ||
          mx == optional_spaces ||
          mx == optional_css_comments ||
          mx == optional_css_whitespace
      ) {
        return it_position;
      }

      // skip over spaces, tabs and sass line comments
      const char* pos = optional_css_whitespace(it_position);
      // always return a valid position
      return pos ? pos : it_position;

    }

    // peek will only skip over space, tabs and line comment
    // return the position where the lexer match will occur
    template <Prelexer::prelexer mx>
    const char* match(const char* start = 0)
    {
      // match the given prelexer
      return mx(position);
    }

    // peek will only skip over space, tabs and line comment
    // return the position where the lexer match will occur
    template <Prelexer::prelexer mx>
    const char* peek(const char* start = 0)
    {

      // sneak up to the actual token we want to lex
      // this should skip over white-space if desired
      const char* it_before_token = sneak < mx >(start);

      // match the given prelexer
      const char* match = mx(it_before_token);

      // check if match is in valid range
      return match <= end ? match : 0;

    }

    // white-space handling is built into the lexer
    // this way you do not need to parse it yourself
    // some matchers don't accept certain white-space
    // we do not support start arg, since we manipulate
    // sourcemap offset and we modify the position pointer!
    // lex will only skip over space, tabs and line comment
    template <Prelexer::prelexer mx>
    const char* lex(bool lazy = true, bool force = false)
    {

      if (*position == 0) return 0;

      // position considered before lexed token
      // we can skip whitespace or comments for
      // lazy developers (but we need control)
      const char* it_before_token = position;

      // sneak up to the actual token we want to lex
      // this should skip over white-space if desired
      if (lazy) it_before_token = sneak < mx >(position);

      // now call matcher to get position after token
      const char* it_after_token = mx(it_before_token);

      // check if match is in valid range
      if (it_after_token > end) return 0;

      // maybe we want to update the parser state anyway?
      if (force == false) {
        // assertion that we got a valid match
        if (it_after_token == 0) return 0;
        // assertion that we actually lexed something
        if (it_after_token == it_before_token) return 0;
      }

      // create new lexed token object (holds the parse results)
      lexed = Token(position, it_before_token, it_after_token);

      // advance position (add whitespace before current token)
      before_token = after_token.add(position, it_before_token);

      // update after_token position for current token
      after_token.add(it_before_token, it_after_token);

      // ToDo: could probably do this incremetal on original object (API wants offset?)
      pstate = ParserState(path, source, lexed, before_token, after_token - before_token);

      // advance internal char iterator
      return position = it_after_token;

    }

    // lex_css skips over space, tabs, line and block comment
    // all block comments will be consumed and thrown away
    // source-map position will point to token after the comment
    template <Prelexer::prelexer mx>
    const char* lex_css()
    {
      // copy old token
      Token prev = lexed;
      // store previous pointer
      const char* oldpos = position;
      Position bt = before_token;
      Position at = after_token;
      ParserState op = pstate;
      // throw away comments
      // update srcmap position
      lex < Prelexer::css_comments >();
      // now lex a new token
      const char* pos = lex< mx >();
      // maybe restore prev state
      if (pos == 0) {
        pstate = op;
        lexed = prev;
        position = oldpos;
        after_token = at;
        before_token = bt;
      }
      // return match
      return pos;
    }

    // all block comments will be skipped and thrown away
    template <Prelexer::prelexer mx>
    const char* peek_css(const char* start = 0)
    {
      // now peek a token (skip comments first)
      return peek< mx >(peek < Prelexer::css_comments >(start));
    }

#ifdef __clang__

#pragma clang diagnostic pop

#endif

    void error(std::string msg, Position pos);
    // generate message with given and expected sample
    // text before and in the middle are configurable
    void css_error(const std::string& msg,
                   const std::string& prefix = " after ",
                   const std::string& middle = ", was: ");
    void read_bom();

    Block_Obj parse();
    Import_Ptr parse_import();
    Definition_Ptr parse_definition(Definition::Type which_type);
    Parameters_Ptr parse_parameters();
    Parameter_Ptr parse_parameter();
    Mixin_Call_Ptr parse_include_directive();
    Arguments_Ptr parse_arguments();
    Argument_Ptr parse_argument();
    Assignment_Ptr parse_assignment();
    Ruleset_Ptr parse_ruleset(Lookahead lookahead, bool is_root = false);
    Selector_Schema_Ptr parse_selector_schema(const char* end_of_selector);
    CommaSequence_Selector_Ptr parse_selector_list(bool at_root = false);
    Sequence_Selector_Ptr parse_complex_selector(bool in_root = true);
    SimpleSequence_Selector_Ptr parse_compound_selector();
    Simple_Selector* parse_simple_selector();
    Wrapped_Selector_Ptr parse_negated_selector();
    Simple_Selector* parse_pseudo_selector();
    Attribute_Selector_Ptr parse_attribute_selector();
    Block_Obj parse_block(bool is_root = false);
    Block_Obj parse_css_block(bool is_root = false);
    bool parse_block_nodes(bool is_root = false);
    bool parse_block_node(bool is_root = false);

    bool parse_number_prefix();
    Declaration_Ptr parse_declaration();
    Expression_Ptr parse_map_value();
    Expression_Ptr parse_map();
    Expression_Ptr parse_list(bool delayed = false);
    Expression_Ptr parse_comma_list(bool delayed = false);
    Expression_Ptr parse_space_list();
    Expression_Ptr parse_disjunction();
    Expression_Ptr parse_conjunction();
    Expression_Ptr parse_relation();
    Expression_Ptr parse_expression();
    Expression_Ptr parse_operators();
    Expression_Ptr parse_factor();
    Expression_Ptr parse_value2();
    Expression_Ptr parse_value();
    Function_Call_Ptr parse_calc_function();
    Function_Call_Ptr parse_function_call();
    Function_Call_Schema_Ptr parse_function_call_schema();
    String_Ptr parse_url_function_string();
    String_Ptr parse_url_function_argument();
    String_Ptr parse_interpolated_chunk(Token, bool constant = false);
    String_Ptr parse_string();
    String_Constant_Ptr parse_static_expression();
    // String_Constant_Ptr parse_static_property();
    String_Constant_Ptr parse_static_value();
    String_Ptr parse_ie_property();
    String_Ptr parse_ie_keyword_arg();
    String_Schema_Ptr parse_value_schema(const char* stop);
    String_Ptr parse_identifier_schema();
    // String_Schema_Ptr parse_url_schema();
    If_Ptr parse_if_directive(bool else_if = false);
    For_Ptr parse_for_directive();
    Each_Ptr parse_each_directive();
    While_Ptr parse_while_directive();
    Return_Ptr parse_return_directive();
    Content_Ptr parse_content_directive();
    void parse_charset_directive();
    Media_Block_Obj parse_media_block();
    List_Ptr parse_media_queries();
    Media_Query_Ptr parse_media_query();
    Media_Query_Expression_Ptr parse_media_expression();
    Supports_Block_Ptr parse_supports_directive();
    Supports_Condition_Ptr parse_supports_condition();
    Supports_Condition_Ptr parse_supports_negation();
    Supports_Condition_Ptr parse_supports_operator();
    Supports_Condition_Ptr parse_supports_interpolation();
    Supports_Condition_Ptr parse_supports_declaration();
    Supports_Condition_Ptr parse_supports_condition_in_parens();
    At_Root_Block_Obj parse_at_root_block();
    At_Root_Query_Ptr parse_at_root_query();
    String_Schema_Ptr parse_almost_any_value();
    Directive_Ptr parse_special_directive();
    Directive_Ptr parse_prefixed_directive();
    Directive_Ptr parse_directive();
    Warning_Ptr parse_warning();
    Error_Ptr parse_error();
    Debug_Ptr parse_debug();

    // be more like ruby sass
    Expression_Ptr lex_almost_any_value_token();
    Expression_Ptr lex_almost_any_value_chars();
    Expression_Ptr lex_interp_string();
    Expression_Ptr lex_interp_uri();
    Expression_Ptr lex_interpolation();

    // these will throw errors
    Token lex_variable();
    Token lex_identifier();

    void parse_block_comments();

    Lookahead lookahead_for_value(const char* start = 0);
    Lookahead lookahead_for_selector(const char* start = 0);
    Lookahead lookahead_for_include(const char* start = 0);

    Expression_Ptr fold_operands(Expression_Ptr base, std::vector<Expression_Ptr>& operands, Operand op);
    Expression_Ptr fold_operands(Expression_Ptr base, std::vector<Expression_Ptr>& operands, std::vector<Operand>& ops, size_t i = 0);

    void throw_syntax_error(std::string message, size_t ln = 0);
    void throw_read_error(std::string message, size_t ln = 0);


    template <Prelexer::prelexer open, Prelexer::prelexer close>
    Expression_Ptr lex_interp()
    {
      if (lex < open >(false)) {
        String_Schema_Ptr schema = SASS_MEMORY_NEW(ctx.mem, String_Schema, pstate);
        // std::cerr << "LEX [[" << std::string(lexed) << "]]\n";
        *schema << SASS_MEMORY_NEW(ctx.mem, String_Constant, pstate, lexed);
        if (position[0] == '#' && position[1] == '{') {
          Expression_Ptr itpl = lex_interpolation();
          if (itpl) *schema << itpl;
          while (lex < close >(false)) {
            // std::cerr << "LEX [[" << std::string(lexed) << "]]\n";
            *schema << SASS_MEMORY_NEW(ctx.mem, String_Constant, pstate, lexed);
            if (position[0] == '#' && position[1] == '{') {
              Expression_Ptr itpl = lex_interpolation();
              if (itpl) *schema << itpl;
            } else {
              return schema;
            }
          }
        } else {
          return SASS_MEMORY_NEW(ctx.mem, String_Constant, pstate, lexed);
        }
      }
      return 0;
    }
  };

  size_t check_bom_chars(const char* src, const char *end, const unsigned char* bom, size_t len);
}

#endif
