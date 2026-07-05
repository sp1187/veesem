#pragma once

#include "core/common.h"

class Cpu;

class Irq {
public:
  Irq(Cpu& cpu);

  void Reset();

  Word GetIoIrqControl();
  void SetIoIrqControl(Word value);

  Word GetIoIrqStatus();
  void ClearIoIrqStatus(Word value);

  Word GetFiqSelect();
  void SetFiqSelect(Word value);

  void SetPpuIrq(bool val);
  void SetSpuChannelIrq(bool val);
  void SetSpuBeatIrq(bool val);

  // Generic ("IO") interrupts
  void Set4HzIrq(bool val);
  void Set1024HzIrq(bool val);
  void Set2048HzIrq(bool val);
  void Set4096HzIrq(bool val);
  void SetTmb1Irq(bool val);
  void SetTmb2Irq(bool val);
  void SetTimerAIrq(bool val);
  void SetTimerBIrq(bool val);
  void SetAdcIrq(bool val);
  void SetUartIrq(bool val);
  void SetExt1Irq(bool val);
  void SetExt2Irq(bool val);

private:
  Cpu& cpu_;

  union IoInterrupts {
    Word raw;
    Bitfield<14, 1> spi;
    Bitfield<13, 1> adc;
    Bitfield<12, 1> ext2;
    Bitfield<11, 1> timer_a;
    Bitfield<10, 1> timer_b;
    Bitfield<9, 1> ext1;
    Bitfield<8, 1> uart;
    Bitfield<7, 1> key_change;
    Bitfield<6, 1> tick_4096hz;
    Bitfield<5, 1> tick_2048hz;
    Bitfield<4, 1> tick_1024hz;
    Bitfield<3, 1> tick_4hz;
    Bitfield<1, 1> tmb2;
    Bitfield<0, 1> tmb1;
  } io_irq_ctrl_, io_irq_status_;

  uint8_t fiq_select_;
  bool ppu_active_;
  bool spu_channel_active_;

  void UpdateIrq2();
  void UpdateIrq3();
  void UpdateIrq5();
  void UpdateIrq6();
  void UpdateIrq7();
  void UpdateFiq();
  IoInterrupts GetActiveIoIrqs() const;
};