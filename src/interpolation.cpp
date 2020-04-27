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

  void StringBuffer::write(const ItplString* string)
  {
    buffer += string->text();
  }

  void InterpolationBuffer::writeCharCode(uint32_t character)
  {
    text.writeCharCode(character);
  }

  void InterpolationBuffer::write(const ItplString* str)
  {
    text.write(str->text(), str->pstate());
  }

  Interpolation* InterpolationBuffer::getInterpolation(const SourceSpan& pstate, bool rtrim)
  {
    InterpolationObj itpl = SASS_MEMORY_NEW(Interpolation, pstate);
    // Append all content Expressions
    for (ValueObj item : contents) {
      itpl->append(std::move(item));
    }
    if (!text.empty()) {
      // Appends a ItplString from the remaining text in the string buffer
      itpl->append(SASS_MEMORY_NEW(ItplString,
        pstate, text.toString43(rtrim)));
    }
    // ToDo: get rid of detach?
    return itpl.detach();
  }

  // Flushes [text] to [contents] if necessary.
  void InterpolationBuffer::flushText()
  {
    // Do nothing if text is empty
    if (text.empty()) return;
    // Create and add string constant to container
    contents.emplace_back(SASS_MEMORY_NEW(
      ItplString, pstate, text.toString43()));
    // Clear buffer now
    text.clear();
  }
  // EO flushText

  // Add an interpolation to the buffer. 
  void InterpolationBuffer::addInterpolation(const InterpolationObj schema)
  {
    if (schema->empty()) return;

    auto& elements = schema->elements();
    auto addStart = elements.begin();
    auto addEnd = elements.end();

    // The schema to add start with a plain string
    if (ItplString* str = elements[0]->getItplString()) {
      // Append the plain string
      text.write(str);
      // First item is consumed
      addStart += 1;
    }

    // Flush plain text to contents (as
    // ItplString) if any text is defined.
    flushText();

    // Is there an item at the end?
    if (addStart != addEnd) {

      auto& last = *(addEnd - 1);
      if (auto str = last->getItplString()) {
        // Add the rest if some is left
        contents.insert(contents.end(),
          addStart, addEnd - 1);
        text.write(
          str->text(),
          str->pstate());
      }
      else {
        // Add the rest if some is left
        contents.insert(contents.end(),
          addStart, addEnd);
      }
    }

    if (auto str = contents.back()->getItplString()) {
      text.write(
        str->text(),
        str->pstate());
      contents.pop_back();
    }

  }
  // EO addInterpolation

}
