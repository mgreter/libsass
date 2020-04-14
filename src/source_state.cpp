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

  // Return the attached source type
  Sass_Import_Type SourceState::getSourceType() const {
    return source->getType();
  }

  size_t SourceState::getSrcIdx() const
	{
		return source->getSrcIdx();
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
		return source->content();
	}

	sass::string SourceState::getDebugPath() const
	{
		const char* path = getAbsPath();
		sass::string cwd(Sass::File::get_cwd());
		sass::string rel_path(Sass::File::abs2rel(path, cwd, cwd));
		return Sass::File::rel2dbg(rel_path, path);
	}

} // namespace Sass
