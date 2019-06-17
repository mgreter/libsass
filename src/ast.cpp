// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include "ast.hpp"

namespace Sass {

  static Null sass_null(ParserState("null"));

  uint8_t sass_op_to_precedence(enum Sass_OP op) {
    switch (op) {
    case OR: return 1;
    case AND: return 2;
    case EQ: return 3;
    case NEQ: return 3;
    case GT: return 4;
    case GTE: return 4;
    case LT: return 4;
    case LTE: return 4;
    case ADD: return 5;
    case SUB: return 5;
    case MUL: return 6;
    case DIV: return 6;
    case MOD: return 6;
    case IESEQ: return 9;
      // this is only used internally!
    case NUM_OPS: return 99;
    default: return 255;
    }
  }

  const char* sass_op_to_name(enum Sass_OP op) {
    switch (op) {
      case AND: return "and";
      case OR: return "or";
      case EQ: return "eq";
      case NEQ: return "neq";
      case GT: return "gt";
      case GTE: return "gte";
      case LT: return "lt";
      case LTE: return "lte";
      case ADD: return "plus";
      case SUB: return "minus";
      case MUL: return "times";
      case DIV: return "div";
      case MOD: return "mod";
      case IESEQ: return "seq";
        // this is only used internally!
      case NUM_OPS: return "[OPS]";
      default: return "invalid";
    }
  }

