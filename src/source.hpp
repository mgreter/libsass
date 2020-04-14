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

  protected:

    // Returns the number of lines. On the first call it will
    // calculate the linefeed lookup table.
    virtual size_t countLines() = 0;

  public:

    // Constructor
    SourceData();

    // The source id is uniquely assigned
    virtual size_t getSrcId() const = 0;

    // The source id is uniquely assigned
    virtual void setSrcId(size_t idx) = 0;

    // Return path as it was given for import
    virtual const char* getImpPath() const = 0;

    // Return path as it was given for import
    virtual const char* getAbsPath() const = 0;

    // Returns the requested line. Will take interpolations into
    // account to show more accurate debug messages. Calling this
    // can be rather expensive, so only use it for debugging.
    virtual sass::string getLine(size_t line) = 0;

    // Get raw iterator for raw source
    virtual const char* content() const = 0;
    virtual const char* srcmaps() const = 0;

    // Get raw iterator for raw source
    const char* contentStart() const { return content(); };
    const char* srcmapsStart() const { return srcmaps(); };
    const char* contentEnd() const { return content() + contentSize(); };
    const char* srcmapsEnd() const { return srcmaps() + srcmapsSize(); };

    // Return raw size in bytes
    virtual size_t contentSize() const = 0;
    virtual size_t srcmapsSize() const = 0;

    // Needed to satisfy SharedObj
    sass::string to_string() const {
      return content();
    }

    // Returns adjusted source span regarding interpolation.
    virtual SourceSpan adjustSourceSpan(SourceSpan& pstate) const {
      return pstate;
    }

    virtual Sass_Import_Type getType() const {
      return SASS_IMPORT_AUTO;
    }

    virtual void setType(Sass_Import_Type type) = 0;

    ~SourceData() {}
  };

  class SourceWithPath :
    public SourceData {
    friend class SourceString;
    friend class SourceFile;
    friend class ItplFile;
    friend class ItplFile2;

  protected:

    // Import path
    sass::string imp_path;

    // Resolved path
    sass::string abs_path;

    // Unique source id
    size_t srcid;

    // Raw length in bytes
    size_t len_content;
    size_t len_srcmaps;

    // Store byte offset for every line.
    // Lazy calculated within `countLines`.
    // Columns per line can be derived from it.
    sass::vector<size_t> lfs;

  public:

    SourceWithPath(
      sass::string&& imp_path,
      sass::string&& abs_path,
      Sass_Import_Type type = SASS_IMPORT_AUTO,
      size_t idx = sass::string::npos);

    SourceWithPath(
      const sass::string& imp_path,
      const sass::string& abs_path,
      Sass_Import_Type type = SASS_IMPORT_AUTO,
      size_t idx = sass::string::npos);

    // Returns the requested line. Will take interpolations into
    // account to show more accurate debug messages. Calling this
    // can be rather expensive, so only use it for debugging.
    virtual sass::string getLine(size_t line);

    // Return path as it was given for import
    const char* getImpPath() const
    {
      return imp_path.c_str();
    }

    // Return path after it was resolved
    const char* getAbsPath() const
    {
      return abs_path.c_str();
    }

    // The source id is uniquely assigned
    void setSrcId(size_t idx) {
      srcid = idx;
    }

    // The source id is uniquely assigned
    size_t getSrcId() const {
      return srcid;
    }

    // the import type
    Sass_Import_Type type;

    Sass_Import_Type getType() const override final {
      return type;
    }

    void setType(Sass_Import_Type type) override final {
      this->type = type;
    }

    size_t contentSize() const override final {
      return len_content;
    }
    size_t srcmapsSize() const override final {
      return len_srcmaps;
    }

  };

  class SourceFile :
    public SourceWithPath {
    friend class ItplFile;
    friend class ItplFile2;

  protected:

    // Raw source data
    char* _content;

    // Raw source data
    char* _srcmaps;

    // Returns the number of lines. On first call
    // it will calculate the linefeed lookup table.
    virtual size_t countLines();

  public:

    SourceFile(
      const char* imp_path,
      const char* abs_path,
      char* content,
      char* srcmaps,
      size_t srcid);

    // Destructor
    ~SourceFile();

    // Get raw iterator for actual source
    const char* content() const override final {
      return _content;
    }

    // Get raw iterator for actual source
    const char* srcmaps() const override final {
      return _srcmaps;
    }

  };

  class SourceString :
    public SourceWithPath {
    friend class ItplFile;
    friend class ItplFile2;

  protected:

    // Raw source data
    sass::string _content;

    // Raw source data
    sass::string _srcmaps;

    // Returns the number of lines. On first call
    // it will calculate the linefeed lookup table.
    virtual size_t countLines();

  public:

    // Constructor will copy `path` and `data`.
    // Will be destroyed when we go out of scope.

    SourceString(
      const char* imp_path,
      const char* abs_path,
      sass::string&& data,
      sass::string&& srcmap,
      size_t srcid);

    SourceString(
      bool foo,
      const char* path,
      sass::string&& data,
      size_t srcid);

    SourceString(
      const Include& include,
      sass::string&& data,
      size_t srcid);

    // Destructor
    ~SourceString() {}

    // Get raw iterator for actual source
    const char* content() const override final {
      return _content.c_str();
    }

    // Get raw iterator for actual source
    const char* srcmaps() const override final {
      return _srcmaps.c_str();
    }

  };




























  class ItplFile2 :
    public SourceString {

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
