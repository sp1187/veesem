#pragma once

#include <cstdint>
#include <unordered_map>

class Adpcm {
public:
  Adpcm() = default;
  void Reset();
  int16_t Decode(uint8_t nibble);

private:
  int8_t step_index_ = 0;
  int16_t last_sample_ = 0;
};