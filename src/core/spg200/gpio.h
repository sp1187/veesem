#pragma once

#include <array>

#include "core/common.h"

class Spg200Io;

class Gpio {
public:
  Gpio(Spg200Io& vsmile_io);
  void Reset();

  word_t GetMode();
  void SetMode(word_t value);

  word_t GetData(int port_index);

  word_t GetBuffer(int port_index);
  void SetBuffer(int port_index, word_t value);

  word_t GetDir(int port_index);
  void SetDir(int port_index, word_t value);

  word_t GetAttrib(int port_index);
  void SetAttrib(int port_index, word_t value);

  word_t GetMask(int port_index);
  void SetMask(int port_index, word_t value);

private:
  word_t ReadGpio(int port_index);
  void WriteGpio(int port_index);

  union IoMode {
    word_t raw;
    Bitfield<4, 1> ioc_wake;
    Bitfield<3, 1> iob_wake;
    Bitfield<2, 1> ioa_wake;
    Bitfield<1, 1> iob_special;
    Bitfield<0, 1> ioa_special;

    static const word_t WriteMask = 0x001f;
  } mode_;

  struct Port {
    word_t buffer = 0;
    word_t dir = 0;
    word_t attrib = 0;
    word_t mask = 0;
  };

  std::array<Port, 3> ports_ = {};

  Spg200Io& io_;
};