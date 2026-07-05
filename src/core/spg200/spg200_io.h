#pragma once

#include "core/common.h"

class Spg200Io {
public:
  virtual ~Spg200Io() = default;

  virtual void RunCycles(int cycles) = 0;

  virtual unsigned GetAdc0() = 0;
  virtual unsigned GetAdc1() = 0;
  virtual unsigned GetAdc2() = 0;
  virtual unsigned GetAdc3() = 0;

  virtual Word GetPortA() = 0;
  virtual void SetPortA(Word value, Word mask) = 0;
  virtual Word GetPortB() = 0;
  virtual void SetPortB(Word value, Word mask) = 0;
  virtual Word GetPortC() = 0;
  virtual void SetPortC(Word value, Word mask) = 0;

  virtual Word ReadRomCsb(Addr addr) = 0;
  virtual void WriteRomCsb(Addr addr, Word value) = 0;
  virtual Word ReadCsb1(Addr addr) = 0;
  virtual void WriteCsb1(Addr addr, Word value) = 0;
  virtual Word ReadCsb2(Addr addr) = 0;
  virtual void WriteCsb2(Addr addr, Word value) = 0;
  virtual Word ReadCsb3(Addr addr) = 0;
  virtual void WriteCsb3(Addr addr, Word value) = 0;

  virtual void TxUart(uint8_t value) = 0;
  virtual void RxUartDone() = 0;
};