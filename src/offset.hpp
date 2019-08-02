#ifndef SASS_OFFSET_H
#define SASS_OFFSET_H

#include "sass.hpp"
#include <string>
#include <stdint.h>

#include "ast_def_macros.hpp"

namespace Sass {

  class Offset {

  public:

    // All properties are public (zero based)
    // Getters return human-readable form (+1)
    size_t line;
    size_t column;

    // Empty constructor
    Offset();

    // Create an `Offset` from the given string
    // Will use `append` internally for the char
    Offset(uint8_t character);

    // Create an `Offset` from the given string
    // Will use `append` internally for all chars
    Offset(const sass::string& text);

    // Create an `Offset` from the given char star
    // Will use `append` internally on all chars
    Offset(const char* beg, const char* end = 0);

    // Append `character` to increment offset
    void plus(uint8_t character);

    // Append `text` to increment offset
    void plus(const sass::string& text);

    // Create offset with given `line` and `column`
    // Needs static constructor to avoid ambiguity
    static Offset init(size_t line, size_t column);

    // Return the `distance` between `start` and `end`
    // Gives the solution to the equation `end = start + x`
    static Offset distance(const Offset& start, const Offset& end);

    // Move/increment char star `text` by `offset`
    static const char* move(const char* text, Offset offset);

    // Assign and increment operator
    void operator+= (const Offset& pos);

    // Plus operator
    Offset operator+ (const Offset& off) const;

    // Implement equal and derive unequal
    ATTACH_EQ_OPERATIONS(Offset);

    // Implement `<`, derive `<=`, `>`, `>=`
    ATTACH_CMP_OPERATIONS(Offset);

  };

}

#endif
