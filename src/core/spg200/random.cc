#include "random.h"

#include <cstdlib>

void Random::Set(word_t value) {
  seed_ = value;
}
word_t Random::Get() {
  return rand() & 0x7fff;
  // word_t value = seed;
  // update_seed();
  // return value;
}

void Random::UpdateSeed() {
  seed_ <<= 1;
  bool shifted_in = ((seed_ & 0x8000) != 0) ^ ((seed_ & 0x4000) != 0);
  seed_ = (seed_ & 0x7FFF) | shifted_in;
}
