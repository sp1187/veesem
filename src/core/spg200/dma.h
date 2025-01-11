#pragma once

#include "core/common.h"

class BusInterface;

class Dma {
public:
  Dma(BusInterface& bus);

  void Reset();

  word_t GetSourceHi();
  void SetSourceHi(word_t value);
  word_t GetSourceLo();
  void SetSourceLo(word_t value);
  word_t GetLength();
  void StartDma(word_t length);
  word_t GetTarget();
  void SetTarget(word_t value);

private:
  addr_t source_ = 0;
  word_t target_ = 0;
  word_t length_ = 0;
  BusInterface& bus_;
};