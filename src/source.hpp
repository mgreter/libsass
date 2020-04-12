#ifndef SASS_SOURCE_H
#define SASS_SOURCE_H

#include "ast_fwd_decl.hpp"
#include "source_span.hpp"
#include "mapping.hpp"

namespace Sass {


  // SourceData is the base class to hold loaded sass content.
  class SourceData :
    public SharedObj {

    friend class ItplFile;
    friend class SourceData;

  protected:

    // Returns the number of lines. On the first call it will
    // calculate the linefeed lookup table.
    virtual size_t countLines() = 0;

  public:

    // Constructor
    SourceData();

    // The source id is uniquely assigned
    virtual size_t getSrcId() const = 0;

    // Return path as it was given for import
    virtual const char* getPath() const = 0;

    // Returns the requested line. Will take interpolations into
    // account to show more accurate debug messages. Calling this
    // can be rather expensive, so only use it for debugging.
    virtual sass::string getLine(size_t line) = 0;

    // Get raw iterator for actual source
    virtual const char* end() const = 0;

    // Get raw iterator for actual source
    virtual const char* begin() const = 0;

    // Return raw sizes in bytes
    virtual size_t size() const = 0;

    // Deprecated, obsolete?
    virtual bool hasMapping(size_t srcMapPos) const = 0;

    virtual SourceSpan adjustSourceSpan(SourceSpan& pstate) const = 0;

    ~SourceData() {}
  };

  class SourceFile :
    public SourceData {
    friend class ItplFile;
  protected:
    Mappings srcmap;
    char* path;
    char* data;
    size_t length;
    size_t srcid;
  protected:

    // Store byte offset for every line.
    // Lazy calculated within `countLines`.
    // Columns per line can be derived from it.
    sass::vector<size_t> lfs;

    // Returns the number of lines.
    virtual size_t countLines() override;

  public:

    SourceFile(
      SourceData* source,
      Mappings srcmap);

    SourceFile(
      const char* path,
      const char* data,
      size_t srcid);

    SourceFile(
      const char* path,
      const char* data,
      Mappings srcmap,
      size_t srcid);

    ~SourceFile();

    const char* end() const override final;
    const char* begin() const override final;

    size_t size() const override final {
      return length;
    }

    // Return the attached source path
		virtual const char* getPath() const override
		{
      return path;
    }

    virtual size_t getSrcId() const override {
      return srcid;
    }

    virtual bool hasMapping(size_t srcMapPos) const override {
      return srcMapPos < srcmap.size();
    }

    virtual SourceSpan adjustSourceSpan(SourceSpan& pstate) const override {
      return pstate;
    }


    virtual sass::string getLine(size_t line) override;

    sass::string to_string() const override final {
      return data;
    }
  };

  class ItplFile :
    public SourceFile {
  protected:

    size_t countLines() override final;

  private:
    SourceFileObj around;
    SourceSpan pstate;
  public:

    ItplFile(const char* data,
      SourceFileObj around,
      SourceSpan pstate);

    ItplFile(const char* data,
      Mappings srcmap,
      SourceFileObj around,
      SourceSpan pstate);

    SourceSpan adjustSourceSpan(SourceSpan& pstate) const override final;

    sass::string getLine(size_t line) override final;

  };

}

#endif
