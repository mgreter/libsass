#include "sass.hpp"
#include "ast.hpp"

#include <sstream>

#include "debugger.hpp"


#include "interpolation.hpp"
#include "utf8/checked.h"

namespace Sass {

  void StringBuffer::writeCharCode(uint32_t character)
  {
    utf8::append(character, std::back_inserter(buffer));
  }

  void InterpolationBuffer::writeCharCode(uint32_t character)
  {
    text.writeCharCode(character);
  }

  void InterpolationBuffer::write(const StringLiteral* str)
  {
    text.write(str->text(), str->pstate());
  }


  void StringBuffer::write(const StringLiteral* string)
  {
    buffer += string->text();
  }

  Interpolation* InterpolationBuffer::getInterpolation(const SourceSpan& pstate, bool rtrim)
  {
    InterpolationObj itpl = SASS_MEMORY_NEW(Interpolation, pstate);
    for (ValueObj item : contents) {
      // Appends an Expression
      itpl->append(item);
    }
    if (!text.empty()) {
      // Appends a StringLiteral from the remaining text in the string buffer
      itpl->append(SASS_MEMORY_NEW(StringLiteral, pstate, text.toString(rtrim)));
    }
    return itpl.detach();
  }

  // Flushes [text] to [contents] if necessary.
  void InterpolationBuffer::flushText()
  {
    // Do nothing if text is empty
    if (text.empty()) return;
    auto qwe = text.toString();
    
      // Create a string constant and do not touch the input that we feed it
    contents.emplace_back(SASS_MEMORY_NEW(StringLiteral, pstate, qwe));
    // Clear buffer now
    text.clear();
  }

  void InterpolationBuffer::addInterpolation(const InterpolationObj schema)
  {
    if (schema->empty()) return;

    auto& elements = schema->elements();
    auto addStart = elements.begin();
    auto addEnd = elements.end();

    if (StringLiteral* str = Cast<StringLiteral>(elements[0])) {
      text.write(str);
      addStart += 1;
    }

    /*
    Iterable toAdd = interpolation.contents;
    var first = interpolation.contents.first;
    if (first is String) {
    _text.write(first);
    toAdd = interpolation.contents.skip(1);
    }
    */

    flushText();

    // There is an item at the end
    if (addStart != addEnd) {
      auto& last = *(addEnd - 1);
      auto str = Cast<StringLiteral>(last);
      if (str != nullptr) addEnd -= 1;
      // Add the rest if any left
      contents.insert(contents.end(),
        addStart, addEnd);
      if (str) {
        text.write(
          str->text(),
          str->pstate());
      }
    }

    if (auto str = Cast<StringLiteral>(contents.back())) {
      text.write(
        str->text(),
        str->pstate());
      contents.pop_back();
    }
  }

}
