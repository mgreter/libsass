#ifndef SASS_SCANNER_LINE_H
#define SASS_SCANNER_LINE_H

#include "position.hpp"
#include "scanner_string.hpp"

namespace Sass {

  struct LineScannerState2 {
    const char* position;
    Position offset;
  };

  class LineScanner
    : public StringScanner {

  public:

    LineScanner(const char* content, const char* path, size_t srcid);

    LineScannerState2 state() {
      return { position, offset };
    }

    void resetState(const LineScannerState2& state);

    ParserState pstate(LineScannerState2 start) {
      // ParserState(const char* path, const char* src = 0, const size_t file = std::string::npos);
      // ParserState(const char* path, const char* src, const Position & position, Offset offset = Offset(0, 0));
      // ParserState(const char* path, const char* src, const Token & token, const Position & position, Offset offset = Offset(0, 0));

      return ParserState(sourceUrl, startpos,
        Position(start.offset), offset - start.offset);
    }

    ParserState pstate(Position start) {
      // ParserState(const char* path, const char* src = 0, const size_t file = std::string::npos);
      // ParserState(const char* path, const char* src, const Position & position, Offset offset = Offset(0, 0));
      // ParserState(const char* path, const char* src, const Token & token, const Position & position, Offset offset = Offset(0, 0));

      return ParserState(sourceUrl, startpos,
        Position(start), offset - start);
    }

    ParserState pstate(Offset start) {
      // ParserState(const char* path, const char* src = 0, const size_t file = std::string::npos);
      // ParserState(const char* path, const char* src, const Position & position, Offset offset = Offset(0, 0));
      // ParserState(const char* path, const char* src, const Token & token, const Position & position, Offset offset = Offset(0, 0));

      return ParserState(sourceUrl, startpos,
        Position(srcid, start), offset - start);
    }

    ParserState pstate() {
      return ParserState(sourceUrl, startpos,
        Position(srcid, position), { 0, 0 });
    }

    ParserState spanFrom(Offset start) {
      return pstate(start);
    }

  };

}

#endif
