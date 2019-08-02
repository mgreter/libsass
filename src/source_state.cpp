#include "source_state.hpp"
#include "source.hpp"

namespace Sass {

  SourceState::SourceState(
    SourceDataObj source,
    Offset position) :
    source(source),
    position(position)
  {}

  SourceData* SourceState::getSource() const {
    return source.ptr();
  }

  size_t SourceState::getSrcId() const {
    return source->getSrcId();
  }

  const char* SourceState::getPath() const
  {
    return source->getPath();
  }

  const char* SourceState::getRawData() const
  {
    return source->begin();
  }

  const char* SourceState::begin() const {
    return Offset::move(source->begin(), position);
  }

}

