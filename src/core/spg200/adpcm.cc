#include "adpcm.h"

#include <algorithm>

namespace {
static const int StepSizeTable[89] = {
    7,     8,     9,     10,    11,    12,    13,    14,    16,    17,    19,    21,    23,
    25,    28,    31,    34,    37,    41,    45,    50,    55,    60,    66,    73,    80,
    88,    97,    107,   118,   130,   143,   157,   173,   190,   209,   230,   253,   279,
    307,   337,   371,   408,   449,   494,   544,   598,   658,   724,   796,   876,   963,
    1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,  2272,  2499,  2749,  3024,  3327,
    3660,  4026,  4428,  4871,  5358,  5894,  6484,  7132,  7845,  8630,  9493,  10442, 11487,
    12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767,
};

static const int StepAdjustTable[8] = {-1, -1, -1, -1, 2, 4, 6, 8};
}  // namespace

void Adpcm::Reset() {
  step_index_ = 0;
  last_sample_ = 0;
}

int16_t Adpcm::Decode(uint8_t code) {
  int ss = StepSizeTable[step_index_];
  int e =
      ss / 8 + ((code & 0x1) ? ss / 4 : 0) + ((code & 0x2) ? ss / 2 : 0) + ((code & 0x4) ? ss : 0);
  if (code & 0x8) {
    e = -e;
  }
  int sample = std::clamp(last_sample_ + e, -32768, 32767);

  last_sample_ = sample;
  step_index_ = std::clamp(step_index_ + StepAdjustTable[code & 0x07], 0, 88);

  return sample;
}
