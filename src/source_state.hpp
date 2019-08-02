#ifndef SASS_SOURCE_STATE_H
#define SASS_SOURCE_STATE_H

#include "ast_fwd_decl.hpp"
#include "offset.hpp"

namespace Sass {

  class SourceState {

  public:

    // The attached source
    SourceDataObj source;

    // Current position
    Offset position;

    // Value constructor
    SourceState(SourceDataObj source,
      Offset position = Offset());

    // Return the attach source id
    size_t getSrcId() const;

    // Return the attached source path
    const char* getPath() const;

    // Return the attached source
    SourceData* getSource() const;

    // Return `char star` for source data
    const char* getRawData() const;

    // Return `char star` for start position
    const char* begin() const;

    // Return line as human readable (starting from one)
    size_t getLine() const {
      return position.line + 1;
    }

    // Return line as human readable (starting from one)
    size_t getColumn() const {
      return position.column + 1;
    }

  };

}

#endif
