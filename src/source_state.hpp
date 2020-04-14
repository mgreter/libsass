#ifndef SASS_SOURCE_STATE_H
#define SASS_SOURCE_STATE_H

#include "ast_fwd_decl.hpp"
#include "offset.hpp"

namespace Sass
{

	/**
	 * Stores a reference (shared ptr) to the source code
	 * and one offset position (line and column information).
	 **/
	class SourceState
	{

	public:
		/** The source code reference **/
    SourceDataObj source;

		/** The position within the source **/
		Offset position;

		/** Regular value constructor **/
		SourceState(SourceDataObj source, Offset position = Offset());

		/** Return the attach source id **/
		size_t getSrcId() const;

    // Return the attached source path
    const char* getImpPath() const;

    // Return the attached source path
    const char* getAbsPath() const;

		// Return the attached source
		SourceData* getSource() const;

		// Return `char star` for source data
		const char* getRawData() const;

		// Return line as human readable
		// Starting from one instead of zero
		uint32_t getLine() const
		{
			return position.line + 1;
		}

		// Return line as human readable
		// Starting from one instead of zero
		uint32_t getColumn() const
		{
			return position.column + 1;
		}

		// Either return path relative to cwd if path is
		// inside cwd, otherwise return absolute path.
		sass::string getDebugPath() const;
	};

} // namespace Sass

#endif
