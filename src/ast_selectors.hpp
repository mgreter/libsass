#ifndef SASS_AST_SEL_H
#define SASS_AST_SEL_H

// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"
#include "ast.hpp"

#include "visitor_selector.hpp"
#include "strings.hpp"

namespace Sass {

  /////////////////////////////////////////////////////////////////////////
  // Some helper functions
  /////////////////////////////////////////////////////////////////////////

  bool compoundIsSuperselector(
    const CompoundSelectorObj& compound1,
    const CompoundSelectorObj& compound2,
    const sass::vector<SelectorComponentObj>& parents);

  bool complexIsParentSuperselector(
    const sass::vector<SelectorComponentObj>& complex1,
    const sass::vector<SelectorComponentObj>& complex2);

  sass::vector<sass::vector<SelectorComponentObj>> weave(
    const sass::vector<sass::vector<SelectorComponentObj>>& complexes);

  // ToDo: What happens if we modify our parent?
  sass::vector<sass::vector<SelectorComponentObj>> weaveParents(
    sass::vector<SelectorComponentObj> parents1,
      sass::vector<SelectorComponentObj> parents2);

  sass::vector<SimpleSelectorObj> unifyCompound(
    const sass::vector<SimpleSelectorObj>& compound1,
    const sass::vector<SimpleSelectorObj>& compound2);

  sass::vector<sass::vector<SelectorComponentObj>> unifyComplex(
    const sass::vector<sass::vector<SelectorComponentObj>>& complexes);

  /////////////////////////////////////////
  // Abstract base class for CSS selectors.
  /////////////////////////////////////////
  class Selector : public AST_Node {
  protected:
    mutable size_t hash_;
  public:
    Selector(const SourceSpan& pstate);
    virtual ~Selector() = 0;
    size_t hash() const override = 0;
    virtual bool has_real_parent_ref() const;
    virtual bool is_invisible() const { return false; }
    // you should reset this to null on containers
    virtual unsigned long specificity() const = 0;
    // by default we return the regular specificity
    // you must override this for all containers
    virtual size_t maxSpecificity() const { return specificity(); }
    virtual size_t minSpecificity() const { return specificity(); }
    // Calls the appropriate visit method on [visitor].
    virtual void accept(SelectorVisitor<void>& visitor) = 0;
    ATTACH_VIRTUAL_COPY_OPERATIONS(Selector)
  };
  inline Selector::~Selector() { }

  ////////////////////////////////////////////
  // Abstract base class for simple selectors.
  ////////////////////////////////////////////
  class SimpleSelector : public Selector {
  public:
    enum Simple_Type {
      ID_SEL,
      TYPE_SEL,
      CLASS_SEL,
      PSEUDO_SEL,
      ATTRIBUTE_SEL,
      PLACEHOLDER_SEL,
    };
  public:
    HASH_CONSTREF(sass::string, ns);
    HASH_CONSTREF(sass::string, name);
    ADD_PROPERTY(Simple_Type, simple_type);
    HASH_PROPERTY(bool, has_ns);
  public:
    SimpleSelector(const SourceSpan& pstate, const sass::string& n = Strings::empty);
    virtual sass::string ns_name() const;
    size_t hash() const override;
    virtual bool empty() const;
    // namespace compare functions
    bool is_ns_eq(const SimpleSelector& r) const;
    // namespace query functions
    bool is_universal_ns() const;
    // name query functions
    bool is_universal() const;
    virtual bool has_placeholder();

    virtual ~SimpleSelector() = 0;
    virtual CompoundSelector* unifyWith(CompoundSelector*);

    /* helper function for syntax sugar */
    virtual IDSelector* getIdSelector() { return NULL; }
    virtual TypeSelector* getTypeSelector() { return NULL; }
    virtual PseudoSelector* getPseudoSelector() { return NULL; }

    ComplexSelectorObj wrapInComplex();
    CompoundSelectorObj wrapInCompound();

    virtual bool isInvisible() const { return false; }
    // virtual bool is_pseudo_element() const;
    virtual bool has_real_parent_ref() const override;

    ABSTRACT_EQ_OPERATIONS(SimpleSelector);
    ATTACH_VIRTUAL_COPY_OPERATIONS(SimpleSelector);
    ATTACH_CRTP_PERFORM_METHODS();

  };
  inline SimpleSelector::~SimpleSelector() { }

