// must be the first include in all compile units
#ifndef SASS_RAND_SEED_H
#define SASS_RAND_SEED_H

#include <cstdint>
#include <limits>

#ifdef _WIN32

#define GET_ENV(key) getenv(key)
#define SET_ENV(key, val) _putenv_s(key, val)

#else

#define GET_ENV(key) getenv(key)
#define SET_ENV(key, val) setenv(key, val, 0)

#endif

namespace Sass {

  // Just a static wrapped around random device
  // Creates one true random number to seed us
  uint32_t getHashSeed();

  // Pseudo random number generators
  int getRandomInt(int low = 0.0, int high = 1.0);
  float getRandomFloat(float low = 0.0, float high = 1.0);
  double getRandomDouble(double low = 0.0, double high = 1.0);

  // Get full 32bit random data
  uint32_t getRandomUint32();

};

#endif
