#include "source.hpp"

#include <cstring>
#include "unicode.hpp"
#include "charcode.hpp"
#include "character.hpp"

namespace Sass {

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  // Import some namespaces
  using namespace Charcode;
  using namespace Character;

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  SourceData::SourceData()
    : RefCounted()
  {}

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  SourceWithPath::SourceWithPath(
    sass::string&& imp_path,
    sass::string&& abs_path,
    size_t idx) :
    imp_path(std::move(imp_path)),
    abs_path(std::move(abs_path)),
    len_content(0),
    len_srcmaps(0),
    srcidx(idx),
    lfs()
  {}

  SourceWithPath::SourceWithPath(
    const sass::string& imp_path,
    const sass::string& abs_path,
    size_t idx) :
    imp_path(imp_path),
    abs_path(abs_path),
    len_content(0),
    len_srcmaps(0),
    srcidx(idx),
    lfs()
  {}

  // Standard implementation for raw char API
  size_t SourceWithPath::countLines()
  {
    if (lfs.empty()) {
      size_t len = 0;
      lfs.emplace_back(len);
      const char* data = content();
      while (data[len] != 0) {
        if (data[len] == $lf) {
          lfs.emplace_back(len + 1);
        }
        ++len;
      }
      lfs.emplace_back(len);
    }

    return lfs.size() - 1;
  }

  // Standard implementation for raw char API
  sass::string SourceWithPath::getLine(size_t line)
  {
    countLines();
    if (line > lfs.size()) {
      return sass::string();
    }
    size_t first = lfs[line];
    size_t last = lfs[line + 1];
    if (first == last) return sass::string();
    const char* beg = content() + first;
    const char* end = content() + last;
    if (end[-1] == $lf) end -= 1;
    if (end[-1] == $cr) end -= 1;
    return sass::string(beg, end);
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  SourceFile::SourceFile(
    const char* imp_path,
    const char* abs_path,
    char* content,
    char* srcmaps,
    size_t srcidx) :
    SourceWithPath(
      imp_path ? imp_path : "",
      abs_path ? abs_path : "",
      srcidx
    ),
    _content(content),
    _srcmaps(srcmaps)
  {
    if (_content != nullptr) {
      len_content = ::strlen(_content);
    }
    if (_srcmaps != nullptr) {
      len_srcmaps = ::strlen(_srcmaps);
    }
  }

  // Only one that has to clean-up
  SourceFile::~SourceFile() {
    sass_free_memory(_content);
    sass_free_memory(_srcmaps);
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  SourceString::SourceString(
    const char* abs_path,
    sass::string&& content) :
    SourceWithPath(
      abs_path ? abs_path : "",
      abs_path ? abs_path : "",
      sass::string::npos
    ),
    _content(std::move(content))
  {
    len_content = _content.length();
  }

  SourceString::SourceString(
    const char* imp_path,
    const char* abs_path,
    sass::string&& content,
    sass::string&& srcmaps,
    size_t srcidx) :
    SourceWithPath(
      imp_path ? imp_path : "",
      abs_path ? abs_path : "",
      srcidx
    ),
    _content(std::move(content)),
    _srcmaps(std::move(srcmaps))
  {
    len_content = _content.length();
    len_srcmaps = _srcmaps.length();
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

  SourceItpl::SourceItpl(SourceSpan pstate,
    sass::string&& data) :
    SourceString(
      pstate.getImpPath(),
      pstate.getAbsPath(),
      std::move(data), "",
      pstate.getSrcIdx()),
    pstate(pstate)
  {
  }

  SourceSpan SourceItpl::adjustSourceSpan(SourceSpan& pstate) const
  {
    pstate.position =
      this->pstate.position
      + pstate.position;
    return pstate;
  }

  size_t SourceItpl::countLines()
  {
    return pstate.getSource()->countLines()
      // Minus lines to replace
      - pstate.span.line - 1
      // Plus lines from insert
      + SourceString::countLines();
  }

  sass::string SourceItpl::getLine(size_t line)
  {
    SourceData* source(pstate.getSource());
    // Calculate last line of insert
    size_t lastLine = pstate.position.line - 1
      + SourceString::countLines();

    // Calculate line difference
    size_t lineDelta = 0
      // Plus lines from insert
      + SourceString::countLines()
      // Minus lines to replace
      - pstate.span.line - 1;

    // Get full line before insert
    if (line < pstate.position.line) {
      return source->getLine(line);
    }
    // Fetch first line of insert
    else if (line == pstate.position.line) {
      // Get the line of around to get before part
      sass::string before(source->getLine(line));
      // Check if pstate offset only spans one line
      // Therefore we need to insert into the line
      // Size of `2` means we have only `start` and `end`
      if (lfs.size() == 2) {
        // We remove some lines, need to doctor
        // those together to one single line
        if (pstate.span.line > 0) {
          sass::string after(source->getLine(
            line + pstate.span.line));
          return Unicode::replace(before,
            pstate.position.column,
            sass::string::npos,
            SourceString::getLine(0))
            + Unicode::substr(after,
              pstate.span.column,
              sass::string::npos);
        }
        else {
          // Replace in the middle
          return Unicode::replace(before,
            pstate.position.column,
            pstate.span.column,
            SourceString::getLine(0));
        }
      }
      else {
        // Otherwise we append to substring
        return Unicode::substr(before,
          0, pstate.position.column)
          + SourceString::getLine(0);
      }
    }
    // Now we must be in the inserting part
    // Only happens if we have a full line
    else if (line < lastLine) {
      // Get full line of insert
      return SourceString::getLine(
        line - pstate.position.line);
    }
    // Fetch last line of insert
    else if (line == lastLine) {
      // Get line to append
      sass::string after(
        source->getLine(
          line - lineDelta));
      // Calculate column to cut appending line
      size_t col = pstate.span.line == 0
        ? pstate.position.column + pstate.span.column
        : pstate.span.column;

      // Append to last line to insert
      return SourceString::getLine(
        line - pstate.position.line) +
        Unicode::substr(after,
          col, sass::string::npos);
    }
    else {
      return source->getLine(
        line - lineDelta);
    }
    return sass::string();
  }

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

}