  const char* sass_op_separator(enum Sass_OP op) {
    switch (op) {
      case AND: return "&&";
      case OR: return "||";
      case EQ: return "==";
      case NEQ: return "!=";
      case GT: return ">";
      case GTE: return ">=";
      case LT: return "<";
      case LTE: return "<=";
      case ADD: return "+";
      case SUB: return "-";
      case MUL: return "*";
      case DIV: return "/";
      case MOD: return "%";
      case IESEQ: return "=";
        // this is only used internally!
      case NUM_OPS: return "[OPS]";
      default: return "invalid";
    }
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  void AST_Node::update_pstate(const ParserState& pstate)
  {
    pstate_.offset += pstate - pstate_ + pstate.offset;
  }

  std::string AST_Node::to_string(Sass_Inspect_Options opt) const
  {
    Sass_Output_Options out(opt);
    Emitter emitter(out);
    Inspect i(emitter);
    i.in_declaration = true;
    // ToDo: inspect should be const
    const_cast<AST_Node*>(this)->perform(&i);
    return i.get_buffer();
  }

  std::string AST_Node::to_css(Sass_Inspect_Options opt) const
  {
    opt.output_style = TO_CSS;
    Sass_Output_Options out(opt);
    Emitter emitter(out);
    Inspect i(emitter);
    i.quotes = false;
    i.in_declaration = true;
    // ToDo: inspect should be const
    const_cast<AST_Node*>(this)->perform(&i);
    return i.get_buffer();
  }

  std::string AST_Node::to_string() const
  {
    return to_string({ NESTED, 5 });
  }

  std::string AST_Node::to_css() const
  {
    return to_css({ NESTED, 5 });
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Statement::Statement(ParserState pstate, Type st, size_t t)
  : AST_Node(pstate), statement_type_(st), tabs_(t), group_end_(false)
  { }
  Statement::Statement(const Statement* ptr)
  : AST_Node(ptr),
    statement_type_(ptr->statement_type_),
    tabs_(ptr->tabs_),
    group_end_(ptr->group_end_)
  { }

  bool Statement::bubbles()
  {
    return false;
  }

  bool Statement::has_content()
  {
    return statement_type_ == CONTENT;
  }

  bool Statement::is_invisible() const
  {
    return false;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Block::Block(ParserState pstate, size_t s, bool r)
  : Statement(pstate),
    Vectorized<Statement_Obj>(s),
    is_root_(r)
  { }
  Block::Block(const Block* ptr)
  : Statement(ptr),
    Vectorized<Statement_Obj>(ptr),
    is_root_(ptr->is_root_)
  { }

  Block::Block(ParserState pstate, std::vector<StatementObj> vec, bool r) :
    Statement(pstate),
    Vectorized<StatementObj>(vec),
    is_root_(r)
  { }

  bool Block::isInvisible() const
  {
    for (auto& item : this->elements()) {
      if (!item->is_invisible()) return false;
    }
    return true;
  }

  bool Block::has_content()
  {
    for (size_t i = 0, L = elements().size(); i < L; ++i) {
      if (elements()[i]->has_content()) return true;
    }
    return Statement::has_content();
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Has_Block::Has_Block(ParserState pstate, Block_Obj b)
  : Statement(pstate), block_(b)
  { }
  Has_Block::Has_Block(const Has_Block* ptr)
  : Statement(ptr), block_(ptr->block_)
  { }

  bool Has_Block::has_content()
  {
    return (block_ && block_->has_content()) || Statement::has_content();
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  StyleRule::StyleRule(ParserState pstate, Interpolation* s, Block_Obj b)
    : Has_Block(pstate, b), interpolation_(s)
  { statement_type(RULESET); }

  StyleRule::StyleRule(const StyleRule* ptr)
  : Has_Block(ptr),
    interpolation_(ptr->interpolation_)
  { statement_type(RULESET); }

  bool CssStyleRule::is_invisible() const {
    bool sel_invisible = true;
    bool els_invisible = true;
    if (const SelectorList * sl = Cast<SelectorList>(selector())) {
      for (size_t i = 0, L = sl->length(); i < L; i += 1) {
        if (!(*sl)[i]->isInvisible()) {
          sel_invisible = false;
          break;
        }
      }
    }
    for (Statement* item : block()->elements()) {
      if (!item->is_invisible()) {
        els_invisible = false;
        break;
      }
    }

    return sel_invisible || els_invisible;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Bubble::Bubble(ParserState pstate, Statement_Obj n, Statement_Obj g, size_t t)
  : Statement(pstate, Statement::BUBBLE, t), node_(n), group_end_(g == nullptr)
  { }
  Bubble::Bubble(const Bubble* ptr)
  : Statement(ptr),
    node_(ptr->node_),
    group_end_(ptr->group_end_)
  { }

  bool Bubble::bubbles()
  {
    return true;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Trace::Trace(ParserState pstate, std::string n, Block_Obj b, char type)
  : Has_Block(pstate, b), type_(type), name_(n)
  { }
  Trace::Trace(const Trace* ptr)
  : Has_Block(ptr),
    type_(ptr->type_),
    name_(ptr->name_)
  { }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  AtRule::AtRule(ParserState pstate, std::string kwd, Block_Obj b, Expression_Obj val)
  : Has_Block(pstate, b), keyword_(kwd), interpolation_(), value_(val), name2_(), value2_() // set value manually if needed
  { statement_type(DIRECTIVE); }

  AtRule::AtRule(ParserState pstate, InterpolationObj itpl, Block_Obj b, Expression_Obj val)
    : Has_Block(pstate, b), keyword_(), interpolation_(itpl), value_(val), name2_(), value2_() // set value manually if needed
  {
    statement_type(DIRECTIVE);
  }

  AtRule::AtRule(const AtRule* ptr)
  : Has_Block(ptr),
    keyword_(ptr->keyword_),
    interpolation_(ptr->interpolation_),
    value_(ptr->value_),
    name2_(ptr->name2_),
    value2_(ptr->value2_) // set value manually if needed
  { statement_type(DIRECTIVE); }

  bool AtRule::bubbles() { return is_keyframes() || is_media(); }

  bool AtRule::is_media() {
    return keyword_.compare("@-webkit-media") == 0 ||
            keyword_.compare("@-moz-media") == 0 ||
            keyword_.compare("@-o-media") == 0 ||
            keyword_.compare("@media") == 0;
  }
  bool AtRule::is_keyframes() {
    return keyword_.compare("@-webkit-keyframes") == 0 ||
            keyword_.compare("@-moz-keyframes") == 0 ||
            keyword_.compare("@-o-keyframes") == 0 ||
            keyword_.compare("@keyframes") == 0;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Keyframe_Rule::Keyframe_Rule(ParserState pstate, Block_Obj b)
  : Has_Block(pstate, b), name_(), name2_()
  { statement_type(KEYFRAMERULE); }
  Keyframe_Rule::Keyframe_Rule(const Keyframe_Rule* ptr)
  : Has_Block(ptr), name_(ptr->name_), name2_(ptr->name2_)
  { statement_type(KEYFRAMERULE); }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Declaration::Declaration(ParserState pstate, String_Obj prop, Expression_Obj val, bool c, Block_Obj b)
  : Has_Block(pstate, b), property_(prop), value_(val), is_custom_property_(c)
  { statement_type(DECLARATION); }

  Declaration::Declaration(const Declaration* ptr)
  : Has_Block(ptr),
    property_(ptr->property_),
    value_(ptr->value_),
    is_custom_property_(ptr->is_custom_property_)
  { statement_type(DECLARATION); }

  bool Declaration::is_invisible() const
  {
    if (is_custom_property()) return false;
    return !(value_ && !Cast<Null>(value_));
  }


  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Assignment::Assignment(ParserState pstate, std::string var, Expression_Obj val, bool is_default, bool is_global)
  : Statement(pstate), variable_(var), value_(val), is_default_(is_default), is_global_(is_global)
  { statement_type(ASSIGNMENT); }
  Assignment::Assignment(const Assignment* ptr)
  : Statement(ptr),
    variable_(ptr->variable_),
    value_(ptr->value_),
    is_default_(ptr->is_default_),
    is_global_(ptr->is_global_)
  { statement_type(ASSIGNMENT); }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  ImportBase::ImportBase(
    ParserState pstate) :
    Statement(pstate)
  {}

  ImportBase::ImportBase(
    const ImportBase* ptr) :
    Statement(ptr)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  StaticImport::StaticImport(
    ParserState pstate,
    InterpolationObj url,
    Supports_Condition_Obj supports,
    InterpolationObj media) :
    ImportBase(pstate),
    url_(url),
    supports_(supports),
    media_(media),
    outOfOrder_(true)
  {}

  StaticImport::StaticImport(
    const StaticImport* ptr) :
    ImportBase(ptr),
    url_(ptr->url_),
    supports_(ptr->supports_),
    media_(ptr->media_),
    outOfOrder_(ptr->outOfOrder_)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  DynamicImport::DynamicImport(
    ParserState pstate,
    std::string url) :
    ImportBase(pstate),
    url_(url)
  {}

  DynamicImport::DynamicImport(
    const DynamicImport* ptr) :
    ImportBase(ptr),
    url_(ptr->url_)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  ImportRule::ImportRule(
    ParserState pstate) :
    Statement(pstate),
    Vectorized()
  {}

  ImportRule::ImportRule(
    const ImportRule* ptr) :
    Statement(ptr),
    Vectorized(ptr)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Import::Import(ParserState pstate)
  : ImportBase(pstate),
    urls_(),
    incs_(),
    imports_(),
    import_queries_(),
    queries_()
  { statement_type(IMPORT); }
  Import::Import(const Import* ptr)
  : ImportBase(ptr),
    urls_(ptr->urls_),
    incs_(ptr->incs_),
    imports_(ptr->imports_),
    import_queries_(ptr->import_queries_),
    queries_(ptr->queries_)
  { statement_type(IMPORT); }

  std::vector<Include>& Import::incs() { return incs_; }
  std::vector<Expression_Obj>& Import::urls() { return urls_; }
  std::vector<ImportBaseObj>& Import::imports() { return imports_; }

  bool Import::is_invisible() const
  {
    return incs_.empty() && urls_.empty() && imports_.empty();
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Import_Stub::Import_Stub(ParserState pstate, Include res/*, Sass_Import_Entry imp*/)
  : ImportBase(pstate), resource_(res)//, import_(imp)
  { statement_type(IMPORT_STUB); }
  Import_Stub::Import_Stub(const Import_Stub* ptr)
  : ImportBase(ptr), resource_(ptr->resource_)//, import_(ptr->import_)
  { statement_type(IMPORT_STUB); }
  Include Import_Stub::resource() { return resource_; };
  // Sass_Import_Entry Import_Stub::import() { return import_; };
  std::string Import_Stub::imp_path() { return resource_.imp_path; };
  std::string Import_Stub::abs_path() { return resource_.abs_path; };

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Warning::Warning(ParserState pstate, Expression_Obj msg)
  : Statement(pstate), message_(msg)
  { statement_type(WARNING); }
  Warning::Warning(const Warning* ptr)
  : Statement(ptr), message_(ptr->message_)
  { statement_type(WARNING); }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Error::Error(ParserState pstate, Expression_Obj msg)
  : Statement(pstate), message_(msg)
  { statement_type(ERROR); }
  Error::Error(const Error* ptr)
  : Statement(ptr), message_(ptr->message_)
  { statement_type(ERROR); }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Debug::Debug(ParserState pstate, Expression_Obj val)
  : Statement(pstate), value_(val)
  { statement_type(DEBUGSTMT); }
  Debug::Debug(const Debug* ptr)
  : Statement(ptr), value_(ptr->value_)
  { statement_type(DEBUGSTMT); }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  LoudComment::LoudComment(ParserState pstate, InterpolationObj itpl)
    : Statement(pstate), text_(itpl)
  {
    statement_type(COMMENT);
  }
  LoudComment::LoudComment(const LoudComment* ptr)
    : Statement(ptr),
    text_(ptr->text_)
  {
    statement_type(COMMENT);
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  SilentComment::SilentComment(ParserState pstate, std::string text)
    : Statement(pstate), text_(text)
  {
    statement_type(COMMENT);
  }
  SilentComment::SilentComment(const SilentComment* ptr)
    : Statement(ptr),
    text_(ptr->text_)
  {
    statement_type(COMMENT);
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  If::If(ParserState pstate, Expression_Obj pred, Block_Obj con, Block_Obj alt)
  : Has_Block(pstate, con), predicate_(pred), alternative_(alt)
  { statement_type(IF); }
  If::If(const If* ptr)
  : Has_Block(ptr),
    predicate_(ptr->predicate_),
    alternative_(ptr->alternative_)
  { statement_type(IF); }

  bool If::has_content()
  {
    return Has_Block::has_content() || (alternative_ && alternative_->has_content());
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  For::For(ParserState pstate,
      std::string var, Expression_Obj lo, Expression_Obj hi, bool inc, Block_Obj b)
  : Has_Block(pstate, b),
    variable_(var), lower_bound_(lo), upper_bound_(hi), is_inclusive_(inc)
  { statement_type(FOR); }
  For::For(const For* ptr)
  : Has_Block(ptr),
    variable_(ptr->variable_),
    lower_bound_(ptr->lower_bound_),
    upper_bound_(ptr->upper_bound_),
    is_inclusive_(ptr->is_inclusive_)
  { statement_type(FOR); }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Each::Each(ParserState pstate, std::vector<std::string> vars, Expression_Obj lst, Block_Obj b)
  : Has_Block(pstate, b), variables_(vars), list_(lst)
  { statement_type(EACH); }
  Each::Each(const Each* ptr)
  : Has_Block(ptr), variables_(ptr->variables_), list_(ptr->list_)
  { statement_type(EACH); }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  While::While(ParserState pstate, Expression_Obj pred, Block_Obj b)
  : Has_Block(pstate, b), predicate_(pred)
  { statement_type(WHILE); }
  While::While(const While* ptr)
  : Has_Block(ptr), predicate_(ptr->predicate_)
  { statement_type(WHILE); }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Return::Return(ParserState pstate, Expression_Obj val)
  : Statement(pstate), value_(val)
  { statement_type(RETURN); }
  Return::Return(const Return* ptr)
  : Statement(ptr), value_(ptr->value_)
  { statement_type(RETURN); }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  ExtendRule::ExtendRule(ParserState pstate, InterpolationObj s, bool optional) :
    Statement(pstate), isOptional_(optional), selector_(s)
  {
    statement_type(EXTEND);
  }

  ExtendRule::ExtendRule(const ExtendRule* ptr)
    : Statement(ptr),
    isOptional_(ptr->isOptional_),
    selector_(ptr->selector_)
  { statement_type(EXTEND); }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Definition::Definition(const Definition* ptr)
  : Has_Block(ptr),
    name_(ptr->name_),
    parameters_(ptr->parameters_),
    environment_(ptr->environment_),
    type_(ptr->type_),
    native_function_(ptr->native_function_),
    c_function_(ptr->c_function_),
    cookie_(ptr->cookie_),
    is_overload_stub_(ptr->is_overload_stub_),
    defaultParams_(ptr->defaultParams_),
    signature_(ptr->signature_)
  { }

  Definition::Definition(ParserState pstate,
    std::string n,
    Parameters_Obj params,
    Block_Obj b,
    Type t)
    : Has_Block(pstate, b),
    name_(n),
    parameters_(params),
    environment_(0),
    type_(t),
    native_function_(0),
    c_function_(0),
    cookie_(0),
    is_overload_stub_(false),
    defaultParams_(0),
    signature_(0)
  { }

  Definition::Definition(ParserState pstate,
              Signature sig,
              std::string n,
              Parameters_Obj params,
              Native_Function func_ptr,
              bool overload_stub)
  : Has_Block(pstate, {}),
    name_(n),
    parameters_(params),
    environment_(0),
    type_(FUNCTION),
    native_function_(func_ptr),
    c_function_(0),
    cookie_(0),
    is_overload_stub_(overload_stub),
    defaultParams_(0),
    signature_(sig)
  { }

  Definition::Definition(ParserState pstate,
              Signature sig,
              std::string n,
              Parameters_Obj params,
              Sass_Function_Entry c_func)
  : Has_Block(pstate, {}),
    name_(n),
    parameters_(params),
    environment_(0),
    type_(FUNCTION),
    native_function_(0),
    c_function_(c_func),
    cookie_(sass_function_get_cookie(c_func)),
    is_overload_stub_(false),
    defaultParams_(0),
    signature_(sig)
  { }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Mixin_Call::Mixin_Call(ParserState pstate, std::string n, Arguments_Obj args, Parameters_Obj b_params, Block_Obj b)
  : Has_Block(pstate, b), name_(n), arguments_(args), block_parameters_(b_params)
  { }
  Mixin_Call::Mixin_Call(const Mixin_Call* ptr)
  : Has_Block(ptr),
    name_(ptr->name_),
    arguments_(ptr->arguments_),
    block_parameters_(ptr->block_parameters_)
  { }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Content::Content(ParserState pstate, Arguments_Obj args)
  : Statement(pstate),
    arguments_(args)
  { statement_type(CONTENT); }
  Content::Content(const Content* ptr)
  : Statement(ptr),
    arguments_(ptr->arguments_)
  { statement_type(CONTENT); }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Expression::Expression(ParserState pstate, bool d, bool e, bool i, Type ct)
  : AST_Node(pstate),
    concrete_type_(ct)
  { }

  Expression::Expression(const Expression* ptr)
  : AST_Node(ptr),
    concrete_type_(ptr->concrete_type_)
  { }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  ParenthesizedExpression::ParenthesizedExpression(
    ParserState pstate,
    Expression* expression) :
    Expression(pstate),
    expression_(expression)
  {}

  ParenthesizedExpression::ParenthesizedExpression(
    const ParenthesizedExpression* ptr) :
    Expression(ptr),
    expression_(ptr->expression())
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Unary_Expression::Unary_Expression(ParserState pstate, Type t, Expression_Obj o)
  : Expression(pstate), optype_(t), operand_(o)
  { }
  Unary_Expression::Unary_Expression(const Unary_Expression* ptr)
  : Expression(ptr),
    optype_(ptr->optype_),
    operand_(ptr->operand_)
  { }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Argument::Argument(ParserState pstate, Expression_Obj val, std::string n, bool rest, bool keyword)
  : Expression(pstate), value_(val), name_(n), is_rest_argument_(rest), is_keyword_argument_(keyword), hash_(0)
  {
  }
  Argument::Argument(const Argument* ptr)
  : Expression(ptr),
    value_(ptr->value_),
    name_(ptr->name_),
    is_rest_argument_(ptr->is_rest_argument_),
    is_keyword_argument_(ptr->is_keyword_argument_),
    hash_(ptr->hash_)
  {
  }

  size_t Argument::hash() const
  {
    if (hash_ == 0) {
      hash_ = std::hash<std::string>()(name());
      hash_combine(hash_, value()->hash());
    }
    return hash_;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Arguments::Arguments(ParserState pstate)
  : Expression(pstate),
    Vectorized<Argument_Obj>()
  { }
  Arguments::Arguments(const Arguments* ptr)
  : Expression(ptr),
    Vectorized<Argument_Obj>(*ptr)
  { }

  ArgumentObj Arguments::get_rest_argument()
  {
    for (Argument* arg : elements()) {
      if (arg->is_rest_argument()) {
        return arg;
      }
    }
    return ArgumentObj();
  }

  ArgumentObj Arguments::get_keyword_argument()
  {
    for (Argument* arg : elements()) {
      if (arg->is_keyword_argument()) {
        return arg;
      }
    }
    return ArgumentObj();
  }

  bool Arguments::hasRestArgument() const {
    for (const Argument* arg : elements()) {
      if (arg->is_rest_argument()) {
        return true;
      }
    }
    return false;
  }

  bool Arguments::hasNamedArgument() const {
    for (const Argument* arg : elements()) {
      if (!arg->name().empty()) {
        return true;
      }
    }
    return false;
  }

  bool Arguments::hasKeywordArgument() const {
    for (const Argument* arg : elements()) {
      if (arg->is_keyword_argument()) {
        return true;
      }
    }
    return false;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  At_Root_Query::At_Root_Query(ParserState pstate, Expression_Obj f, Expression_Obj v, bool i)
  : Expression(pstate), feature_(f), value_(v)
  { }
  At_Root_Query::At_Root_Query(const At_Root_Query* ptr)
  : Expression(ptr),
    feature_(ptr->feature_),
    value_(ptr->value_)
  { }

  bool At_Root_Query::exclude(std::string str)
  {
    bool with = feature() && unquote(feature()->to_string()).compare("with") == 0;
    // bool with = feature() && Util::equalsLiteral("(with", unquote(feature()->to_string())) /*&&
    //  !Util::equalsLiteral("(without", unquote(feature()->to_string()))*/;
    List* l = Cast<List>(value().ptr());
    std::string v;

    if (with)
    {
      if (!l || l->length() == 0) return str.compare("rule") != 0;
      for (size_t i = 0, L = l->length(); i < L; ++i)
      {
        v = unquote((*l)[i]->to_string());
        if (v.compare("all") == 0 || v == str) return false;
      }
      return true;
    }
    else
    {
      if (!l || !l->length()) return str.compare("rule") == 0;
      for (size_t i = 0, L = l->length(); i < L; ++i)
      {
        v = unquote((*l)[i]->to_string());
        if (v.compare("all") == 0 || v == str) return true;
      }
      return false;
    }
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  At_Root_Block::At_Root_Block(ParserState pstate, At_Root_Query_Obj e, Block_Obj b)
  : Has_Block(pstate, b), expression_(e)
  { statement_type(ATROOT); }
  At_Root_Block::At_Root_Block(const At_Root_Block* ptr)
  : Has_Block(ptr), expression_(ptr->expression_)
  { statement_type(ATROOT); }

  bool At_Root_Block::bubbles() {
    return true;
  }

  bool At_Root_Block::exclude_node(Statement_Obj s) {
    if (expression() == nullptr)
    {
      return s->statement_type() == Statement::RULESET;
    }

    if (s->statement_type() == Statement::DIRECTIVE)
    {
      if (AtRuleObj dir = Cast<AtRule>(s))
      {
        std::string keyword(dir->keyword());
        if (keyword.length() > 0) keyword.erase(0, 1);
        return expression()->exclude(keyword);
      }
    }
    if (s->statement_type() == Statement::MEDIA)
    {
      return expression()->exclude("media");
    }
    if (s->statement_type() == Statement::RULESET)
    {
      return expression()->exclude("rule");
    }
    if (s->statement_type() == Statement::SUPPORTS)
    {
      return expression()->exclude("supports");
    }
    if (AtRuleObj dir = Cast<AtRule>(s))
    {
      if (dir->is_keyframes()) return expression()->exclude("keyframes");
    }
    return false;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Parameter::Parameter(ParserState pstate, std::string n, Expression_Obj def, bool rest)
  : AST_Node(pstate), name_(n), default_value_(def), is_rest_parameter_(rest)
  { }
  Parameter::Parameter(const Parameter* ptr)
  : AST_Node(ptr),
    name_(ptr->name_),
    default_value_(ptr->default_value_),
    is_rest_parameter_(ptr->is_rest_parameter_)
  { }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Parameters::Parameters(ParserState pstate)
  : AST_Node(pstate),
    Vectorized<Parameter_Obj>(),
    has_optional_parameters_(false),
    has_rest_parameter_(false)
  { }
  Parameters::Parameters(const Parameters* ptr)
  : AST_Node(ptr),
    Vectorized<Parameter_Obj>(*ptr),
    has_optional_parameters_(ptr->has_optional_parameters_),
    has_rest_parameter_(ptr->has_rest_parameter_)
  { }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // If you forget to add a class here you will get
  // undefined reference to `vtable for Sass::Class'

  IMPLEMENT_AST_OPERATORS(StyleRule);
  IMPLEMENT_AST_OPERATORS(MediaRule);
  IMPLEMENT_AST_OPERATORS(Import);
  IMPLEMENT_AST_OPERATORS(Import_Stub);
  IMPLEMENT_AST_OPERATORS(ImportRule);
  IMPLEMENT_AST_OPERATORS(StaticImport);
  IMPLEMENT_AST_OPERATORS(DynamicImport);
  IMPLEMENT_AST_OPERATORS(AtRule);
  IMPLEMENT_AST_OPERATORS(At_Root_Block);
  IMPLEMENT_AST_OPERATORS(While);
  IMPLEMENT_AST_OPERATORS(Each);
  IMPLEMENT_AST_OPERATORS(For);
  IMPLEMENT_AST_OPERATORS(If);
  IMPLEMENT_AST_OPERATORS(Mixin_Call);
  IMPLEMENT_AST_OPERATORS(ExtendRule);
  IMPLEMENT_AST_OPERATORS(Debug);
  IMPLEMENT_AST_OPERATORS(Error);
  IMPLEMENT_AST_OPERATORS(Warning);
  IMPLEMENT_AST_OPERATORS(Assignment);
  IMPLEMENT_AST_OPERATORS(Return);
  IMPLEMENT_AST_OPERATORS(At_Root_Query);
  IMPLEMENT_AST_OPERATORS(LoudComment);
  IMPLEMENT_AST_OPERATORS(SilentComment);
  IMPLEMENT_AST_OPERATORS(Parameters);
  IMPLEMENT_AST_OPERATORS(Parameter);
  IMPLEMENT_AST_OPERATORS(Arguments);
  IMPLEMENT_AST_OPERATORS(Argument);
  IMPLEMENT_AST_OPERATORS(Unary_Expression);
  IMPLEMENT_AST_OPERATORS(ParenthesizedExpression);
  IMPLEMENT_AST_OPERATORS(Block);
  IMPLEMENT_AST_OPERATORS(Content);
  IMPLEMENT_AST_OPERATORS(Trace);
  IMPLEMENT_AST_OPERATORS(Keyframe_Rule);
  IMPLEMENT_AST_OPERATORS(Bubble);
  IMPLEMENT_AST_OPERATORS(Definition);
  IMPLEMENT_AST_OPERATORS(Declaration);

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

}
