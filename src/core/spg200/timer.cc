#include "timer.h"

#include "irq.h"

Timer::Timer(Irq& irq) : irq_(irq) {}

void Timer::Reset() {
  timer_clock_.Reset();

  timebase_setup_.raw = 0;

  timer_a_divisor_ = -1;
  timer_a_data_ = 0;
  timer_a_preload_ = 0;
  timer_a_enabled_ = false;
  timer_a_control_.raw = 0;
  timer_a_control_.source_a = 6;
  timer_a_control_.source_b = 6;
  UpdateTimerADivisors();

  timer_b_divisor_ = -1;
  timer_b_data_ = 0;
  timer_b_preload_ = 0;
  timer_b_enabled_ = false;
  timer_b_control_.source_c = 6;
  UpdateTimerBDivisors();
}

void Timer::RunCycles(int cycles) {
  while (timer_clock_.Tick(cycles)) {  // 32768 Hz tick
    if (timer_a_enabled_ && timer_a_divisor_ >= 0 && timer_clock_.GetDividedTick(timer_a_divisor_))
      TickTimerA();

    if (timer_b_enabled_ && timer_b_divisor_ >= 0 && timer_clock_.GetDividedTick(timer_b_divisor_))
      TickTimerB();

    if (!timer_clock_.GetDividedTick(3))
      continue;

    irq_.Set4096HzIrq(true);

    if (!timer_clock_.GetDividedTick(4))
      continue;

    irq_.Set2048HzIrq(true);

    if (!timer_clock_.GetDividedTick(5))
      continue;

    irq_.Set1024HzIrq(true);

    if (timer_clock_.GetDividedTick(8 - timebase_setup_.tmb2))
      irq_.SetTmb2Irq(true);

    if (timer_clock_.GetDividedTick(12 - timebase_setup_.tmb1))
      irq_.SetTmb1Irq(true);

    if (!timer_clock_.GetDividedTick(13))
      continue;

    irq_.Set4HzIrq(true);
  }
}

word_t Timer::GetTimerAData() {
  return timer_a_data_;
}

void Timer::SetTimerAData(word_t value) {
  timer_a_preload_ = value;
  timer_a_data_ = value;
}

word_t Timer::GetTimerAControl() {
  return timer_a_control_.raw;
}

void Timer::SetTimerAControl(word_t value) {
  timer_a_control_.raw = value;

  UpdateTimerADivisors();
}

word_t Timer::GetTimerAEnabled() {
  return timer_a_enabled_;
}

void Timer::SetTimerAEnabled(word_t value) {
  timer_a_enabled_ = value & 1;
}

void Timer::ClearTimerAIrq() {
  irq_.SetTimerAIrq(false);
}

word_t Timer::GetTimerBData() {
  return timer_b_data_;
}

void Timer::SetTimerBData(word_t value) {
  timer_b_preload_ = value;
  timer_b_data_ = value;
}

word_t Timer::GetTimerBControl() {
  return timer_b_control_.raw;
}

void Timer::SetTimerBControl(word_t value) {
  timer_b_control_.raw = value;

  UpdateTimerBDivisors();
}

word_t Timer::GetTimerBEnabled() {
  return timer_b_enabled_;
}

void Timer::SetTimerBEnabled(word_t value) {
  timer_b_enabled_ = value & 1;
}

void Timer::ClearTimerBIrq() {
  irq_.SetTimerBIrq(false);
}

word_t Timer::GetTimebaseSetup() {
  return timebase_setup_.raw;
}

void Timer::SetTimebaseSetup(word_t value) {
  timebase_setup_.raw = value & timebase_setup_.WriteMask;

  UpdateTimerADivisors();
}

void Timer::ClearTimebaseCounter() {
  timer_clock_.ClearDivCounter();
}

void Timer::TickTimerA() {
  timer_a_data_++;
  if (timer_a_data_ == 0) {
    timer_a_data_ = timer_a_preload_;
    irq_.SetTimerAIrq(true);
  }
}

void Timer::TickTimerB() {
  timer_b_data_++;
  if (timer_b_data_ == 0) {
    timer_b_data_ = timer_b_preload_;
    irq_.SetTimerBIrq(true);
  }
}

void Timer::UpdateTimerADivisors() {
  timer_a_divisor_ = -1;

  if (timer_a_control_.source_a == 0 || timer_a_control_.source_a == 1 ||
      timer_a_control_.source_a == 6 || timer_a_control_.source_a == 7)
    return;

  // TODO: combine two clocks through AND
  if (timer_a_control_.source_a == 5) {
    switch (timer_a_control_.source_b) {
      case 0:  // 2048 Hz
        timer_a_divisor_ = 4;
        break;
      case 1:  // 1024 Hz
        timer_a_divisor_ = 5;
        break;
      case 2:  // 256 Hz
        timer_a_divisor_ = 7;
        break;
      case 3:  // TMB1
        timer_a_divisor_ = 12 - timebase_setup_.tmb1;
        break;
      case 4:  // 4 Hz
        timer_a_divisor_ = 13;
        break;
      case 5:  // 2 Hz
        timer_a_divisor_ = 14;
        break;
    }
  } else if (timer_a_control_.source_b == 6) {
    switch (timer_a_control_.source_a) {
      case 2:  // 32768 Hz
        timer_a_divisor_ = 0;
        break;
      case 3:  // 8192 Hz
        timer_a_divisor_ = 2;
        break;
      case 4:  // 4096 Hz
        timer_a_divisor_ = 3;
        break;
    }
  } else {
    die("Unsupported timer setting");
  }
}

void Timer::UpdateTimerBDivisors() {
  timer_b_divisor_ = -1;

  switch (timer_b_control_.source_c) {
    case 2:  // 32768 Hz
      timer_b_divisor_ = 0;
      break;
    case 3:  // 8192 Hz
      timer_b_divisor_ = 2;
      break;
    case 4:  // 4096 Hz
      timer_b_divisor_ = 3;
      break;
  }
}