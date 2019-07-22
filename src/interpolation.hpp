#ifndef SASS_INTERPOLATION_H
#define SASS_INTERPOLATION_H

#include "ast_fwd_decl.hpp"
#include "util_string.hpp"
#include "position.hpp"

namespace Sass {

  struct SourceSpan {
    char* start;
    char* end;
  };


  class StringBuffer {

  private:

    std::string buffer;

  public:
    StringBuffer() :
      buffer()
    {}

    void writeCharCode(uint32_t character);

    // void write(uint32_t character);

    void write(unsigned char character)
    {
      buffer.push_back(character);
    }

    void write(char character)
    {
      buffer.push_back(character);
    }

    void write(const std::string& text)
    {
      buffer += text;
    }

    void writeln(const std::string& text = "")
    {
      buffer += text;
      buffer += "\n";
    }

    const std::string& toString() const
    {
      return buffer;
    }

    bool empty() const {
      return buffer.empty();
    }

    void clear() {
      buffer.clear();
    }
    /*
    StringLiteral* toString() {
      return nullptr;
    }
    */

  };

  class InterpolationBuffer {

  private:

    StringBuffer text;

    std::vector<ValueObj> contents;

  public:
    InterpolationBuffer() :
      text()
    {}


    Interpolation* getInterpolation();
    Interpolation* getInterpolation(ParserState pstate);

    bool empty() const {
      return contents.empty() && text.empty();
    }

    // Empties this buffer.
    void clear() {
      contents.clear();
    }

  private:

    // Flushes [_text] to [_contents] if necessary.
    void flushText();

  public:

    void writeCharCode(uint32_t character);

    void addInterpolation(const InterpolationObj schema);

    void write(char character)
    {
      text.write(character);
    }

    void write(const char str[])
    {
      text.write(std::string(str));
    }

    void write(const std::string& str)
    {
      text.write(str);
    }

    void writeln(const std::string& str = "")
    {
      text.writeln(str);
    }

    void write(const ValueObj& expression) {
      flushText();
      contents.push_back(expression);
    }

    void add(const ValueObj& expression) {
      flushText();
      contents.push_back(expression);
    }

    std::string trailingString() {
      return text.toString();
    }

    bool trailingStringEndsWith(std::string cmp) {
      std::string tail(text.toString());
      Util::ascii_str_rtrim(tail);
      return Util::ascii_str_ends_with(tail, cmp);
    }

  };

}

#endif
