#ifndef SASS_SOURCE_SPAN_H
#define SASS_SOURCE_SPAN_H

#include "source_state.hpp"

namespace Sass {

  // ParseState is SourceSpan
  class SourceSpan : public SourceState {

  public: // c-tor

    SourceSpan(SourceDataObj source,
      const Offset& position = Offset(),
      const Offset& offset = Offset());

    // Offset size
    Offset length;

    static SourceSpan fake(const char* label);

    // Create span between `lhs.start` and `rhs.end`
    static SourceSpan delta(SourceSpan lhs, SourceSpan rhs);
    static SourceSpan delta(AST_Node* lhs, AST_Node* rhs);


  public: // down casts
    void backupWhitespace(const char* position);
    void backupWhitespace();

    const char* end() const;

    size_t getEndLine() const {
      return position.line + length.line + 1;
    }

    size_t getEndColumn() const {
      if (length.line > 0) {
        return length.column + 1;
      }
      return position.column + 1
        + length.column;
    }

    Offset getEndPos() const {
      return position + length;
    }

  };

}

#endif
