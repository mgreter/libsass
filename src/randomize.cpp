#include "sass.hpp"
#include <random>
#include "randomize.hpp"

#if defined (_MSC_VER) // Visual studio
#define thread_local __declspec( thread )
#elif defined (__GCC__) // GCC
#define thread_local __thread
#endif

namespace Sass {

  // ToDo: this seems not thread safe!!
  uint32_t readHashSeed()
  {
    // Load optional fixed seed from environment
    // Mainly used to pass the seed to plugins
    const char* seed = GET_ENV("SASS_HASH_SEED");
    if (seed != nullptr) {
      return atol(seed);
    }
    else {
      // This is expensive!
      std::random_device rd;
      return rd();
    }
  }

  // Just a static wrapped around random device
  // Creates one true random number to seed us
  uint32_t getHashSeed()
  {
    // Lets hope this is indeed thread safe
    static uint32_t seed = readHashSeed();
    return seed; // static seed per thread
  }

  std::mt19937& getRng()
  {
    // Lets hope this is indeed thread safe (seed once)
    static std::mt19937 rng(getHashSeed());
    return rng; // static twister per thread
  }

  int getRandomInt(int low, int high)
  {
    static std::uniform_int_distribution<int> urd;
    return urd(getRng(), decltype(urd)::param_type{ low, high });
  }

  float getRandomFloat(float low, float high)
  {
    static std::uniform_real_distribution<float> urd;
    return urd(getRng(), decltype(urd)::param_type{ low, high });
  }

  double getRandomDouble(double low, double high)
  {
    static std::uniform_real_distribution<double> urd;
    return urd(getRng(), decltype(urd)::param_type{ low, high });
  }

  // Get full 32bit random data
  uint32_t getRandomUint32()
  {
    return getRng()();
  }

}
