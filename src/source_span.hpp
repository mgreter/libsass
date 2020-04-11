#ifndef SASS_SOURCE_SPAN_H
#define SASS_SOURCE_SPAN_H

#include "source_state.hpp"

namespace Sass
{

	// ParseState is SourceSpan
	class SourceSpan : public SourceState
	{

	public:
		SourceSpan(const char* path);

		SourceSpan(SourceDataObj source,
							 const Offset& position = Offset(),
							 const Offset& span = Offset());

		// Offset size
		Offset span;

		// Create span between `lhs.start` and `rhs.end`
		static SourceSpan delta(const SourceSpan& lhs, const SourceSpan& rhs);

		static SourceSpan delta(AST_Node* lhs, AST_Node* rhs);

	public: // down casts
		const char* end() const;
	};

} // namespace Sass

#endif
