#pragma once

#include <array>
#include <memory>

#include "core/common.h"

class Spg200Io;

class Extmem {
public:
  Extmem(Spg200Io& io);

  void Reset();

  Word GetControl();
  void SetControl(Word value);

  Word ReadWord(Addr addr);
  void WriteWord(Addr addr, Word value);

private:
  union ExternalMemControl {
    Word raw;
    Bitfield<8, 4> ram_decode;
    Bitfield<6, 2> address_decode;
    Bitfield<3, 3> bus_arbiter;
    Bitfield<1, 2> wait_state;

    static const Word WriteMask = 0x0ffe;
  } ctrl_;

  Spg200Io& io_;
};