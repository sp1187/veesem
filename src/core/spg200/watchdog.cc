#include "watchdog.h"

#include "cpu.h"

void Watchdog::Reset() {
  enabled_ = false;
  clock_.Reset();
}

void Watchdog::RunCycles(int cycles) {
  if (!enabled_)
    return;

  if (clock_.Tick(cycles)) {
    cpu_.Reset();
  }
}

void Watchdog::SetEnabled(bool enable) {
  if (enable && !enabled_) {
    clock_.Reset();
  }
  enabled_ = enable;
}

void Watchdog::ClearTimer(word_t value) {
  if (!enabled_ || value != 0x55aa)
    return;

  clock_.Reset();
}