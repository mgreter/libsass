#ifndef SASS_ENVIRONMENT_KEY_H
#define SASS_ENVIRONMENT_KEY_H

// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

namespace Sass {

  class EnvKey {

  private:

    // The original name
    sass::string _orig;
    // The normalized name
    sass::string _norm;

    // Lazy calculated hash
    mutable size_t _hash;

    // Called by constructors
    inline void normalize()
    {
      // Normalize string
      std::replace(
        _norm.begin(),
        _norm.end(),
        '_', '-');
    }

  public:

    // Empty constructor
    EnvKey() :
      _hash(0)
    {}

    // String copy constructor
    EnvKey(const sass::string& orig) :
      _orig(orig),
      _norm(orig),
      _hash(0)
    {
      normalize();
    }

    // String move constructor
    EnvKey(sass::string&& orig) :
      _orig(std::move(orig)),
      _norm(_orig),
      _hash(0)
    {
      normalize();
    }

    // Copy constructor
    EnvKey(const EnvKey& nstr) :
      _orig(nstr._orig),
      _norm(nstr._norm),
      _hash(nstr._hash)
    {
    }

    // Move constructor
    EnvKey(EnvKey&& nstr) noexcept :
      _orig(std::move(nstr._orig)),
      _norm(std::move(nstr._norm)),
      _hash(std::move(nstr._hash))
    {
    }

    // Copy assignment operator
    EnvKey& operator=(const EnvKey& nstr)
    {
      _orig = nstr._orig;
      _norm = nstr._norm;
      _hash = nstr._hash;
      return *this;
    }

    // Move assignment operator
    EnvKey& operator=(EnvKey&& nstr) noexcept
    {
      _orig = std::move(nstr._orig);
      _norm = std::move(nstr._norm);
      _hash = std::move(nstr._hash);
      return *this;
    }

    // Compare normalization forms
    bool operator==(const EnvKey& rhs) const
    {
      return norm() == rhs.norm();
    }

    // Simple constant getter functions
    const sass::string& orig() const { return _orig; }
    const sass::string& norm() const { return _norm; }

    // Calculate only on demand
    size_t hash() const {
      if (_hash == 0) {
        _hash = MurmurHash2(
          (void*)_norm.c_str(),
          (int)_norm.size(),
          getHashSeed());
      }
      return _hash;
    }

  };

  struct hashEnvKey {
    inline size_t operator()(const EnvKey& str) const
    {
      return str.hash();
    }
  };

  struct equalsEnvKey {
    bool operator() (const EnvKey& lhs, const EnvKey& rhs) const {
      return lhs.norm() == rhs.norm();
    }
  };

};

#endif


/*

  inline uint32_t MurmurHash2X(const void* key, int len, uint32_t seed)
  {
    // 'm' and 'r' are mixing constants generated off-line.
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
      uint8_t* x = (uint8_t*)&k;
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

*/
