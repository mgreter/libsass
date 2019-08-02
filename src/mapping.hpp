#ifndef SASS_MAPPING_H
#define SASS_MAPPING_H

#include <vector>
#include "allocator.hpp"
#include "position.hpp"

namespace Sass {

  class Mapping {
  public:
    size_t file;
    Offset original_position;
    Offset generated_position;
    Mapping(size_t file, const Offset& original_position, const Offset& generated_position)
    : file(file), original_position(original_position), generated_position(generated_position) { }

  };

  typedef sass::vector<Mapping> Mappings;

}

#endif
