// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include "sass_functions.hpp"
#include "sass_context.hpp"
#include "backtrace.hpp"
#include "utf8/checked.h"
#include "utf8/core.h"
#include "util_string.hpp"
#include "source.hpp"
#include "json.hpp"
#include <iomanip>

#include <algorithm>
#include <iostream>
#include <typeinfo>
#include <string>
#include <cmath>

#include "logger.hpp"
#include "terminal.hpp"
#include "character.hpp"
#include "string_utils.hpp"

namespace Sass {

  using namespace Constants;

  Logger::Logger(size_t precision, enum Sass_Logger_Style style) :
    epsilon(std::pow(0.1, precision + 1)),
    style(style)
  {}

  // Write warning header to error stream
  void Logger::writeWarnHead(bool deprecation)
  {
    if (style & SASS_LOGGER_COLOR) {
      errors << getColor(Terminal::yellow);
      if (!deprecation) errors << "Warning";
      else errors << "Deprecation Warning";
      errors << getColor(Terminal::reset);
    }
    else {
      if (!deprecation) errors << "WARNING";
      else errors << "DEPRECATION WARNING";
    }
  }

  // Convert back-traces which only hold references
  // to e.g. the source content to stack-traces which
  // manages copies of the temporary string references.
  StackTraces convertBackTraces(BackTraces traces)
  {
    // They convert implicitly, so simply assign them
    return StackTraces(traces.begin(), traces.end());
  }

  // Print the `input` string onto the output stream `os` and
  // wrap words around to fit into the given column `width`.
  void wrap(sass::string const& input, size_t width, sass::ostream& os)
  {
    sass::istream in(input);

    size_t current = 0;
    sass::string word;

    while (in >> word) {
      if (current + word.size() > width) {
        os << STRMLF;
        current = 0;
      }
      os << word << ' ';
      current += word.size() + 1;
    }
    if (current != 0) {
      os << STRMLF;
    }
  }

  // Print a warning without any SourceSpan (used by @warn)
  void Logger::addWarn43(const sass::string& message, bool deprecation)
  {
    writeWarnHead(deprecation);
    errors << ": ";

    wrap(message, 80, errors);
    StackTraces stack(callStack.begin(), callStack.end());
		writeStackTraces(errors, stack, "    ", true, 0);
  }


  // Print a regular warning or deprecation
  void Logger::addWarn33(const sass::string& message, const SourceSpan& pstate, bool deprecation)
  {

    callStackFrame frame(*this, pstate);

    writeWarnHead(deprecation);
    errors << " on line " << pstate.getLine();
    errors << ", column " << pstate.getColumn();
    errors << " of " << pstate.getDebugPath() << ':' << STRMLF;

    // Might be expensive to call?
    // size_t cols = Terminal::getWidth();
    // size_t width = std::min<size_t>(cols, 80);
    wrap(message, 80, errors);

    errors << STRMLF;

    StackTraces stack(callStack.begin(), callStack.end());
		writeStackTraces(errors, stack, "    ", true);

  }
  /*
  StdLogger::StdLogger(size_t precision, enum Sass_Logger_Style style) :
    Logger(precision, style)
  {
  }

  void StdLogger::warn(sass::string message) const
  {
  }

  void StdLogger::debug(sass::string message) const
  {
  }

  void StdLogger::error(sass::string message) const
  {
  }
  */






