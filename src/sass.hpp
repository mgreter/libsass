// must be the first include in all compile units
#ifndef SASS_SASS_H
#define SASS_SASS_H

// #define GOFAST

// undefine extensions macro to tell sys includes
// that we do not want any macros to be exported
// mainly fixes an issue on SmartOS (SEC macro)
#undef __EXTENSIONS__

#ifdef _MSC_VER
#pragma warning(disable : 4005)
#endif

// aplies to MSVC and MinGW
#ifdef _WIN32
// we do not want the ERROR macro
# ifndef NOGDI
#  define NOGDI
# endif
// we do not want the min/max macro
# ifndef NOMINMAX
#  define NOMINMAX
# endif
// we do not want the IN/OUT macro
# ifndef _NO_W32_PSEUDO_MODIFIERS
#  define _NO_W32_PSEUDO_MODIFIERS
# endif
#endif


// should we be case insensitive
// when dealing with files or paths
#ifndef FS_CASE_SENSITIVE
# ifdef _WIN32
#  define FS_CASE_SENSITIVE 0
# else
#  define FS_CASE_SENSITIVE 1
# endif
#endif

// path separation char
#ifndef PATH_SEP
# ifdef _WIN32
#  define PATH_SEP ';'
# else
#  define PATH_SEP ':'
# endif
#endif


// include C-API header
#include "sass/base.h"
#include "allocator.hpp"
#include "randomize.hpp"

// For C++ helper
#include <string>
#include <cstdint>
#include <vector>
#include <deque>

// output behaviours
namespace Sass {

  inline uint32_t MurmurHash2X(const void* key, int len, uint32_t seed)
  {
    // 'm' and 'r' are mixing constants generated offline.
    // They're not really 'magic', they just happen to work well.

    // 0101 1111
    // 0010 1101

    const uint32_t m = 0x5bd1e995;
    const int r = 24;

    // Initialize the hash to a 'random' value

    uint32_t h = seed ^ len;

    // Mix 4 bytes at a time into the hash

    const unsigned char* data = (const unsigned char*)key;

    while (len >= 4)
    {
      uint32_t k = *(uint32_t*)data;

      // Create a view into k
      uint8_t* x = (uint8_t*)& k;
      if (x[0] == 95) x[0] = 45;
      if (x[1] == 95) x[1] = 45;
      if (x[2] == 95) x[2] = 45;
      if (x[3] == 95) x[3] = 45;

      k *= m;
      k ^= k >> r;
      k *= m;

      h *= m;
      h ^= k;

      data += 4;
      len -= 4;
    }

    // Handle the last few bytes of the input array

    switch (len)
    {
    case 3:
      if (data[2] == 95) h ^= 45 << 16;
      else h ^= data[2] << 16;
    case 2:
      if (data[1] == 95) h ^= 45 << 8;
      else h ^= data[1] << 8;
    case 1:
      if (data[0] == 95) h ^= 45;
      else h ^= data[0];
      h *= m;
    };

    // Do a few final mixes of the hash to ensure the last few
    // bytes are well-incorporated.

    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
  }

  // template <typename T> using VECTOR = std::vector<T/*, YourAlloc<T>*/>;

  // ToDo: Call it EnvKey?
  class EnvString {

  private:

    sass::string _orig;
    sass::string _norm;
    size_t _hash;

    inline void update()
    {
      // Normalize string
      std::replace(
        _norm.begin(),
        _norm.end(),
        '_', '-');
      // Calculate hash
      // ToDo: Randomize seed
      _hash = MurmurHash2(
        (void*)_norm.c_str(),
        (int)_norm.size(),
        getHashSeed());
    }

  public:

    // Empty constructor
    EnvString() {}

    EnvString(const sass::string& orig) :
      _orig(orig),
      _norm(orig),
      _hash(0)
    {
      update();
    }

    EnvString(sass::string&& orig) :
      _orig(std::move(orig)),
      _norm(_orig),
      _hash(0)
    {
      update();
    }

    EnvString(const EnvString& nstr) :
      _orig(nstr._orig),
      _norm(nstr._norm),
      _hash(nstr._hash)
    {
    }

