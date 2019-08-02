#include "position.hpp"
#include "position.hpp"
#include "position.hpp"
#include "position.hpp"
// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include "source.hpp"
#include "position.hpp"
#include "scanner_line.hpp"
#include "util_string.hpp"
#include "character.hpp"

namespace Sass {

  using namespace Charcode;
  using namespace Character;


  Position::Position(
    LineScanner& scanner) :
    Offset(scanner.offset),
    position(scanner.position)
  {}



}