  void Logger::format_pstate(sass::ostream& msg_stream, SourceSpan pstate, enum Sass_Logger_Style logstyle)
  {

    // ASCII reporting
    sass::string top(",");
    sass::string upper("/");
    sass::string middle("|");
    sass::string lower("\\");
    sass::string runin("-");
    sass::string bottom("'");

    // Or use Unicode versions
    if (logstyle & SASS_LOGGER_UNICODE) {
      top = "\xE2\x95\xB7";
      upper = "\xE2\x94\x8C";
      middle = "\xE2\x94\x82";
      lower = "\xE2\x94\x94";
      runin = "\xE2\x94\x80";
      bottom = "\xE2\x95\xB5";
    }

    // now create the code trace (ToDo: maybe have utility functions?)
    if (pstate.source->begin() == nullptr) return;

    // Calculate offset positions
    Offset beg = pstate.position;
    Offset end = beg + pstate.span;

    // Dart-sass claims this might fail due to float errors
    size_t padding = (size_t)floor(log10(end.line + 1)) + 1;

    // Do multi-line reporting
    if (pstate.span.line > 0 && pstate.source) {

      sass::vector<sass::string> lines;
      SourceData* source = pstate.source;

      // Fetch all lines we need to print the state
      for (size_t i = 0; i < pstate.span.line + 1; i++) {
        sass::string line(source->getLine(pstate.position.line + i));
        lines.emplace_back(line);
      }

      // Write intro line
      msg_stream
        << getColor(Terminal::blue)
        << std::right << std::setfill(' ')
        << std::setw(padding) << ' '
        << ' ' << top
        << getColor(Terminal::reset)
        << STRMLF;

      for (size_t i = 0; i < lines.size(); i++) {

        // Right trim the line to report
        StringUtils::makeRightTrimmed(lines[i]);

        // Write the line number and the code
        msg_stream
          << getColor(Terminal::blue)
          << std::right << std::setfill(' ')
          << std::setw(padding) << (beg.line + i + 1)
          << ' ' << middle
          << getColor(Terminal::reset)
          << ' ';

        // Report the first line
        // Gets a line underneath
        if (i == 0) {

          // Rewind position to last relevant to report cleaner if possible
          while (beg.column > 0 && Character::isWhitespace(lines[i][beg.column - 1])) beg.column--;

          // Only need a line if not at start
          if (beg.column > 0) {

            // Print the initial code line
            auto line_beg = lines[i].begin();
            utf8::advance(line_beg, beg.column, lines[i].end());
            lines[i].insert(line_beg - lines[i].begin(), getColor(Terminal::red));
            msg_stream << "  " << lines[i] << getColor(Terminal::reset) << STRMLF;

            // Print the line beneath
            msg_stream
              << getColor(Terminal::blue)
              << std::right << std::setfill(' ')
              << std::setw(padding) << ' '
              << ' ' << middle << ' '
              << getColor(Terminal::reset)
              << getColor(Terminal::red)
              << upper;
            // This needs a loop unfortunately
            for (size_t n = 0; n < beg.column + 2; n++) {
              msg_stream << runin;
            }
            // Final indicator
            msg_stream
              << '^'
              << getColor(Terminal::reset)
              << STRMLF;
          }
          // Just print the code line
          else {
            msg_stream
              << getColor(Terminal::red)
              << upper << ' ' << lines[i]
              << getColor(Terminal::reset)
              << STRMLF;
          }
      }
      // Last line might get another indicator line
      else if (i == lines.size() - 1) {

          if (end.column < lines[i].size()) {

            // Print the final code line
            auto line_beg = lines[i].begin();
            utf8::advance(line_beg, end.column, lines[i].end());
            lines[i].insert(line_beg - lines[i].begin(), getColor(Terminal::reset));
            msg_stream << getColor(Terminal::red) << middle << ' ' << lines[i] << STRMLF;

            // Print the line beneath
            msg_stream
              << getColor(Terminal::blue)
              << std::right << std::setfill(' ')
              << std::setw(padding) << ' '
              << ' ' << middle << ' '
              << getColor(Terminal::reset)
              << getColor(Terminal::red)
              << lower;
            // This needs a loop unfortunately
            for (size_t n = 0; n < end.column; n++) {
              msg_stream << runin;
            }
            // Final indicator
            msg_stream
              << '^'
              << getColor(Terminal::reset)
              << STRMLF;
          }
          else {
            // Just print the code line
            msg_stream
              << getColor(Terminal::red)
              << lower << ' ' << lines[i]
              << getColor(Terminal::reset)
              << STRMLF;
          }
        } 
        else {
          // Just print the code line
          msg_stream
            << getColor(Terminal::red)
            << middle << ' ' << lines[i]
            << getColor(Terminal::reset)
            << STRMLF;
        }
      }

      // Write outro line
      msg_stream
        << getColor(Terminal::blue)
        << std::right << std::setfill(' ')
        << std::setw(padding) << ' '
        << ' ' << bottom
        << getColor(Terminal::reset)
        << STRMLF;

    }
    // Single line reporting
    else {

      sass::string raw = pstate.source->getLine(pstate.position.line);
      sass::string line; utf8::replace_invalid(raw.begin(), raw.end(),
        std::back_inserter(line), logstyle & SASS_LOGGER_UNICODE ? 0xfffd : '?');

      // Convert to ASCII only string
      if (logstyle & SASS_LOGGER_ASCII) {
        std::replace_if(line.begin(), line.end(),
          Character::isUtf8StartByte, '?');
        line.erase(std::remove_if(line.begin(), line.end(),
          Character::isUtf8Continuation), line.end());
      }

      size_t lhs_len, mid_len; //, rhs_len;
      // Get the sizes (characters) for each part
      mid_len = pstate.span.column;
      lhs_len = pstate.position.column;

      // Normalize tab characters to spaces for better counting
      for (size_t i = line.length(); i != std::string::npos; i -= 1) {
        if (line[i] == '\t') {
          // Adjust highlight positions
          if (i < lhs_len) lhs_len += 3;
          else if (i < lhs_len + mid_len) mid_len += 3;
          // Replace tab with spaces
          line.replace(i, 1, "    ");
        }
      }

      // Split line into parts to report
      // They will be shortened if needed
      sass::string lhs, mid, rhs;
      splitLine(line, lhs_len, mid_len,
        76 - padding, lhs, mid, rhs);

      // Get character length of each part
      lhs_len = utf8::distance(lhs.begin(), lhs.end());
      mid_len = utf8::distance(mid.begin(), mid.end());
      // rhs_len = utf8::distance(rhs.begin(), rhs.end());

      // Report the trace
      msg_stream

        // Write the leading line
        << getColor(Terminal::blue)
        << std::right << std::setfill(' ')
        << std::setw(padding) << ' '
        << ' ' << top
        << getColor(Terminal::reset)

        << STRMLF

        // Write the line number
        << getColor(Terminal::blue)
        << std::right << std::setfill(' ')
        << std::setw(padding) << (beg.line + 1)
        << ' ' << middle
        << getColor(Terminal::reset)

        << ' '

        // Write the parts
        << lhs
        << getColor(Terminal::red)
        << mid
        << getColor(Terminal::reset)
        << rhs

        << STRMLF

        // Write left part of marker line
        << getColor(Terminal::blue)
        << std::right << std::setfill(' ')
        << std::setw(padding) << ' '
        << ' ' << middle
        << getColor(Terminal::reset)

        << ' '

        // Write the actual marker
        << sass::string(lhs_len, ' ')
        << getColor(Terminal::red)
        << sass::string(mid_len ? mid_len : 1, '^')
        << getColor(Terminal::reset)

        << STRMLF

        // Write the trailing line
        << getColor(Terminal::blue)
        << std::right << std::setfill(' ')
        << std::setw(padding) << ' '
        << ' ' << bottom
        << getColor(Terminal::reset)

        << STRMLF;

    }

  }

