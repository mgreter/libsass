#include "sass.hpp"
#include "charcode.hpp"
#include "character.hpp"
#include "scanner_line.hpp"
#include "source_span.hpp"
#include "source_state.hpp"

namespace Sass {

  // Import some namespaces
  using namespace Charcode;
  using namespace Character;

  LineScanner::LineScanner(
    Logger& logger,
    SourceDataObj source) :
    StringScanner(logger, source)
  {
  }

  void StringScanner::resetState(
    const LineScannerState2& state)
  {
	  position = state.position;
	  offset.line = state.offset.line;
	  offset.column = state.offset.column;
    // We assume we always store states
    // only from relevant positions.
    lastRelevant.position = position;
    lastRelevant.offset = offset;
  }

  SourceSpan StringScanner::pstate(const Position& start, bool backupWhitespace)
  {
    SourceSpan pstate(source, start,
      Offset::distance(start, offset));
    if (backupWhitespace) {
      pstate.backupWhitespace();
    }
    return source ? source->adjustSourceSpan(pstate) : pstate;
  }

  SourceSpan StringScanner::pstate(const Position& start, const char* position)
  {
    SourceSpan pstate(source, start,
      Offset::distance(start, offset));
    if (position != 0) {
      pstate.backupWhitespace(position);
    }
    return source ? source->adjustSourceSpan(pstate) : pstate;
  }

  SourceSpan StringScanner::pstate(const Offset& start, bool backupWhitespace)
  {
    SourceSpan pstate(source, start,
      Offset::distance(start, offset));
    if (backupWhitespace) {
      pstate.backupWhitespace();
    }
    return source ? source->adjustSourceSpan(pstate) : pstate;
  }

  SourceSpan StringScanner::pstate9()
  {
    SourceSpan pstate(source, lastRelevant.offset);
    return source ? source->adjustSourceSpan(pstate) : pstate;
  }

  SourceSpan StringScanner::pstate43()
  {
    SourceSpan pstate(source, offset);
    return source ? source->adjustSourceSpan(pstate) : pstate;
  }

  SourceSpan StringScanner::pstate9(const Offset& start)
  {
    // Check if we should re-map the span
    // if (false && nextMap != sass::string::npos) {
    //   while (true) {
    //     if (!source->hasMapping(nextMap)) break;
    //     const Mapping& map = source->getMapping(nextMap);
    //     if (map.original_position > start) {
    //       nextMap += 1;
    //     }
    //     else {
    //       break;
    //     }
    //   }
    //   const Mapping& map = source->getMapping(nextMap);
    // }
    // 
    SourceSpan pstate(source, start,
      Offset::distance(start, lastRelevant.offset));
    return source ? source->adjustSourceSpan(pstate) : pstate;
  }

  // Return SourceSpan for current position
  SourceSpan StringScanner::pstate() const {
    // Check if we should re-map the span
    // if (nextMap != sass::string::npos) {
    //   // std::cerr << "DODA\n";
    // }
    if (lastRelevant.offset.line != offset.line) {
      SourceSpan pstate(source,
        lastRelevant.offset,
        Offset());
      return source ? source->adjustSourceSpan(pstate) : pstate;
    }
    else {
      SourceSpan pstate(source,
        offset, // lastRelevant.offset,
        Offset());
      return source ? source->adjustSourceSpan(pstate) : pstate;
    }
  }


  SourceSpan StringScanner::spanFrom(const Offset& start) {
    return pstate(start);
  }

}
