#ifndef SASS_SOURCE_H
#define SASS_SOURCE_H

// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include "file.hpp"
#include "ast_fwd_decl.hpp"
#include "source_span.hpp"

namespace Sass {


  // SourceData is the base class to hold loaded sass content.
  class SourceData :
    public SharedObj {

    friend class ItplFile;
    friend class ItplFile2;
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
    virtual const char* getAbsPath() const = 0;

    // Returns the requested line. Will take interpolations into
    // account to show more accurate debug messages. Calling this
    // can be rather expensive, so only use it for debugging.
    virtual sass::string getLine(size_t line) = 0;

    // Get raw iterator for raw source
    virtual const char* end() const = 0;

    // Get raw iterator for raw source
    virtual const char* begin() const = 0;

    // Return raw size in bytes
    virtual size_t size() const = 0;

    virtual SourceSpan adjustSourceSpan(SourceSpan& pstate) const = 0;

    ~SourceData() {}
  };

  class SourceFile :
    public SourceData {
    friend class ItplFile;
    friend class ItplFile2;

  protected:

    // Import path
    // should probably use std::string anyway?
    char* path;

    // Raw source data
    char* data;

    // Raw length in bytes
    size_t length;

    // Unique source id
    size_t srcid;

    // Also store srcmap

    // Store byte offset for every line.
    // Lazy calculated within `countLines`.
    // Columns per line can be derived from it.
    sass::vector<size_t> lfs;

    // Returns the number of lines. On first call
    // it will calculate the linefeed lookup table.
    virtual size_t countLines();

  public:

    // Constructor will copy `path` and `data`.
    // Will be destroyed when we go out of scope.
    SourceFile(
      const char* path,
      const char* data,
      size_t srcid);
/*
    SourceFile(
      const char* path,
      const sass::string& data,
      size_t srcid);
      */
    SourceFile(
      const char* path,
      sass::string&& data,
      size_t srcid);

    SourceFile(
      const Include& include,
      const char* data,
      size_t srcid);
    /*
    SourceFile(
      const Include& include,
      const sass::string& data,
      size_t srcid);
      */
    SourceFile(
      const Include& include,
      sass::string&& data,
      size_t srcid);

    // Destructor
    ~SourceFile() {
      sass_free_memory(path);
      sass_free_memory(data);
    }

    // Returns the requested line. Will take interpolations into
    // account to show more accurate debug messages. Calling this
    // can be rather expensive, so only use it for debugging.
    virtual sass::string getLine(size_t line);

    // Returns adjusted source span regarding interpolation.
    virtual SourceSpan adjustSourceSpan(SourceSpan& pstate) const {
      return pstate;
    }

    // Return raw sizes in bytes
    size_t size() const {
      return length;
    }

    // Get raw iterator for actual source
    const char* end() const {
      return data + length;
    }

    // Get raw iterator for actual source
    const char* begin() const {
      return data;
    }

    // Return path as it was given for import
    const char* getAbsPath() const
		{
      return path;
    }

    // The source id is uniquely assigned
    size_t getSrcId() const {
      return srcid;
    }

    // Needed to satisfy SharedObj
    sass::string to_string() const {
      return data;
    }

  };

  class ItplFile :
    public SourceFile {

  protected:

    // Account additional lines if needed.
    size_t countLines() override final;

  private:

    // The original source where the interpolation occurred.
    SourceFileObj around;

    // The position where the interpolation occurred.
    SourceSpan pstate;

  public:

    // Create a synthetic interpolated source. The `data` is the
    // evaluated interpolation, while `around` is the original source
    // where the actual interpolation was given at `pstate` position.
    ItplFile(const char* data,
      SourceFileObj around,
      SourceSpan pstate);

    // Returns source with interpolation inserted.
    sass::string getLine(size_t line) override final;

    // Returns adjusted source span with interpolation in mind.
    // The input `pstate` is relative to the interpolation, will
    // return a source span with absolute position in regard of
    // the original document with the interpolation inserted.
    SourceSpan adjustSourceSpan(SourceSpan& pstate) const override final;

  };


  class ItplFile2 :
    public SourceFile {

  protected:

    // Account additional lines if needed.
    size_t countLines() override final;

  private:

    // The position where the interpolation occurred.
    // We also get the parent source from this state.
    SourceSpan pstate;

  public:

    // Create a synthetic interpolated source. The `data` is the
    // evaluated interpolation, while `around` is the original source
    // where the actual interpolation was given at `pstate` position.
    ItplFile2(const char* data,
      SourceSpan pstate);

    ItplFile2(sass::string&& data,
      SourceSpan pstate);

    // Returns source with interpolation inserted.
    sass::string getLine(size_t line) override final;

    // Returns adjusted source span with interpolation in mind.
    // The input `pstate` is relative to the interpolation, will
    // return a source span with absolute position in regard of
    // the original document with the interpolation inserted.
    SourceSpan adjustSourceSpan(SourceSpan& pstate) const override final;

  };

}

#endif
