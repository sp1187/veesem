#pragma once

#include "core/common.h"

class BusInterface {
public:
  virtual ~BusInterface() = default;

  virtual Word ReadWord(Addr addr) = 0;
  virtual void WriteWord(Addr addr, Word value) = 0;
};