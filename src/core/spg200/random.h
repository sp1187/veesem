#pragma once

#include "core/common.h"

class Random {
public:
  void Set(word_t value);
  word_t Get();

private:
  void UpdateSeed();
  word_t seed_ = 0;
};