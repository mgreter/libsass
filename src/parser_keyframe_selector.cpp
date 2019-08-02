#include "parser_keyframe_selector.hpp"

#include "character.hpp"
#include "utf8/checked.h"

namespace Sass {

  // Import some namespaces
  using namespace Charcode;
  using namespace Character;

  sass::vector<sass::string> KeyframeSelectorParser::parse() {

    sass::vector<sass::string> selectors;
    do {
      whitespace();
      if (lookingAtIdentifier()) {
        if (scanIdentifier("from")) {
          selectors.emplace_back("from");
        }
        else {
          callStackFrame frame(*context.logger,
            Backtrace(scanner.pstate9()));
          expectIdentifier("to", "\"to\" or \"from\"");
          selectors.emplace_back("to");
        }
      }
      else {
        selectors.emplace_back(_percentage());
      }
      whitespace();
    } while (scanner.scanChar($comma));
    scanner.expectDone();

    return selectors;
  }

  sass::string KeyframeSelectorParser::_percentage()
  {

    StringBuffer buffer;
    if (scanner.scanChar($plus)) {
      buffer.writeCharCode($plus);
    }

    uint8_t second = scanner.peekChar();
    if (!isDigit(second) && second != $dot) {
      scanner.error("Expected number.",
        *context.logger, scanner.pstate());
    }

    while (isDigit(scanner.peekChar())) {
      buffer.writeCharCode(scanner.readChar());
    }

    if (scanner.peekChar() == $dot) {
      buffer.writeCharCode(scanner.readChar());

      while (isDigit(scanner.peekChar())) {
        buffer.writeCharCode(scanner.readChar());
      }
    }

    if (scanIdentifier("e")) {
      buffer.write(scanner.readChar());
      uint8_t next = scanner.peekChar();
      if (next == $plus || next == $minus) buffer.write(scanner.readChar());
      if (!isDigit(scanner.peekChar())) {
        scanner.error("Expected digit.",
          *context.logger, scanner.pstate());
      }

      while (isDigit(scanner.peekChar())) {
        buffer.writeCharCode(scanner.readChar());
      }
    }

    scanner.expectChar($percent);
    buffer.writeCharCode($percent);
    return buffer.toString();

  }

}
