#include <stdio.h>
#include <string.h>
#include "source.hpp"
#include "source_span.hpp"
#include "utf8/checked.h"
#include "charcode.hpp"
#include "character.hpp"

namespace Sass {

  // Import some namespaces
  using namespace Charcode;
  using namespace Character;

  SourceData::SourceData()
    : SharedObj()
  {
  }

  SourceFile::SourceFile(SourceData* source,
    Mappings srcmap) :
    SourceData(),
    srcmap(std::move(srcmap)),
    // Make a copy, delete when destroyed
    path(sass_copy_c_string(source->getPath())),
    data(sass_copy_c_string(source->begin())),
    length(source->size()),
    srcid(0), // srcid),
    lfs()
  {
  }

  SourceFile::SourceFile(
    const char* path,
    const char* data,
    size_t srcid) :
    SourceData(),
    srcmap(),
    // Make a copy, delete when destroyed
    path(sass_copy_c_string(path)),
    data(sass_copy_c_string(data)),
    length(0),
    srcid(srcid),
    lfs()
  {
    length = strlen(data);
  }

  SourceFile::SourceFile(
    const char* path,
    const char* data,
    Mappings srcmap,
    size_t srcid) :
    SourceData(),
    srcmap(std::move(srcmap)),
    // Make a copy, delete when destroyed
    path(sass_copy_c_string(path)),
    data(sass_copy_c_string(data)),
    length(0),
    srcid(srcid),
    lfs()
  {
    length = strlen(data);
  }

  const char* SourceFile::end() const
  {
    return data + length;
  }

  const char* SourceFile::begin() const
  {
    return data;
  }

  sass::vector<sass::string> SourceFile::getLines(size_t start, size_t length) const
  {
    const char* pos = data;
    const char* end = this->end();
    sass::vector<sass::string> result;
    while (start > 0 && pos < end) {
      if (*pos == $lf) start -= 1;
      pos += 1;
    }
    const char* beg = pos;
    while (length > 0 && pos < end) {
      // found the end of the line
      if (*pos == $lf) {
        result.emplace_back(sass::string(beg,
          pos[-1] == $cr ? pos - 1 : pos));
        length -= 1; beg = pos + 1;
      }
      // at the end of the content
      if (pos == end - 1) {
        result.emplace_back(sass::string(beg,
          pos[0] == $cr ? pos : end));
        length -= 1; beg = pos + 1;
        break;
      }
      pos += 1;
    }
    return result;
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

  SyntheticFile::SyntheticFile(
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

  SyntheticFile::SyntheticFile(
    const char* data,
    Mappings srcmap,
    SourceFileObj around,
    SourceSpan pstate) :
    SourceFile(
      pstate.getPath(),
      data, srcmap,
      pstate.getSrcId()),
    around(around),
    pstate(pstate)
  {
  }

  SourceSpan SyntheticFile::adjustSourceSpan(SourceSpan pstate) const
  {
    pstate.position =
      this->pstate.position
      + pstate.position;
    return pstate;
    return SourceSpan(
      pstate.getSource(),
      this->pstate.position + pstate.position,
      pstate.length);
    size_t pos = 0;
    while (pos < srcmap.size()) {
      if (srcmap[pos].generated_position > pstate.position) {
        // std::cerr << "GOTCHA\n";
      }
      pos += 1;
    }
    return pstate;
  }

  size_t SyntheticFile::countLines()
  {
    return around->countLines()
      // Minus lines to replace
      - pstate.length.line - 1
      // Plus lines from insert
      + SourceFile::countLines();
  }

  sass::string utf8Substr(
    sass::string& text,
    size_t start,
    size_t len)
  {
    auto first = text.begin();
    utf8::advance(first,
      start, text.end());
    auto last = first;
    if (len != sass::string::npos) {
      utf8::advance(last,
        len, text.end());
    }
    else {
      last = text.end();
    }
    return sass::string(
      first, last);
  }

  sass::string utf8Replace(
    sass::string& text,
    size_t start, size_t len,
    const sass::string& replace)
  {
    auto first = text.begin();
    utf8::advance(first,
      start, text.end());
    auto last = first;
    if (len != sass::string::npos) {
      utf8::advance(last,
        len, text.end());
    }
    else {
      last = text.end();
    }
    return text.replace(
      first, last,
      replace);
  }

  sass::string SyntheticFile::getLine(size_t line)
  {
    // Calculate last line of insert
    size_t lastLine = pstate.position.line - 1
      + SourceFile::countLines();

    // Calculate line difference
    size_t lineDelta = 0
      // Plus lines from insert
      + SourceFile::countLines()
      // Minus lines to replace
      - pstate.length.line - 1;

    // std::cerr << "lastLine: " << lastLine << "\n";

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
        if (pstate.length.line > 0) {
          sass::string after(around->getLine(
            line + pstate.length.line));
          return utf8Replace(before,
            pstate.position.column,
            sass::string::npos,
            SourceFile::getLine(0))
            + utf8Substr(after,
              pstate.length.column,
              sass::string::npos);
        }
        else {
          // Replace in the middle
          return utf8Replace(before,
            pstate.position.column,
            pstate.length.column,
            SourceFile::getLine(0));
        }
      }
      else {
        // Otherwise we append to substring
        return utf8Substr(before, 0,
          pstate.position.column)
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
      size_t col = pstate.length.line == 0
        // Since we insert 
        ? pstate.position.column + pstate.length.column
        : pstate.length.column;

      // std::cerr << "AFTER [" << after << "\n";

      // Append to last line to insert
      return SourceFile::getLine(
        line - pstate.position.line) +
        utf8Substr(after, col,
          sass::string::npos);
    }
    else {
      return around->getLine(
        line - lineDelta);
    }
    return sass::string();
  }

  sass::vector<sass::string>
    SyntheticFile::getLines(
      size_t start, size_t length) const
  {
    sass::vector<sass::string> result;
    // Fetch parts before insert
    if (start < pstate.position.line) {

    }

    result.emplace_back("asd");
    return result;
  }

}