  void Logger::splitLine(sass::string line, size_t lhs_len, size_t mid_len,
    size_t columns, sass::string& lhs, sass::string& mid, sass::string& rhs)
  {

    // Get the ellipsis character(s) either in unicode or ASCII
    size_t ellipsis_len = style & SASS_LOGGER_UNICODE ? 1 : 3;
    sass::string ellipsis(style & SASS_LOGGER_UNICODE ? "\xE2\x80\xA6": "...");

    // Normalize tab characters to spaces for better counting
    for (size_t i = line.length(); i != std::string::npos; i -= 1) {
      if (line[i] == '\t') line.replace(i, 1, "    ");
    }

    // Get line iterators
    auto line_beg = line.begin();
    auto line_end = line.end();

    // Get the sizes (characters) for each part
    size_t line_len = utf8::distance(line_beg, line_end);
    size_t rhs_len = line_len - lhs_len - mid_len;

    // Prepare iterators for left side of highlighted string
    auto lhs_beg = line.begin(), lhs_end = lhs_beg;
    utf8::advance(lhs_end, lhs_len, line_end);
    // Prepare iterators for right side of highlighted string
    auto rhs_beg = lhs_end, rhs_end = line.end();
    utf8::advance(rhs_beg, mid_len, line_end);

    // Create substring of each part
    lhs.append(lhs_beg, lhs_end);
    rhs.append(rhs_beg, rhs_end);
    mid.append(lhs_end, rhs_beg);

    // Trim trailing spaces
    StringUtils::makeRightTrimmed(rhs);

    // Re-count characters after trimming is done
    lhs_len = utf8::distance(lhs.begin(), lhs.end());
    rhs_len = utf8::distance(rhs.begin(), rhs.end());

    // Get how much we want to show at least
    size_t min_left = 12, min_right = 12;
    // But we can't show more than we have
    min_left = std::min(min_left, lhs_len);
    min_right = std::min(min_right, rhs_len);

    // Calculate available size for highlighted part
    size_t mid_max = columns - min_left - min_right;

    // Check if middle parts needs shortening
    if (mid_len > mid_max) {
      // Calculate how much we must shorten
      size_t shorten = mid_len - mid_max + ellipsis_len;
      // Get the sizes for left and right parts and adjust for epsilon
      size_t lhs_size = size_t(std::ceil(0.5 * (mid_len - shorten)));
      size_t rhs_size = size_t(std::floor(0.5 * (mid_len - shorten)));
      // Prepare iterators for later substring operation
      auto lhs_start = mid.begin(), rhs_stop = mid.end();
      auto lhs_stop = lhs_start; utf8::advance(lhs_stop, lhs_size, rhs_stop);
      auto rhs_start = lhs_stop; utf8::advance(rhs_start, shorten, rhs_stop);
      // Recreate shortened middle (highlight) part
      mid = sass::string(lhs_start, lhs_stop) +
        ellipsis + sass::string(rhs_start, rhs_stop);
      // Update middle size (should be mid_max)
      mid_len = lhs_size + rhs_size + ellipsis_len;
    }
    else {
      // We can give some space back
      size_t leftover = mid_max - mid_len;
      // Distribute the leftovers
      // ToDo: do it with math only
      while (leftover) {
        if (min_left < lhs_len) {
          min_left += 1;
          leftover -= 1;
          if (min_right < rhs_len) {
            if (leftover > 0) {
              min_right += 1;
              leftover -= 1;
            }
          }
        }
        else if (min_right < rhs_len) {
          min_right += 1;
          leftover -= 1;
        }
        else {
          break;
        }
      }
    }

    // Shorten left side
    if (min_left < lhs_len) {
      min_left = min_left - ellipsis_len;
      auto beg = lhs.begin(), end = lhs.end();
      utf8::advance(beg, lhs_len - min_left, end);
      lhs = sass::string(beg, end);
      StringUtils::makeLeftTrimmed(lhs);
      lhs = ellipsis + lhs;
    }

    // Shorten right side
    if (min_right < rhs_len) {
      min_right = min_right - ellipsis_len;
      auto beg = rhs.begin(), end = rhs.begin();
      utf8::advance(end, min_right, rhs.end());
      rhs = sass::string(beg, end);
      StringUtils::makeRightTrimmed(rhs);
      rhs = rhs + ellipsis;
    }

  }
  // EO splitLine

