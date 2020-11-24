/*****************************************************************************/
/* Part of LibSass, released under the MIT license (See LICENSE.txt).        */
/*****************************************************************************/
#include "ast_statements.hpp"

#include "ast_supports.hpp"
#include "exceptions.hpp"
#include "compiler.hpp"

namespace Sass {

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  WithConfig::WithConfig(
    WithConfig* pwconfig,
    sass::vector<WithConfigVar> configs,
    bool hasConfig,
    bool hasShowFilter,
    bool hasHideFilter,
    std::set<EnvKey> filters,
    const sass::string& prefix) :
    parent(pwconfig),
    hasConfig(hasConfig),
    hasShowFilter(hasShowFilter),
    hasHideFilter(hasHideFilter),
    filters(filters),
    prefix(prefix)
  {

    // Only calculate this once
    doRAII = hasConfig || hasShowFilter
      || hasHideFilter || !prefix.empty();

    if (!doRAII) return;

    // Read the list of config variables into
    // a map and error if items are duplicated
    for (auto cfgvar : configs) {
      if (config.count(cfgvar.name) == 1) {
        // throw Exception::RuntimeException(compiler,
        //   "Defined Twice");
      }
      config[cfgvar.name] = cfgvar;
    }
    // Push the lookup table onto the stack
    // compiler.withConfigStack.push_back(this);
  }

  void WithConfig::finalize(Logger& logger)
  {
    // Check if everything was consumed
    for (auto cfgvar : config) {
      if (cfgvar.second.wasUsed == false) {
        if (cfgvar.second.isGuarded == false) {
          logger.addFinalStackTrace(cfgvar.second.pstate2);
          throw Exception::RuntimeException(logger, "$" +
            cfgvar.second.name + " was not declared "
            "with !default in the @used module.");
        }
      }
    }
  }

  WithConfig::~WithConfig()
  {
    // Do nothing if we don't have any config
    // Since we are used as a stack RAI object
    // this mode is very useful to ease coding
    if (!doRAII) return;
    // Then remove the config from the stack
    // compiler.withConfigStack.pop_back();
  }

  WithConfigVar* WithConfig::getCfgVar(const EnvKey& name)
  {

    EnvKey key(name);
    WithConfig* withcfg = this;
    while (withcfg) {
      // Check if we should apply any filtering first
      if (withcfg->hasHideFilter) {
        if (withcfg->filters.count(key.norm())) {
          break;
        }
      }
      if (withcfg->hasShowFilter) {
        if (!withcfg->filters.count(key.norm())) {
          break;
        }
      }
      // Then try to find the named item
      auto varcfg = withcfg->config.find(key);
      if (varcfg != withcfg->config.end()) {
        // Found an unguarded value
        if (!varcfg->second.isGuarded) {
          if (!varcfg->second.isNull) {
            varcfg->second.wasUsed = true;
            return &varcfg->second;
          }
        }
      }
      // Should we apply some prefixes
      if (!withcfg->prefix.empty()) {
        sass::string prefix = withcfg->prefix;
        key = EnvKey(prefix + key.orig());
      }
      withcfg = withcfg->parent;
    }
    // Since we found no unguarded value we can assume
    // the full stack only contains guarded values.
    // Therefore they overwrite all each other.
    withcfg = this; key = name;
    WithConfigVar* guarded = nullptr;
    while (withcfg) {
      // Check if we should apply any filtering first
      if (withcfg->hasHideFilter) {
        if (withcfg->filters.count(key.norm())) {
          break;
        }
      }
      if (withcfg->hasShowFilter) {
        if (!withcfg->filters.count(key.norm())) {
          break;
        }
      }
      // Then try to find the named item
      auto varcfg = withcfg->config.find(key);
      if (varcfg != withcfg->config.end()) {
        varcfg->second.wasUsed = true;
        if (!guarded) guarded = &varcfg->second;
      }
      // Should we apply some prefixes
      if (!withcfg->prefix.empty()) {
        sass::string prefix = withcfg->prefix;
        key = EnvKey(prefix + key.orig());
      }
      withcfg = withcfg->parent;
    }
    return guarded;
  }

