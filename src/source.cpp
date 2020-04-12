#include <stdio.h>
#include <string.h>
#include "source.hpp"
#include "source_span.hpp"
#include "utf8/checked.h"
#include "charcode.hpp"
#include "character.hpp"
#include "unicode.hpp"

namespace Sass {

  // Import some namespaces
  using namespace Charcode;
  using namespace Character;

  /*#########################################################################*/
  /*#########################################################################*/

  SourceFile::SourceFile(
    const char* path,
    const char* data,
    size_t srcid) :
    SharedObj(),
    // Make a copy, delete when destroyed
    path(sass_copy_c_string(path)),
    data(sass_copy_c_string(data)),
    length(0),
    srcid(srcid),
    lfs()
  {
    length = strlen(data);
  }

  size_t SourceFile::countLines()
  {
    if (lfs.empty()) {
      size_t len = 0;
      lfs.emplace_back(len);
      // ToDo: do only on demand
      while (data[len] != 0) {
        if (data[len] == $lf) {
          lfs.emplace_back(len + 1);
        }
        ++len;
      }
      lfs.emplace_back(len);
      length = len;
    }

    return lfs.size() - 1;
  }

  sass::string SourceFile::getLine(size_t line)
  {
    countLines();
    if (line > lfs.size()) {
      return sass::string();
    }
    size_t first = lfs[line];
    size_t last = lfs[line + 1];
    if (first == last) return sass::string();
    const char* beg = data + first;
    const char* end = data + last;
    if (end[-1] == $lf) end -= 1;
    if (end[-1] == $cr) end -= 1;
    return sass::string(beg, end);
  }

  /*#########################################################################*/
  /*#########################################################################*/

  ItplFile::ItplFile(
    const char* data,
    SourceFileObj around,
    SourceSpan pstate) :
    SourceFile(
      pstate.getPath(),
      data,
      pstate.getSrcId()),
    around(around),
    pstate(pstate)
  {
  }

  SourceSpan ItplFile::adjustSourceSpan(SourceSpan& pstate) const
  {
    pstate.position =
      this->pstate.position
      + pstate.position;
    return pstate;
  }

  size_t ItplFile::countLines()
  {
    return around->countLines()
      // Minus lines to replace
      - pstate.span.line - 1
      // Plus lines from insert
      + SourceFile::countLines();
  }

  sass::string ItplFile::getLine(size_t line)
  {
    // Calculate last line of insert
    size_t lastLine = pstate.position.line - 1
      + SourceFile::countLines();

    // Calculate line difference
    size_t lineDelta = 0
      // Plus lines from insert
      + SourceFile::countLines()
      // Minus lines to replace
      - pstate.span.line - 1;

    // Get full line before insert
    if (line < pstate.position.line) {
      return around->getLine(line);
    }
    // Fetch first line of insert
    else if (line == pstate.position.line) {
      // Get the line of around to get before part
      sass::string before(around->getLine(line));
      // Check if pstate offset only spans one line
      // Therefore we need to insert into the line
      // Size of `2` means we have only `start` and `end`
      if (lfs.size() == 2) {
        // We remove some lines, need to doctor
        // those together to one single line
        if (pstate.span.line > 0) {
          sass::string after(around->getLine(
            line + pstate.span.line));
          return Unicode::replace(before,
            pstate.position.column,
            sass::string::npos,
            SourceFile::getLine(0))
            + Unicode::substr(after,
              pstate.span.column,
              sass::string::npos);
        }
        else {
          // Replace in the middle
          return Unicode::replace(before,
            pstate.position.column,
            pstate.span.column,
            SourceFile::getLine(0));
        }
      }
      else {
        // Otherwise we append to substring
        return Unicode::substr(before,
          0, pstate.position.column)
          + SourceFile::getLine(0);
      }
    }
    // Now we must be in the inserting part
    // Only happens if we have a full line
    else if (line < lastLine) {
      // Get full line of insert
      return SourceFile::getLine(
        line - pstate.position.line);
    }
    // Fetch last line of insert
    else if (line == lastLine) {
      // Get line to append
      sass::string after(
        around->getLine(
          line - lineDelta));
      // Calculate column to cut appending line
      size_t col = pstate.span.line == 0
        ? pstate.position.column + pstate.span.column
        : pstate.span.column;

      // Append to last line to insert
      return SourceFile::getLine(
        line - pstate.position.line) +
        Unicode::substr(after,
          col, sass::string::npos);
    }
    else {
      return around->getLine(
        line - lineDelta);
    }
    return sass::string();
  }

}
