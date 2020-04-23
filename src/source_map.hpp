#ifndef SASS_SOURCE_MAP_H
#define SASS_SOURCE_MAP_H

#include <string>
#include <vector>

#include "ast_fwd_decl.hpp"
#include "source_span.hpp"
#include "base64vlq.hpp"
#include "position.hpp"
#include "mapping.hpp"

#include "backtrace.hpp"
#include "memory.hpp"

#define VECTOR_PUSH(vec, ins) vec.insert(vec.end(), ins.begin(), ins.end())
#define VECTOR_UNSHIFT(vec, ins) vec.insert(vec.begin(), ins.begin(), ins.end())

namespace Sass {

  class Context;
  class OutputBuffer;

  class SourceMap {

  public:
    sass::vector<size_t> source_index;
    bool enabled;
    SourceMap(bool enabled);
    // SourceMap(const sass::string& file);

    void append(const Offset& offset);
    void prepend(const Offset& offset);
    void append(const OutputBuffer& out);
    void prepend(const OutputBuffer& out);
    void add_open_mapping(const AST_Node* node);
    void add_close_mapping(const AST_Node* node);

    SourceSpan remap(const SourceSpan& pstate);

  private:

  public:
    sass::string render(const std::unordered_map<size_t, size_t>& remap_srcidx) const;

    // Deque is not faster, I checked
    sass::vector<Mapping> mappings;

  private:
    Offset current_position;
public:
    sass::string file;
private:
    Base64VLQ base64vlq;
  };

  class OutputBuffer {
    public:
      sass::string buffer;
      SourceMap* smap;
      OutputBuffer(bool enabled)
        : smap(new SourceMap(enabled)) {
      }
  };

}

#endif
