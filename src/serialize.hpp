#ifndef SASS_SERIALIZE_H
#define SASS_SERIALIZE_H

#include "character.hpp"
#include "interpolation.hpp"
#include "visitor_css.hpp"
#include "visitor_value.hpp"
#include "visitor_selector.hpp"

namespace Sass {

  // Import some namespaces
  using namespace Charcode;
  using namespace Character;

  // Converts [selector] to a CSS string. If [inspect] is `true`, this will emit an
  // unambiguous representation of the source structure. Note however that, although
  // this will be valid SCSS, it may not be valid CSS. If [inspect] is `false` and
  // [selector] can't be represented in plain CSS, throws a [SassScriptException].
  std::string serializeSelector(Selector* selector, bool inspect = false);


  class SerializeVisitor :
    public CssVisitor<void>,
    public ValueVisitor<void>,
    public SelectorVisitor<void>
  {

    /// A buffer that contains the CSS produced so far.
    StringBuffer _buffer;

    /// The current indentation of the CSS output.
    size_t _indentation = 0;

    /// The style of CSS to generate.
    // OutputStyle _style;

    // Whether we're emitting an unambiguous representation
    // of the source structure, as opposed to valid CSS.
    bool _inspect;

    // Whether quoted strings should be emitted with quotes.
    // bool _quote;

    // Whether we're emitting compressed output.
    bool _isCompressed; // = > _style == OutputStyle.compressed;

    // The number of spaces or tabs to be used for indentation.
    // std::string _indenter;

    uint8_t _indentCharacter;

    // The characters to use for a line feed.
    std::string _lineFeed;

    size_t _indentWidth;

  public:

    SerializeVisitor(
      bool inspect = false,
      bool quote = true,
      bool useSpaces = true);

    std::string getString()
    {
      return _buffer.toString();
    }

  private:

    template <typename T>
    void _writeBetween(
      std::vector<T> iterable,
      std::string joiner,
      void(*cb)(T& value))
    {
      bool first = true;
      for (T& value : iterable) {
        if (first) {
          first = false;
        }
        else {
          // _buffer.write(text);
        }
        cb(value);
      }
    }

  public:

    // Writes a line feed, unless this emitting compressed CSS.
    void _writeLineFeed() {
      if (_isCompressed) return;
      _buffer.write(_lineFeed);
    }

    // Writes a space unless [_style] is [OutputStyle.compressed].
    void _writeOptionalSpace() {
      if (_isCompressed) return;
      _buffer.write($space);
    }

    // Writes indentation based on [_indentation].
    void writeIndentation() {
      if (_isCompressed) return;
      _writeTimes(_indentCharacter,
        _indentation * _indentWidth);
    }

    // Writes [char] to [_buffer] with [times] repetitions.
    void _writeTimes(uint8_t chr, size_t times) {
      for (size_t i = 0; i < times; i++) {
        _buffer.write(chr);
      }
    }

  public:

    void visitBoolean(Boolean* value) override;
    bool _isSymmetricalHex(uint8_t color);
    bool _canUseShortHex(Color* color);
    void visitColor(Color* value) override;

    void visitList(List* value) override;
    void visitMap(Map* value) override;
    void visitNull(Null* value) override;
    void visitNumber(Number* value) override;

    // void visitCssImport(CssImport* node) override;
   // void visitCssMediaRule(CssMediaRule* node) override;

    void visitAttributeSelector(AttributeSelector* attribute) override;
    void visitClassSelector(ClassSelector* klass) override;
    void visitComplexSelector(ComplexSelector* complex) override;
    bool _omitSpacesAround(SelectorComponent* component);
    void visitCompoundSelector(CompoundSelector* compound) override;
    void visitSelectorCombinator(SelectorCombinator* combinator) override;
    void visitIDSelector(IDSelector* id) override;
    void visitPlaceholderSelector(PlaceholderSelector* placeholder) override;
    void visitPseudoSelector(PseudoSelector* pseudo) override;
    void visitSelectorList(SelectorList* list) override;
    void visitTypeSelector(TypeSelector* type) override;

  };

}

#endif
