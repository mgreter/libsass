#ifndef SASS_POSITION_H
#define SASS_POSITION_H

#include <string>
#include <cstring>
// #include <iostream>

namespace Sass {


  class Offset {

    public: // c-tor
      Offset(const char* string);
      Offset(const std::string& text);
      Offset(const size_t line, const size_t column);

      // return new position, incremented by the given string
      inline Offset add(const char* begin, const char* end) {
        if (end == 0) return *this;
        while (begin < end && *begin) {
          if (*begin == '\n') {
            ++ line;
            // start new line
            column = 0;
          } else {
            ++ column;
          }
          ++begin;
        }
        return *this;
      }
      inline Offset inc(const char* begin, const char* end) const {
        Offset offset(line, column);
        offset.add(begin, end);
        return offset;
      }

      // init/create instance from const char substring
      static Offset init(const char* beg, const char* end);

    public: // overload operators for position
      inline bool operator== (const Offset &pos) const {
        return line == pos.line && column == pos.column;
      }
      inline bool operator!= (const Offset &pos) const {
        return line != pos.line || column != pos.column;
      }
      inline void operator+= (const Offset &off){
        *this = Offset(line + off.line, off.line > 0 ? off.column : column + off.column);
      }
      inline Offset operator+ (const Offset &off) const {
        return Offset(line + off.line, off.line > 0 ? off.column : column + off.column);
      }
      inline Offset operator- (const Offset &off) const {
        return Offset(line - off.line, off.line == line ? column - off.column : column);
      }

    public: // overload output stream operator
      // friend std::ostream& operator<<(std::ostream& strm, const Offset& off);

    public:
      Offset off() { return *this; }

    public:
      size_t line;
      size_t column;

  };

  class Position : public Offset {

    public: // c-tor
      Position(const size_t file); // line(0), column(0)
      Position(const size_t file, const Offset& offset);
      Position(const size_t line, const size_t column); // file(-1)
      Position(const size_t file, const size_t line, const size_t column);

    public: // overload operators for position
      inline bool operator== (const Position &pos) const {
        return file == pos.file && line == pos.line && column == pos.column;
      };
      inline bool operator!= (const Position &pos) const {
        return file == pos.file || line != pos.line || column != pos.column;
      };
      inline void operator+= (const Offset &off) {
        *this = Position(file, line + off.line, off.line > 0 ? off.column : column + off.column);
      };
      inline const Position operator+ (const Offset &off) const {
        return Position(file, line + off.line, off.line > 0 ? off.column : column + off.column);
      };
      inline const Offset operator- (const Offset &off) const {
        return Offset(line - off.line, off.line == line ? column - off.column : column);
      };

      // return new position, incremented by the given string
      inline Position add(const char* begin, const char* end) {
        Offset::add(begin, end);
        return *this;
      };

      inline Position inc(const char* begin, const char* end) const {
        Offset offset(line, column);
        offset = offset.inc(begin, end);
        return Position(file, offset);
      };

    public: // overload output stream operator
      // friend std::ostream& operator<<(std::ostream& strm, const Position& pos);

    public:
      size_t file;

  };

  // Token type for representing lexed chunks of text
  class Token {
  public:
    const char* prefix;
    const char* begin;
    const char* end;

    Token()
    : prefix(0), begin(0), end(0) { }
    Token(const char* b, const char* e)
    : prefix(b), begin(b), end(e) { }
    Token(const char* str)
    : prefix(str), begin(str), end(str + strlen(str)) { }
    Token(const char* p, const char* b, const char* e)
    : prefix(p), begin(b), end(e) { }

    size_t length()    const { return end - begin; }
    std::string ws_before() const { return std::string(prefix, begin); }
    std::string to_string() const { return std::string(begin, end); }
    std::string time_wspace() const {
      std::string str(to_string());
      std::string whitespaces(" \t\f\v\n\r");
      return str.erase(str.find_last_not_of(whitespaces)+1);
    }

    operator bool()        { return begin && end && begin >= end; }
    operator std::string() { return to_string(); }

    bool operator==(Token t)  { return to_string() == t.to_string(); }
  };

  class ParserState : public Position {

    public: // c-tor
      ParserState(const char* path, const char* src = 0, const size_t file = std::string::npos);
      ParserState(const char* path, const char* src, const Position& position, Offset offset = Offset(0, 0));
      ParserState(const char* path, const char* src, const Token& token, const Position& position, Offset offset = Offset(0, 0));

    public: // down casts
      inline Offset off() { return *this; }
      inline Position pos() { return *this; }
      inline ParserState pstate() { return *this; }

    public:
      const char* path;
      const char* src;
      Offset offset;
      Token token;

  };

}

#endif
