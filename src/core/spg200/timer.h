#pragma once

#include "core/common.h"

class Irq;

class Timer {
public:
  Timer(Irq& irq);
  void Reset();
  void RunCycles(int cycles);

  Word GetTimebaseSetup();
  void SetTimebaseSetup(Word value);

  void ClearTimebaseCounter();

  Word GetTimerAData();
  void SetTimerAData(Word value);

  Word GetTimerAControl();
  void SetTimerAControl(Word value);

  Word GetTimerAEnabled();
  void SetTimerAEnabled(Word value);

  void ClearTimerAIrq();

  Word GetTimerBData();
  void SetTimerBData(Word value);

  Word GetTimerBControl();
  void SetTimerBControl(Word value);

  Word GetTimerBEnabled();
  void SetTimerBEnabled(Word value);

  void ClearTimerBIrq();

private:
  Irq& irq_;
  DivisibleClock<27000000, 32768> timer_clock_;
  int timer_a_divisor_ = -1;
  int timer_b_divisor_ = -1;

  bool timer_a_enabled_ = false;
  Word timer_a_data_ = 0;
  Word timer_a_preload_ = 0;

  bool timer_b_enabled_ = false;
  Word timer_b_data_ = 0;
  Word timer_b_preload_ = 0;

  void TickTimerA();
  void TickTimerB();
  void UpdateTimerADivisors();
  void UpdateTimerBDivisors();

  union TimebaseSetup {
    Word raw = 0;
    Bitfield<2, 2> tmb2;
    Bitfield<0, 2> tmb1;

    static constexpr Word WriteMask = 0x000f;
  } timebase_setup_;

  union TimerAControl {
    Word raw = 0;
    Bitfield<6, 4> pulse_ctrl;
    Bitfield<3, 3> source_b;
    Bitfield<0, 3> source_a;
  } timer_a_control_;

  union TimerBControl {
    Word raw = 0;
    Bitfield<6, 4> pulse_ctrl;
    Bitfield<0, 3> source_c;
  } timer_b_control_;
};