#include "source_state.hpp"
#include "file.hpp"
#include "source.hpp"

namespace Sass
{

	SourceState::SourceState(SourceDataObj source, Offset position)
		: source(source)
		, position(position)
	{}

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

  Sass_Import_Type SourceState::getSourceType() const
  {
    return source->getType();
  }

  sass::string SourceState::getDebugPath() const
	{
		const char* path = getAbsPath();
		sass::string cwd(Sass::File::get_cwd());
		sass::string rel_path(Sass::File::abs2rel(path, cwd, cwd));
		return Sass::File::rel2dbg(rel_path, path);
	}

} // namespace Sass
