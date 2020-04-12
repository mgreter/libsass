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
