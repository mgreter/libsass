#ifndef SASS_INTERPOLATION_H
#define SASS_INTERPOLATION_H

#include "ast_fwd_decl.hpp"
#include "scanner_string.hpp"
#include "util_string.hpp"
#include "position.hpp"

namespace Sass {

  class StringBuffer {

  private:

    sass::string buffer;

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

    void write(const sass::string& text)
    {
      buffer.append(text);
    }

    void write(const sass::string& text, const SourceSpan& pstate)
    {
      buffer.append(text);
    }

    void write(sass::string&& text)
    {
      buffer.append(std::move(text));
    }

    void write(sass::string&& text, const SourceSpan& pstate)
    {
      buffer.append(std::move(text));
    }

    void write(const StringLiteral* string);

    // Once you call this function, buffer is emptied
    const sass::string& toString(bool rtrim = false)
    {
      if (rtrim) {
        Util::ascii_str_rtrim(buffer);
        return buffer;
      }
      else {
        return buffer;
      }
    }

    bool empty() const {
      return buffer.empty();
    }

    void clear() {
      buffer.clear();
    }

  };

  class InterpolationBuffer {

  private:

    SourceSpan pstate;

    StringBuffer text;

    sass::vector<ExpressionObj> contents;

  public:

    InterpolationBuffer(const SourceSpan& pstate) :
      pstate(pstate), text()
    {}

    InterpolationBuffer(const StringScanner& scanner) :
      pstate(scanner.pstate()), text()
    {}

    Interpolation* getInterpolation(const SourceSpan& pstate, bool rtrim = false);

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
      text.write(sass::string(str));
    }

    void write(const sass::string& str)
    {
      text.write(str);
    }

    void write(const sass::string& str, SourceSpan pstate)
    {
      text.write(str, pstate);
    }

    void write(sass::string&& str)
    {
      text.write(std::move(str));
    }

    void write(sass::string&& str, SourceSpan pstate)
    {
      text.write(std::move(str), pstate);
    }

    void write(const StringLiteral* str);

    void write(const ExpressionObj& expression) {
      flushText();
      contents.emplace_back(expression);
    }

    void add(const ExpressionObj& expression) {
      flushText();
      contents.emplace_back(expression);
    }

    sass::string trailingString() {
      return text.toString();
    }

    bool trailingStringEndsWith(const sass::string& cmp) {
      sass::string tail(text.toString());
      Util::ascii_str_rtrim(tail);
      // could optimize the check to account trim
      return Util::ascii_str_ends_with(tail, cmp);
    }

  };

}

#endif
