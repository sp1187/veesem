#pragma once

#include "core/common.h"

class Cpu;

class Watchdog {
public:
  Watchdog(Cpu& cpu) : cpu_(cpu) {}

  void RunCycles(int cycles);
  void Reset();

  void SetEnabled(bool enabled);

  void ClearTimer(word_t value);

private:
  Cpu& cpu_;
  bool enabled_ = false;

  SimpleClock<27000000 * 3 / 4> clock_;  // 0.75 s
};