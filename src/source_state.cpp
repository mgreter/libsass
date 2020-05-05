#include "source_state.hpp"
#include "file.hpp"
#include "source.hpp"

namespace Sass
{

	SourceState::SourceState(SourceDataObj source, Offset position)
		: source(source)
		, position(position)
	{}

//  SourceState::SourceState(const SourceState& state)
//    : source(state.source),
//    position(state.position)
//  {}

  // SourceState::SourceState(SourceState&& state)
  //   : source(std::move(state.source))
  //   , position(std::move(state.position))
  // {}

  SourceData* SourceState::getSource() const
	{
		return source.ptr();
	}

  size_t SourceState::getSrcIdx() const
	{
		return source->getSrcIdx();
	}

	const char* SourceState::getImpPath() const
	{
		return source->getImpPath();
	}

  const char* SourceState::getAbsPath() const
  {
    return source->getAbsPath();
  }

  const char* SourceState::getContent() const
  {
    return source->content();
  }

  sass::string SourceState::getDebugPath() const
	{
		const char* path = getAbsPath();
		sass::string rel_path(File::abs2rel(path, CWD, CWD));
		return File::rel2dbg(rel_path, path);
	}

} // namespace Sass
