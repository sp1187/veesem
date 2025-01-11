#pragma once

#include "core/common.h"

class Irq;

class Timer {
public:
  Timer(Irq& irq);
  void Reset();
  void RunCycles(int cycles);

  word_t GetTimebaseSetup();
  void SetTimebaseSetup(word_t value);

  void ClearTimebaseCounter();

  word_t GetTimerAData();
  void SetTimerAData(word_t value);

  word_t GetTimerAControl();
  void SetTimerAControl(word_t value);

  word_t GetTimerAEnabled();
  void SetTimerAEnabled(word_t value);

  void ClearTimerAIrq();

  word_t GetTimerBData();
  void SetTimerBData(word_t value);

  word_t GetTimerBControl();
  void SetTimerBControl(word_t value);

  word_t GetTimerBEnabled();
  void SetTimerBEnabled(word_t value);

  void ClearTimerBIrq();

private:
  Irq& irq_;
  DivisibleClock<27000000, 32768> timer_clock_;
  int timer_a_divisor_ = -1;
  int timer_b_divisor_ = -1;

  bool timer_a_enabled_ = false;
  word_t timer_a_data_ = 0;
  word_t timer_a_preload_ = 0;

  bool timer_b_enabled_ = false;
  word_t timer_b_data_ = 0;
  word_t timer_b_preload_ = 0;

  void TickTimerA();
  void TickTimerB();
  void UpdateTimerADivisors();
  void UpdateTimerBDivisors();

  union TimebaseSetup {
    word_t raw = 0;
    Bitfield<2, 2> tmb2;
    Bitfield<0, 2> tmb1;

    static constexpr word_t WriteMask = 0x000f;
  } timebase_setup_;

  union TimerAControl {
    word_t raw = 0;
    Bitfield<6, 4> pulse_ctrl;
    Bitfield<3, 3> source_b;
    Bitfield<0, 3> source_a;
  } timer_a_control_;

  union TimerBControl {
    word_t raw = 0;
    Bitfield<6, 4> pulse_ctrl;
    Bitfield<0, 3> source_c;
  } timer_b_control_;
};