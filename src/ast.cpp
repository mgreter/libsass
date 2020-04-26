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
    SassOutputOptionsCpp out(opt);
    Inspect i(out, false);
    i.in_declaration = true;
    // ToDo: inspect should be const
    const_cast<AST_Node*>(this)->perform(&i);
    return i.get_buffer();
  }

  sass::string AST_Node::to_css(Sass_Inspect_Options opt, bool quotes) const
  {
    opt.output_style = SASS_STYLE_TO_CSS;
    SassOutputOptionsCpp out(opt);
    Inspect i(out, false);
    i.quotes = quotes;
    i.in_declaration = true;
    // ToDo: inspect should be const
    const_cast<AST_Node*>(this)->perform(&i);
    return i.get_buffer();
  }

  sass::string AST_Node::to_css(Sass_Inspect_Options opt, sass::vector<Mapping>& mappings, bool quotes) const
  {
    opt.output_style = SASS_STYLE_TO_CSS;
    SassOutputOptionsCpp out(opt);
    Inspect i(out, false);
    i.quotes = quotes;
    i.in_declaration = true;
    // ToDo: inspect should be const
    const_cast<AST_Node*>(this)->perform(&i);
    // SourceMap map = i.smap;
    // std::copy(map.mappings.begin(),
    //   map.mappings.end(),
    //   std::back_inserter(mappings));
    return i.get_buffer();
  }

  sass::string AST_Node::to_string() const
  {
    return to_string({ SASS_STYLE_NESTED, SassDefaultPrecision });
  }

  sass::string AST_Node::to_css(sass::vector<Mapping>& mappings, bool quotes) const
  {
    return to_css({ SASS_STYLE_NESTED, SassDefaultPrecision }, mappings, quotes);
  }

  sass::string AST_Node::to_css(bool quotes) const
  {
    return to_css({ SASS_STYLE_NESTED, SassDefaultPrecision }, quotes);
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Statement::Statement(const SourceSpan& pstate, size_t t)
  : AST_Node(pstate), tabs_(t)
  { }
  Statement::Statement(SourceSpan&& pstate, size_t t)
    : AST_Node(std::move(pstate)), tabs_(t)
  { }
  Statement::Statement(const Statement* ptr, bool childless)
  : AST_Node(ptr),
    tabs_(ptr->tabs_)
  { }

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

  Root::Root(const SourceSpan& pstate, size_t s)
    : AST_Node(pstate), Vectorized<Statement>(s) {}
  Root::Root(const SourceSpan& pstate, sass::vector<StatementObj>&& vec)
    : AST_Node(pstate), Vectorized<Statement>(std::move(vec)) {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////


  ParentStatement::ParentStatement(const SourceSpan& pstate, sass::vector<StatementObj>&& els)
    : Statement(pstate), Vectorized<Statement>(std::move(els)), idxs_(0)
  {
  }

  ParentStatement::ParentStatement(const ParentStatement* ptr, bool childless)
  : Statement(ptr), Vectorized<Statement>(), idxs_(ptr->idxs_)
  {
    if (!childless) elements_ = ptr->elements();
  }

  bool ParentStatement::has_content()
  {
    if (Statement::has_content()) return true;
    for (auto child : elements_) {
      if (child->has_content()) return true;
    }
    return false;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  StyleRule::StyleRule(SourceSpan&& pstate, Interpolation* s)
    : ParentStatement(std::move(pstate)), interpolation_(s), idxs_(0)
  {}
  StyleRule::StyleRule(SourceSpan&& pstate, Interpolation* s, sass::vector<StatementObj>&& els)
    : ParentStatement(std::move(pstate), std::move(els)), interpolation_(s), idxs_(0)
  {}

  bool CssStyleRule::is_invisible() const {
    bool sel_invisible = true;
    bool els_invisible = true;
    if (const SelectorList * sl = selector()) {
      for (auto child : sl->elements()) {
        if (!child->isInvisible()) {
          sel_invisible = false;
          break;
        }
      }
    }
    for (CssNode* item : elements()) {
      if (!item->is_invisible()) {
        els_invisible = false;
        break;
      }
    }

    return sel_invisible || els_invisible;
  }

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

  AtRule::AtRule(const SourceSpan& pstate, InterpolationObj name, InterpolationObj value, bool is_childless) :
    ParentStatement(pstate, sass::vector<StatementObj>{}),
    name_(name),
    value_(value),
    is_childless_(is_childless)
  {
  }

  // AtRule::AtRule(const AtRule* ptr, bool childless)
  // : ParentStatement(ptr, childless),
  //   name_(ptr->name_),
  //   value_(ptr->value_),
  //   is_childless_(ptr->is_childless_)
  // {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Declaration::Declaration(const SourceSpan& pstate, InterpolationObj name, ExpressionObj value, bool c)
    : ParentStatement(pstate), name_(name), value_(value), is_custom_property_(c)
  {}
  Declaration::Declaration(const SourceSpan& pstate, InterpolationObj name, ExpressionObj value, bool c, sass::vector<StatementObj>&& b)
    : ParentStatement(pstate, std::move(b)), name_(name), value_(value), is_custom_property_(c)
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
    const ImportBase* ptr, bool childless) :
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

  IncludeImport::IncludeImport(
    DynamicImport* import2,
    Include include) :
    DynamicImport(import2->pstate(), import2->url()),
    include_(include)
  {
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  ImportRule::ImportRule(
    const SourceSpan& pstate) :
    ImportBase(pstate),
    Vectorized()
  {}

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

  If::If(const SourceSpan& pstate, ExpressionObj pred, sass::vector<StatementObj>&& els, sass::vector<StatementObj>&& alt)
    : ParentStatement(pstate, std::move(els)), idxs_(0), predicate_(pred), alternatives_(std::move(alt))
  {}

  bool If::has_content()
  {
    if (ParentStatement::has_content()) return true;
    if (alternatives_.empty()) return false;
    for (size_t i = 0, L = alternatives_.size(); i < L; ++i) {
      if (alternatives_[i]->has_content()) return true;
    }
    return false;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  For::For(const SourceSpan& pstate,
    const EnvKey& var, ExpressionObj lo, ExpressionObj hi, bool inc)
    : ParentStatement(pstate),
    variable_(var), lower_bound_(lo), upper_bound_(hi), idxs_(0), is_inclusive_(inc)
  {}

  For::For(const SourceSpan& pstate,
    const EnvKey& var, ExpressionObj lo, ExpressionObj hi, bool inc, sass::vector<StatementObj>&& els)
    : ParentStatement(pstate, std::move(els)),
    variable_(var), lower_bound_(lo), upper_bound_(hi), idxs_(0), is_inclusive_(inc)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Each::Each(const SourceSpan& pstate, const sass::vector<EnvKey>& vars, ExpressionObj lst)
    : ParentStatement(pstate), variables_(vars), idxs_(0), list_(lst)
  {}
  Each::Each(const SourceSpan& pstate, const sass::vector<EnvKey>& vars, ExpressionObj lst, sass::vector<StatementObj>&& els)
    : ParentStatement(pstate, std::move(els)), variables_(vars), idxs_(0), list_(lst)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////
  WhileRule::WhileRule(
    const SourceSpan& pstate,
    ExpressionObj condition) :
    ParentStatement(pstate),
    condition_(condition),
    idxs_(0)
  {}
  WhileRule::WhileRule(
    const SourceSpan& pstate,
    ExpressionObj condition,
    sass::vector<StatementObj>&& b) :
    ParentStatement(pstate, std::move(b)),
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

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Expression::Expression(const SourceSpan& pstate)
  : Interpolant(pstate)
  { }

  Expression::Expression(SourceSpan&& pstate)
    : Interpolant(std::move(pstate))
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

  UnaryExpression::UnaryExpression(const SourceSpan& pstate, Type t, ExpressionObj o)
  : Expression(pstate), optype_(t), operand_(o)
  { }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Argument::Argument(const SourceSpan& pstate, ExpressionObj val, const EnvKey& n, bool rest, bool keyword)
  : Expression(pstate), value_(val), name_(n), is_rest_argument_(rest), is_keyword_argument_(keyword), hash_(0)
  {
  }
  // Argument::Argument(const Argument* ptr, bool childless)
  // : Expression(ptr),
  //   value_(ptr->value_),
  //   name_(ptr->name_),
  //   is_rest_argument_(ptr->is_rest_argument_),
  //   is_keyword_argument_(ptr->is_keyword_argument_),
  //   hash_(ptr->hash_)
  // {
  // }


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
  sass::string AtRootQuery::_nameFor(CssNode* node) const {
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

  bool AtRootQuery::excludes2312(CssParentNode* node) const
  {
    if (all()) return !include_;
    if (rule() && Cast<CssStyleRule>(node)) return !include_;
    return excludesName(_nameFor(node));
  }

  // Whether this excludes style rules.
  // Note that this takes [include] into account.
  bool AtRootQuery::excludesStyleRules() const {
    return (all() || rule()) != include();
  }

  AtRootQuery* AtRootQuery::parse(SourceData* source, Context& ctx)
  {
    AtRootQueryParser parser(ctx, source);
    return parser.parse();
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
    InterpolationObj query)
    : ParentStatement(pstate),
    query_(query),
    idxs_(0)
  {}
  AtRootRule::AtRootRule(
    const SourceSpan& pstate,
    InterpolationObj query,
    sass::vector<StatementObj>&& els)
    : ParentStatement(pstate, std::move(els)),
    query_(query),
    idxs_(0)
  {}

  AtRootRule::AtRootRule(
    SourceSpan&& pstate,
    InterpolationObj query)
    : ParentStatement(std::move(pstate)),
    query_(query),
    idxs_(0)
  {}
  AtRootRule::AtRootRule(
    SourceSpan&& pstate,
    InterpolationObj query,
    sass::vector<StatementObj>&& els)
    : ParentStatement(std::move(pstate), std::move(els)),
    query_(query),
    idxs_(0)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // If you forget to add a class here you will get
  // undefined reference to `vtable for Sass::Class'

  // IMPLEMENT_AST_OPERATORS(StyleRule);

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // ToDO: add move ctor
  ArgumentInvocation::ArgumentInvocation(const SourceSpan& pstate,
    sass::vector<ExpressionObj>&& positional,
    EnvKeyFlatMap<ExpressionObj>&& named,
    Expression* restArg,
    Expression* kwdRest) :
    AST_Node(pstate),
    positional_(std::move(positional)),
    named_(std::move(named)),
    restArg_(restArg),
    kwdRest_(kwdRest)
  {
  }

  ArgumentInvocation::ArgumentInvocation(SourceSpan&& pstate,
    sass::vector<ExpressionObj>&& positional,
    EnvKeyFlatMap<ExpressionObj>&& named,
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
  /*
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
  */
  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

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
    Context& context, SourceData* source)
  {
    ScssParser parser(context, source);
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
    sass::vector<StatementObj>&& els) :
    ParentStatement(pstate, std::move(els)),
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
    sass::vector<StatementObj>&& els) :
    CallableDeclaration(pstate,
      name, arguments, comment, std::move(els))
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
    sass::vector<StatementObj>&& els) :
    CallableDeclaration(pstate,
      name, arguments, comment, std::move(els))
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
    ContentBlock* content) :
    InvocationStatement(pstate, arguments),
    ns_(ns), name_(name), content_(content)
  {
  }

  bool IncludeRule::has_content() {
    return !content_.isNull();
  }

  ContentBlock::ContentBlock(
    const SourceSpan& pstate,
    ArgumentDeclaration* arguments) :
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
    struct SassFunction* function) :
    Callable(SourceSpan::tmp("[external]")),
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

}
