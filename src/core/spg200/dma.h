#pragma once

#include "core/common.h"

class BusInterface;

class Dma {
public:
  Dma(BusInterface& bus);

  void Reset();

  Word GetSourceHi();
  void SetSourceHi(Word value);
  Word GetSourceLo();
  void SetSourceLo(Word value);
  Word GetLength();
  void StartDma(Word length);
  Word GetTarget();
  void SetTarget(Word value);

private:
  Addr source_ = 0;
  Word target_ = 0;
  Word length_ = 0;
  BusInterface& bus_;
};