#ifndef SASS_EMITTER_H
#define SASS_EMITTER_H

// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include "sass/base.h"
#include "sass/values.h"
#include "source_map.hpp"
#include "ast_fwd_decl.hpp"

namespace Sass {
  class Context;

  class Emitter {

    public:
      Emitter(struct SassOutputOptionsCpp& opt);
      virtual ~Emitter() { }

    protected:
      OutputBuffer wbuf;
    public:
      const sass::string& buffer(void) { return wbuf.buffer; }
      const SourceMap smap(void) { return wbuf.smap; }
      const OutputBuffer& output(void) { return wbuf; }
      // proxy methods for source maps
      void add_source_index(size_t idx);
      void set_filename(const sass::string& str);
      void add_open_mapping(const AST_Node* node);
      void add_close_mapping(const AST_Node* node);
      void schedule_mapping(const AST_Node* node);
      sass::string render_srcmap(Context &ctx, bool include_sources, bool source_map_file_urls);
      SourceSpan remap(const SourceSpan& pstate);

    public:
      struct SassOutputOptionsCpp& opt;
      size_t indentation;
      size_t scheduled_space;
      size_t scheduled_linefeed;
      bool scheduled_delimiter;
      const AST_Node* scheduled_crutch;
      const AST_Node* scheduled_mapping;

    public:
      // output strings different in custom css properties
      bool in_custom_property;
      // nested list must not have parentheses
      bool in_declaration;
      // nested lists need parentheses
      sass::vector<Sass_Separator> separators;
      bool in_space_array;
      bool in_comma_array;

    public:
      // return buffer as sass::string
      sass::string get_buffer(void);
      // flush scheduled space/linefeed
      Sass_Output_Style output_style(void) const;
      // add outstanding linefeed
      void finalize(bool final = true);
      // flush scheduled space/linefeed
      void flush_schedules(void);
      // prepend some text or token to the buffer
      void prepend_string(const sass::string& text);
      void prepend_output(const OutputBuffer& out);
      // append some text or token to the buffer
      void write_string(const sass::string& text);
      void append_string(const sass::string& text);
      void append_string(const char* text, size_t repeat);
      void append_string(const sass::string& text, size_t repeat);
      // append a single character to buffer
      void write_char(uint8_t chr);
      void append_char(uint8_t chr);
      // append some text or token to the buffer
      // this adds source-mappings for node start and end
      void append_token(const sass::string& text, const AST_Node* node);
      // query last appended character
      char last_char();

    public: // syntax sugar
      void append_indentation();
      void append_optional_space(void);
      void append_mandatory_space(void);
      void append_special_linefeed(void);
      void append_optional_linefeed(void);
      void append_mandatory_linefeed(void);
      void append_scope_opener(AST_Node* node = 0);
      void append_scope_closer(AST_Node* node = 0);
      void append_comma_separator(void);
      void append_colon_separator(void);
      void append_delimiter(void);

  };

}

#endif