  /////////////////////////////////////////////////////////////////////////////
  // A placeholder selector. (e.g. `%foo`). This doesn't match any elements.
  // It's intended to be extended using `@extend`. It's not a plain CSS
  // selector — it should be removed before emitting a CSS document.
  /////////////////////////////////////////////////////////////////////////////
  class PlaceholderSelector final : public SimpleSelector {
  public:
    PlaceholderSelector(const SourceSpan& pstate, const sass::string& n);
    bool isInvisible() const override { return true; }
    virtual unsigned long specificity() const override;
    virtual bool has_placeholder() override;
    bool operator==(const SimpleSelector& rhs) const override;

    // Returns whether this is a private selector.
    // That is, whether it begins with `-` or `_`.
    bool isPrivate() const;

    // Calls the appropriate visit method on [visitor].
    void accept(SelectorVisitor<void>& visitor) override {
      return visitor.visitPlaceholderSelector(this);
    }

    VIRTUAL_EQ_OPERATIONS(PlaceholderSelector)
    ATTACH_COPY_OPERATIONS(PlaceholderSelector)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  /////////////////////////////////////////////////////////////////////
  // A type selector. (e.g., `div`, `span` or `*`).
  // This selects elements whose name equals the given name.
  /////////////////////////////////////////////////////////////////////
  class TypeSelector final : public SimpleSelector {
  public:
    TypeSelector(const SourceSpan& pstate, const sass::string& n);
    virtual unsigned long specificity() const override;
    SimpleSelector* unifyWith(const SimpleSelector*);
    CompoundSelector* unifyWith(CompoundSelector*) override;
    TypeSelector* getTypeSelector() override { return this; }
    bool operator==(const SimpleSelector& rhs) const final override;

    // Calls the appropriate visit method on [visitor].
    void accept(SelectorVisitor<void>& visitor) override {
      return visitor.visitTypeSelector(this);
    }

    VIRTUAL_EQ_OPERATIONS(TypeSelector)
    ATTACH_COPY_OPERATIONS(TypeSelector)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  /////////////////////////////////////////////////////////////////////
  // Class selectors  -- i.e., .foo.
  /////////////////////////////////////////////////////////////////////
  class ClassSelector final : public SimpleSelector {
  public:
    ClassSelector(const SourceSpan& pstate, const sass::string& name);
    virtual unsigned long specificity() const override;
    bool operator==(const SimpleSelector& rhs) const final override;

    // Calls the appropriate visit method on [visitor].
    void accept(SelectorVisitor<void>& visitor) override {
      return visitor.visitClassSelector(this);
    }

    VIRTUAL_EQ_OPERATIONS(ClassSelector)
    ATTACH_COPY_OPERATIONS(ClassSelector)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  /////////////////////////////////////////////////////////
  // An ID selector (i.e. `#foo`). This selects elements 
  // whose `id` attribute exactly matches the given name.
  /////////////////////////////////////////////////////////
  class IDSelector final : public SimpleSelector {
  public:
    IDSelector(const SourceSpan& pstate, const sass::string& name);
    virtual unsigned long specificity() const override;
    CompoundSelector* unifyWith(CompoundSelector*) override;
    IDSelector* getIdSelector() final override { return this; }
    bool operator==(const SimpleSelector& rhs) const final override;

    // Calls the appropriate visit method on [visitor].
    void accept(SelectorVisitor<void>& visitor) override {
      return visitor.visitIDSelector(this);
    }

    VIRTUAL_EQ_OPERATIONS(IDSelector)
    ATTACH_COPY_OPERATIONS(IDSelector)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  /////////////////////////////////////////////////////////
  // An attribute selector. This selects for elements
  // with the given attribute, and optionally with a
  // value matching certain conditions as well.
  /////////////////////////////////////////////////////////
  class AttributeSelector final : public SimpleSelector {

    // The operator that defines the semantics of [value].
    // If this is empty, this matches any element with the given property,
    // regardless of this value. It's empty if and only if [value] is empty.
    ADD_CONSTREF(sass::string, op);

    // An assertion about the value of [name].
    // The precise semantics of this string are defined by [op].
    // If this is `null`, this matches any element with the given property,
    // regardless of this value. It's `null` if and only if [op] is `null`.
    ADD_CONSTREF(sass::string, value);

