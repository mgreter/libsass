#include "sass.hpp"
#include "position.hpp"

namespace Sass {


  Offset::Offset(const char* string)
  : line(0), column(0)
  {
    *this = inc(string, string + strlen(string));
  }

  Offset::Offset(const std::string& text)
  : line(0), column(0)
  {
    *this = inc(text.c_str(), text.c_str() + text.size());
  }

  Offset::Offset(const size_t line, const size_t column)
  : line(line), column(column) { }

  // init/create instance from const char substring
  Offset Offset::init(const char* beg, const char* end)
  {
    Offset offset(0, 0);
    if (end == 0) {
      end += strlen(beg);
    }
    offset.add(beg, end);
    return offset;
  }

  // increase offset by given string (mostly called by lexer)
  // increase line counter and count columns on the last line
  // ToDo: make the col count utf8 aware


  Position::Position(const size_t file)
  : Offset(0, 0), file(file) { }

  Position::Position(const size_t file, const Offset& offset)
  : Offset(offset), file(file) { }

  Position::Position(const size_t line, const size_t column)
  : Offset(line, column), file(-1) { }

  Position::Position(const size_t file, const size_t line, const size_t column)
  : Offset(line, column), file(file) { }


  ParserState::ParserState(const char* path, const char* src, const size_t file)
  : Position(file, 0, 0), path(path), src(src), offset(0, 0), token() { }

  ParserState::ParserState(const char* path, const char* src, const Position& position, Offset offset)
  : Position(position), path(path), src(src), offset(offset), token() { }

  ParserState::ParserState(const char* path, const char* src, const Token& token, const Position& position, Offset offset)
  : Position(position), path(path), src(src), offset(offset), token(token) { }



  /* not used anymore - remove?
  std::ostream& operator<<(std::ostream& strm, const Offset& off)
  {
    if (off.line == string::npos) strm << "-1:"; else strm << off.line << ":";
    if (off.column == string::npos) strm << "-1"; else strm << off.column;
    return strm;
  } */

  /* not used anymore - remove?
  std::ostream& operator<<(std::ostream& strm, const Position& pos)
  {
    if (pos.file != string::npos) strm << pos.file << ":";
    if (pos.line == string::npos) strm << "-1:"; else strm << pos.line << ":";
    if (pos.column == string::npos) strm << "-1"; else strm << pos.column;
    return strm;
  } */

}
