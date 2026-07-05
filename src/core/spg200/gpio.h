#pragma once

#include <array>

#include "core/common.h"

class Spg200Io;

class Gpio {
public:
  Gpio(Spg200Io& vsmile_io);
  void Reset();

  Word GetMode();
  void SetMode(Word value);

  Word GetData(int port_index);

  Word GetBuffer(int port_index);
  void SetBuffer(int port_index, Word value);

  Word GetDir(int port_index);
  void SetDir(int port_index, Word value);

  Word GetAttrib(int port_index);
  void SetAttrib(int port_index, Word value);

  Word GetMask(int port_index);
  void SetMask(int port_index, Word value);

private:
  Word ReadGpio(int port_index);
  void WriteGpio(int port_index);

  union IoMode {
    Word raw;
    Bitfield<4, 1> ioc_wake;
    Bitfield<3, 1> iob_wake;
    Bitfield<2, 1> ioa_wake;
    Bitfield<1, 1> iob_special;
    Bitfield<0, 1> ioa_special;

    static const Word WriteMask = 0x001f;
  } mode_;

  struct Port {
    Word buffer = 0;
    Word dir = 0;
    Word attrib = 0;
    Word mask = 0;
  };

  std::array<Port, 3> ports_ = {};

  Spg200Io& io_;
};