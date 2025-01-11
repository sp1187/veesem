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

  virtual word_t GetPortA() = 0;
  virtual void SetPortA(word_t value, word_t mask) = 0;
  virtual word_t GetPortB() = 0;
  virtual void SetPortB(word_t value, word_t mask) = 0;
  virtual word_t GetPortC() = 0;
  virtual void SetPortC(word_t value, word_t mask) = 0;

  virtual word_t ReadRomCsb(addr_t addr) = 0;
  virtual void WriteRomCsb(addr_t addr, word_t value) = 0;
  virtual word_t ReadCsb1(addr_t addr) = 0;
  virtual void WriteCsb1(addr_t addr, word_t value) = 0;
  virtual word_t ReadCsb2(addr_t addr) = 0;
  virtual void WriteCsb2(addr_t addr, word_t value) = 0;
  virtual word_t ReadCsb3(addr_t addr) = 0;
  virtual void WriteCsb3(addr_t addr, word_t value) = 0;

  virtual void TxUart(uint8_t value) = 0;
  virtual void RxUartDone() = 0;
};