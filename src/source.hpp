#ifndef SASS_SOURCE_H
#define SASS_SOURCE_H

#include "ast_fwd_decl.hpp"
#include "source_span.hpp"
#include "mapping.hpp"

namespace Sass {

  class SourceData :
    public SharedObj {
  public:
    SourceData();
    virtual sass::vector<sass::string>
      getLines(size_t start, size_t length = 1) const = 0;
    virtual sass::string getLine(size_t line) = 0;
    virtual size_t countLines() = 0;
    virtual const char* end() const = 0;
    virtual const char* begin() const = 0;
    virtual const char* getPath() const = 0;
    virtual size_t size() const = 0;

    virtual bool hasMapping(size_t srcMapPos) const = 0;
    virtual const Mapping& getMapping(size_t srcMapPos) const = 0;

    virtual SourceSpan adjustSourceSpan(SourceSpan pstate) const = 0;

    virtual size_t getSrcId() const = 0;
    ~SourceData() {}
  };

  class SourceFile :
    public SourceData {
  protected:
    Mappings srcmap;
    const char* path;
    const char* data;
    size_t length;
    size_t srcid;
  protected:
    sass::vector<size_t> lfs;
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

    const char* end() const override final;
    const char* begin() const override final;

    size_t size() const override final {
      return length;
    }

    virtual const char* getPath() const override {
      return path;
    }

    virtual size_t getSrcId() const override {
      return srcid;
    }

    virtual bool hasMapping(size_t srcMapPos) const override {
      return srcMapPos < srcmap.size();
    }
    virtual const Mapping& getMapping(size_t srcMapPos) const override {
      return srcmap[srcMapPos];
    }

    virtual SourceSpan adjustSourceSpan(SourceSpan pstate) const override {
      return pstate;
    }


    virtual sass::vector<sass::string> getLines(
      size_t start, size_t length = 1)
        const override;

    virtual size_t countLines() override;
    virtual sass::string getLine(size_t line) override;

    sass::string to_string() const override final {
      return data;
    }
  };

  class SyntheticFile :
    public SourceFile {
  private:
    SourceFileObj around;
    SourceSpan pstate;
  public:

    SyntheticFile(const char* data,
      SourceFileObj around,
      SourceSpan pstate);

    SyntheticFile(const char* data,
      Mappings srcmap,
      SourceFileObj around,
      SourceSpan pstate);

    SourceSpan adjustSourceSpan(SourceSpan pstate) const override final;

    size_t countLines() override final;
    sass::string getLine(size_t line) override final;
    sass::vector<sass::string> getLines(
      size_t start, size_t length = 1)
        const override final;
  };

}

#endif
