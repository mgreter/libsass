#include "source_span.hpp"
#include "util_string.hpp"
#include "source.hpp"
#include "ast.hpp"

namespace Sass {

  SourceSpan::SourceSpan(
    SourceDataObj source,
    const Offset& position,
    const Offset& offset) :
    SourceState(source, position),
    length(std::move(offset))
  {}

  SourceSpan SourceSpan::fake(const char* label) {
    SourceFile* source = SASS_MEMORY_NEW(
      SourceFile, label, "", sass::string::npos);
    return SourceSpan(source);
  }

  void SourceSpan::backupWhitespace()
  {
    /*
    bool recountCols = false;
    // Check until start of string
    while (position > src) {
      // Line-feeds are complicated, as we need to
      // Re-calculate the line-length for columns
      if (Util::ascii_islinefeed(*(position-1))) {
        recountCols = true;
        // Ignore carriage returns
        if (*(position-1) == '\n') {
          --offset.line;
        }
        --position;
      }
      // Pure whitespace is simple, just back up
      else if (Util::ascii_iswhitespace(*(position-1))) {
        --offset.column; --position; // back up
      }
      // Otherwise we're done
      else {
        break;
      }
    }
    if (recountCols) {
      offset.column = 0;
      while (position > src) {
        if (Util::ascii_islinefeed(*(position-1))) {
          break;
        }
        ++offset.column;
        --position;
      }
    }
    */
  }
  // EO backupWhitespace

  const char* SourceSpan::end() const {
    return Offset::move(source->begin(), position + length);
  }

  void SourceSpan::backupWhitespace(const char* position)
  {
    bool recountCols = false;
    // Check until start of string
    while (position > source->begin()) {
      // Line-feeds are complicated, as we need to
      // Re-calculate the line-length for columns
      if (Util::ascii_islinefeed(*(position - 1))) {
        recountCols = true;
        // Ignore carriage returns
        if (*(position - 1) == '\n') {
          --length.line;
        }
        --position;
      }
      // Pure whitespace is simple, just back up
      else if (Util::ascii_iswhitespace(*(position - 1))) {
        --length.column; --position; // back up
      }
      // Otherwise we're done
      else {
        break;
      }
    }
    if (recountCols) {
      length.column = 0;
      while (position > source->begin()) {
        if (Util::ascii_islinefeed(*(position - 1))) {
          break;
        }
        ++length.column;
        --position;
      }
    }
  }

  SourceSpan SourceSpan::delta(SourceSpan lhs, SourceSpan rhs)
  {
    return SourceSpan(
      lhs.source, lhs.position,
      Offset::distance(lhs.position,
        rhs.position + rhs.length));
  }

  SourceSpan SourceSpan::delta(AST_Node* lhs, AST_Node* rhs)
  {
    return SourceSpan::delta(
      lhs->pstate(), rhs->pstate());
  }

}