    EnvString(EnvString&& nstr) noexcept :
      _orig(std::move(nstr._orig)),
      _norm(std::move(nstr._norm)),
      _hash(std::move(nstr._hash))
    {
    }

    EnvString& operator=(const EnvString& nstr)
    {
      _orig = nstr._orig;
      _norm = nstr._norm;
      _hash = nstr._hash;
      return *this;
    }

    EnvString& operator=(EnvString&& nstr)
    {
      _orig = std::move(nstr._orig);
      _norm = std::move(nstr._norm);
      _hash = std::move(nstr._hash);
      return *this;
    }
    // Simple getter functions
    const sass::string& orig() const { return _orig; }
    const sass::string& norm() const { return _norm; }
    size_t hash() const { return _hash; }

  };

  struct hashNormalizedString {
    inline size_t operator()(const EnvString& str) const
    {
      return str.hash();
    }
  };

  struct equalsNormalizedString {
    bool operator() (const EnvString& lhs, const EnvString& rhs) const {
      return lhs.norm() ==  rhs.norm();
    }
  };

  #define SassIgnoreDashPair hashNormalizedString, equalsNormalizedString
  // #define SassIgnoreDashPair SSTRHASH, std::equal_to<sass::string>

  // create some C++ aliases for the most used options
  const static Sass_Output_Style NESTED = SASS_STYLE_NESTED;
  const static Sass_Output_Style COMPACT = SASS_STYLE_COMPACT;
  const static Sass_Output_Style EXPANDED = SASS_STYLE_EXPANDED;
  const static Sass_Output_Style COMPRESSED = SASS_STYLE_COMPRESSED;
  // only used internal to trigger ruby inspect behavior
  const static Sass_Output_Style INSPECT = SASS_STYLE_INSPECT;
  const static Sass_Output_Style TO_SASS = SASS_STYLE_TO_SASS;
  const static Sass_Output_Style TO_CSS = SASS_STYLE_TO_CSS;

  // helper to aid dreaded MSVC debug mode
  // see implementation for more details
  char* sass_copy_string(sass::string str);

}

// input behaviours
enum Sass_Input_Style {
  SASS_CONTEXT_NULL,
  SASS_CONTEXT_FILE,
  SASS_CONTEXT_DATA,
  SASS_CONTEXT_FOLDER
};

// simple linked list
struct string_list {
  string_list* next;
  char* string;
};

// sass config options structure
struct Sass_Inspect_Options {

  // Output style for the generated css code
  // A value from above SASS_STYLE_* constants
  enum Sass_Output_Style output_style;

  // Precision for fractional numbers
  int precision;

  // initialization list (constructor with defaults)
  Sass_Inspect_Options(Sass_Output_Style style = Sass::NESTED,
                       int precision = 10)
  : output_style(style), precision(precision)
  { }

};

// sass config options structure
struct Sass_Output_Options : Sass_Inspect_Options {

  // String to be used for indentation
  const char* indent;
  // String to be used to for line feeds
  const char* linefeed;

  // Emit comments in the generated CSS indicating
  // the corresponding source line.
  bool source_comments;

  // initialization list (constructor with defaults)
  Sass_Output_Options(struct Sass_Inspect_Options opt,
                      const char* indent = "  ",
                      const char* linefeed = "\n",
                      bool source_comments = false)
  : Sass_Inspect_Options(opt),
    indent(indent), linefeed(linefeed),
    source_comments(source_comments)
  { }

  // initialization list (constructor with defaults)
  Sass_Output_Options(Sass_Output_Style style = Sass::NESTED,
                      int precision = 10,
                      const char* indent = "  ",
                      const char* linefeed = "\n",
                      bool source_comments = false)
  : Sass_Inspect_Options(style, precision),
    indent(indent), linefeed(linefeed),
    source_comments(source_comments)
  { }

};

#include <cassert>

#ifdef NDEBUG
#define SASS_ASSERT(cond, msg) ((void)0)
#else
#ifdef DEBUG
#define SASS_ASSERT(cond, msg) assert(cond && msg)
#else
#define SASS_ASSERT(cond, msg) ((void)0)
#endif
#endif

#endif