  // Sass::WithConfigScope::WithConfigScope(
  //   Compiler& compiler,
  //   WithConfig* config) :
  //   compiler(compiler),
  //   config(config)
  // {
  //   if (config == nullptr) return;
  //   compiler.withConfigStack.push_back(config);
  // }
  // 
  // Sass::WithConfigScope::~WithConfigScope()
  // {
  //   if (config == nullptr) return;
  //   compiler.withConfigStack.pop_back();
  // }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  ParentStatement::ParentStatement(
    SourceSpan&& pstate,
    StatementVector&& children,
    VarRefs* idxs) :
    Statement(std::move(pstate)),
    Vectorized<Statement>(std::move(children)),
    Env(idxs)
  {}

  // Returns whether we have a child content block
  bool ParentStatement::hasContent() const
  {
    if (Statement::hasContent()) return true;
    for (const StatementObj& child : elements_) {
      if (child->hasContent()) return true;
    }
    return false;
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  StyleRule::StyleRule(
    SourceSpan&& pstate,
    Interpolation* interpolation,
    VarRefs* idxs,
    StatementVector&& children) :
    ParentStatement(
      std::move(pstate),
      std::move(children),
      idxs),
    interpolation_(interpolation)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  Declaration::Declaration(
    SourceSpan&& pstate,
    Interpolation* name,
    Expression* value,
    bool is_custom_property,
    StatementVector&& children) :
    ParentStatement(
      std::move(pstate),
      std::move(children),
      nullptr),
    name_(name),
    value_(value),
    is_custom_property_(is_custom_property)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  ForRule::ForRule(
    SourceSpan&& pstate,
    const EnvKey& varname,
    Expression* lower_bound,
    Expression* upper_bound,
    bool is_inclusive,
    VarRefs* idxs,
    StatementVector&& children) :
    ParentStatement(
      std::move(pstate),
      std::move(children),
      idxs),
    varname_(varname),
    lower_bound_(lower_bound),
    upper_bound_(upper_bound),
    is_inclusive_(is_inclusive)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  EachRule::EachRule(
    SourceSpan&& pstate,
    const EnvKeys& variables,
    Expression* expressions,
    VarRefs* idxs,
    StatementVector&& children) :
    ParentStatement(
      std::move(pstate),
      std::move(children),
      idxs),
    variables_(variables),
    expressions_(expressions)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  WhileRule::WhileRule(
    SourceSpan&& pstate,
    Expression* condition,
    VarRefs* idxs,
    StatementVector&& children) :
    ParentStatement(
      std::move(pstate),
      std::move(children),
      idxs),
    condition_(condition)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  MediaRule::MediaRule(
    SourceSpan&& pstate,
    Interpolation* query,
    VarRefs* idxs,
    StatementVector&& children) :
    ParentStatement(
      std::move(pstate),
      std::move(children),
      idxs),
    query_(query)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  AtRule::AtRule(
    SourceSpan&& pstate,
    Interpolation* name,
    Interpolation* value,
    VarRefs* idxs,
    bool isChildless,
    StatementVector&& children) :
    ParentStatement(
      std::move(pstate),
      std::move(children),
      idxs),
    name_(name),
    value_(value),
    isChildless_(isChildless)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  AtRootRule::AtRootRule(
    SourceSpan&& pstate,
    Interpolation* query,
    VarRefs* idxs,
    StatementVector&& children) :
    ParentStatement(
      std::move(pstate),
      std::move(children),
      idxs),
    query_(query)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  IfRule::IfRule(
    SourceSpan&& pstate,
    VarRefs* idxs,
    StatementVector&& children,
    Expression* predicate,
    IfRule* alternative) :
    ParentStatement(
      std::move(pstate),
      std::move(children),
      idxs),
    predicate_(predicate),
    alternative_(alternative)
  {}

  // Also check alternative for content block
  bool IfRule::hasContent() const
  {
    if (ParentStatement::hasContent()) return true;
    return alternative_ && alternative_->hasContent();
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  SupportsRule::SupportsRule(
    SourceSpan&& pstate,
    SupportsCondition* condition,
    VarRefs* idxs,
    StatementVector&& children) :
    ParentStatement(
      std::move(pstate),
      std::move(children),
      idxs),
    condition_(condition)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  CallableDeclaration::CallableDeclaration(
    SourceSpan&& pstate,
    const EnvKey& name,
    ArgumentDeclaration* arguments,
    StatementVector&& children,
    SilentComment* comment,
    VarRefs* idxs) :
    ParentStatement(
      std::move(pstate),
      std::move(children),
      idxs),
    name_(name),
    comment_(comment),
    arguments_(arguments)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  IncludeRule::IncludeRule(
    SourceSpan&& pstate,
    const EnvKey& name,
    const sass::string& ns,
    ArgumentInvocation* arguments,
    ContentBlock* content) :
    Statement(std::move(pstate)),
    CallableInvocation(arguments),
    ns_(ns),
    name_(name),
    content_(content)
  {}

  bool IncludeRule::hasContent() const
  {
    return !content_.isNull();
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  ContentBlock::ContentBlock(
    SourceSpan&& pstate,
    ArgumentDeclaration* arguments,
    VarRefs* idxs,
    StatementVector&& children,
    SilentComment* comment) :
    CallableDeclaration(
      std::move(pstate),
      Keys::contentRule,
      arguments,
      std::move(children),
      comment, idxs)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  FunctionRule::FunctionRule(
    SourceSpan&& pstate,
    const EnvKey& name,
    ArgumentDeclaration* arguments,
    VarRefs* idxs,
    StatementVector&& children,
    SilentComment* comment) :
    CallableDeclaration(
      std::move(pstate),
      name, arguments,
      std::move(children),
      comment, idxs)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  MixinRule::MixinRule(
    SourceSpan&& pstate,
    const sass::string& name,
    ArgumentDeclaration* arguments,
    VarRefs* idxs,
    StatementVector&& children,
    SilentComment* comment) :
    CallableDeclaration(
      std::move(pstate),
      name, arguments,
      std::move(children),
      comment, idxs)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  WarnRule::WarnRule(
    SourceSpan&& pstate,
    Expression* expression) :
    Statement(std::move(pstate)),
    expression_(expression)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  ErrorRule::ErrorRule(
    SourceSpan&& pstate,
    Expression* expression) :
    Statement(std::move(pstate)),
    expression_(expression)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  DebugRule::DebugRule(
    SourceSpan&& pstate,
    Expression* expression) :
    Statement(std::move(pstate)),
    expression_(expression)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  ReturnRule::ReturnRule(
    SourceSpan&& pstate,
    Expression* value) :
    Statement(std::move(pstate)),
    value_(value)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  ContentRule::ContentRule(
    SourceSpan&& pstate,
    ArgumentInvocation* arguments) :
    Statement(std::move(pstate)),
    arguments_(arguments)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  ExtendRule::ExtendRule(
    SourceSpan&& pstate,
    Interpolation* selector,
    bool is_optional) :
    Statement(std::move(pstate)),
    selector_(selector),
    is_optional_(is_optional)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  LoudComment::LoudComment(
    SourceSpan&& pstate,
    Interpolation* text) :
    Statement(std::move(pstate)),
    text_(text)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  SilentComment::SilentComment(
    SourceSpan&& pstate,
    sass::string&& text) :
    Statement(pstate),
    text_(text)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  ImportRule::ImportRule(
    const SourceSpan& pstate) :
    Statement(pstate)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  UseRule::UseRule(
    const SourceSpan& pstate,
    const sass::string& prev,
    const sass::string& url,
    Import* import,
    WithConfig* pwconfig,
    sass::vector<WithConfigVar>&& config,
    bool hasLocalWith) :
    Statement(pstate),
    import_(import),
    prev_(prev),
    url_(url),
    hasLocalWith_(hasLocalWith),
    waxExported_(false),
    config_(std::move(config)),
    wconfig_(new WithConfig(pwconfig,
      config_, hasLocalWith_)),
    module_(nullptr)
  {
    // ;
  }

  ForwardRule::ForwardRule(
    const SourceSpan& pstate,
    const sass::string& prev,
    const sass::string& url,
    Import* import,
    const sass::string& prefix,
    WithConfig* pwconfig,
    std::set<EnvKey>&& toggledVariables,
    std::set<EnvKey>&& toggledCallables,
    sass::vector<WithConfigVar>&& config,
    bool isShown,
    bool isHidden,
    bool hasWith) :
    Statement(pstate),
    import_(import),
    prev_(prev),
    url_(url),
    prefix_(prefix),
    isShown_(isShown),
    isHidden_(isHidden),
    hasLocalWith_(hasWith),
    wasMerged_(false),
    toggledVariables2_(std::move(toggledVariables)),
    toggledCallables_(std::move(toggledCallables)),
    config_(std::move(config)),
    wconfig(new WithConfig(pwconfig,
      config_, hasLocalWith_,
      isShown_, isHidden_,
      toggledVariables2_,
      prefix_)),
    module_(nullptr)
  {
    // The show or hide config also hides these
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  AssignRule::AssignRule(
    const SourceSpan& pstate,
    const EnvKey& variable,
    bool withinLoop,
    const sass::string ns,
    sass::vector<VarRef> vidxs,
    Expression* value,
    bool is_default,
    bool is_global) :
    Statement(pstate),
    variable_(variable),
    ns_(ns),
    value_(value),
    vidxs_(std::move(vidxs)),
    withinLoop_(withinLoop),
    is_default_(is_default),
    is_global_(is_global)
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

}