      // The modifier which indicates how the attribute selector should be
    // processed. See for example [case-sensitivity][] modifiers.
    // [case-sensitivity]: https://www.w3.org/TR/selectors-4/#attribute-case
    // If [op] is empty, this is always empty as well.
    ADD_PROPERTY(char, modifier);

    // Defines if we parsed an identifier value. Dart-sass
    // does this check again in serialize.visitAttributeSelector.
    // We want to avoid this and do the check at parser stage.
    ADD_PROPERTY(bool, isIdentifier);

  public:

    // By value constructor
    AttributeSelector(
      const SourceSpan& pstate,
      const sass::string& name,
      const sass::string& op,
      const sass::string& value,
      char modifier = 0);

    virtual unsigned long specificity() const override;

    bool operator==(const SimpleSelector& rhs) const final override;

    // Calls the appropriate visit method on [visitor].
    void accept(SelectorVisitor<void>& visitor) override {
      return visitor.visitAttributeSelector(this);
    }

    VIRTUAL_EQ_OPERATIONS(AttributeSelector)
    ATTACH_COPY_OPERATIONS(AttributeSelector)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  /////////////////////////////////////////////////////////////////////
  // A pseudo-class or pseudo-element selector (e.g., `:content`
  // or `:nth-child`). The semantics of a specific pseudo selector
  // depends on its name. Some selectors take arguments, including
  // other selectors. Sass manually encodes logic for each pseudo
  // selector that takes a selector as an argument, to ensure that
  // extension and other selector operations work properly.
  /////////////////////////////////////////////////////////////////////
  class PseudoSelector final : public SimpleSelector {

    // Like [name], but without any vendor prefixes.
    ADD_CONSTREF(sass::string, normalized);

    // The non-selector argument passed to this selector. This is
    // `null` if there's no argument. If [argument] and [selector]
    // are both non-`null`, the selector follows the argument.
    ADD_CONSTREF(sass::string, argument);

    // The selector argument passed to this selector. This is `null`
    // if there's no selector. If [argument] and [selector] are
    // both non-`null`, the selector follows the argument.
    ADD_PROPERTY(SelectorListObj, selector);

    // Whether this is syntactically a pseudo-class selector. This is
    // the same as [isClass] unless this selector is a pseudo-element
    // that was written syntactically as a pseudo-class (`:before`,
    // `:after`, `:first-line`, or `:first-letter`). This is
    // `true` if and only if [isSyntacticElement] is `false`.
    ADD_PROPERTY(bool, isSyntacticClass);

    // Whether this is a pseudo-class selector.
    // This is `true` if and only if [isElement] is `false`.
    ADD_PROPERTY(bool, isClass);

  public:

    // By value constructor
    PseudoSelector(
      const SourceSpan& pstate,
      const sass::string& name,
      bool element = false);

    size_t hash() const override;
    bool empty() const override;
    bool is_invisible() const override;
    bool has_real_parent_ref() const override;
    bool is_pseudo_element() const;

    // Whether this is a pseudo-element selector.
    // This is `true` if and only if [isClass] is `false`.
    bool isElement() const { return !isClass(); }

    // Whether this is syntactically a pseudo-element selector.
    // This is `true` if and only if [isSyntacticClass] is `false`.
    bool isSyntacticElement() const { return !isSyntacticClass(); }

    // Returns a new [PseudoSelector] based on ourself,
    // but with the selector replaced with [selector].
    PseudoSelector* withSelector(SelectorList* selector);

    virtual unsigned long specificity() const override;
    CompoundSelector* unifyWith(CompoundSelector*) override;
    PseudoSelector* getPseudoSelector() final override { return this; }
    bool operator==(const SimpleSelector& rhs) const final override;

    // Calls the appropriate visit method on [visitor].
    void accept(SelectorVisitor<void>& visitor) override {
      return visitor.visitPseudoSelector(this);
    }

    VIRTUAL_EQ_OPERATIONS(PseudoSelector)
    ATTACH_COPY_OPERATIONS(PseudoSelector)
    ATTACH_CRTP_PERFORM_METHODS()
  };


  ////////////////////////////////////////////////////////////////////////////
  // Complex Selectors are the most important class of selectors.
  // A Selector List consists of Complex Selectors (separated by comma)
  // Complex Selectors are itself a list of Compounds and Combinators
  // Between each item there is an implicit ancestor of combinator
  ////////////////////////////////////////////////////////////////////////////
  class ComplexSelector final : public Selector, public Vectorized<SelectorComponentObj> {
    ADD_PROPERTY(bool, chroots)
    // line break before list separator
    ADD_PROPERTY(bool, hasPreLineFeed)
  public:
    ComplexSelector(const SourceSpan& pstate);

