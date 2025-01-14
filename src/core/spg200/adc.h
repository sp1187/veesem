#pragma once

#include "core/common.h"

class Irq;
class Spg200Io;

class Adc {
public:
  Adc(Irq& irq, Spg200Io& spg200);

  void Reset();
  void RunCycles(int cycles);

  void SetControl(word_t value);
  word_t GetControl();

  word_t GetData();

private:
  union AdcControl {
    word_t raw;
    Bitfield<12, 1> request;
    Bitfield<10, 1> req_auto_8k;
    Bitfield<9, 1> int_enable;
    Bitfield<8, 1> vrt;
    Bitfield<4, 2> channel;
    Bitfield<2, 2> clock;
    Bitfield<1, 1> cs;
    Bitfield<0, 1> enabled;

    static const word_t WriteMask = 0x177f;
  } ctrl_;

  union AdcControlStatus {
    word_t raw;
    Bitfield<13, 1> irq;

    static const word_t WriteMask = 0x2000;
  } status_;

  union AdcData {
    word_t raw;
    Bitfield<15, 1> ready;
    Bitfield<0, 10> data;
  } data_;

  DivisibleClock<16, 1> adc_clock_;
  int active_channel_;

  Irq& irq_;
  Spg200Io& io_;
};