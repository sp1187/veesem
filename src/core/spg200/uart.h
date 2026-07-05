#pragma once

#include "core/common.h"

#include <array>

class Irq;
class Spg200Io;

class Uart {
public:
  Uart(Irq& irq, Spg200Io& io);

  void Reset();
  void RunCycles(int cycles);

  Word GetControl();
  void SetControl(Word value);

  Word GetStatus();
  void SetStatus(Word value);

  void SoftReset();

  Word GetBaudLo();
  void SetBaudLo(Word value);

  Word GetBaudHi();
  void SetBaudHi(Word value);

  Word GetTx();
  void Tx(Word value);

  Word Rx();
  Word PeekRx();  // variant without side effects

  void RxStart(uint8_t value);

private:
  Irq& irq_;
  Spg200Io& io_;

  union UartControl {
    Word raw;
    Bitfield<7, 1> tx_enable;
    Bitfield<6, 1> rx_enable;
    Bitfield<5, 1> mode;
    Bitfield<4, 1> mulpro;
    Bitfield<2, 2> bit9;
    Bitfield<1, 1> tx_irq_enable;
    Bitfield<0, 1> rx_irq_enable;

    static const Word WriteMask = 0xff;
  } control_;

  union UartStatus {
    Word raw;
    Bitfield<7, 1> rx_full;
    Bitfield<6, 1> tx_busy;
    Bitfield<5, 1> bit9;
    Bitfield<4, 1> overrun_err;
    Bitfield<3, 1> frame_err;
    Bitfield<2, 1> parity_err;
    Bitfield<1, 1> tx_ready;
    Bitfield<0, 1> rx_ready;

    static const Word ClearMask = 0x0003;
  } status_;

  uint8_t baud_lo_;
  uint8_t baud_hi_;
  uint8_t tx_buf_;
  uint8_t tx_running_;
  uint8_t rx_buf_;
  uint8_t rx_running_;

  int tx_counter_;
  int rx_counter_;
};