    // Returns true if the first components
    // is a compound selector and fullfills
    // a few other criterias.
    bool isInvisible() const;

    size_t hash() const override;
    void cloneChildren() override;
    bool has_placeholder() const;
    bool has_real_parent_ref() const override;

    sass::vector<ComplexSelectorObj> resolveParentSelectors(SelectorList* parent, Backtraces& traces, bool implicit_parent = true);
    virtual unsigned long specificity() const override;

    SelectorList* unifyWith(ComplexSelector* rhs);

    bool isSuperselectorOf(const ComplexSelector* sub) const;

    SelectorListObj wrapInList();

    size_t maxSpecificity() const override;
    size_t minSpecificity() const override;

    // Calls the appropriate visit method on [visitor].
    void accept(SelectorVisitor<void>& visitor) override {
      return visitor.visitComplexSelector(this);
    }

    VIRTUAL_EQ_OPERATIONS(ComplexSelector)
    ATTACH_COPY_OPERATIONS(ComplexSelector)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ////////////////////////////////////////////////////////////////////////////
  // Base class for complex selector components
  ////////////////////////////////////////////////////////////////////////////
  class SelectorComponent : public Selector {
    // line break after list separator
    ADD_PROPERTY(bool, hasPostLineBreak)
  public:
    SelectorComponent(const SourceSpan& pstate, bool postLineBreak = false);
    size_t hash() const override = 0;
    void cloneChildren() override;

    // By default we consider instances not empty
    virtual bool empty() const { return false; }

    virtual bool has_placeholder() const = 0;
    bool has_real_parent_ref() const override = 0;

    ComplexSelector* wrapInComplex();

    size_t maxSpecificity() const override { return 0; }
    size_t minSpecificity() const override { return 0; }

    virtual bool isCompound() const { return false; };
    virtual bool isCombinator() const { return false; };

    /* helper function for syntax sugar */
    virtual CompoundSelector* getCompound() { return NULL; }
    virtual SelectorCombinator* getCombinator() { return NULL; }
    virtual const CompoundSelector* getCompound() const { return NULL; }
    virtual const SelectorCombinator* getCombinator() const { return NULL; }

    virtual unsigned long specificity() const override;
    ABSTRACT_EQ_OPERATIONS(SelectorComponent);
    ATTACH_VIRTUAL_COPY_OPERATIONS(SelectorComponent);
  };

  ////////////////////////////////////////////////////////////////////////////
  // A specific combinator between compound selectors
  ////////////////////////////////////////////////////////////////////////////
  class SelectorCombinator final : public SelectorComponent {
  public:

    // Enumerate all possible selector combinators. There is some
    // discrepancy with dart-sass. Opted to name them as in CSS33
    enum Combinator { CHILD /* > */, GENERAL /* ~ */, ADJACENT /* + */};

  private:

    // Store the type of this combinator
    HASH_CONSTREF(Combinator, combinator)

  public:
    SelectorCombinator(const SourceSpan& pstate, Combinator combinator, bool postLineBreak = false);

    bool has_real_parent_ref() const override { return false; }
    bool has_placeholder() const override { return false; }

    /* helper function for syntax sugar */
    SelectorCombinator* getCombinator() final override { return this; }
    const SelectorCombinator* getCombinator() const final override { return this; }

    // Query type of combinator
    bool isCombinator() const override { return true; };

    // Matches the right-hand selector if it's a direct child of the left-
    // hand selector in the DOM tree. Dart-sass also calls this `child`
    // https://developer.mozilla.org/en-US/docs/Web/CSS/Child_combinator
    bool isChildCombinator() const { return combinator_ == CHILD; } // >

    // Matches the right-hand selector if it comes after the left-hand
    // selector in the DOM tree. Dart-sass class this `followingSibling`
    // https://developer.mozilla.org/en-US/docs/Web/CSS/General_sibling_combinator
    bool isGeneralCombinator() const { return combinator_ == GENERAL; } // ~

    // Matches the right-hand selector if it's immediately adjacent to the
    // left-hand selector in the DOM tree. Dart-sass calls this `nextSibling`
    // https://developer.mozilla.org/en-US/docs/Web/CSS/Adjacent_sibling_combinator
    bool isAdjacentCombinator() const { return combinator_ == ADJACENT; } // +

