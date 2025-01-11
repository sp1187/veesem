#pragma once

#include <array>
#include <memory>

#include "core/common.h"

class Spg200Io;

class Extmem {
public:
  Extmem(Spg200Io& io);

  void Reset();

  word_t GetControl();
  void SetControl(word_t value);

  word_t ReadWord(addr_t addr);
  void WriteWord(addr_t addr, word_t value);

private:
  union ExternalMemControl {
    word_t raw;
    Bitfield<8, 4> ram_decode;
    Bitfield<6, 2> address_decode;
    Bitfield<3, 3> bus_arbiter;
    Bitfield<1, 2> wait_state;

    static const word_t WriteMask = 0x0ffe;
  } ctrl_;

  Spg200Io& io_;
};