#pragma once

#include "core/common.h"

class BusInterface {
public:
  virtual ~BusInterface() = default;

  virtual word_t ReadWord(addr_t addr) = 0;
  virtual void WriteWord(addr_t addr, word_t value) = 0;
};