    size_t maxSpecificity() const override { return 0; }
    size_t minSpecificity() const override { return 0; }

    size_t hash() const override {
      return std::hash<int>()(combinator_);
    }

    virtual unsigned long specificity() const override;
    bool operator==(const SelectorComponent& rhs) const override;


    // Calls the appropriate visit method on [visitor].
    void accept(SelectorVisitor<void>& visitor) override {
      return visitor.visitSelectorCombinator(this);
    }

    VIRTUAL_EQ_OPERATIONS(SelectorCombinator)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ////////////////////////////////////////////////////////////////////////////
  // A compound selector consists of multiple simple selectors
  ////////////////////////////////////////////////////////////////////////////
  class CompoundSelector final : public SelectorComponent, public Vectorized<SimpleSelectorObj> {
    ADD_PROPERTY(bool, hasRealParent)
    ADD_PROPERTY(bool, extended)
  public:
    CompoundSelector(const SourceSpan& pstate, bool postLineBreak = false);

    // Returns true if this compound selector
    // fullfills various criterias.
    bool isInvisible() const;

    bool empty() const override {
      return Vectorized::empty();
    }

    size_t hash() const override;
    CompoundSelector* unifyWith(CompoundSelector* rhs);

    /* helper function for syntax sugar */
    CompoundSelector* getCompound() final override { return this; }
    const CompoundSelector* getCompound() const final override { return this; }

    bool isSuperselectorOf(const CompoundSelector* sub, sass::string wrapped = "") const;

    void cloneChildren() override;
    bool has_real_parent_ref() const override;
    bool has_placeholder() const override;
    sass::vector<ComplexSelectorObj> resolveParentSelectors(SelectorList* parent, Backtraces& traces, bool implicit_parent = true);

    virtual bool isCompound() const override { return true; };
    virtual unsigned long specificity() const override;

    size_t maxSpecificity() const override;
    size_t minSpecificity() const override;

    bool operator==(const SelectorComponent& rhs) const override;

    // Calls the appropriate visit method on [visitor].
    void accept(SelectorVisitor<void>& visitor) override {
      return visitor.visitCompoundSelector(this);
    }

    VIRTUAL_EQ_OPERATIONS(CompoundSelector)
    ATTACH_COPY_OPERATIONS(CompoundSelector)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ///////////////////////////////////
  // Comma-separated selector groups.
  ///////////////////////////////////
  class SelectorList final : public Selector, public Vectorized<ComplexSelectorObj> {
  public:
    SelectorList(const SourceSpan& pstate, size_t s = 0);
    SelectorList(const SourceSpan& pstate, const sass::vector<ComplexSelectorObj>&);
    // sass::string type() const override { return "list"; }
    size_t hash() const override;

    SelectorList* unifyWith(SelectorList*);

    // SassList* asSassList()
    // {
    // }

    // Returns true if all complex selectors
    // can have real parents, meaning every
    // first component does allow for it
    bool isInvisible() const;

    void cloneChildren() override;
    bool has_real_parent_ref() const override;

    SelectorList* resolveParentSelectors(SelectorList* parent, Backtraces& traces, bool implicit_parent = true);
    virtual unsigned long specificity() const override;

    bool isSuperselectorOf(const SelectorList* sub) const;

    size_t maxSpecificity() const override;
    size_t minSpecificity() const override;

    // Calls the appropriate visit method on [visitor].
    void accept(SelectorVisitor<void>& visitor) override {
      return visitor.visitSelectorList(this);
    }

    VIRTUAL_EQ_OPERATIONS(SelectorList)
    ATTACH_COPY_OPERATIONS(SelectorList)
    ATTACH_CRTP_PERFORM_METHODS()
  };

  ////////////////////////////////
  // The Sass `@extend` directive.
  ////////////////////////////////
  class ExtendRule final : public Statement {
    ADD_PROPERTY(bool, isOptional)
    // This should be a simple selector only!
    ADD_PROPERTY(InterpolationObj, selector)
  public:
    ExtendRule(const SourceSpan& pstate, InterpolationObj s, bool optional = false);
    VIRTUAL_EQ_OPERATIONS(ExtendRule)
    ATTACH_CRTP_PERFORM_METHODS()
  };

}

#endif