  // Print `amount` of `traces` to output stream `os`.
  void Logger::writeStackTraces(sass::ostream& os, StackTraces traces,
    sass::string indent, bool showPos, size_t amount)
	{

    sass::sstream strm;
    sass::string cwd(File::get_cwd());

    // bool first = true;
    size_t max = 0;
    size_t i_beg = traces.size() - 1;
    size_t i_end = sass::string::npos;

    sass::string last("root stylesheet");
    std::vector<std::pair<sass::string, sass::string>> traced;
    for (size_t i = 0; i < traces.size(); i++) {

      const StackTrace& trace = traces[i];

      // make path relative to the current directory
      sass::string rel_path(File::abs2rel(trace.pstate.getAbsPath(), cwd, cwd));

      strm.str(sass::string());
      strm << rel_path << ' ';
      strm << trace.pstate.getLine();
      strm << ":" << trace.pstate.getColumn();

      sass::string str(strm.str());
      max = std::max(max, str.length());

      if (i == 0) {
        traced.emplace_back(std::make_pair(
          str, last));
      }
      else if (!traces[i - 1].name.empty()) {
        last = traces[i - 1].name;
        if (traces[i - 1].fn) last += "()";
        traced.emplace_back(std::make_pair(str, last));
      }
      else {
        traced.emplace_back(std::make_pair(
          str, last));
      }

    }

    i_beg = traces.size() - 1;
    i_end = sass::string::npos;
    for (size_t i = i_beg; i != i_end; i--) {

      const StackTrace& trace = traces[i];

      // make path relative to the current directory
      sass::string rel_path(File::abs2rel(trace.pstate.getAbsPath(), cwd, cwd));

      // skip functions on error cases (unsure why ruby sass does this)
      // if (trace.caller.substr(0, 6) == ", in f") continue;

      if (amount == sass::string::npos || amount > 0) {
        format_pstate(os, trace.pstate, style);
        if (amount > 0) --amount;
      }

      if (showPos) {
				os << indent;
				os << std::left << std::setfill(' ')
           << std::setw(max + 2) << traced[i].first;
				os << traced[i].second;
				os << STRMLF;
      }

    }

    os << STRMLF;

  }














}
