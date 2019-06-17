#include "sass.hpp"
#include "charcode.hpp"
#include "character.hpp"
#include "scanner_line.hpp"

namespace Sass {

  // Import some namespaces
  using namespace Charcode;
  using namespace Character;

  LineScanner::LineScanner(const char* content, const char* path, size_t srcid)
    : StringScanner(content, path, srcid)
  {
  }

  void LineScanner::resetState(
    const LineScannerState2& state)
  {
	  position = state.position;
	  offset.line = state.offset.line;
	  offset.column = state.offset.column;
  }

}
