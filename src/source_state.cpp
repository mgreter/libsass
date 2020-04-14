#include "source_state.hpp"
#include "file.hpp"
#include "source.hpp"

namespace Sass
{

	SourceState::SourceState(SourceDataObj source, Offset position)
		: source(source)
		, position(position)
	{}

	/**
  * asdasd
  */
  SourceData* SourceState::getSource() const
	{
		return source.ptr();
	}

	size_t SourceState::getSrcId() const
	{
		return source->getSrcId();
	}

  /// Return the attached source path
	const char* SourceState::getImpPath() const
	{
		return source->getImpPath();
	}

  /// Return the attached source path
  const char* SourceState::getAbsPath() const
  {
    return source->getAbsPath();
  }

  const char* SourceState::getRawData() const
	{
		return source->begin();
	}

	sass::string SourceState::getDebugPath() const
	{
		const char* path = getAbsPath();
		sass::string cwd(Sass::File::get_cwd());
		sass::string rel_path(Sass::File::abs2rel(path, cwd, cwd));
		return Sass::File::rel2dbg(rel_path, path);
	}

} // namespace Sass
