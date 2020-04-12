// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include "ast.hpp"
#include "debugger.hpp"
#include "parser_scss.hpp"
#include "parser_at_root_query.hpp"
#include "strings.hpp"

namespace Sass {

  // static Null sass_null(SourceSpan("null"));

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
      default: return "invalid";
    }
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // void AST_Node::update_pstate(const SourceSpan& pstate)
  // {
  //   // pstate_.offset += pstate - pstate_ + pstate.offset;
  // }

  sass::string AST_Node::to_string(Sass_Inspect_Options opt) const
  {
    Sass_Output_Options out(opt);
    Inspect i(Emitter{ out });
    i.in_declaration = true;
    // ToDo: inspect should be const
    const_cast<AST_Node*>(this)->perform(&i);
    return i.get_buffer();
  }

  sass::string AST_Node::to_css(Sass_Inspect_Options opt, bool quotes) const
  {
    opt.output_style = TO_CSS;
    Sass_Output_Options out(opt);
    Inspect i(Emitter{ out });
    i.quotes = quotes;
    i.in_declaration = true;
    // ToDo: inspect should be const
    const_cast<AST_Node*>(this)->perform(&i);
    return i.get_buffer();
  }

  sass::string AST_Node::to_css(Sass_Inspect_Options opt, sass::vector<Mapping>& mappings, bool quotes) const
  {
    opt.output_style = TO_CSS;
    Sass_Output_Options out(opt);
    Inspect i(Emitter{ out });
    i.quotes = quotes;
    i.in_declaration = true;
    // ToDo: inspect should be const
    const_cast<AST_Node*>(this)->perform(&i);
    SourceMap map = i.smap();
    std::copy(map.mappings.begin(),
      map.mappings.end(),
      std::back_inserter(mappings));
    return i.get_buffer();
  }

  sass::string AST_Node::to_string() const
  {
    return to_string({ NESTED, SassDefaultPrecision });
  }

  sass::string AST_Node::to_css(sass::vector<Mapping>& mappings, bool quotes) const
  {
    return to_css({ NESTED, SassDefaultPrecision }, mappings, quotes);
  }

  sass::string AST_Node::to_css(bool quotes) const
  {
    return to_css({ NESTED, SassDefaultPrecision }, quotes);
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Statement::Statement(const SourceSpan& pstate, size_t t)
  : AST_Node(pstate), tabs_(t)
  { }
  Statement::Statement(SourceSpan&& pstate, size_t t)
    : AST_Node(std::move(pstate)), tabs_(t)
  { }
  Statement::Statement(const Statement* ptr)
  : AST_Node(ptr),
    tabs_(ptr->tabs_)
  { }

  bool Statement::bubbles() const
  {
    return false;
  }

  bool Statement::has_content()
  {
    return Cast<ContentRule>(this) != nullptr;
  }

  bool Statement::is_invisible() const
  {
    return false;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Block::Block(const SourceSpan& pstate, size_t s, bool r)
  : Statement(pstate),
    VectorizedBase<Statement>(s),
    idxs_(0), is_root_(r)
  { }

  Block::Block(const SourceSpan& pstate, const sass::vector<StatementObj>& vec, bool r) :
    Statement(pstate),
    VectorizedBase<Statement>(vec),
    idxs_(0), is_root_(r)
  { }

  Block::Block(const SourceSpan& pstate, sass::vector<StatementObj>&& vec, bool r) :
    Statement(pstate),
    VectorizedBase<Statement>(std::move(vec)),
    idxs_(0), is_root_(r)
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

  ParentStatement::ParentStatement(const SourceSpan& pstate, Block_Obj b)
    : Statement(pstate), block_(b)
  { }
  ParentStatement::ParentStatement(SourceSpan&& pstate, Block_Obj b)
    : Statement(std::move(pstate)), block_(b)
  { }
  ParentStatement::ParentStatement(const ParentStatement* ptr)
  : Statement(ptr), block_(ptr->block_)
  { }

  bool ParentStatement::has_content()
  {
    return (block_ && block_->has_content()) || Statement::has_content();
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  StyleRule::StyleRule(SourceSpan&& pstate, Interpolation* s, Block_Obj b)
    : ParentStatement(std::move(pstate), b), interpolation_(s), idxs_(0)
  {}

  bool CssStyleRule::is_invisible() const {
    bool sel_invisible = true;
    bool els_invisible = true;
    if (const SelectorList * sl = selector()) {
      for (size_t i = 0, L = sl->length(); i < L; i += 1) {
        if (!(*sl)[i]->isInvisible()) {
          sel_invisible = false;
          break;
        }
      }
    }
    for (Statement* item : elements()) {
      if (!item->is_invisible()) {
        els_invisible = false;
        break;
      }
    }

    return sel_invisible || els_invisible;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Bubble::Bubble(const SourceSpan& pstate, Statement_Obj n, Statement_Obj g, size_t t)
  : Statement(pstate, t), node_(n)
  { }

  bool Bubble::bubbles() const
  {
    return true;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Trace::Trace(const SourceSpan& pstate, const sass::string& n, Block_Obj b, char type)
  : ParentStatement(pstate, b), type_(type), name_(n)
  { }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  ListExpression::ListExpression(const SourceSpan& pstate, Sass_Separator separator) :
    Expression(pstate),
    contents_(),
    separator_(separator),
    hasBrackets_(false)
  {
  }

  MapExpression::MapExpression(const SourceSpan& pstate) :
    Expression(pstate),
    kvlist_()
  {
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  AtRule::AtRule(const SourceSpan& pstate, InterpolationObj name, ExpressionObj value, Block_Obj block) :
    ParentStatement(pstate, block),
    name_(name),
    value_(value)
  {}

  AtRule::AtRule(const AtRule* ptr)
  : ParentStatement(ptr),
    name_(ptr->name_),
    value_(ptr->value_)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Declaration::Declaration(const SourceSpan& pstate, InterpolationObj name, ExpressionObj value, bool c, Block_Obj b)
  : ParentStatement(pstate, b), name_(name), value_(value), is_custom_property_(c)
  {}

  bool Declaration::is_invisible() const
  {
    if (is_custom_property()) return false;
    return !(value_ && !value_.isNull());
  }


  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Assignment::Assignment(const SourceSpan& pstate, const sass::string& var, IdxRef vidx, ExpressionObj val, bool is_default, bool is_global)
  : Statement(pstate), variable_(var), value_(val), vidx_(vidx), is_default_(is_default), is_global_(is_global)
  {}

  MapMerge::MapMerge(const SourceSpan& pstate, const sass::string& var, IdxRef vidx, ExpressionObj val, bool is_default, bool is_global)
    : Statement(pstate), variable_(var), value_(val), vidx_(vidx), is_default_(is_default), is_global_(is_global)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  ImportBase::ImportBase(
    const SourceSpan& pstate) :
    Statement(pstate)
  {}

  ImportBase::ImportBase(
    const ImportBase* ptr) :
    Statement(ptr)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  StaticImport::StaticImport(
    const SourceSpan& pstate,
    InterpolationObj url,
    SupportsCondition_Obj supports,
    InterpolationObj media) :
    ImportBase(pstate),
    url_(url),
    supports_(supports),
    media_(media),
    outOfOrder_(true)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  DynamicImport::DynamicImport(
    const SourceSpan& pstate,
    const sass::string& url) :
    ImportBase(pstate),
    url_(url)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  ImportRule::ImportRule(
    const SourceSpan& pstate) :
    Statement(pstate),
    VectorizedBase()
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Import::Import(const SourceSpan& pstate)
  : ImportBase(pstate),
    urls_(),
    incs_(),
    // imports_(),
    import_queries_(),
    queries_()
  {}

  sass::vector<Include>& Import::incs() { return incs_; }
  sass::vector<ExpressionObj>& Import::urls() { return urls_; }
  // sass::vector<ImportBaseObj>& Import::imports() { return imports_; }
  sass::vector<ExpressionObj>& Import::queries() { return import_queries_; }

  bool Import::is_invisible() const
  {
    return incs_.empty() && urls_.empty();
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Import_Stub::Import_Stub(const SourceSpan& pstate, Include res/*, Sass_Import_Entry imp*/)
  : ImportBase(pstate), resource_(res)//, import_(imp)
  {}
  Include Import_Stub::resource() { return resource_; };
  // Sass_Import_Entry Import_Stub::import() { return import_; };
  sass::string Import_Stub::imp_path() { return resource_.imp_path; };
  sass::string Import_Stub::abs_path() { return resource_.abs_path; };

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  WarnRule::WarnRule(const SourceSpan& pstate, ExpressionObj expression)
  : Statement(pstate), expression_(expression)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  ErrorRule::ErrorRule(const SourceSpan& pstate, ExpressionObj expression)
  : Statement(pstate), expression_(expression)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  DebugRule::DebugRule(const SourceSpan& pstate, ExpressionObj expression)
  : Statement(pstate), expression_(expression)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  LoudComment::LoudComment(const SourceSpan& pstate, InterpolationObj itpl)
    : Statement(pstate), text_(itpl)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  SilentComment::SilentComment(const SourceSpan& pstate, const sass::string& text)
    : Statement(pstate), text_(text)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  If::If(const SourceSpan& pstate, ExpressionObj pred, Block_Obj con, Block_Obj alt)
  : ParentStatement(pstate, con), idxs_(0), predicate_(pred), alternative_(alt)
  {}

  bool If::has_content()
  {
    return ParentStatement::has_content() || (alternative_ && alternative_->has_content());
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  For::For(const SourceSpan& pstate,
    const EnvKey& var, ExpressionObj lo, ExpressionObj hi, bool inc, Block_Obj b)
  : ParentStatement(pstate, b),
    variable_(var), lower_bound_(lo), upper_bound_(hi), idxs_(0), is_inclusive_(inc)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Each::Each(const SourceSpan& pstate, const sass::vector<EnvKey>& vars, ExpressionObj lst, Block_Obj b)
  : ParentStatement(pstate, b), variables_(vars), idxs_(0), list_(lst)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  WhileRule::WhileRule(
    const SourceSpan& pstate,
    ExpressionObj condition,
    Block_Obj b) :
    ParentStatement(pstate, b),
    condition_(condition),
    idxs_(0)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Return::Return(const SourceSpan& pstate, ExpressionObj val)
  : Statement(pstate), value_(val)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  ExtendRule::ExtendRule(const SourceSpan& pstate, InterpolationObj s, bool optional) :
    Statement(pstate), isOptional_(optional), selector_(s)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  ContentRule::ContentRule(const SourceSpan& pstate, ArgumentInvocation* args)
  : Statement(pstate),
    arguments_(args)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Interpolant::Interpolant(const SourceSpan& pstate)
    : AST_Node(pstate)
  { }

  bool Interpolant::operator==(const Interpolant & rhs) const
  {
    if (const Expression* ex = rhs.getExpression()) {
      return *this == *ex;
    }
    // We now that Interpolant is either expression or itpl string
    else if (const ItplString* str = static_cast<const ItplString*>(&rhs)) {
      return *this == *str;
    }
    return false;
  }

  Interpolant::Interpolant(SourceSpan&& pstate)
    : AST_Node(std::move(pstate))
  { }

  Interpolant::Interpolant(const Interpolant* ptr)
    : AST_Node(ptr)
  { }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Expression::Expression(const SourceSpan& pstate)
  : Interpolant(pstate)
  { }

  Expression::Expression(SourceSpan&& pstate)
    : Interpolant(std::move(pstate))
  { }

  Expression::Expression(const Expression* ptr)
  : Interpolant(ptr)
  { }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  ParenthesizedExpression::ParenthesizedExpression(
    const SourceSpan& pstate,
    Expression* expression) :
    Expression(pstate),
    expression_(expression)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Unary_Expression::Unary_Expression(const SourceSpan& pstate, Type t, ExpressionObj o)
  : Expression(pstate), optype_(t), operand_(o)
  { }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Argument::Argument(const SourceSpan& pstate, ExpressionObj val, const EnvKey& n, bool rest, bool keyword)
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


  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  AtRootQuery::AtRootQuery(
    const SourceSpan& pstate,
    const StringSet& names,
    bool include) :
    AST_Node(pstate),
    names_(names),
    include_(include)
  {}

  // Whether this includes or excludes style rules.
  inline bool AtRootQuery::rule() const {
    return names_.count("rule") == 1;
  }

  // Whether this includes or excludes media rules.
  inline bool AtRootQuery::media() const {
    return names_.count("media") == 1;
  }

  // Whether this includes or excludes *all* rules.
  inline bool AtRootQuery::all() const {
    return names_.count("all") == 1;
  }

  // Returns the at-rule name for [node], or `null` if it's not an at-rule.
  sass::string AtRootQuery::_nameFor(Statement* node) const {
    if (Cast<CssMediaRule>(node)) return "media";
    if (Cast<CssSupportsRule>(node)) return "supports";
    if (CssAtRule* at = Cast<CssAtRule>(node)) {
      return StringUtils::toLowerCase(at->name()->text());
    }
    return "";
  }

  // Returns whether [this] excludes a node with the given [name].
  bool AtRootQuery::excludesName(const sass::string& name) const {
    return (names_.count(name) == 1) != include();
  }

  // Returns whether [this] excludes [node].
  bool AtRootQuery::excludes(CssParentNode* node) const
  {
    if (all()) return !include();
    if (rule() && Cast<CssStyleRule>(node)) return !include();
    return excludesName(_nameFor(node));
  }

  // Whether this excludes `@media` rules.
  // Note that this takes [include] into account.
  bool AtRootQuery::excludesMedia() const {
    return (all() || media()) != include();
  }

  // Whether this excludes style rules.
  // Note that this takes [include] into account.
  bool AtRootQuery::excludesStyleRules() const {
    return (all() || rule()) != include();
  }

  AtRootQuery* AtRootQuery::parse(const sass::string& contents, Context& ctx) {

    // char* str = sass_copy_c_string(contents.c_str());
    // ctx.strings.emplace_back(str);
    auto qwe = SASS_MEMORY_NEW(SourceFile,
      "sass://parse-at-root-query", contents.c_str(), -1);
    AtRootQueryParser p2(ctx, qwe);
    return p2.parse();
  }

  AtRootQuery* AtRootQuery::defaultQuery(const SourceSpan& pstate)
  {
    StringSet wihtoutStyleRule;
    wihtoutStyleRule.insert("rule");
    return SASS_MEMORY_NEW(AtRootQuery,
      pstate, wihtoutStyleRule, false);
  }

  sass::string AtRootQuery::toString() const
  {
    sass::sstream msg;
    if (include_) msg << "with: ";
    else msg << "without: ";
    bool addComma = false;
    for (auto item : names_) {
      if (addComma) msg << ", ";
      msg << item;
    }
    return msg.str();
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  AtRootRule::AtRootRule(
    const SourceSpan& pstate,
    InterpolationObj query,
    Block_Obj block)
    : ParentStatement(pstate, block),
    query_(query),
    idxs_(0)
  {}

  AtRootRule::AtRootRule(
    SourceSpan&& pstate,
    InterpolationObj query,
    Block_Obj block)
    : ParentStatement(std::move(pstate), block),
    query_(query),
    idxs_(0)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // If you forget to add a class here you will get
  // undefined reference to `vtable for Sass::Class'

  // IMPLEMENT_AST_OPERATORS(StyleRule);
  // IMPLEMENT_AST_OPERATORS(MediaRule);
  // IMPLEMENT_AST_OPERATORS(Import);
  // IMPLEMENT_AST_OPERATORS(Import_Stub);
  // IMPLEMENT_AST_OPERATORS(ImportRule);
  // IMPLEMENT_AST_OPERATORS(StaticImport);
  // IMPLEMENT_AST_OPERATORS(DynamicImport);
  IMPLEMENT_AST_OPERATORS(AtRule);
  // IMPLEMENT_AST_OPERATORS(CssAtRootRule);
  // IMPLEMENT_AST_OPERATORS(WhileRule);
  // IMPLEMENT_AST_OPERATORS(Each);
  // IMPLEMENT_AST_OPERATORS(For);
  // IMPLEMENT_AST_OPERATORS(If);
  // IMPLEMENT_AST_OPERATORS(ExtendRule);
  // IMPLEMENT_AST_OPERATORS(DebugRule);
  // IMPLEMENT_AST_OPERATORS(ErrorRule);
  // IMPLEMENT_AST_OPERATORS(WarnRule);
  // IMPLEMENT_AST_OPERATORS(Assignment);
  // IMPLEMENT_AST_OPERATORS(Return);
  // IMPLEMENT_AST_OPERATORS(LoudComment);
  // IMPLEMENT_AST_OPERATORS(SilentComment);
  IMPLEMENT_AST_OPERATORS(Argument);
  // IMPLEMENT_AST_OPERATORS(Unary_Expression);
  // IMPLEMENT_AST_OPERATORS(ParenthesizedExpression);
  // IMPLEMENT_AST_OPERATORS(Block);
  // IMPLEMENT_AST_OPERATORS(ContentRule);
  // IMPLEMENT_AST_OPERATORS(Trace);
  // IMPLEMENT_AST_OPERATORS(Bubble);
  // IMPLEMENT_AST_OPERATORS(Declaration);

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // ToDO: add move ctor
  ArgumentInvocation::ArgumentInvocation(const SourceSpan& pstate,
    const sass::vector<ExpressionObj>& positional,
    const EnvKeyFlatMap2& named,
    Expression* restArg,
    Expression* kwdRest) :
    AST_Node(pstate),
    positional_(positional),
    named_(named),
    restArg_(restArg),
    kwdRest_(kwdRest)
  {
  }

  ArgumentInvocation::ArgumentInvocation(SourceSpan&& pstate,
    sass::vector<ExpressionObj>&& positional,
    EnvKeyFlatMap2&& named,
    Expression* restArg,
    Expression* kwdRest) :
    AST_Node(std::move(pstate)),
    positional_(std::move(positional)),
    named_(std::move(named)),
    restArg_(restArg),
    kwdRest_(kwdRest)
  {
  }

  // Returns whether this invocation passes no arguments.
  bool ArgumentInvocation::isEmpty() const
  {
    return positional_.empty()
      && named_.empty()
      && restArg_.isNull();
  }

  sass::string ArgumentInvocation::toString() const
  {
    sass::sstream strm;
    strm << "(";
    bool addComma = false;
    for (Expression* named : positional_) {
      if (addComma) strm << ", ";
      strm << named->to_string();
      addComma = true;
    }
    for (auto kv : named_) {
      if (addComma) strm << ", ";
      strm << kv.first.orig() << ": ";
//      strm << named_->get(named)->to_string();
      addComma = true;
    }
    if (!restArg_.isNull()) {
      if (addComma) strm << ", ";
      strm << restArg_->to_string() << "...";
      addComma = true;
    }
    if (!kwdRest_.isNull()) {
      if (addComma) strm << ", ";
      strm << kwdRest_->to_string() << "...";
      addComma = true;
    }
    strm << ")";
    return strm.str();
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  ArgumentResults::ArgumentResults(
    const sass::vector<ValueObj>& positional,
    const EnvKeyFlatMap<ValueObj>& named,
    Sass_Separator separator) :
    positional_(positional),
    named_(named),
    separator_(separator)
  {
  }

  ArgumentResults::ArgumentResults(
    sass::vector<ValueObj>&& positional,
    EnvKeyFlatMap<ValueObj>&& named,
    Sass_Separator separator) :
    positional_(std::move(positional)),
    named_(std::move(named)),
    separator_(separator)
  {
  }

  ArgumentResults::ArgumentResults(
    const ArgumentResults& other) :
    positional_(other.positional_),
    named_(other.named_),
    separator_(other.separator_)
  {
  }

  ArgumentResults::ArgumentResults(
    ArgumentResults&& other) noexcept :
    positional_(std::move(other.positional_)),
    named_(std::move(other.named_)),
    separator_(other.separator_)
  {
  }

  ArgumentResults& ArgumentResults::operator=(ArgumentResults&& other) noexcept {
    positional_ = std::move(other.positional_);
    named_ = std::move(other.named_);
    separator_ = other.separator_;
    return *this;
  }

  ArgumentDeclaration::ArgumentDeclaration(
    const SourceSpan& pstate,
    const sass::vector<ArgumentObj>& arguments,
    const sass::string& restArg) :
    AST_Node(pstate),
    arguments_(arguments),
    restArg_(restArg)
  {
  }

  ArgumentDeclaration* ArgumentDeclaration::parse(
    Context& context, const sass::string& contents)
  {
    sass::string text = "(" + contents + ")";
    // char* cstr = sass_copy_c_string(text.c_str());
    // context.strings.emplace_back(cstr); // clean up later
    auto qwe = SASS_MEMORY_NEW(SourceFile,
      "sass://builtin", text.c_str(), -1);
    ScssParser parser(context, qwe);
    // We added
    EnvRoot root;
    ScopedStackFrame<EnvFrame>
      scoped(context.varStack, &root);
    // context.varStack.push_back(&root);
    return parser.parseArgumentDeclaration2();
  }

  /// Throws a [SassScriptException] if [positional] and [names] aren't valid
  /// for this argument declaration.
  void ArgumentDeclaration::verify(
    size_t positional,
    const EnvKeyFlatMap<ValueObj>& names,
    const SourceSpan& pstate,
    const BackTraces& traces)
  {

    size_t i = 0;
    size_t namedUsed = 0;
    size_t iL = arguments_.size();
    while (i < std::min(positional, iL)) {
      if (names.count(arguments_[i]->name()) == 1) {
        throw Exception::InvalidSyntax(traces,
          "Argument " + arguments_[i]->name().orig() +
          " name was passed both by position and by name.");
      }
      i++;
    }
    while (i < iL) {
      if (names.count(arguments_[i]->name()) == 1) {
        namedUsed++;
      }
      else if (arguments_[i]->value() == nullptr) {
        throw Exception::InvalidSyntax(traces,
          "Missing argument " + arguments_[i]->name().orig() + ".");
      }
      i++;
    }

    if (!restArg_.empty()) return;

    if (positional > arguments_.size()) {
      sass::sstream strm;
      strm << "Only " << arguments_.size() << " "; //  " positional ";
      strm << pluralize("argument", arguments_.size());
      strm << " allowed, but " << positional << " ";
      strm << pluralize("was", positional, "were");
      strm << " passed.";
      // callStackFrame frame(traces,
      //   BackTrace(this->pstate()));
      throw Exception::InvalidSyntax(
        traces, strm.str());
    }

    if (namedUsed < names.size()) {
      EnvKeyFlatMap<ValueObj> unknownNames(names);
      for (Argument* arg : arguments_) {
        unknownNames.erase(arg->name());
      }
      // sass::sstream strm;
      // strm << "No " << pluralize("argument", unknownNames.size());
      // strm << " named " << toSentence(unknownNames, "or");
      throw Exception::InvalidSyntax(traces,
        "No argument named " + toSentence(unknownNames, "or") + ".");
    }

  }

  // Returns whether [positional] and [names] are valid for this argument declaration.
  bool ArgumentDeclaration::matches(size_t positional, const EnvKeyFlatMap<ValueObj>& names)
  {
    size_t namedUsed = 0; Argument* argument;
    for (size_t i = 0, iL = arguments_.size(); i < iL; i++) {
      argument = arguments_[i];
      if (i < positional) {
        if (names.count(argument->name()) == 1) {
          return false;
        }
      }
      else if (names.count(argument->name()) == 1) {
        namedUsed++;
      }
      else if (argument->value().isNull()) {
        return false;
      }
    }
    if (!restArg_.empty()) return true;
    if (positional > arguments_.size()) return false;
    if (namedUsed < names.size()) return false;
    return true;
  }

  CallableDeclaration::CallableDeclaration(
    const SourceSpan& pstate,
    const EnvKey& name,
    ArgumentDeclaration* arguments,
    SilentComment* comment,
    Block* block) :
    ParentStatement(pstate, block),
    name_(name),
    idxs_(0),
    comment_(comment),
    arguments_(arguments)
  {
    if (arguments_ == nullptr) {
      std::cerr << "Callable without arg decl\n";
    }
  }

  FunctionRule::FunctionRule(
    const SourceSpan& pstate,
    const EnvKey& name,
    ArgumentDeclaration* arguments,
    SilentComment* comment,
    Block* block) :
    CallableDeclaration(pstate,
      name, arguments, comment, block)
  {
  }

  sass::string FunctionRule::toString1() const
  {
    return sass::string();
  }

  MixinRule::MixinRule(
    const SourceSpan& pstate,
    const sass::string& name,
    ArgumentDeclaration* arguments,
    SilentComment* comment,
    Block* block) :
    CallableDeclaration(pstate,
      name, arguments, comment, block)
  {
  }

  sass::string MixinRule::toString1() const
  {
    return sass::string();
  }




  IncludeRule::IncludeRule(
    const SourceSpan& pstate,
    const EnvKey& name,
    ArgumentInvocation* arguments,
    const sass::string& ns,
    ContentBlock* content,
    Block* block) :
    InvocationStatement(pstate, arguments),
    ns_(ns), name_(name), content_(content)
  {
  }

  bool IncludeRule::has_content() {
    return !content_.isNull();
  }

  ContentBlock::ContentBlock(
    const SourceSpan& pstate,
    ArgumentDeclaration* arguments,
    const sass::vector<StatementObj>& children) :
    CallableDeclaration(pstate, Keys::contentRule, arguments)
  {
  }

  sass::string ContentBlock::toString1() const
  {
    return sass::string();
  }

  UserDefinedCallable::UserDefinedCallable(
    const SourceSpan& pstate,
    const EnvKey& name,
    CallableDeclarationObj declaration,
    UserDefinedCallable* content) :
    Callable(pstate),
    name_(name),
    declaration_(declaration),
    content_(content)
  {
  }

  bool UserDefinedCallable::operator==(const Callable& rhs) const
  {
    if (const UserDefinedCallable * user = Cast<UserDefinedCallable>(&rhs)) {
      return this == user;
    }
    return false;
  }

  ValueExpression::ValueExpression(
    const SourceSpan& pstate,
    ValueObj value) :
    Expression(pstate),
    value_(value)
  {
  }

  ExternalCallable::ExternalCallable(
    const sass::string& name,
    ArgumentDeclaration* parameters,
    Sass_Function_Entry function) :
    Callable(SourceSpan("[external]")),
    name_(name),
    declaration_(parameters),
    function_(function)
  {
  }

  bool ExternalCallable::operator==(const Callable& rhs) const
  {
    if (const ExternalCallable * user = Cast<ExternalCallable>(&rhs)) {
      return this == user;
    }
    return false;
  }

  EnvKeyFlatMap2::EnvKeyFlatMap2(const SourceSpan& pstate, EnvKeyFlatMap<ExpressionObj>&& named)
    : AST_Node(pstate), EnvKeyFlatMap<ExpressionObj>(std::move(named))
  {
  }

}
